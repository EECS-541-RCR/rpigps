#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <math.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>

#define MAX_NMEA_SENTENCE_LEN 1024

#include "network.h"
#include "command.h"
#include "gpsutil.h"

typedef enum { false, true } bool;

// By default, wait for commands from Android device.
bool				autonomousMode = false;

GpsPoint			currGpsFix;		// Current GPS fix. Parsed from GPS device NMEA strings.
GpsPoint			prevGpsFix;		// Previous GPS fix, used for heading estimation.
pthread_mutex_t		gpsFixMutex;	// Mutex for accessing curr/prev GpsFix structs.

pthread_t			gpsPollThread;			// Thread for getting GPS data from device.
pthread_t			droneAutopilotThread;		// Thread for sending commands to drone.
pthread_t			androidGpsUpdateThread;	// Thread for sending periodic updates to android.
pthread_t			androidCommandThread;	// Thread for getting Android directional commands.

void *gpsPoll( void *arg );
void *droneAutopilot( void *arg );
void *sendAndroidGpsUpdates( void *arg );
void *getAndroidCommands( void *arg );

int main( int argc, char **argv )
{
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

	pthread_mutex_init( &gpsFixMutex, NULL );

	// Note that droneCmdSock and droneCmdAddr are extern globals from command.h.
	// This socket is used by threads that send piloting commands to the drone, including
	// the autopilot thread and the android command thread.
	droneCmdSock = createUdpClientConnection( DRONE_IP, DRONE_COMMAND_PORT, &droneCmdAddr );
	if( droneCmdSock < 0 )
	{
		fprintf( stderr, "couldn't connect to drone at %s.\n", DRONE_IP );
		exit( EXIT_FAILURE );
	}
	else
	{
		printf( "Connected to drone at %s.\n", DRONE_IP );
	}

	pthread_create( &gpsPollThread, &attr, gpsPoll, (void *)NULL );
	pthread_create( &droneAutopilotThread, &attr, droneAutopilot, (void *)NULL );
	pthread_create( &androidGpsUpdateThread, &attr, sendAndroidGpsUpdates, (void *)NULL );
	pthread_create( &androidCommandThread, &attr, getAndroidCommands, (void *)NULL );

	void *status;
	pthread_join( gpsPollThread, &status );
	pthread_join( droneAutopilotThread, &status );
	pthread_join( androidGpsUpdateThread, &status );
	pthread_join( androidCommandThread, &status );

	pthread_mutex_destroy( &gpsFixMutex );
	pthread_attr_destroy( &attr );
	pthread_exit( NULL );
	return 0;
}

void *gpsPoll( void *arg )
{
	struct sockaddr_un saun;

	saun.sun_family = AF_UNIX;
	strcpy( saun.sun_path, "serial_rpigps_data" );

	int sockfd = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( sockfd < 0 )
	{
		fprintf( stderr, "gpsPoll() socket creation failure, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	if( connect( sockfd, (struct sockaddr *)&saun, sizeof( saun ) ) < 0 )
	{
		fprintf( stderr, "gpsPoll() couldn't connect to usbgps, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	for(;;)
	{
		char *buffer[MAX_BUFFER_SIZE];
		if( read( sockfd, buffer, MAX_BUFFER_SIZE ) < 0 )
		{
			fprintf( stderr, "Reading GPS data from usbgps failed, errno = %d.\n", errno );
			close( sockfd );
			exit( EXIT_FAILURE );
		}

		pthread_mutex_lock( &gpsFixMutex );
		prevGpsFix = currGpsFix;
		memcpy( (char *)&currGpsFix, buffer, sizeof( GpsPoint ) / sizeof( char ) );
		pthread_mutex_unlock( &gpsFixMutex );
	}
	
	pthread_exit( NULL );
}

void *droneAutopilot( void *arg )
{
	// Set static destination to Allen Fieldhouse. Make it dynamic later.
	GpsPoint destination;
	destination.latitude = 38.954352;
	destination.longitude = -95.252811;

	sleep( 5 );
	droneTakeOff();
	
	for(;;)
	{
		if( !autonomousMode )
		{
			continue;
		}

		GpsPoint currFix = currGpsFix;
		GpsPoint prevFix = prevGpsFix;

		int justRotated = false;
		if( getDistance( currFix, destination ) > LOCATION_EPSILON )
		{
			double desiredHeading = getBearing( currFix, destination );
			double currHeading = getHeading( currFix, prevFix );
			double headingError = ( ( currHeading + 360 ) - ( desiredHeading + 360 ) ) - 720;

			if( !justRotated && fabs( headingError ) > HEADING_EPSILON )
			{
				justRotated = true;
				if( headingError < 0 )
				{
					droneRotateRight();
				}
				else
				{
					droneRotateLeft();
				}
			}
			else
			{
				justRotated = false;
				droneForward();
			}
		}
		else
		{
			droneLand();
		}
	}

	pthread_exit( NULL );
}

void *sendAndroidGpsUpdates( void *arg )
{
	int updateSock = createTcpClientConnection( ANDROID_IP, ANDROID_GPS_UPDATE_PORT );
	if( updateSock < 0 )
	{
		fprintf( stderr, "Android GPS update thread couldn't connect to %s.\n", ANDROID_IP );
		exit( EXIT_FAILURE );
	}
	else
	{
		printf( "Android GPS update thread connected to %s.\n", ANDROID_IP );
	}

	int i = 0;
	for(;;)
	{
		char str[MAX_BUFFER_SIZE];
		sprintf( str, "%f %f %d", currGpsFix.latitude, currGpsFix.longitude, i++ );
		if( send( updateSock, str, sizeof( str ), 0 ) < 0 )
		{
			printf( "Error sending GPS update to Android device.\n" );
			exit( EXIT_FAILURE );
		}

		sleep( 1 );
	}

	pthread_exit( NULL );
}

void *getAndroidCommands( void *arg )
{
	int handshakeSocket;
	struct sockaddr_in myaddr;

	handshakeSocket = socket( PF_INET, SOCK_STREAM, 0 );
	if( handshakeSocket == -1 )
	{
		printf( "Android command thread: socket() failure, errno = %d\n", errno );
		exit( EXIT_FAILURE );
	}

	int yes = 1;
	if( setsockopt( handshakeSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( int ) ) == -1 )
	{
		printf( "Android command thread: setsockopt() failure, errno = %d\n", errno );
		close( handshakeSocket );
		exit( EXIT_FAILURE );
	}

	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons( atoi( ANDROID_COMMAND_PORT ) );
	myaddr.sin_addr.s_addr = htonl( INADDR_ANY );
	memset( &( myaddr.sin_zero ), '\0', 8 );

	if( bind( handshakeSocket, (struct sockaddr *)&myaddr, sizeof( struct sockaddr ) ) == -1 )
	{
		printf( "Android command thread: bind() failure, errno = %d\n", errno );
		close( handshakeSocket );
		exit( EXIT_FAILURE );
	}

	if( listen( handshakeSocket, 10 ) == -1 )
	{
		printf( "Android command thread: listen() failure, errno = %d\n", errno );
		close( handshakeSocket );
		exit( EXIT_FAILURE );
	}

	while( 1 )
	{
		struct sockaddr_in theiraddr;
		socklen_t theiraddr_size = sizeof( struct sockaddr_in );

		/* Accept the incoming connection request. */
		int connectionSocket = accept( handshakeSocket, (struct sockaddr *)&theiraddr, &theiraddr_size );
		if( connectionSocket == -1 )
		{
			printf( "Android command thread: accept() failure, errno = %d\n", errno );
			continue;
		}

		pid_t pid = fork();
		if( pid < 0 )
		{
			fprintf( stderr, "Android command server couldn't fork.\n" );
			exit( EXIT_FAILURE );
		}
		if( pid == 0 )
		{
			char buffer[MAX_BUFFER_SIZE];
			close( handshakeSocket );	// Child doesn't need this socket.

			while( 1 )
			{
				int size = recv( connectionSocket, buffer, MAX_BUFFER_SIZE, 0 );
				if( size == -1 )
				{
					printf( "recv() failure, errno = %d\n", errno );
					close( connectionSocket );
					exit( EXIT_FAILURE );
				}
				else if( size == 0 )
				{
					// Client disconnected.
					break;
				}
				else
				{
					buffer[size] = '\0';
					if( strcmp( buffer, "cmd takeoff" ) == 0 )
					{
						droneTakeOff();
					}
					else if( strcmp( buffer, "cmd land" ) == 0 )
					{
						droneLand();
					}
					else if( strcmp( buffer, "cmd moveforward" ) == 0 )
					{
						droneForward();
					}
					else if( strcmp( buffer, "cmd moveback" ) == 0 )
					{
						droneBack();
					}
					else if( strcmp( buffer, "cmd moveleft" ) == 0 )
					{
						droneLeft();
					}
					else if( strcmp( buffer, "cmd moveright" ) == 0 )
					{
						droneRight();
					}
					else if( strcmp( buffer, "cmd moveup" ) == 0 )
					{
						droneUp();
					}
					else if( strcmp( buffer, "cmd movedown" ) == 0 )
					{
						droneDown();
					}
					else if( strcmp( buffer, "cmd turnleft" ) == 0 )
					{
						droneRotateLeft();
					}
					else if( strcmp( buffer, "cmd turnright" ) == 0 )
					{
						droneRotateRight();
					}
					else
					{
						fprintf( stderr, "Unrecognized Android command: %s\n", buffer );
					}
				}
			}

			close( connectionSocket );
			exit( EXIT_SUCCESS );
		}
		else
		{
			// Parent no longer needs this socket.
			close( connectionSocket );

			// Getting commands from Android device, go into slave mode.
			autonomousMode = false;

			// Wait for currently connected client to disconnect.
			// This allows only one device to connect at a time.
			int status;
			wait( &status );

			// When above child terminates, the Android device has disconnected,
			// fo back to autonomous mode.
			autonomousMode = true;
		}
	}

	return 0;
}


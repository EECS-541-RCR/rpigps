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

#define MAX_NUM_WAYPOINTS 20
#define MAX_NMEA_SENTENCE_LEN 1024
#define ENABLE_GPS 0
#define ENABLE_NAVDATA 0

#include "network.h"
#include "navdata.h"
#include "command.h"
#include "gpsutil.h"

typedef enum { false, true } bool;

// By default, wait for commands from Android device.
bool				autonomousMode = false;
bool				programmedMode = true;

int  netYaw = 0;
navdata_t navdata_struct;
bool navdata_ready = false;

GpsPoint			currGpsFix;		// Current GPS fix. Parsed from GPS device NMEA strings.
GpsPoint			prevGpsFix;		// Previous GPS fix, used for heading estimation.
pthread_mutex_t		gpsFixMutex;	// Mutex for accessing curr/prev GpsFix structs.

GpsPoint			waypoints[MAX_NUM_WAYPOINTS];
unsigned int		numWaypoints = 0;

pthread_t					gpsPollThread;			// Thread for getting GPS data from device.
pthread_t					droneAutopilotThread;		// Thread for sending commands to drone.
pthread_t					droneNavDataThread;		// Thread for receiving NavData from drone.
pthread_t					androidGpsUpdateThread;	// Thread for sending periodic updates to android.
pthread_t					androidCommandThread;	// Thread for getting Android directional commands.

void *gpsPoll( void *arg );
void *droneAutopilot( void *arg );
void *sendAndroidGpsUpdates( void *arg );
void *getAndroidCommands( void *arg );
void *getNavData( void *arg );

void printAngles();
void printState();
void rotate(int theta);

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

	#if ENABLE_GPS
	pthread_create( &gpsPollThread, &attr, gpsPoll, (void *)NULL );
	pthread_create( &androidGpsUpdateThread, &attr, sendAndroidGpsUpdates, (void *)NULL );
	#endif
	#if ENABLE_NAVDATA
	pthread_create( &droneNavDataThread, &attr, getNavData, (void *)NULL );
	#endif
	pthread_create( &droneAutopilotThread, &attr, droneAutopilot, (void *)NULL );
	pthread_create( &androidCommandThread, &attr, getAndroidCommands, (void *)NULL );

	void *status;
	#if ENABLE_GPS
	pthread_join( gpsPollThread, &status );
	pthread_join( androidGpsUpdateThread, &status );
	#endif
	#if ENABLE_NAVDATA
	pthread_join( droneNavDataThread, &status );
	#endif
	pthread_join( droneAutopilotThread, &status );
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

        int i = 0;
        while (true)
	{
		if( autonomousMode )
		{
                  GpsPoint destination;
                  destination.latitude = 38.954352;
                  destination.longitude = -95.252811;

                  sleep( 5 );
                  droneTakeOff();
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
                          droneLand(); }
                }
                else if ( programmedMode )
                {
          if (navdata_ready) {
                  if (i % 4 == 0) {
                    droneTakeOff();
                    printf("%d: Take Off\n", i);
                    printAngles();
                    sleep(5);
                  } else if (i % 4 == 1) {
                    droneRotateRight();
                    printf("%d: Turn Right\n", i);
                    printAngles();
                    sleep(5);
                  } else if (i % 4 == 2) {
                    droneRotateLeft();
                    printf("%d: Turn Left\n", i);
                    printAngles();
                    sleep(5);
                  } else {
                    droneLand();
                    printf("%d: Land\n", i);
                    printAngles();
                    sleep(5);
                  } 
          }
                } 
          i++;
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
                if( isnan( currGpsFix.latitude ) || isnan( currGpsFix.longitude ) )
                {
                  continue;
                }
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

		char buffer[MAX_BUFFER_SIZE];
		int size = recv( connectionSocket, buffer, MAX_BUFFER_SIZE, 0 );
		if( size == -1 )
		{
			printf( "recv() failure, errno = %d\n", errno );
			close( handshakeSocket );
			close( connectionSocket );
			exit( EXIT_FAILURE );

		}

		buffer[size] = '\0';
		if( strcmp( buffer, "manual" ) == 0 )
		{
			pid_t pid = fork();
			if( pid < 0 )
			{
				fprintf( stderr, "Android command server couldn't fork.\n" );
				exit( EXIT_FAILURE );
			}
			if( pid == 0 )
			{
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
						printf( "Android client disconnected\n" );
						// Client disconnected.
						break;
					}
					else
					{
						buffer[size] = '\0';
						printf( "Recv :: %s\n", buffer );
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
		// If the first string wasn't "manual", assume it's a waypoint list.
		else if( strncmp( buffer, "list", 4 ) == 0 )
		{
			sscanf( &buffer[4], "%d", &numWaypoints );

			printf( "%s\n", buffer );
			unsigned int i;
			unsigned int len = 0;
			while( buffer[len] != ' ' ) len++;
			len++;
			while( buffer[len] != ' ' ) len++;
			len++;
			for( i = 0; i < numWaypoints; i++ )
			{
				sscanf( &buffer[len], "%lf %lf", &waypoints[i].latitude, &waypoints[i].longitude );
				while( buffer[len] != ' ' ) len++;
				len++;
				while( buffer[len] != ' ' ) len++;
				len++;
				printf( "%lf %lf\n", waypoints[i].latitude, waypoints[i].longitude );
			}
		}
		else
		{
			printf( "Unrecognized string \'%s\'.\n", buffer );
		}

		close( connectionSocket );
	}

	return 0;
}

void *getNavData( void *arg ) {
  // Note that navDataSock and navDataAddr are extern globals from navdata.h.
  createNavdataSocket();
  if( navDataSock < 0 )
  {
    fprintf( stderr, "Navdata thread couldn't connect to %s.\n", DRONE_IP );
    exit( EXIT_FAILURE );
  }
  else
  {
    printf( "Navdata thread connected to %s.\n", DRONE_IP );
  }

  // Start up dat navdata
  tickleNavData();
  navdataInit();

  int navdata_size;
  socklen_t socketsize;

  socketsize = sizeof(droneAddr_navdata);

  for(;;)
  {
    navdataKeepAlive();
    tickleNavData();

    //receive data 
    navdata_size = recvfrom(navDataSock, &navdata_struct, sizeof(navdata_struct), 0, (struct sockaddr *)&droneAddr_navdata, &socketsize);
    
    if (!navdata_ready && navdata_size > 0) {
      navdata_ready = true;
      printf("Navdata READY!\n");
    }

  }

  pthread_exit( NULL );
}

void printAngles() {
  printf("drone's position:\n");
  printf("\t%13.3f:%s\n", navdata_struct.navdata_option.theta, "pitch angle");
  printf("\t%13.3f:%s\n", navdata_struct.navdata_option.phi, "roll  angle");
  printf("\t%13.3f:%s\n", navdata_struct.navdata_option.psi, "yaw   angle");
  printf("\n");
}

void printState() {
  printf("drone's state:\n");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  0))!=0, "FLY MASK");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  1))!=0, "VIDEO MASK");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  2))!=0, "VISION MASK");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  3))!=0, "CONTROL ALGO");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  4))!=0, "ALTITUDE CONTROL ALGO");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  5))!=0, "USER feedback");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  6))!=0, "Control command ACK");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  7))!=0, "Trim command ACK");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  8))!=0, "Trim running");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 <<  9))!=0, "Trim result");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 10))!=0, "Navdata demo");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 11))!=0, "Navdata bootstrap");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 12))!=0, "Motors status");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 13))!=0, "Communication Lost");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 14))!=0, "problem with gyrometers");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 15))!=0, "VBat low");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 16))!=0, "VBat high");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 17))!=0, "Timer elapsed");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 18))!=0, "Power");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 19))!=0, "Angles");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 20))!=0, "Wind");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 21))!=0, "Ultrasonic sensor");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 22))!=0, "Cutout system detection");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 23))!=0, "PIC Version number OK");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 24))!=0, "ATCodec thread");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 25))!=0, "Navdata thread");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 26))!=0, "Video thread");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 27))!=0, "Acquisition thread");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 28))!=0, "CTRL watchdog");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 29))!=0, "ADC Watchdog");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 30))!=0, "Communication Watchdog");
  printf("\t%13d:%s\n",(navdata_struct.navdata_header.state & (1 << 31))!=0, "Emergency landing");
}

/**
 * @param theta - number between +180 and -180
 */
void rotate(int theta) {
  int initialYaw = navdata_struct.navdata_option.psi;
  int deltaYaw = initialYaw + 1000*theta;
  int curYaw = initialYaw;
  if (theta > 0) {
    //Going clkwise
  } else {
    //Going counterclkwise 
  }
  netYaw += theta;
}

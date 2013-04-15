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
#include "navdata.h"
#include "command.h"
#include "gpsutil.h"

GpsPoint					gpsFix;			// For all other processing. Parsed from GPS device NMEA strings.
pthread_mutex_t				gpsFixMutex;	// Mutex for accessing gpsFix struct.

pthread_t					gpsPollThread;			// Thread for getting GPS data from device.
pthread_t					droneCommandThread;		// Thread for sending commands to drone.
pthread_t					droneNavDataThread;		// Thread for receiving NavData from drone.
pthread_t					androidGpsUpdateThread;	// Thread for sending periodic updates to android.
pthread_t					androidCommandThread;	// Thread for getting Android directional commands.

void *gpsPoll( void *arg );
void *sendDroneCommands( void *arg );
void *sendAndroidGpsUpdates( void *arg );
void *getAndroidCommands( void *arg );
void *getNavData( void *arg );

int main( int argc, char **argv )
{
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

	pthread_mutex_init( &gpsFixMutex, NULL );

	pthread_create( &gpsPollThread, &attr, gpsPoll, (void *)NULL );
	pthread_create( &droneCommandThread, &attr, sendDroneCommands, (void *)NULL );
	pthread_create( &droneNavDataThread, &attr, getNavData, (void *)NULL );
	pthread_create( &androidGpsUpdateThread, &attr, sendAndroidGpsUpdates, (void *)NULL );
	pthread_create( &androidCommandThread, &attr, getAndroidCommands, (void *)NULL );

	void *status;
	pthread_join( gpsPollThread, &status );
	pthread_join( droneCommandThread, &status );
	pthread_join( droneNavDataThread, &status );
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

/*	saun.sun_family = AF_UNIX;
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
		memcpy( (char *)&gpsFix, buffer, sizeof( GpsPoint ) / sizeof( char ) );
		pthread_mutex_unlock( &gpsFixMutex );
	}
*/	
	pthread_exit( NULL );
}

void *sendDroneCommands( void *arg )
{
	// Note that droneCmdSock and droneCmdAddr are extern globals from command.h.
	droneCmdSock = createUdpClientConnection( DRONE_IP, DRONE_COMMAND_PORT, &droneCmdAddr );
	if( droneCmdSock < 0 )
	{
		fprintf( stderr, "Command thread couldn't connect to %s.\n", DRONE_IP );
		exit( EXIT_FAILURE );
	}
	else
	{
		printf( "Command thread connected to %s.\n", DRONE_IP );
	}

	unsigned int i = 0;
	for(;;)
	{
/*		double lat;
		double lon;

		lat = gpsFix.latitude;
		lon = gpsFix.longitude;

		if( i++ % 2 == 0 )
		{
			droneTakeOff();
		}
		else
		{
			droneLand();
		}
*/
		sleep( 10 );
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
		double lat;
		double lon;

		lat = gpsFix.latitude;
		lon = gpsFix.longitude;

		sprintf( str, "%f %f %d", lat, lon, i++ );
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

		if( !fork() )
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
					printf( "Receive %s\n", buffer );
				}
			}

			close( connectionSocket );
			exit( EXIT_SUCCESS );
		}

		close( connectionSocket );	// Parent should get ready for next transmission.
		sleep( 10 );
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
  navdata_t navdata_struct;
  socklen_t socketsize;

  socketsize = sizeof(droneAddr_navdata);

  unsigned int i = 0;
  for(;;)
  {
    navdataKeepAlive();
    tickleNavData();

    //receive data 
    navdata_size = recvfrom(navDataSock, &navdata_struct, sizeof(navdata_struct), 0, (struct sockaddr *)&droneAddr_navdata, &socketsize);

    printf("\t%13.3f:%s\n", navdata_struct.navdata_option.theta,                  "pitch angle");
    printf("\t%13.3f:%s\n", navdata_struct.navdata_option.phi,                    "roll  angle");
    printf("\t%13.3f:%s\n", navdata_struct.navdata_option.psi,                    "yaw   angle");
    printf("\n");
  }

  pthread_exit( NULL );
}

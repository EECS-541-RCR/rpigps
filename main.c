#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
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
#include <gps.h>

#define DRONE_IP "127.0.0.1"			// Static IP of drone. Set to localhost when testing.
#define DRONE_COMMAND_PORT "5556"		// Port the drone receives AT commands from.
#define DRONE_NAVDATA_PORT "5554"		// Port the drone sends navdata from.

#define ANDROID_IP "127.0.0.1"			// Set to localhost for testing.
#define ANDROID_COMMAND_PORT "5558"		// Port the android device sends commands from.
#define ANDROID_GPS_UPDATE_PORT "5559"	// Port the android devices listens on for GPS updates.

#define MAX_BUFFER_SIZE 1024

typedef struct
{
	double latitude;
	double longitude;
} GpsFix;

static struct gps_data_t	gpsData;		// For getting data from gpsd.
GpsFix						gpsFix;			// For all other processing, taken from gpsData struct when fix obtained.
pthread_mutex_t				gpsFixMutex;	// Mutex for accessing gpsFix struct.

pthread_t					gpsPollThread;	// Thread for getting GPS data from gpsd.
pthread_t					droneCommandThread;		// Thrad for sending commands to drone.
pthread_t					androidGpsUpdateThread;	// Thread for sending periodic updates to android.
pthread_t					androidCommandThread;	// Thread for getting Android directional commands.

int createTcpClientConnection( const char *hostname, const char *port );	// port number as string
int createUdpClientConnection( const char *hostname, const char *port, struct sockaddr_in *theiraddr );
void *gpsPoll( void *arg );
void *sendDroneCommands( void *arg );
void *sendAndroidGpsUpdates( void *arg );
void *getAndroidCommands( void *arg );

int main( int argc, char **argv )
{
	// Connect to gpsd.
	if( gps_open( "localhost", DEFAULT_GPSD_PORT, &gpsData ) != 0 )
	{
		fprintf( stderr, "Couldn't connect to gpsd, errno = %d, %s.\n", errno, gps_errstr( errno ) );
		exit( EXIT_FAILURE );
	}
	else
	{
		printf( "Connected to gpsd.\n" );
	}

	// Register for updates from gpsd.
	gps_stream( &gpsData, WATCH_ENABLE | WATCH_JSON, NULL );

	pthread_attr_t	attr;

	pthread_mutex_init( &gpsFixMutex, NULL );
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

//	pthread_create( &gpsPollThread, &attr, gpsPoll, (void *)NULL );
	pthread_create( &droneCommandThread, &attr, sendDroneCommands, (void *)NULL );
	pthread_create( &androidGpsUpdateThread, &attr, sendAndroidGpsUpdates, (void *)NULL );
	pthread_create( &androidCommandThread, &attr, getAndroidCommands, (void *)NULL );

	void *status;
//	pthread_join( gpsPollThread, &status );
	pthread_join( droneCommandThread, &status );
	pthread_join( androidGpsUpdateThread, &status );
	pthread_join( androidCommandThread, &status );

	pthread_mutex_destroy( &gpsFixMutex );
	pthread_attr_destroy( &attr );
	pthread_exit( NULL );
	return 0;
}

int createTcpClientConnection( const char *hostname, const char *port )
{
	int sockfd;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if( getaddrinfo( hostname, port, &hints, &servinfo ) != 0 )
	{
		fprintf( stderr, "getaddrinfo() failure.\n" );
		return -1;
	}

	// Connect to the first result possible.
	struct addrinfo *p = NULL;
	for( p = servinfo; p != NULL; p = p->ai_next )
	{
		sockfd = socket( p->ai_family, p->ai_socktype, p->ai_protocol );
		if( sockfd == -1 )
		{
			continue;
		}

		if( connect( sockfd, p->ai_addr, p->ai_addrlen ) == -1 )
		{
			continue;
		}

		break;
	}

	if( p == NULL )
	{
		fprintf( stderr, "connect() failure.\n" );
		return -1;
	}

	freeaddrinfo( servinfo );

	return sockfd;
}

int createUdpClientConnection( const char *hostname, const char *port, struct sockaddr_in *theiraddr )
{
	int sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( sockfd == -1 )
	{
		fprintf( stderr, "socket() failure, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	memset( (char *)theiraddr, 0, sizeof( *theiraddr ) );
	theiraddr->sin_family = AF_INET;
	theiraddr->sin_port = htons( atoi( port ) );
	if( inet_aton( hostname, &theiraddr->sin_addr ) == 0 )
	{
		fprintf( stderr, "inet_aton() failure, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	return sockfd;
}

void *gpsPoll( void *arg )
{
	for(;;)
	{
		if( !gps_waiting( &gpsData, 5000000 ) )
		{
			fprintf( stderr, "GPS fix timed out.\n" );
			exit( EXIT_FAILURE );
		}
		else
		{
			if( gps_read( &gpsData ) == -1 )
			{
				fprintf( stderr, "gps_read() error, errno = %d\n", errno );
			}
			else
			{
				if( isnan( gpsData.fix.latitude ) || isnan( gpsData.fix.longitude ) )
				{
					fprintf( stderr, "Bad GPS fix.\n" );
				}
				else
				{
					pthread_mutex_lock( &gpsFixMutex );
						gpsFix.latitude = gpsData.fix.latitude;
						gpsFix.longitude = gpsData.fix.longitude;
					pthread_mutex_unlock( &gpsFixMutex );
				}
			}
		}
	}

	pthread_exit( NULL );
}

void *sendDroneCommands( void *arg )
{
	struct sockaddr_in theiraddr;
	int cmdSock = createUdpClientConnection( DRONE_IP, DRONE_COMMAND_PORT, &theiraddr );
	if( cmdSock < 0 )
	{
		fprintf( stderr, "Couldn't connect to %s.\n", DRONE_IP );
		exit( EXIT_FAILURE );
	}
	else
	{
		printf( "Connected to %s.\n", DRONE_IP );
	}

	int i = 0;
	for(;;)
	{
		char str[MAX_BUFFER_SIZE];
		double lat;
		double lon;

		pthread_mutex_lock( &gpsFixMutex );
			lat = gpsFix.latitude;
			lon = gpsFix.longitude;
		pthread_mutex_unlock( &gpsFixMutex );

		sprintf( str, "%f %f %d", lat, lon, i++ );
		if( sendto( cmdSock, str, sizeof( str ), 0, (struct sockaddr *)&theiraddr, sizeof( theiraddr ) ) < 0 )
		{
			printf( "Error sending command to drone\n." );
			exit( EXIT_FAILURE );
		}

		sleep( 1 );
	}

	pthread_exit( NULL );
}

void *sendAndroidGpsUpdates( void *arg )
{
	int updateSock = createTcpClientConnection( ANDROID_IP, ANDROID_GPS_UPDATE_PORT );
	if( updateSock < 0 )
	{
		fprintf( stderr, "Couldn't connect to %s.\n", DRONE_IP );
		exit( EXIT_FAILURE );
	}
	else
	{
		printf( "Connected to %s.\n", DRONE_IP );
	}

	int i = 0;
	for(;;)
	{
		char str[MAX_BUFFER_SIZE];
		double lat;
		double lon;

		pthread_mutex_lock( &gpsFixMutex );
			lat = gpsFix.latitude;
			lon = gpsFix.longitude;
		pthread_mutex_unlock( &gpsFixMutex );

		sprintf( str, "%f %f %d", lat, lon, i++ );
		if( send( updateSock, str, sizeof( str ), 0 ) < 0 )
		{
			printf( "Error sending command to drone\n." );
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
		printf( "socket() failure, errno = %d\n", errno );
		exit( EXIT_FAILURE );
	}

	int yes = 1;
	if( setsockopt( handshakeSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( int ) ) == -1 )
	{
		printf( "setsockopt() failure, errno = %d\n", errno );
		close( handshakeSocket );
		exit( EXIT_FAILURE );
	}

	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons( atoi( ANDROID_COMMAND_PORT ) );
	myaddr.sin_addr.s_addr = htonl( INADDR_ANY );
	memset( &( myaddr.sin_zero ), '\0', 8 );

	if( bind( handshakeSocket, (struct sockaddr *)&myaddr, sizeof( struct sockaddr ) ) == -1 )
	{
		printf( "bind() failure, errno = %d\n", errno );
		close( handshakeSocket );
		exit( EXIT_FAILURE );
	}

	if( listen( handshakeSocket, 10 ) == -1 )
	{
		printf( "listen() failure, errno = %d\n", errno );
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
			printf( "accept() failure, errno = %d\n", errno );
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


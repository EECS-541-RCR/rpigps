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

#define DRONE_IP "localhost"			// Static IP of drone. Set to localhost when testing.
#define DRONE_COMMAND_PORT "5556"		// Port the drone receives AT commands from.
#define DRONE_NAVDATA_PORT "5554"		// Port the drone sends navdata from.
#define ANDROID_COMAND_PORT "5558"		// Port the android device sends commands from.
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
pthread_t					androidGpsUpdateThread;	// Thread for sending periodic updates to android.
pthread_t					androidCommandThread;	// Thread for getting Android directional commands.

int cmdSock;				// Socket used to send commands to drone.
int androidHandshakeSock;	// Socket used to initialize connections with Android device.
int androidGpsUpdateSock;	// Socket used to send periodic GPS updates to Android device.

int createClientConnection( const char *hostname, const char *port );	// port number as string
void *gpsPoll( void *arg );
void *sendAndroidGpsUpdates( void *arg );
void *getAndroidCommands( void *arg );

int main( int argc, char **argv )
{
	// Connect to AR drone.
	printf( "Trying to connect to AR drone...\n" );
	int cmdSock = createClientConnection( DRONE_IP, DRONE_COMMAND_PORT );
	if( cmdSock < 0 )
	{
		fprintf( stderr, "Couldn't connect to %s.\n", DRONE_IP );
		exit( EXIT_FAILURE );
	}
	else
	{
		printf( "Connected to %s.\n", DRONE_IP );
	}

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

	pthread_create( &gpsPollThread, &attr, gpsPoll, (void *)NULL );

	void *status;
	pthread_join( gpsPollThread, &status );

	pthread_mutex_destroy( &gpsFixMutex );
	pthread_attr_destroy( &attr );
	pthread_exit( NULL );
	close( cmdSock );
	return 0;
}

int createClientConnection( const char *hostname, const char *port )
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
					printf( "Latitude: %f\n", gpsData.fix.latitude );
					printf( "Longitude: %f\n", gpsData.fix.longitude );
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

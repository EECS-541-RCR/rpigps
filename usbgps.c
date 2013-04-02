#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <math.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "gpsutil.h"

#define SOCKET_ADDRESS "serial_rpigps_data"
#define MAX_NMEA_SENTENCE_LEN 1024

// Utility functions.
int initSerialIO( char *ttyName, struct termios *tio, struct termios *stdio );
GpsPoint parseGpggaSentence( char *sentence );

// Thread functions.
void *getSerialGps( void *arg );
void *gpsServer( void *arg );

// Thread globals.
GpsPoint		gpsFix;
pthread_mutex_t	gpsMutex;
pthread_t		serialThread;
pthread_t		serverThread;

int main( int argc, char **argv )
{
	if( argc != 2 )
	{
		printf( "Usage: %s <serial tty device>\n", argv[0] );
		printf( "The device should have a baud rate 38400.\n" );
		exit( 0 );
	}

	pthread_attr_t attr;
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_JOINABLE );

	pthread_mutex_init( &gpsMutex, NULL );

	pthread_create( &serialThread, &attr, getSerialGps, (void *)argv[1] );
	pthread_create( &serverThread, &attr, gpsServer, (void *)NULL );

	pthread_attr_destroy( &attr );

	void *status = NULL;
	pthread_join( serialThread, &status );
	pthread_join( serverThread, &status );
	
	return 0;
}

int initSerialIO( char *ttyName, struct termios *tio, struct termios *stdio )
{
	memset( stdio, 0, sizeof( struct termios ) );
	stdio->c_iflag = 0;
	stdio->c_oflag = 0;
	stdio->c_cflag = 0;
	stdio->c_lflag = 0;
	stdio->c_cc[VMIN] = 1;
	stdio->c_cc[VTIME] = 0;
	tcsetattr( STDOUT_FILENO, TCSANOW, stdio );
	tcsetattr( STDOUT_FILENO, TCSAFLUSH, stdio );
	fcntl( STDIN_FILENO, F_SETFL, O_NONBLOCK );

	memset( tio, 0, sizeof( struct termios ) );
	tio->c_iflag = 0;
	tio->c_oflag = 0;
	tio->c_cflag = CS8 | CREAD | CLOCAL;
	tio->c_lflag = 0;
	tio->c_cc[VMIN] = 1;
	tio->c_cc[VTIME] = 5;

	int ttyfd = open( ttyName, O_RDWR | O_NONBLOCK );      
	cfsetospeed( tio, B38400 );
	cfsetispeed( tio, B38400 );
	tcsetattr( ttyfd, TCSANOW, tio );

	return ttyfd;
}

GpsPoint parseGpggaSentence( char *sentence )
{
	GpsPoint result;
	result.latitude = NAN;
	result.longitude = NAN;

	unsigned int i = 0;
	while( sentence[i++] != ',' );
	while( sentence[i++] != ',' );

	if( sentence[i] == ',' )
	{
		return result;
	}
	else
	{
		char deg[10];
		char min[10];

		strncpy( deg, &sentence[i], 2 );
		strncpy( min, &sentence[i + 2], 7 );
		i += 9;

		deg[2] = '\0';
		min[5] = '\0';

		result.latitude = atof( deg ) + atof( min )/ 60.0;
	}

	i++;
	if( sentence[i] == 'S' )
	{
		result.latitude = -result.latitude;
	}

	i += 2;
	if( sentence[i] != ',' )
	{
		char deg[10];
		char min[10];

		strncpy( deg, &sentence[i], 3 );
		strncpy( min, &sentence[i + 3], 5 );
		i += 8;

		deg[3] = '\0';
		min[5] = '\0';

		result.longitude = atof( deg ) + atof( min ) / 60.0;
	}

	i++;
	if( sentence[i] == 'E' )
	{
		result.longitude = -result.longitude;
	}

	return result;
}

void *getSerialGps( void *arg )
{
	struct termios tio;
	struct termios stdio;
	struct termios oldStdio;

	// Save STDOUT state to restore it later.
	tcgetattr( STDOUT_FILENO, &oldStdio );
	
	int ttyfd = initSerialIO( (char *)arg, &tio, &stdio );

	char nmeaSentence[MAX_NMEA_SENTENCE_LEN];
	unsigned char c = 0;
	unsigned int i = 0;
	for(;;)
	{
		if( read( ttyfd, &c, 1 ) > 0 )
		{
			if( c == '\n' )
			{
				nmeaSentence[i] = '\0';
				i = 0;
				if( strncmp( nmeaSentence, "$GPGGA", 6 ) == 0 )
				{
					GpsPoint location = parseGpggaSentence( nmeaSentence );
					pthread_mutex_lock( &gpsMutex );
					gpsFix.latitude = location.latitude;
					gpsFix.longitude = location.longitude;
					pthread_mutex_unlock( &gpsMutex );
				}
			}
			else
			{
				nmeaSentence[i++] = c;
			}
		}
		if( read( STDIN_FILENO, &c, 1 ) > 0 )
		{
			if( c == 'q' )
			{
				close( ttyfd );
				tcsetattr( STDOUT_FILENO, TCSANOW, &oldStdio );
				pthread_exit( 0 );
			}
		}
	}
}

void *gpsServer( void *arg )
{
	struct sockaddr_un saun;

	saun.sun_family = AF_UNIX;
	strcpy( saun.sun_path, SOCKET_ADDRESS );

	int handshakeSocket = socket( AF_UNIX, SOCK_STREAM, 0 );
	if( handshakeSocket < 0 )
	{
		fprintf( stderr, "Couldn't create handshake socket, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	unlink( SOCKET_ADDRESS );
	if( bind( handshakeSocket, (struct sockaddr *)&saun, sizeof( saun ) ) < 0 )
	{
		fprintf( stderr, "Couldn't bind handshake socket, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	if( listen( handshakeSocket, 10 ) < 0 )
	{
		fprintf( stderr, "Couldn't set handshake socket to listen, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	for(;;)
	{
		int sessionSocket = accept( handshakeSocket, NULL, 0 );
		if( sessionSocket < 0 )
		{
			fprintf( stderr, "Couldn't accept connection, errno = %d.\n", errno );
			exit( EXIT_FAILURE );
		}

		for(;;)
		{
			GpsPoint curr;
			pthread_mutex_lock( &gpsMutex );
			curr.latitude = gpsFix.latitude;
			curr.longitude = gpsFix.longitude;
			pthread_mutex_unlock( &gpsMutex );

			if( write( sessionSocket, (char *)&curr, sizeof( curr ) / sizeof( char ) ) < 0 )
			{
				close( sessionSocket );
				break;
			}
		}
	}

	close( handshakeSocket );
	pthread_exit( 0 );
}


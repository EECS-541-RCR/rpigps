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

#define MAX_BUFFER_SIZE 1024

int main( int argc, char **argv )
{
	if( argc != 2 )
	{
		printf( "Usage: %s <port number>\n", argv[0] );
		printf( "Creates dummy client that just sends integers to the desired port.\n" );
		exit( EXIT_FAILURE );
	}

	int sockfd;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if( getaddrinfo( "localhost", argv[1], &hints, &servinfo ) != 0 )
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

	int i = 0;
	while( 1 )
	{
		char buffer[MAX_BUFFER_SIZE];
		sprintf( buffer, "%d", i++ );
		if( send( sockfd, buffer, strlen( buffer ), 0 ) < 0 )
		{
			printf( "send() failure, errno = %d.\n", errno );
			close( sockfd );
			exit( EXIT_FAILURE );
		}

		sleep( 1 );
	}

	close( sockfd );
	return 0;
}


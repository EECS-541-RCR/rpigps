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

#define MAX_BUFFER_SIZE 1024

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


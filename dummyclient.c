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

void printUsage();
void runTcpClient( const char *port );
void runUdpClient( const char *port );

int main( int argc, char **argv )
{
	if( argc != 3 )
	{
		printUsage();
		exit( EXIT_FAILURE );
	}

	if( strcmp( argv[1], "tcp" ) == 0 )
	{
		runTcpClient( argv[2] );
	}
	else if( strcmp( argv[1], "udp" ) == 0 )
	{
		runUdpClient( argv[2] );
	}
	else
	{
		printUsage();
		exit( EXIT_FAILURE );
	}

	return 0;
}

void printUsage()
{
	printf( "Usage: ./dummylcient <protocol> <port number>\n" );
	printf( "<protocol> = tcp or udp.\n" );
	printf( "Creates dummy client that just sends integers to the desired port.\n" );
}

void runTcpClient( const char *port )
{
	int sockfd;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if( getaddrinfo( "localhost", port, &hints, &servinfo ) != 0 )
	{
		fprintf( stderr, "getaddrinfo() failure.\n" );
		exit( EXIT_FAILURE );
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
		exit( EXIT_FAILURE );
	}

	freeaddrinfo( servinfo );

	int i = 0;
	while( 1 )
	{
		char buffer[MAX_BUFFER_SIZE];
		sprintf( buffer, "%d", i++ );
		printf( "Send %s\n", buffer );

		if( send( sockfd, buffer, strlen( buffer ), 0 ) < 0 )
		{
			printf( "send() failure, errno = %d.\n", errno );
			close( sockfd );
			exit( EXIT_FAILURE );
		}

		sleep( 1 );
	}

	close( sockfd );
}

void runUdpClient( const char *port )
{
	struct sockaddr_in theiraddr;

	int sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( sockfd == -1 )
	{
		fprintf( stderr, "socket() failure, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	memset( (char *)&theiraddr, 0, sizeof( theiraddr ) );
	theiraddr.sin_family = AF_INET;
	theiraddr.sin_port = htons( atoi( port ) );
	if( inet_aton( "127.0.0.1", &theiraddr.sin_addr ) == 0 )
	{
		fprintf( stderr, "inet_aton() failure, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	int i = 0;
	while( 1 )
	{
		char buffer[MAX_BUFFER_SIZE];

		sprintf( buffer, "%d", i++ );
		printf( "Send %s\n", buffer );

		int size = sendto( sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&theiraddr, sizeof( theiraddr ) );
		if( size == -1 )
		{
			fprintf( stderr, "sendto() failure, errno = %d.\n", errno );
			close( sockfd );
			exit( EXIT_FAILURE );
		}

		sleep( 1 );
	}
}


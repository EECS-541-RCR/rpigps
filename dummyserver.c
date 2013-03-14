#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>

#define MAX_BUFFER_SIZE 1024

void printUsage();
void runTcpServer( const char *port );
void runUdpServer( const char *port );

int main( int argc, char **argv )
{
	if( argc != 3 )
	{
		printUsage();
		exit( EXIT_FAILURE );
	}

	if( strcmp( argv[1], "tcp" ) == 0 )
	{
		runTcpServer( argv[2] );
	}
	else if( strcmp( argv[1], "udp" ) == 0 )
	{
		runUdpServer( argv[2] );
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
	printf( "Usage: ./dummyserver <protocol> <port number>\n" );
	printf( "<protocol> = tcp or udp.\n" );
	printf( "Creates a dummy server for testing that listens on the given port\n" );
	printf( "for incoming connections, and prints any data it receives to STDOUT.\n" );
}

void runTcpServer( const char *port )
{
	int handshake_socket;
	struct sockaddr_in myaddr;

	handshake_socket = socket( PF_INET, SOCK_STREAM, 0 );
	if( handshake_socket == -1 )
	{
		printf( "socket() failure, errno = %d\n", errno );
		exit( EXIT_FAILURE );
	}

	int yes = 1;
	if( setsockopt( handshake_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof( int ) ) == -1 )
	{
		printf( "setsockopt() failure, errno = %d\n", errno );
		close( handshake_socket );
		exit( EXIT_FAILURE );
	}

	myaddr.sin_family = AF_INET; 				/* Host byte order */
	myaddr.sin_port = htons( atoi( port ) );			/* Network byte order */
	myaddr.sin_addr.s_addr = htonl( INADDR_ANY ); 		/* Get my IP address */
	memset( &( myaddr.sin_zero ), '\0', 8 ); 			/* Zero it out */

	/* Bind it to your IP address and a suitable port number. */
	if( bind( handshake_socket, (struct sockaddr *)&myaddr, sizeof( struct sockaddr ) ) == -1 )
	{
		printf( "bind() failure, errno = %d\n", errno );
		close( handshake_socket );
		exit( EXIT_FAILURE );
	}

	if( listen( handshake_socket, 10 ) == -1 )
	{
		printf( "listen() failure, errno = %d\n", errno );
		close( handshake_socket );
		exit( EXIT_FAILURE );
	}

	while( 1 )
	{
		struct sockaddr_in theiraddr;
		socklen_t theiraddr_size = sizeof( struct sockaddr_in );

		/* Accept the incoming connection request. */
		int connection_socket = accept( handshake_socket, (struct sockaddr *)&theiraddr, &theiraddr_size );
		if( connection_socket == -1 )
		{
			printf( "accept() failure, errno = %d\n", errno );
			continue;
		}

		char ipaddr[INET_ADDRSTRLEN];
		inet_ntop( AF_INET, &( (struct sockaddr_in *)&theiraddr )->sin_addr.s_addr, ipaddr, INET_ADDRSTRLEN );
		printf( "Connection from %s\n", ipaddr );

		if( !fork() )
		{
			char buffer[MAX_BUFFER_SIZE];
			close( handshake_socket );	// Child doesn't need this socket.

			while( 1 )
			{
				int size = 0;
				
				size = recv( connection_socket, buffer, MAX_BUFFER_SIZE, 0 );
				if( size == -1 )
				{
					printf( "recv() failure, errno = %d\n", errno );
					close( connection_socket );
					exit( EXIT_FAILURE );
				}
				else if( size == 0 )
				{
					printf( "%s disconnected.\n", ipaddr );
					exit( EXIT_SUCCESS );
				}
				else
				{
					buffer[size] = '\0';
					printf( "Recv %s\n", buffer );
				}
			}

			close( connection_socket );
			exit( EXIT_SUCCESS );
		}

		close( connection_socket );	// Parent should get ready for next transmission.
		sleep( 10 );
	}
}

void runUdpServer( const char *port )
{
	struct sockaddr_in myaddr;
	struct sockaddr_in theiraddr;

	int sockfd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
	if( sockfd == -1 )
	{
		fprintf( stderr, "socket() failure, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	memset( (char *)&myaddr, 0, sizeof( myaddr ) );
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons( atoi( port ) );
	myaddr.sin_addr.s_addr = htonl( INADDR_ANY );

	if( bind( sockfd, (struct sockaddr *)&myaddr, sizeof( myaddr ) ) == -1 )
	{
		fprintf( stderr, "bind() failure, errno = %d.\n", errno );
		exit( EXIT_FAILURE );
	}

	while( 1 )
	{
		char buffer[MAX_BUFFER_SIZE];
		socklen_t theiraddrsize = sizeof( theiraddr );

		int size = recvfrom( sockfd, buffer, MAX_BUFFER_SIZE, 0, (struct sockaddr *)&theiraddr, &theiraddrsize );
		if( size == -1 )
		{
			fprintf( stderr, "recvfrom() failure, errno = %d.\n", errno );
			close( sockfd );
			exit( EXIT_FAILURE );
		}
		else
		{
			buffer[size] = '\0';
			printf( "Recv %s\n", buffer );
		}
	}

	close( sockfd );
}


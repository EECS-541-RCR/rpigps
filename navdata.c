#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "navdata.h"
#include "network.h"

// Externs
int navDataSock;
struct sockaddr_in droneAddr_navdata;
struct sockaddr_in clientAddr_navdata;

void createNavdataSocket() {
  struct hostent      *h;
  h = gethostbyname(DRONE_IP);

  // create structure for ardrone address & port
  droneAddr_navdata.sin_family = h->h_addrtype;
  droneAddr_navdata.sin_port   = htons(DRONE_NAVDATA_PORT);
  memcpy((char *) &(droneAddr_navdata.sin_addr.s_addr), h->h_addr_list[0], h->h_length);
  
  // create structure for this client
  clientAddr_navdata.sin_family = AF_INET;
  clientAddr_navdata.sin_addr.s_addr = htonl(INADDR_ANY);
  clientAddr_navdata.sin_port = htons(0);
  
  // socket creation for NAV_PORT
  navDataSock = socket(AF_INET,SOCK_DGRAM,0);
  if(navDataSock<0) {
    printf("%s: cannot open socket \n", h->h_addr_list[0]);
    exit(1);
  }
  
  // bind client's the port and address
  if(bind(navDataSock, (struct sockaddr *) &clientAddr_navdata, sizeof(clientAddr_navdata))<0) {
    printf("%d: cannot bind port\n", DRONE_NAVDATA_PORT);
    exit(1);
  }
  
}

void sendNavData( char *cmd )
{
	if( sendto( navDataSock, cmd, strlen(cmd)+1, 0, (struct sockaddr *) &droneAddr_navdata, sizeof( droneAddr_navdata ) ) < 0 )
	{
		fprintf( stderr, "Error sending navdata to drone.\n" );
		exit( EXIT_FAILURE );
	}
}

void tickleNavData()
{
  // tickle drone's port: drone send one packe of navdata in navdata_demo mode
  sendNavData ("\x01\x00");
}

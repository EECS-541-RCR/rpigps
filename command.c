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

#include "command.h"

// Externs so they can be used in main.c as needed.
unsigned int droneSeqNum = 1;
int droneCmdSock;
struct sockaddr_in droneCmdAddr;

void sendCommand( const char *cmd )
{
	if( sendto( droneCmdSock, cmd, MAX_COMMAND_LEN, 0, (struct sockaddr *)&droneCmdAddr, sizeof( droneCmdAddr ) ) < 0 )
	{
		fprintf( stderr, "Error sending command to drone.\n" );
		exit( EXIT_FAILURE );
	}
}

void droneTakeOff()
{
	char cmd[MAX_COMMAND_LEN];
	
	snprintf( cmd, MAX_COMMAND_LEN, "AT*REF=%u,290718208\r", droneSeqNum++ );
	sendCommand( cmd );
}
void droneLand()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*REF=%u,290717696\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneHover()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,0,0,0,0\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneUp()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,0,0,1045220557,0\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneDown()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,0,0,-1102263091,0\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneForward()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,0,-1102263091,0,0\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneBack()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,0,1045220557,0,0\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneLeft()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,-1102263091,0,0,0\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneRight()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,1045220557,0,0,0\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneRotateLeft()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,0,0,0,-1085485875\r", droneSeqNum++ );
	sendCommand( cmd );
}

void droneRotateRight()
{
	char cmd[MAX_COMMAND_LEN];

	snprintf( cmd, MAX_COMMAND_LEN, "AT*PCMD=%u,1,0,0,0,1061997773\r", droneSeqNum++ );
	sendCommand( cmd );
}

void navdataInit()
{
  char cmd[MAX_COMMAND_LEN];

  // stop bootstrap mode
  snprintf( cmd, MAX_COMMAND_LEN, "AT*CONFIG=%d,\"general:navdata_demo\",\"TRUE\"\r", droneSeqNum++ );
  sendCommand( cmd );
    
  // send ack to start navdata
  snprintf( cmd, MAX_COMMAND_LEN, "AT*CTRL=%d,0\r", droneSeqNum++ );
  sendCommand( cmd );
  
  // send command to trim sensors
  snprintf( cmd, MAX_COMMAND_LEN, "AT*FTRIM=%d,\r", droneSeqNum++ );
  sendCommand( cmd );
}

void navdataKeepAlive()
{
  char cmd[MAX_COMMAND_LEN];

  // send watchdog if no command is sent to command port, so as to prevent drone from entering hover mode
  snprintf( cmd, MAX_COMMAND_LEN, "AT*COMWDG=%d\r", droneSeqNum++ );
  sendCommand( cmd );
}

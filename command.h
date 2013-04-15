#ifndef _COMMAND_H_
#define _COMMAND_H_

#define MAX_COMMAND_LEN 80

#include <netinet/in.h>

// Externs so they can be used in main.c.
extern unsigned int droneSeqNumber;
extern int droneCmdSock;
extern struct sockaddr_in droneCmdAddr;

void sendCommand( const char *cmd );

void droneInit();
void droneTakeOff();
void droneLand();
void droneHover();
void droneUp();
void droneDown();
void droneForward();
void droneBack();
void droneLeft();
void droneRight();
void droneRotateLeft();
void droneRotateRight();

void navdataInit();
void navdataKeepAlive();

#endif


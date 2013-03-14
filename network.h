#ifndef _NETWORK_H_
#define _NETWORK_H_

#define DRONE_IP "127.0.0.1"			// Static IP of drone. Set to localhost when testing.
#define DRONE_COMMAND_PORT "5556"		// Port the drone receives AT commands from.
#define DRONE_NAVDATA_PORT "5554"		// Port the drone sends navdata from.

#define ANDROID_IP "127.0.0.1"			// Set to localhost for testing.
#define ANDROID_COMMAND_PORT "5558"		// Port the android device sends commands from.
#define ANDROID_GPS_UPDATE_PORT "5559"	// Port the android devices listens on for GPS updates.

#define MAX_BUFFER_SIZE 1024

int createTcpClientConnection( const char *hostname, const char *port );	// port number as string
int createUdpClientConnection( const char *hostname, const char *port, struct sockaddr_in *theiraddr );

#endif


#ifndef _GPS_UTIL_H_
#define _GPS_UTIL_H_

// Locations determined via GPS can be off by as much as about 3 meters.
// This is the minimum distance at which the current location is
// considered the same as a particular destination point's location.
#define LOCATION_EPSILON 0.003	// Kilometers (i.e. 3 meters)
// As the unit navigates, if its heading differs from the desired bearing by more than  
// the below amount, rotate toward the bearing.
#define HEADING_EPSILON 10	// Degrees

typedef struct
{
	double latitude;
	double longitude;
} GpsPoint;

// Calculates great-circle distance between GPS positions p1 and p2.
// Returns a value in kilometers.
double getDistance( GpsPoint p1, GpsPoint p2 );
// Calculates the bearing needed to get from GPS position p1 to p2.
// Returns a value in degrees from 0 to 360. 0 is north, 90 is east, etc.
double getBearing( GpsPoint p1, GpsPoint p2 );

// Approximates current drone heading based on the bearing between the
// previous GPS fix and current GPS fix.
// NOTE: This might be something the GPS chip provides already.
double getHeading( GpsPoint curr, GpsPoint prev );

#endif


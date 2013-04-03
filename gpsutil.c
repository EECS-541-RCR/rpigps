#include "gpsutil.h"
#include <math.h>

// Formulas taken from:
// http://www.movable-type.co.uk/scripts/latlong.html

#define PI 3.14159265358979323846
#define EARTH_RADIUS 6371.0 // Mean radius of Earth in km.

#define DEG2RAD( DEG ) ( (DEG) * (PI) / (180.0) )
#define RAD2DEG( DEG ) ( (DEG) * (180.0) / (PI) )

double getDistance( GpsPoint p1, GpsPoint p2 )
{
	double dLat = DEG2RAD( p2.latitude - p1.latitude );
	double dLon = DEG2RAD( p2.longitude - p1.longitude );

	double lat1 = DEG2RAD( p1.latitude );
	double lat2 = DEG2RAD( p2.latitude );

	double a = pow( sin( dLat / 2 ), 2 ) + pow( sin( dLon / 2 ), 2 ) * cos( lat1 ) * cos( lat2 );

	double c = 2 * atan2( sqrt( a ), sqrt( 1 - a ) );

	return EARTH_RADIUS * c;
}

double getBearing( GpsPoint p1, GpsPoint p2 )
{
	double dLon = DEG2RAD( p2.longitude - p1.longitude );

	double lat1 = DEG2RAD( p1.latitude );
	double lat2 = DEG2RAD( p2.latitude );

	double x = cos( lat1 ) * sin( lat2 ) - sin( lat1 ) * cos( lat2 ) * cos( dLon );
	double y = sin( dLon ) * cos( lat2 );

	double bearing = RAD2DEG( atan2( y, x ) );

	// atan2() returns values from -180 to 180, want values from 0 to 360.
	if( bearing < 0 )
	{
		bearing += 360;
	}

	return bearing;
}

double getHeading( GpsPoint curr, GpsPoint prev )
{
	// Bearing and heading are different things, Google it.
	return getBearing( prev, curr );
}


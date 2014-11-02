
#include <math.h>

#include "range.h"
#include "cfg.h"
#include "hlog.h"

float lat2rad(float lat)
{
	return (lat * (M_PI / 180.0));
}

float lon2rad(float lon)
{
	return (lon * (M_PI / 180.0));
}

static float maidenhead_km_distance(float lat1, float lon1, float lat2, float lon2)
{
	float sindlat2 = sinf((lat1 - lat2) * 0.5);
	float sindlon2 = sinf((lon1 - lon2) * 0.5);
        
	float coslat1 = cosf(lat1);
	float coslat2 = cosf(lat2);
	
	float a = (sindlat2 * sindlat2 + coslat1 * coslat2 * sindlon2 * sindlon2);
	float c = 2.0 * atan2f( sqrtf(a), sqrtf(1.0 - a));
	
	return ((111.2 * 180.0 / M_PI) * c);
}

void update_range(struct demod_state_t *d, float lat, float lon)
{
	// ignore bad GPS fixes, sent commonly by some AIS stations
	if (lat > 89.0 || lat < -89.0 || lon > 180.01 || lon < -180.01)
		return;
	
	if (lat < 0.001 && lat > -0.001 && lon < 0.001 && lon > -0.001)
		return;
	
	float distance = maidenhead_km_distance(mylat, mylng, lat2rad(lat), lon2rad(lon));
	
	if (distance > d->best_range)
		d->best_range = distance;
}

void log_range(struct demod_state_t *rx)
{
	if (rx->best_range > 0.1)
		hlog(LOG_INFO, "Best range ch %c: %.1f km", rx->chanid, rx->best_range);
		
	rx->best_range = 0.0;
}

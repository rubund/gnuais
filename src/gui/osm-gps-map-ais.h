/* osm-gps-map-ais-classic.h
 *
 * Copyright (C) 2010 John Stowers <john.stowers@gmail.com>
 *
 * This is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __OSM_GPS_MAP_AIS_H__
#define __OSM_GPS_MAP_AIS_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define OSM_TYPE_GPS_MAP_AIS            (osm_gps_map_ais_get_type())
#define OSM_GPS_MAP_AIS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj),  OSM_TYPE_GPS_MAP_AIS, OsmGpsMapAis))
#define OSM_GPS_MAP_AIS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),   OSM_TYPE_GPS_MAP_AIS, OsmGpsMapAisClass))
#define OSM_IS_GPS_MAP_AIS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj),  OSM_TYPE_GPS_MAP_AIS))
#define OSM_IS_GPS_MAP_AIS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),   OSM_TYPE_GPS_MAP_AIS))
#define OSM_GPS_MAP_AIS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),   OSM_TYPE_GPS_MAP_AIS, OsmGpsMapAisClass))

typedef struct _OsmGpsMapAis        OsmGpsMapAis;
typedef struct _OsmGpsMapAisClass   OsmGpsMapAisClass;
typedef struct _OsmGpsMapAisPrivate OsmGpsMapAisPrivate;

struct _OsmGpsMapAis
{
    GObject parent;

	/*< private >*/
	OsmGpsMapAisPrivate *priv;
};

struct _OsmGpsMapAisClass
{
	GObjectClass parent_class;

	/* vtable */
	
};

GType               osm_gps_map_ais_get_type (void);
OsmGpsMapAis*       osm_gps_map_ais_new      (void);

G_END_DECLS

#define MAXSHIPS 1000

typedef struct {
	int mmsi;
	double longitude;
	double latitude;
	float heading;
	float course;
	int type;
} ship;



#endif /* __OSM_GPS_MAP_AIS_H__ */

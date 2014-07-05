/* osm-gps-map-ais-classic.c
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

#include <math.h>
#include <cairo.h>
#include "osd-utils.h"

#include "private.h"

#include "osm-gps-map-layer.h"
#include "osm-gps-map-ais.h"

static void osm_gps_map_ais_interface_init (OsmGpsMapLayerIface *iface);

ship *ships[MAXSHIPS];
int numberofships = 0;
G_LOCK_EXTERN(updatemap);
//G_LOCK_DEFINE_STATIC(updatemap);

G_DEFINE_TYPE_WITH_CODE (OsmGpsMapAis, osm_gps_map_ais, G_TYPE_OBJECT,
         G_IMPLEMENT_INTERFACE (OSM_TYPE_GPS_MAP_LAYER,
                                osm_gps_map_ais_interface_init));

enum
{
	PROP_0,
    PROP_OSD_X,
    PROP_OSD_Y,
	PROP_DPAD_RADIUS,
	PROP_SHOW_SCALE,
	PROP_SHOW_COORDINATES,
	PROP_SHOW_CROSSHAIR,
	PROP_SHOW_DPAD,
	PROP_SHOW_ZOOM,
	PROP_SHOW_GPS_IN_DPAD,
	PROP_SHOW_GPS_IN_ZOOM
};

typedef struct _OsdScale {
    cairo_surface_t *surface;
    int zoom;
    float lat;
} OsdScale_t;

typedef struct _OsdCoordinates {
    cairo_surface_t *surface;
    float lat, lon;
} OsdCoordinates_t;

typedef struct _OsdCorosshair {
    cairo_surface_t *surface;
    gboolean rendered;
} OsdCrosshair_t;

typedef struct _OsdControls {
    cairo_surface_t *surface;
    gboolean rendered;
    gint gps_enabled;
} OsdControls_t;

struct _OsmGpsMapAisPrivate
{
    OsdScale_t          *scale;
    OsdCoordinates_t    *coordinates;
    OsdCrosshair_t      *crosshair;
    OsdControls_t       *controls;
    guint               osd_w;
    guint               osd_h;
    guint               osd_shadow;
    guint               osd_pad;
    guint               zoom_w;
    guint               zoom_h;

    /* properties */
    gint                osd_x;
    gint                osd_y;
	guint               dpad_radius;
	gboolean            show_scale;
	gboolean            show_coordinates;
	gboolean            show_crosshair;
	gboolean            show_dpad;
	gboolean            show_zoom;
	gboolean            show_gps_in_dpad;
	gboolean            show_gps_in_zoom;
};

static void                 osm_gps_map_ais_render       (OsmGpsMapLayer *osd, OsmGpsMap *map);
static void                 osm_gps_map_ais_draw         (OsmGpsMapLayer *osd, OsmGpsMap *map, cairo_t *cr);
static gboolean             osm_gps_map_ais_busy         (OsmGpsMapLayer *osd);
static gboolean             osm_gps_map_ais_button_press (OsmGpsMapLayer *osd, OsmGpsMap *map, GdkEventButton *event);

static void                 coordinates_render                   (OsmGpsMapAis *self, OsmGpsMap *map);
static void                 coordinates_draw                     (OsmGpsMapAis *self, GtkAllocation *allocation, cairo_t *cr);

#define OSD_MAX_SHADOW (4)

#define OSD_SCALE_FONT_SIZE (12.0)
#define OSD_SCALE_W   (10*OSD_SCALE_FONT_SIZE)
#define OSD_SCALE_H   (5*OSD_SCALE_FONT_SIZE/2)

#define OSD_SCALE_H2   (OSD_SCALE_H/2)
#define OSD_SCALE_TICK (2*OSD_SCALE_FONT_SIZE/3)
#define OSD_SCALE_M    (OSD_SCALE_H2 - OSD_SCALE_TICK)
#define OSD_SCALE_I    (OSD_SCALE_H2 + OSD_SCALE_TICK)
#define OSD_SCALE_FD   (OSD_SCALE_FONT_SIZE/4)

#define OSD_COORDINATES_FONT_SIZE (12.0)
#define OSD_COORDINATES_OFFSET (OSD_COORDINATES_FONT_SIZE/6)
#define OSD_COORDINATES_W  (8*OSD_COORDINATES_FONT_SIZE+2*OSD_COORDINATES_OFFSET)
#define OSD_COORDINATES_H  (2*OSD_COORDINATES_FONT_SIZE+2*OSD_COORDINATES_OFFSET+OSD_COORDINATES_FONT_SIZE/4)

#define OSD_CROSSHAIR_RADIUS 10
#define OSD_CROSSHAIR_TICK  (OSD_CROSSHAIR_RADIUS/2)
#define OSD_CROSSHAIR_BORDER (OSD_CROSSHAIR_TICK + OSD_CROSSHAIR_RADIUS/4)
#define OSD_CROSSHAIR_W  ((OSD_CROSSHAIR_RADIUS+OSD_CROSSHAIR_BORDER)*2)
#define OSD_CROSSHAIR_H  ((OSD_CROSSHAIR_RADIUS+OSD_CROSSHAIR_BORDER)*2)

static void
osm_gps_map_ais_interface_init (OsmGpsMapLayerIface *iface)
{
    iface->render = osm_gps_map_ais_render;
    iface->draw = osm_gps_map_ais_draw;
    iface->busy = osm_gps_map_ais_busy;
    iface->button_press = osm_gps_map_ais_button_press;
}

static void
osm_gps_map_ais_get_property (GObject    *object,
                                      guint       property_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
	switch (property_id) {
	case PROP_OSD_X:
		g_value_set_int (value, OSM_GPS_MAP_AIS (object)->priv->osd_x);
		break;
	case PROP_OSD_Y:
		g_value_set_int (value, OSM_GPS_MAP_AIS (object)->priv->osd_y);
		break;
	case PROP_DPAD_RADIUS:
		g_value_set_uint (value, OSM_GPS_MAP_AIS (object)->priv->dpad_radius);
		break;
	case PROP_SHOW_SCALE:
		g_value_set_boolean (value, OSM_GPS_MAP_AIS (object)->priv->show_scale);
		break;
	case PROP_SHOW_COORDINATES:
		g_value_set_boolean (value, OSM_GPS_MAP_AIS (object)->priv->show_coordinates);
		break;
	case PROP_SHOW_CROSSHAIR:
		g_value_set_boolean (value, OSM_GPS_MAP_AIS (object)->priv->show_crosshair);
		break;
	case PROP_SHOW_DPAD:
		g_value_set_boolean (value, OSM_GPS_MAP_AIS (object)->priv->show_dpad);
		break;
	case PROP_SHOW_ZOOM:
		g_value_set_boolean (value, OSM_GPS_MAP_AIS (object)->priv->show_zoom);
		break;
	case PROP_SHOW_GPS_IN_DPAD:
		g_value_set_boolean (value, OSM_GPS_MAP_AIS (object)->priv->show_gps_in_dpad);
		break;
	case PROP_SHOW_GPS_IN_ZOOM:
		g_value_set_boolean (value, OSM_GPS_MAP_AIS (object)->priv->show_gps_in_zoom);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
osm_gps_map_ais_set_property (GObject      *object,
                                      guint         property_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
	switch (property_id) {
	case PROP_OSD_X:
		OSM_GPS_MAP_AIS (object)->priv->osd_x = g_value_get_int (value);
		break;
	case PROP_OSD_Y:
		OSM_GPS_MAP_AIS (object)->priv->osd_y = g_value_get_int (value);
		break;
	case PROP_DPAD_RADIUS:
		OSM_GPS_MAP_AIS (object)->priv->dpad_radius = g_value_get_uint (value);
		break;
	case PROP_SHOW_SCALE:
		OSM_GPS_MAP_AIS (object)->priv->show_scale = g_value_get_boolean (value);
		break;
	case PROP_SHOW_COORDINATES:
		OSM_GPS_MAP_AIS (object)->priv->show_coordinates = g_value_get_boolean (value);
		break;
	case PROP_SHOW_CROSSHAIR:
		OSM_GPS_MAP_AIS (object)->priv->show_crosshair = g_value_get_boolean (value);
		break;
	case PROP_SHOW_DPAD:
		OSM_GPS_MAP_AIS (object)->priv->show_dpad = g_value_get_boolean (value);
		break;
	case PROP_SHOW_ZOOM:
		OSM_GPS_MAP_AIS (object)->priv->show_zoom = g_value_get_boolean (value);
		break;
	case PROP_SHOW_GPS_IN_DPAD:
		OSM_GPS_MAP_AIS (object)->priv->show_gps_in_dpad = g_value_get_boolean (value);
		break;
	case PROP_SHOW_GPS_IN_ZOOM:
		OSM_GPS_MAP_AIS (object)->priv->show_gps_in_zoom = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static GObject *
osm_gps_map_ais_constructor (GType gtype, guint n_properties, GObjectConstructParam *properties)
{
    GObject *object;
    OsmGpsMapAisPrivate *priv;

    /* Always chain up to the parent constructor */
    object = G_OBJECT_CLASS(osm_gps_map_ais_parent_class)->constructor(gtype, n_properties, properties);
    priv = OSM_GPS_MAP_AIS(object)->priv;

    /* shadow also depends on control size */
    priv->osd_shadow = MAX(priv->dpad_radius/8, OSD_MAX_SHADOW);

    /* distance between dpad and zoom */
    priv->osd_pad = priv->dpad_radius/4;

    /* size of zoom pad is wrt. the dpad size */
    priv->zoom_w = 2*priv->dpad_radius;
    priv->zoom_h = priv->dpad_radius;

    /* total width and height of controls incl. shadow */
    priv->osd_w = 2*priv->dpad_radius + priv->osd_shadow + priv->zoom_w;
    priv->osd_h = 2*priv->dpad_radius + priv->osd_pad + priv->zoom_h + 2*priv->osd_shadow;

    priv->scale = g_new0(OsdScale_t, 1);
    priv->scale->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, OSD_SCALE_W, OSD_SCALE_H);
    priv->scale->zoom = -1;
    priv->scale->lat = 360.0; /* init to an invalid lat so we get re-rendered */

    priv->coordinates = g_new0(OsdCoordinates_t, 1);
    priv->coordinates->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1600, 1200); // FIXME: This should set dependent on window size.
    priv->coordinates->lat = priv->coordinates->lon = OSM_GPS_MAP_INVALID;

    priv->crosshair = g_new0(OsdCrosshair_t, 1);
    priv->crosshair->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, OSD_CROSSHAIR_W, OSD_CROSSHAIR_H);
    priv->crosshair->rendered = FALSE;

    priv->controls = g_new0(OsdControls_t, 1);
    //FIXME: SIZE DEPENDS ON IF DPAD AND ZOOM IS THERE OR NOT
    priv->controls->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, priv->osd_w+2, priv->osd_h+2);
    priv->controls->rendered = FALSE;
    priv->controls->gps_enabled = -1;

    return object;
}

#define OSD_STRUCT_DESTROY(_x)                                  \
    if ((_x)) {                                                 \
        if ((_x)->surface)                                      \
            cairo_surface_destroy((_x)->surface);               \
        g_free((_x));                                           \
    }

static void
osm_gps_map_ais_finalize (GObject *object)
{
    OsmGpsMapAisPrivate *priv = OSM_GPS_MAP_AIS(object)->priv;

    OSD_STRUCT_DESTROY(priv->scale)
    OSD_STRUCT_DESTROY(priv->coordinates)
    OSD_STRUCT_DESTROY(priv->crosshair)
    OSD_STRUCT_DESTROY(priv->controls)

	G_OBJECT_CLASS (osm_gps_map_ais_parent_class)->finalize (object);
}

static void
osm_gps_map_ais_class_init (OsmGpsMapAisClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	g_type_class_add_private (klass, sizeof (OsmGpsMapAisPrivate));

	object_class->get_property = osm_gps_map_ais_get_property;
	object_class->set_property = osm_gps_map_ais_set_property;
	object_class->constructor = osm_gps_map_ais_constructor;
	object_class->finalize     = osm_gps_map_ais_finalize;

	/**
	 * OsmGpsMapAis:osd-x:
	 *
	 * The osd x property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_OSD_X,
	                                 g_param_spec_int ("osd-x",
	                                                     "osd-x",
	                                                     "osd-x",
	                                                     G_MININT,
	                                                     G_MAXINT,
	                                                     10,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:osd-y:
	 *
	 * The osd y property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_OSD_Y,
	                                 g_param_spec_int ("osd-y",
	                                                     "osd-y",
	                                                     "osd-y",
	                                                     G_MININT,
	                                                     G_MAXINT,
	                                                     10,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:dpad-radius:
	 *
	 * The dpad radius property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_DPAD_RADIUS,
	                                 g_param_spec_uint ("dpad-radius",
	                                                     "dpad-radius",
	                                                     "dpad radius",
	                                                     0,
	                                                     G_MAXUINT,
	                                                     30,
	                                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:show-scale:
	 *
	 * The show scale on the map property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_SCALE,
	                                 g_param_spec_boolean ("show-scale",
	                                                       "show-scale",
	                                                       "show scale on the map",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:show-coordinates:
	 *
	 * The show coordinates of map centre property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_COORDINATES,
	                                 g_param_spec_boolean ("show-coordinates",
	                                                       "show-coordinates",
	                                                       "show coordinates of map centre",
	                                                       TRUE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:show-crosshair:
	 *
	 * The show crosshair at map centre property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_CROSSHAIR,
	                                 g_param_spec_boolean ("show-crosshair",
	                                                       "show-crosshair",
	                                                       "show crosshair at map centre",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:show-dpad:
	 *
	 * The show dpad for map navigation property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_DPAD,
	                                 g_param_spec_boolean ("show-dpad",
	                                                       "show-dpad",
	                                                       "show dpad for map navigation",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:show-zoom:
	 *
	 * The show zoom control for map navigation property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_ZOOM,
	                                 g_param_spec_boolean ("show-zoom",
	                                                       "show-zoom",
	                                                       "show zoom control for map navigation",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:show-gps-in-dpad:
	 *
	 * The show gps indicator in middle of dpad property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_GPS_IN_DPAD,
	                                 g_param_spec_boolean ("show-gps-in-dpad",
	                                                       "show-gps-in-dpad",
	                                                       "show gps indicator in middle of dpad",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
	/**
	 * OsmGpsMapAis:show-gps-in-zoom:
	 *
	 * The show gps indicator in middle of zoom control property.
	 */
	g_object_class_install_property (object_class,
	                                 PROP_SHOW_GPS_IN_ZOOM,
	                                 g_param_spec_boolean ("show-gps-in-zoom",
	                                                       "show-gps-in-zoom",
	                                                       "show gps indicator in middle of zoom control",
	                                                       FALSE,
	                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
}

static void
osm_gps_map_ais_init (OsmGpsMapAis *self)
{
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
	                                          OSM_TYPE_GPS_MAP_AIS,
	                                          OsmGpsMapAisPrivate);
}

static void
osm_gps_map_ais_render (OsmGpsMapLayer *osd,
                                OsmGpsMap *map)
{
	printf("Rendering\n");
    OsmGpsMapAis *self;
    //OsmGpsMapAisPrivate *priv;

    g_return_if_fail(OSM_IS_GPS_MAP_AIS(osd));

    self = OSM_GPS_MAP_AIS(osd);
    //priv = self->priv;

	coordinates_render(self, map);

}

static void
osm_gps_map_ais_draw (OsmGpsMapLayer *osd,
                              OsmGpsMap *map,
                              cairo_t *cr)
{
	printf("Drawing\n");
    OsmGpsMapAis *self;
    //OsmGpsMapAisPrivate *priv;
    GtkAllocation allocation;

    g_return_if_fail(OSM_IS_GPS_MAP_AIS(osd));

    self = OSM_GPS_MAP_AIS(osd);
    //priv = self->priv;

    gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);

	coordinates_draw(self, &allocation, cr);

}

static gboolean
osm_gps_map_ais_busy (OsmGpsMapLayer *osd)
{
	return FALSE;
}

static gboolean
osm_gps_map_ais_button_press (OsmGpsMapLayer *osd,
                                      OsmGpsMap *map,
                                      GdkEventButton *event)
{
    gboolean handled = FALSE;
    OsdControlPress_t but = OSD_NONE;
    OsmGpsMapAis *self;
    OsmGpsMapAisPrivate *priv;
    GtkAllocation allocation;

    g_return_val_if_fail(OSM_IS_GPS_MAP_AIS(osd), FALSE);

    self = OSM_GPS_MAP_AIS(osd);
    priv = self->priv;
    gtk_widget_get_allocation(GTK_WIDGET(map), &allocation);

    if ((event->button == 1) && (event->type == GDK_BUTTON_PRESS)) {
        gint mx = event->x - priv->osd_x;
        gint my = event->y - priv->osd_y;

        if(priv->osd_x < 0)
            mx -= (allocation.width - priv->osd_w);
    
        if(priv->osd_y < 0)
            my -= (allocation.height - priv->osd_h);

        /* first do a rough test for the OSD area. */
        /* this is just to avoid an unnecessary detailed test */
        if(mx > 0 && mx < priv->osd_w && my > 0 && my < priv->osd_h) {
            if (priv->show_dpad) {
                but = osd_check_dpad(mx, my, priv->dpad_radius, priv->show_gps_in_dpad);
                my -= (2*priv->dpad_radius);
                my -= priv->osd_pad;
            }
            if (but == OSD_NONE && priv->show_zoom)
                but = osd_check_zoom(mx, my, priv->zoom_w, priv->zoom_h, 0 /*show gps*/);
        }
    }

    switch (but) {
        case OSD_LEFT:
            osm_gps_map_scroll(map, -5, 0);
            handled = TRUE;
            break;
        case OSD_RIGHT:
            osm_gps_map_scroll(map, 5, 0);
            handled = TRUE;
            break;
        case OSD_UP:
            osm_gps_map_scroll(map, 0, -5);
            handled = TRUE;
            break;
        case OSD_DOWN:
            osm_gps_map_scroll(map, 0, 5);
            handled = TRUE;
            break;
        case OSD_OUT:
            osm_gps_map_zoom_out(map);
            handled = TRUE;
            break;
        case OSD_IN:
            osm_gps_map_zoom_in(map);
            handled = TRUE;
            break;
        case OSD_NONE:
        case OSD_GPS:
        default:
            handled = FALSE;
            break;
    }

    return handled;
}

/**
 * osm_gps_map_ais_new:
 *
 * Creates a new instance of #OsmGpsMapAis.
 *
 * Return value: the newly created #OsmGpsMapAis instance
 */
OsmGpsMapAis*
osm_gps_map_ais_new (void)
{
	return g_object_new (OSM_TYPE_GPS_MAP_AIS, NULL);
}


int draw_one_boat(cairo_t *cr, int x, int y, float theta){
	GdkPoint punkter[3];
	GdkPoint punkter2[4];

	punkter[0].x = x - 5 * cos(theta) + 18 * sin(theta);
	punkter[0].y = y - 5 * sin(theta) - 18 * cos(theta);
	punkter[1].x = x + 0 * cos(theta) + 29 * sin(theta);
	punkter[1].y = y + 0 * sin(theta) - 29 * cos(theta);
	punkter[2].x = x + 5 * cos(theta) + 18 * sin(theta);
	punkter[2].y = y + 5 * sin(theta) - 18 * cos(theta);

	punkter2[0].x = x - 5 * cos(theta) + 20 * sin(theta);
	punkter2[0].y = y - 5 * sin(theta) - 20 * cos(theta);
	punkter2[1].x = x + 5 * cos(theta) + 20 * sin(theta);
	punkter2[1].y = y + 5 * sin(theta) - 20 * cos(theta);
	punkter2[2].x = x + 5 * cos(theta) - 10 * sin(theta);
	punkter2[2].y = y + 5 * sin(theta) + 10 * cos(theta);
	punkter2[3].x = x - 5 * cos(theta) - 10 * sin(theta);
	punkter2[3].y = y - 5 * sin(theta) + 10 * cos(theta);

	cairo_move_to(cr, punkter[0].x,punkter[0].y);
	cairo_line_to(cr, punkter[1].x,punkter[1].y);
	cairo_line_to(cr, punkter[2].x,punkter[2].y);
	cairo_fill(cr);
	cairo_move_to(cr, punkter2[0].x,punkter2[0].y);
	cairo_line_to(cr, punkter2[1].x,punkter2[1].y);
	cairo_line_to(cr, punkter2[2].x,punkter2[2].y);
	cairo_line_to(cr, punkter2[3].x,punkter2[3].y);
	cairo_fill(cr);

	return 0;
}

static void
coordinates_render(OsmGpsMapAis *self, OsmGpsMap *map)
{
    OsdCoordinates_t *coordinates = self->priv->coordinates;
	OsmGpsMapPoint point;

    if(!coordinates->surface)
        return;
	gfloat lat, lon;
	g_object_get(G_OBJECT(map), "latitude", &lat, "longitude", &lon, NULL);
	coordinates->lat = lat;
	coordinates->lon = lon;

	gint pixel_x, pixel_y;

    g_assert(coordinates->surface);
    cairo_t *cr = cairo_create(coordinates->surface);
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
	cairo_set_line_width(cr, 1);

	int i;
	float theta;
	printf("Number of vessels: %d\n",numberofships);
	G_LOCK(updatemap);
	for(i=0;i<numberofships;i++) {
		osm_gps_map_point_set_degrees(&point, ships[i]->latitude, ships[i]->longitude);
		osm_gps_map_convert_geographic_to_screen(map, &point, &pixel_x,&pixel_y);
		if (ships[i]->heading != 511)
			theta = ships[i]->heading * M_PI / 180;
		else
			theta = ships[i]->course * M_PI / 180;
		draw_one_boat(cr,(int)pixel_x,(int)pixel_y,theta);
		printf("%f %f\n",ships[i]->latitude, ships[i]->longitude);
	}
	G_UNLOCK(updatemap);


    cairo_destroy(cr);
}

static void
coordinates_draw(OsmGpsMapAis *self, GtkAllocation *allocation, cairo_t *cr)
{
    //OsmGpsMapAisPrivate *priv = self->priv;
    OsdCoordinates_t *coordinates = self->priv->coordinates;

    cairo_set_source_surface(cr, coordinates->surface, 0, 0);
    cairo_paint(cr);
}







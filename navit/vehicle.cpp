/*
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2009 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/** @file vehicle.c
 * @defgroup vehicle-plugins vehicle plugins
 * @ingroup plugins
 * @brief Generic components of the vehicle object.
 *
 * This file implements the generic vehicle interface, i.e. everything which is
 * not specific to a single data source.
 *
 * @author Navit Team
 * @date 2005-2014
 */

#include "vehicle.h"

extern "C"
{
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <time.h>
#include <math.h> /* for sqrt from coord.h */
#include "config.h"
#include "debug.h"
#include "coord.h"
#include "item.h"
#include "xmlconfig.h"
#include "log.h"
#include "plugin.h"
#include "transform.h"
#include "util.h"
#include "event.h"
#include "coord.h"
#include "transform.h"
#include "projection.h"
#include "point.h"
#include "callback.h"
#include "navit_nls.h"
}

/**
 * @brief Creates a new vehicle
 *
 * @param parent
 * @param attrs Points to a null-terminated array of pointers to the attributes
 * for the new vehicle type.
 *
 * @return The newly created vehicle object
 */

Vehicle::Vehicle(NavitInterface *navit, NavitVehicleConfig *config, NavitVehicleFactory *factory, QObject *parent) : QObject(parent),
                                                                                                                     m_config(config),
                                                                                                                     m_plugin(factory->instantiate(navit))
{
    assert(m_plugin);

    struct pcoord center;

    center.pro = projection_screen;
    center.x = 0;
    center.y = 0;
    m_trans = transform_new(&center, 16, 0);

    dbg(lvl_debug, "leave");
    m_log_to_cb = g_hash_table_new(NULL, NULL);
}
/**
 * @brief Destroys a vehicle
 *
 */
Vehicle::~Vehicle()
{
    dbg(lvl_debug, "enter");
    if (m_animate_callback)
    {
        callback_destroy(m_animate_callback);
        event_remove_timeout(m_animate_timer);
    }
    transform_destroy(m_trans);
    // m_plugin->destroy();
    callback_list_destroy(m_cbl);
}

/**
 * Creates an attribute iterator to be used with vehicles
 */
struct attr_iter *
Vehicle::attr_iter_new(void *unused)
{
    return (struct attr_iter *)g_new0(void *, 1);
}

/**
 * Destroys a vehicle attribute iterator
 *
 * @param iter a vehicle attr_iter
 */
void Vehicle::attr_iter_destroy(struct attr_iter *iter)
{
    g_free(iter);
}

/**
 * @brief Generic remove function.
 *
 * Used to remove a callback from the vehicle.
 * * @param attr
 */
int Vehicle::remove_attr(struct attr *attr)
{
    struct callback *cb;
    switch (attr->type)
    {
    case attr_callback:
        callback_list_remove(m_cbl, attr->u.callback);
        break;
    case attr_log:
        cb = (callback *)g_hash_table_lookup(m_log_to_cb, attr->u.log);
        if (!cb)
            return 0;
        g_hash_table_remove(m_log_to_cb, attr->u.log);
        callback_list_remove(m_cbl, cb);
        break;
    default:
        qDebug() << "Trying to remove generic vehicle attribute" << attr_to_name(attr->type);
        return 0;
    }
    return 1;
}

void draw_do_callback(Vehicle *vehicle)
{
    vehicle->draw_do();
}
/**
 * Sets the cursor of a vehicle.
 *
 * * @param cursor A cursor
 * @author Ralph Sennhauser (10/2009)
 */
void Vehicle::set_cursor(LayoutCursor *cursor, int overwrite)
{
    if (m_cursor_fixed && !overwrite)
        return;
    if (m_animate_callback && m_animate_timer != nullptr)
    {
        event_remove_timeout(m_animate_timer);
        m_animate_timer = NULL; // dangling pointer! prevent double freeing.
        callback_destroy(m_animate_callback);
        m_animate_callback = NULL; // dangling pointer! prevent double freeing.
    }
    if (cursor && cursor->getInterval() > 0)
    {
        m_animate_callback = callback_new_1(callback_cast(draw_do_callback), this);
        m_animate_timer = event_add_timeout(cursor->getInterval(), 1, m_animate_callback);
    }
    /* we changed the cursor, so the overlay (if existing) may need a resize */
    m_need_resize = 1;
    m_cursor = cursor;

    /* if the graphics was already created, but a NULL cursor was set, we need to disable
     * otherwise stale overlay
     */
    if (m_gra)
    {
        if (m_cursor)
            m_gra->overlay_disable(0);
        else
            m_gra->overlay_disable(1);
    }
    /* vehicle_draw will care for the graphics */
}

/**
 * Draws a vehicle on top of a graphics.
 *
 * @param gra The graphics
 * @param pnt Screen coordinates of the vehicle.
 * @param angle The angle relative to the map.
 * @param speed The speed of the vehicle.
 */
void Vehicle::draw(Graphics *gra, struct point *pnt, int angle, int speed)
{
    struct point sc;
    if (angle < 0)
        angle += 360;
    dbg(lvl_debug, "point %d,%d", pnt->x, pnt->y);
    m_cursor_pnt = *pnt;
    m_angle = angle;
    m_speed = speed;
    if (!m_cursor)
        return;

    if ((m_need_resize) || !(m_gra))
    {
        /* recalculate real size of the required overlay */
        navit_float radius;

        /* get the radius of the out circle. Pythagoras greets */
        radius = navit_sqrt((m_cursor->getW() * m_cursor->getW()) + (m_cursor->getH() * m_cursor->getH()));
        /* since we rotate the rectangle around the center to indicate direction, the overlay needs to be at least the
         * radius of the out circle big. The +1 compensates the rounding error.
         */
        m_real_w = (int)radius + 1;
        m_real_h = (int)radius + 1;

        /* set transform center to the middle of the cursor */
        sc.x = m_real_w / 2;
        sc.y = m_real_h / 2;
        transform_set_screen_center(m_trans, &sc);
    }

    /* move the cursor point from te center to the top left*/
    m_cursor_pnt.x -= (m_real_w / 2);
    m_cursor_pnt.y -= (m_real_h / 2);
    dbg(lvl_debug, "real point %d,%d real size %d,%d", m_cursor_pnt.x, m_cursor_pnt.y, m_real_w,
        m_real_h);

    if (!m_gra)
    {
        QColor c;
        m_need_resize = 0;

        m_gra = new Graphics(gra->get_navit_interface(), *gra, &m_cursor_pnt, m_real_w, m_real_h, 0);
        if (m_gra)
        {
            m_gra->init();
            GraphicsFunctions &graphics_functions = ((Graphics *)m_gra)->get_graphics_functions();

            m_bg = new GraphicsContext(*graphics_functions.new_graphics_context(), (Graphics *)m_gra);

            c.setAlpha(0);

            m_bg->set_foreground(c);
            m_gra->background_gc(m_bg);
        }
    }
    else if (m_need_resize)
    {
        /* seems the cursor was changed. Need to resize */
        m_need_resize = 0;
        m_gra->overlay_resize(&m_cursor_pnt, m_real_w, m_real_h, 0);
    }

    draw_do();
}

void Vehicle::draw_do()
{
    LayoutCoord p;
    int speed = m_speed;
    int angle = m_angle;
    int sequence = m_sequence;
    struct attr **attr;
    char *label = NULL;
    int match = 0;

    if (!m_cursor || !m_gra)
        return;

    while (attr && *attr)
    {
        if ((*attr)->type == attr_name)
            label = (*attr)->u.str;
        attr++;
    }
    transform_set_yaw(m_trans, -m_angle);
    m_gra->draw_mode(draw_mode_begin);

    /* clear old content by overwriting with an rectangle */
    m_gra->draw_rectangle(m_bg, &p, m_real_w, m_real_h);
    for (LayoutItemGraph *itemGraph : m_cursor->getItemgra())
    {
        LayoutRange *speed_range = itemGraph->getSpeedRange();
        LayoutRange *angle_range = itemGraph->getSpeedRange();
        LayoutRange *sequence_range = itemGraph->getSpeedRange();
        if (speed >= speed_range->getMin() && speed <= speed_range->getMax() &&
            angle >= angle_range->getMin() && angle <= angle_range->getMax() &&
            sequence >= sequence_range->getMin() && sequence <= sequence_range->getMax())
        {
            m_gra->draw_itemgra(itemGraph, m_trans, label);
        }
    }

    m_gra->draw_drag(&m_cursor_pnt);
    m_gra->draw_mode(draw_mode_end);
    if (m_animate_callback)
    {
        ++m_sequence;
        LayoutRange *range = m_cursor->getSequenceRange();
        if (range->getMin() != range->getMax())
        {
            if (range->getMax() < m_sequence)
                m_sequence = range->getMin();
        }
        else if (!match)
        {
            m_sequence = 0;
        }
    }
}
bool Vehicle::isPositionValid() { return m_plugin->isPositionValid(); }
coord_geo Vehicle::getPosition() { return m_plugin->getPosition(); }
QString Vehicle::getIso8601Time() { return m_plugin->getIso8601Time(); }
double Vehicle::getSpeed() { return m_plugin->getSpeed(); }
double Vehicle::getDirection() { return m_plugin->getDirection(); }
int Vehicle::getFixType() { return m_plugin->getFixType(); }
int Vehicle::getLag() { return m_plugin->getLag(); }

const int &Vehicle::getFollow()
{
    return m_follow;
}
const int &Vehicle::getFollowCursor()
{
    return m_followCursor;
}
const QString &Vehicle::getName()
{
    return m_config->name;
}
const QString &Vehicle::getCursorName()
{
    return m_cursor->getName();
}
const QString &Vehicle::getProfileName()
{
    return m_config->profilename;
}

void Vehicle::setFollowCursor(const int &followCursor)
{
    m_followCursor = followCursor;
}
/**
 * @brief Writes to an NMEA log.
 *
 * @param log The log to write to
 */
void Vehicle::log_nmea(struct log *log)
{
    // struct attr pos_attr;
    // if (!m_plugin->position_attr_get)
    //     return;
    // if (!m_plugin->position_attr_get( attr_position_nmea, &pos_attr))
    //     return;
    // log_write(log, pos_attr.u.str, strlen(pos_attr.u.str), 0);
}

/**
 * Add a tag to the extensions section of a GPX trackpoint.
 *
 * @param tag The tag to add
 * @param logstr Pointer to a pointer to a string to be inserted into the log.
 * When calling this function, {@code *logstr} must point to the substring into which the new tag is
 * to be inserted. If {@code *logstr} is NULL, a new string will be created for the extensions section.
 * Upon returning, {@code *logstr} will point to the new string with the additional tag inserted.
 */
void Vehicle::log_gpx_add_tag(char *tag, char **logstr)
{
    // char *ext_start = "\t<extensions>\n";
    // char *ext_end = "\t</extensions>\n";
    // char *trkpt_end = "</trkpt>";
    // char *start = NULL, *end = NULL;
    // if (!*logstr)
    // {
    //     start = g_strdup(ext_start);
    //     end = g_strdup(ext_end);
    // }
    // else
    // {
    //     char *str = strstr(*logstr, ext_start);
    //     int len;
    //     if (str)
    //     {
    //         len = str - *logstr + strlen(ext_start);
    //         start = g_strdup(*logstr);
    //         start[len] = '\0';
    //         end = g_strdup(str + strlen(ext_start));
    //     }
    //     else
    //     {
    //         str = strstr(*logstr, trkpt_end);
    //         len = str - *logstr;
    //         end = g_strdup_printf("%s%s", ext_end, str);
    //         str = g_strdup(*logstr);
    //         str[len] = '\0';
    //         start = g_strdup_printf("%s%s", str, ext_start);
    //         g_free(str);
    //     }
    // }
    // *logstr = g_strdup_printf("%s%s%s", start, tag, end);
    // g_free(start);
    // g_free(end);
}

/**
 * @brief Writes a trackpoint to a GPX log.
 *
 * @param log The log to write to
 */
void Vehicle::log_gpx(struct log *log)
{
    // struct attr attr, *attrp, fix_attr;
    // enum attr_type *attr_types;
    // char *logstr;
    // char *extensions = "\t<extensions>\n";

    // if (!m_plugin->position_attr_get)
    //     return;
    // if (log_get_attr(log, attr_attr_types, &attr, NULL))
    //     attr_types = attr.u.attr_types;
    // else
    //     attr_types = NULL;
    // if (m_plugin->position_attr_get( attr_position_fix_type, &fix_attr))
    // {
    //     if (fix_attr.u.num == 0)
    //         return;
    // }
    // if (!m_plugin->position_attr_get( attr_position_coord_geo, &attr))
    //     return;
    // logstr = g_strdup_printf("<trkpt lat=\"%f\" lon=\"%f\">\n", attr.u.coord_geo->lat, attr.u.coord_geo->lng);
    // if (attr_types && attr_types_contains_default(attr_types, attr_position_time_iso8601, 0))
    // {
    //     if (m_plugin->position_attr_get( attr_position_time_iso8601, &attr))
    //     {
    //         logstr = g_strconcat_printf(logstr, "\t<time>%s</time>\n", attr.u.str);
    //     }
    //     else
    //     {
    //         char *timep = current_to_iso8601();
    //         logstr = g_strconcat_printf(logstr, "\t<time>%s</time>\n", timep);
    //         g_free(timep);
    //     }
    // }
    // if (m_gpx_desc)
    // {
    //     logstr = g_strconcat_printf(logstr, "\t<desc>%s</desc>\n", m_gpx_desc);
    //     g_free(m_gpx_desc);
    //     m_gpx_desc = NULL;
    // }
    // if (attr_types_contains_default(attr_types, attr_position_height, 0) && m_plugin->position_attr_get( attr_position_height, &attr))
    //     logstr = g_strconcat_printf(logstr, "\t<ele>%.6f</ele>\n", *attr.u.numd);
    // // <magvar> magnetic variation in degrees; we might use position_magnetic_direction and position_direction to figure it out
    // // <geoidheight> Height (in meters) of geoid (mean sea level) above WGS84 earth ellipsoid. As defined in NMEA GGA message (field 11, which vehicle_wince.c ignores)
    // // <name> GPS name (arbitrary)
    // // <cmt> comment
    // // <src> Source of data
    // // <link> Link to additional information (URL)
    // // <sym> Text of GPS symbol name
    // // <type> Type (classification)
    // // <fix> Type of GPS fix {'none'|'2d'|'3d'|'dgps'|'pps'}, leave out if unknown. Similar to position_fix_type but more detailed.
    // if (attr_types_contains_default(attr_types, attr_position_sats_used, 0) && m_plugin->position_attr_get( attr_position_sats_used, &attr))
    //     logstr = g_strconcat_printf(logstr, "\t<sat>%d</sat>\n", attr.u.num);
    // if (attr_types_contains_default(attr_types, attr_position_hdop, 0) && m_plugin->position_attr_get( attr_position_hdop, &attr))
    //     logstr = g_strconcat_printf(logstr, "\t<hdop>%.6f</hdop>\n", *attr.u.numd);
    // // <vdop>, <pdop> Vertical and position dilution of precision, no corresponding attribute
    // if (attr_types_contains_default(attr_types, attr_position_direction, 0) && m_plugin->position_attr_get( attr_position_direction, &attr))
    //     logstr = g_strconcat_printf(logstr, "\t<course>%.1f</course>\n", *attr.u.numd);
    // if (attr_types_contains_default(attr_types, attr_position_speed, 0) && m_plugin->position_attr_get( attr_position_speed, &attr))
    //     logstr = g_strconcat_printf(logstr, "\t<speed>%.2f</speed>\n", (*attr.u.numd / 3.6));
    // if (attr_types_contains_default(attr_types, attr_profilename, 0) && (attrp = attr_search(m_attrs, attr_profilename)))
    // {
    //     logstr = g_strconcat_printf(logstr, "%s\t\t<navit:profilename>%s</navit:profilename>\n", extensions, attrp->u.str);
    //     extensions = "";
    // }
    // if (attr_types_contains_default(attr_types, attr_position_radius, 0) && m_plugin->position_attr_get( attr_position_radius, &attr))
    // {
    //     logstr = g_strconcat_printf(logstr, "%s\t\t<navit:radius>%.2f</navit:radius>\n", extensions, *attr.u.numd);
    //     extensions = "";
    // }
    // if (!strcmp(extensions, ""))
    // {
    //     logstr = g_strconcat_printf(logstr, "\t</extensions>\n");
    // }
    // logstr = g_strconcat_printf(logstr, "</trkpt>\n");
    // callback_list_call_attr_1(m_cbl, attr_log_gpx, &logstr);
    // log_write(log, logstr, strlen(logstr), 0);
    // g_free(logstr);
}

/**
 * @brief Writes to a text log.
 *
 * @param log The log to write to
 */
void Vehicle::log_textfile(struct log *log)
{
    // struct attr pos_attr, fix_attr;
    // char *logstr;
    // if (!m_plugin->position_attr_get)
    //     return;
    // if (m_plugin->position_attr_get( attr_position_fix_type, &fix_attr))
    // {
    //     if (fix_attr.u.num == 0)
    //         return;
    // }
    // if (!m_plugin->position_attr_get( attr_position_coord_geo, &pos_attr))
    //     return;
    // logstr = g_strdup_printf("%f %f type=trackpoint\n", pos_attr.u.coord_geo->lng, pos_attr.u.coord_geo->lat);
    // callback_list_call_attr_1(m_cbl, attr_log_textfile, &logstr);
    // log_write(log, logstr, strlen(logstr), 0);
}

/**
 * @brief Writes to a binary log.
 *
 * @param log The log to write to
 */
void Vehicle::log_binfile(struct log *log)
{
    // struct attr pos_attr, fix_attr;
    // int *buffer;
    // int *buffer_new;
    // int len, limit = 1024, done = 0, radius = 25;
    // struct coord c;
    // enum log_flags flags;

    // if (!m_plugin->position_attr_get)
    //     return;
    // if (m_plugin->position_attr_get( attr_position_fix_type, &fix_attr))
    // {
    //     if (fix_attr.u.num == 0)
    //         return;
    // }
    // if (!m_plugin->position_attr_get( attr_position_coord_geo, &pos_attr))
    //     return;
    // transform_from_geo(projection_mg, pos_attr.u.coord_geo, &c);
    // if (!c.x || !c.y)
    //     return;
    // while (!done)
    // {
    //     buffer = log_get_buffer(log, &len);
    //     if (!buffer || !len)
    //     {
    //         buffer_new = g_malloc(5 * sizeof(int));
    //         buffer_new[0] = 2;
    //         buffer_new[1] = type_track;
    //         buffer_new[2] = 0;
    //     }
    //     else
    //     {
    //         buffer_new = g_malloc((buffer[0] + 3) * sizeof(int));
    //         memcpy(buffer_new, buffer, (buffer[0] + 1) * sizeof(int));
    //     }
    //     dbg(lvl_debug, "c=0x%x,0x%x", c.x, c.y);
    //     buffer_new[buffer_new[0] + 1] = c.x;
    //     buffer_new[buffer_new[0] + 2] = c.y;
    //     buffer_new[0] += 2;
    //     buffer_new[2] += 2;
    //     if (buffer_new[2] > limit)
    //     {
    //         int count = buffer_new[2] / 2;
    //         struct coord *out = g_alloca(sizeof(struct coord) * (count));
    //         struct coord *in = (struct coord *)(buffer_new + 3);
    //         int count_out = transform_douglas_peucker(in, count, radius, out);
    //         memcpy(in, out, count_out * 2 * sizeof(int));
    //         buffer_new[0] += (count_out - count) * 2;
    //         buffer_new[2] += (count_out - count) * 2;
    //         flags = log_flag_replace_buffer | log_flag_force_flush | log_flag_truncate;
    //     }
    //     else
    //     {
    //         flags = log_flag_replace_buffer | log_flag_keep_pointer | log_flag_keep_buffer | log_flag_force_flush;
    //         done = 1;
    //     }
    //     log_write(log, (char *)buffer_new, (buffer_new[0] + 1) * sizeof(int), flags);
    // }
}

/**
 * @brief Registers a new log to receive data.
 *
 * @param log The log to write to
 *
 * @return False if the log is of an unknown type, true otherwise (including when {@code attr_type} is missing).
 */
int Vehicle::add_log(struct log *log)
{
    // struct callback *cb;
    // struct attr type_attr;
    // if (!log_get_attr(log, attr_type, &type_attr, NULL))
    //     return 1;

    // if (!strcmp(type_attr.u.str, "nmea"))
    // {
    //     cb = callback_new_attr_2(callback_cast(Vehicle::log_nmea), attr_position_coord_geo,  log);
    // }
    // else if (!strcmp(type_attr.u.str, "gpx"))
    // {
    //     char *header = "<?xml version='1.0' encoding='UTF-8'?>\n"
    //                    "<gpx version='1.1' creator='Navit http://navit.sourceforge.net'\n"
    //                    "     xmlns:xsi='http://www.w3.org/2001/XMLSchema-instance'\n"
    //                    "     xmlns:navit='http://www.navit-project.org/schema/navit'\n"
    //                    "     xmlns='http://www.topografix.com/GPX/1/1'\n"
    //                    "     xsi:schemaLocation='http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd'>\n"
    //                    "<trk>\n"
    //                    "<trkseg>\n";
    //     char *trailer = "</trkseg>\n</trk>\n</gpx>\n";
    //     log_set_header(log, header, strlen(header));
    //     log_set_trailer(log, trailer, strlen(trailer));
    //     cb = callback_new_attr_2(callback_cast(Vehicle::log_gpx), attr_position_coord_geo,  log);
    // }
    // else if (!strcmp(type_attr.u.str, "textfile"))
    // {
    //     char *header = "type=track\n";
    //     log_set_header(log, header, strlen(header));
    //     cb = callback_new_attr_2(callback_cast(Vehicle::log_textfile), attr_position_coord_geo,  log);
    // }
    // else if (!strcmp(type_attr.u.str, "binfile"))
    // {
    //     cb = callback_new_attr_2(callback_cast(Vehicle::log_binfile), attr_position_coord_geo,  log);
    // }
    // else
    //     return 0;
    // g_hash_table_insert(m_log_to_cb, log, cb);
    // callback_list_add(m_cbl, cb);
    return 1;
}
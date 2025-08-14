/*
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
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

extern "C"
{
#include <glib.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "debug.h"
#include "coord.h"
#include "item.h"
#include "navit_wrapper.h"
#include "map.h"
#include "route.h"
#include "callback.h"
#include "transform.h"
#include "plugin.h"
#include "vehicle_wrapper.h"
#include "event.h"
#include "util.h"
#include "track.h"
#include "attr_def.h"
}

#include "vehicle_demo.h"

NavitVehicleInterface *VehicleDemoFactory::newVehicle(NavitInterface &navit, NavitVehicleAttrs &attrs, callback_list *cbl)
{
    return new VehicleDemo(navit, attrs, cbl);
}

static void timer_callback(void *data)
{
    if (data != nullptr)
    {
        VehicleDemo *vehiclePointer = static_cast<VehicleDemo *>(data);
        vehiclePointer->timer();
    }
}

VehicleDemo::VehicleDemo(NavitInterface &navit, NavitVehicleAttrs &attrs, callback_list *cbl, QObject *parent)
    : NavitVehicleInterface(parent)
{
    dbg(lvl_debug, "enter");
    m_cbl = cbl;
    m_interval = 1000;
    m_config_speed = 40;
    m_height = -10;
    m_timer_callback = callback_new_1(callback_cast(timer_callback), this);
    m_valid = attr_position_valid_invalid;

    if (!m_timer)
        m_timer = event_add_timeout(m_interval, 1, m_timer_callback);

    // No attrs, so set everything
    m_route = navit.get_route();
    m_config_speed = attrs.config_speed;
    m_height = attrs.height;
    m_interval = attrs.interval;
    m_geo = attrs.geo;
}

void VehicleDemo::destroy()
{
    if (m_timer)
        event_remove_timeout(m_timer);
    callback_destroy(m_timer_callback);
    g_free(m_timep);
}

void VehicleDemo::nmea_chksum(char *nmea)
{
    int i;
    if (nmea && strlen(nmea) > 3)
    {
        unsigned char csum = 0;
        for (i = 1; i < strlen(nmea) - 4; i++)
            csum ^= (unsigned char)(nmea[i]);
        sprintf(nmea + strlen(nmea) - 3, "%02X\n", csum);
    }
}

int VehicleDemo::position_attr_get(
    enum attr_type type, struct attr *attr)
{
    char ns = 'N', ew = 'E', *timep, *rmc, *gga;
    int hr, min, sec, year, mon, day;
    double lat, lng;
    int *flags;
    switch (type)
    {
    case attr_position_speed:
        attr->u.numd = &m_speed;
        break;
    case attr_position_direction:
        attr->u.numd = &m_direction;
        break;
    case attr_position_coord_geo:
        attr->u.coord_geo = &m_geo;
        break;
    case attr_position_time_iso8601:
        g_free(m_timep);
        m_timep = current_to_iso8601();
        attr->u.str = m_timep;
        break;
    case attr_position_fix_type:
        if ((flags = tracking_get_current_flags(navit_get_tracking(m_navit))))
        {
            if (*flags & AF_UNDERGROUND)
                attr->u.num = 0;
        }
        else
            attr->u.num = 2;
        break;
    case attr_position_sats_used:
        attr->u.num = 3 + ((rand() % 2 + 1) * (rand() % 2 == 0 ? -1 : 1));
        break;
    case attr_position_height:
        attr->u.numd = &m_height;
        break;
    case attr_position_nmea:
        lat = m_geo.lat;
        if (lat < 0)
        {
            lat = -lat;
            ns = 'S';
        }
        lng = m_geo.lng;
        if (lng < 0)
        {
            lng = -lng;
            ew = 'W';
        }
        timep = current_to_iso8601();
        sscanf(timep, "%d-%d-%dT%d:%d:%d", &year, &mon, &day, &hr, &min, &sec);
        g_free(timep);
        gga = g_strdup_printf("$GPGGA,%02d%02d%02d,%02.0f%07.4f,%c,%03.0f%07.4f,%c,1,08,2.5,0,M,,,,0000*  \n", hr, min, sec,
                              floor(lat), (lat - floor(lat)) * 60.0, ns, floor(lng), (lng - floor(lng)) * 60, ew);
        nmea_chksum(gga);
        rmc = g_strdup_printf("$GPRMC,%02d%02d%02d,A,%02.0f%07.4f,%c,%03.0f%07.4f,%c,%3.1f,%3.1f,%02d%02d%02d,,*  \n", hr, min, sec,
                              floor(lat), (lat - floor(lat)) * 60.0, ns, floor(lng), (lng - floor(lng)) * 60, ew, m_speed / 1.852, (double)m_direction, day, mon,
                              year % 100);
        nmea_chksum(rmc);
        g_free(m_nmea);
        m_nmea = g_strdup_printf("%s%s", gga, rmc);
        g_free(gga);
        g_free(rmc);
        attr->u.str = m_nmea;
        break;
    case attr_position_valid:
        attr->u.num = m_valid;
        break;
    default:
        return 0;
    }
    attr->type = type;
    return 1;
}

int VehicleDemo::set_attr_do(struct attr *attr)
{
    switch (attr->type)
    {
    case attr_navit:
        m_navit = attr->u.navit;
        break;
    case attr_route:
        m_route = attr->u.route;
        break;
    case attr_speed:
        m_config_speed = attr->u.num;
        break;
    case attr_position_height:
        m_height = *attr->u.numd;
        break;
    case attr_interval:
        m_interval = attr->u.num;
        if (m_timer)
            event_remove_timeout(m_timer);
        m_timer = event_add_timeout(m_interval, 1, m_timer_callback);
        break;
    case attr_position_coord_geo:
        m_geo = *(attr->u.coord_geo);
        if (m_valid != attr_position_valid_valid)
        {
            m_valid = attr_position_valid_valid;
            emit positionValidChanged(true);
        }
        m_position_set = 1;
        dbg(lvl_debug, "position_set %f %f", m_geo.lat, m_geo.lng);
        break;
    case attr_profilename:
    case attr_source:
    case attr_name:
    case attr_follow:
    case attr_active:
        // Ignore; used by Navit's infrastructure, but not relevant for this vehicle.
        break;
    default:
        dbg(lvl_error, "unsupported attribute %s", attr_to_name(attr->type));
        return 0;
    }
    return 1;
}

int VehicleDemo::set_attr(struct attr *attr)
{
    return set_attr_do(attr);
}

void VehicleDemo::timer()
{
    struct coord c, c2, pos, ci;
    int slen, len, dx, dy;
    struct route *route = NULL;
    struct map *route_map = NULL;
    struct map_rect *mr = NULL;
    struct item *item = NULL;

    len = (m_config_speed * m_interval / 1000) / 3.6;
    dbg(lvl_debug, "###### Entering simulation loop");
    if (!m_config_speed)
        return;
    if (m_route)
        route = m_route;
    else if (m_navit)
        route = navit_get_route(m_navit);
    if (route)
        route_map = route_get_map(route);
    if (route_map)
        mr = map_rect_new(route_map, NULL);
    if (mr)
        item = map_rect_get_item(mr);
    if (item && item->type == type_route_start)
        item = map_rect_get_item(mr);
    while (item && item->type != type_street_route)
        item = map_rect_get_item(mr);

    if (!item && m_speed != 0)
    {
        dbg(lvl_warning, "Routing finished setting speed to 0");
        m_speed = 0;
        emit positionChanged(m_geo);
    }

    if (item && item_coord_get(item, &pos, 1))
    {
        m_position_set = 0;
        dbg(lvl_debug, "current pos=0x%x,0x%x", pos.x, pos.y);
        dbg(lvl_debug, "last pos=0x%x,0x%x", m_last.x, m_last.y);
        if (m_last.x == pos.x && m_last.y == pos.y)
        {
            dbg(lvl_warning, "endless loop");
            if (m_speed != 0)
            {
                dbg(lvl_warning, "Routing finished, but we're still stuck(?), setting speed to 0");
                m_speed = 0;
                emit positionChanged(m_geo);
            }
        }
        m_last = pos;
        while (item && m_config_speed)
        {
            if (!item_coord_get(item, &c, 1))
            {
                item = map_rect_get_item(mr);
                continue;
            }
            dbg(lvl_debug, "next pos=0x%x,0x%x", c.x, c.y);
            slen = transform_distance(projection_mg, &pos, &c);
            dbg(lvl_debug, "len=%d slen=%d", len, slen);
            if (slen < len)
            {
                len -= slen;
                pos = c;
            }
            else
            {
                if (item_coord_get(item, &c2, 1) || map_rect_get_item(mr))
                {
                    dx = c.x - pos.x;
                    dy = c.y - pos.y;
                    ci.x = pos.x + dx * len / slen;
                    ci.y = pos.y + dy * len / slen;
                    m_direction =
                        transform_get_angle_delta(&pos, &c, 0);
                    m_speed = m_config_speed + ((rand() % 5 + 1) * (rand() % 2 == 0 ? -1 : 1)); // a little random + or - 1 to 5 km/h
                    m_height = m_height + ((rand() % 50 + 1) * (rand() % 2 == 0 ? -1 : 1));     // a little random + or - 1 to 50 m
                }
                else
                {
                    ci.x = pos.x;
                    ci.y = pos.y;
                    m_speed = 0;
                    dbg(lvl_debug, "destination reached");
                }
                dbg(lvl_debug, "ci=0x%x,0x%x", ci.x, ci.y);
                transform_to_geo(projection_mg, &ci,
                                 &m_geo);
                if (m_valid != attr_position_valid_valid)
                {
                    m_valid = attr_position_valid_valid;
                    emit positionValidChanged(true);
                }
                emit positionChanged(m_geo);
                break;
            }
        }
    }
    else
    {
        if (m_position_set)
            emit positionChanged(m_geo);
    }
    if (mr)
        map_rect_destroy(mr);
}

NavitVehicleInterface *get_vehicle_functions()
{
}

void plugin_init(void)
{
    dbg(lvl_debug, "enter");
    // plugin_register_category(plugin_category_vehicle, "demo", get_vehicle_class);
}

/** @} */

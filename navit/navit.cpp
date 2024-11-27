/**
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

#define _USE_MATH_DEFINES 1
#include "config.h"
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>
#include <glib.h>
#include <math.h>
#include <time.h>
#include "debug.h"
#include "navit.h"
#include "callback.h"
#include "item.h"
#include "xmlconfig.h"
#include "projection.h"
#include "map.h"
#include "mapset.h"
#include "main.h"
#include "coord.h"
#include "point.h"
#include "transform.h"
#include "traffic.h"
#include "param.h"
#include "data_window.h"
#include "route.h"
#include "navigation.h"
#include "speech.h"
#include "track.h"
#include "vehicle.h"
#include "layout.h"
#include "log.h"
#include "event.h"
#include "file.h"
#include "profile.h"
#include "command.h"
#include "navit_nls.h"
#include "map.h"
#include "util.h"
#include "messages.h"
#include "vehicleprofile.h"
#include "sunriset.h"
#include "bookmarks.h"
#include "attr.h"
#include "graphics.h"
#ifdef HAVE_API_WIN32_BASE
#include <windows.h>
#include "util.h"
#endif
#ifdef HAVE_API_WIN32_CE
#include "libc.h"
#endif

/* define string for bookmark handling */
#define TEXTFILE_COMMENT_NAVI_STOPPED "# navigation stopped\n"

/**
 * @defgroup navit The navit core instance
 * @brief navit is the object containing most global data structures.
 *
 * Among others:
 * - a set of maps
 * - one or more vehicles
 * - a graphics object for rendering the map
 * - a gui object for displaying the user interface
 * - a route object
 * - a navigation object
 * @{
 */

//! The vehicle used for navigation.
struct navit_vehicle
{
    int follow;
    /*! Limit of the follow counter. See navit_add_vehicle */
    int follow_curr;
    /*! Deprecated : follow counter itself. When it reaches 'update' counts, map is recentered*/
    struct coord coord;
    int dir;
    int speed;
    struct coord last; /*< Position of the last update of this vehicle */
    struct vehicle *vehicle;
    struct attr callback;
    int animate_cursor;
};

struct attr_iter
{
    void *iter;
    union
    {
        GList *list;
        struct mapset_handle *mapset_handle;
    } u;
};

struct object_func navit_func;

Navit::Navit(struct attr *parent, struct attr **attrs) : m_displaylist(m_graphics)
{
    struct pcoord center;
    struct coord co;
    struct coord_geo g;
    enum projection pro = projection_mg;
    int zoom = 256;
    g.lat = 53.13;
    g.lng = 11.70;

    m_func = &navit_func;

    m_attrs = attr_list_dup(attrs);
    m_self.type = attr_navit;
    m_attr_cbl = callback_list_new();

    m_orientation = -1;
    m_tracking_flag = 1;
    m_recentdest_count = 10;
    m_default_layout_name = NULL;

    m_center_timeout = 1;
    m_use_mousewheel = 1;
    m_autozoom_secs = 1;
    m_autozoom_min = 5;
    m_autozoom_active = 0;
    m_autozoom_paused = 0;
    m_zoom_min = 1;
    m_zoom_max = 2097152;
    m_autozoom_max = m_zoom_max;
    m_follow_cursor = 1;
    m_radius = 30;
    m_border = 2;
    m_auto_switch = TRUE;
    m_tunnel_nightlayout = FALSE;
    m_layout_before_tunnel = "";
    m_sunrise_degrees = -5;

    transform_from_geo(pro, &g, &co);
    center.x = co.x;
    center.y = co.y;
    center.pro = pro;
    m_trans = transform_new(&center, zoom, (m_orientation != -1) ? m_orientation : 0);
    m_trans_cursor = transform_new(&center, zoom, (m_orientation != -1) ? m_orientation : 0);

    m_bookmarks = bookmarks_new(&m_self, NULL, m_trans);

    m_prevTs = 0;

    for (; *attrs; attrs++)
    {
        set_attr_do(*attrs, 1);
    }

    m_messages = messagelist_new(attrs);

    dbg(lvl_debug, "return %p", this);
}

void Navit::add_mapset(struct mapset *ms)
{
    m_mapsets = g_list_append(m_mapsets, ms);
}

/**
 * @brief Get the current mapset
 *
 * @param this_ The navit instance
 *
 * @return A pointer to the current mapset
 */
struct mapset *Navit::get_mapset()
{
    if (m_mapsets)
    {
        return (struct mapset *)m_mapsets->data;
    }
    else
    {
        dbg(lvl_error, "No mapsets enabled! Is it on purpose? Navit can't draw a map. Please check your navit.xml");
    }
    return NULL;
}

/**
 * @brief Get the search result map (and create it if it does not exist)
 *
 * @param this_ The navit instance
 *
 * @return A pointer to the map named "search_results" or NULL if there wasa failure
 */
struct map *Navit::get_search_results_map()
{

    struct mapset *ms;
    struct map *map;

    ms = get_mapset();

    if (!ms)
        return NULL;

    map = mapset_get_map_by_name(ms, "search_results");
    if (!map)
    {
        struct attr *attrs[10], attrmap;
        enum attr_type types[] = {attr_position_longitude, attr_position_latitude, attr_label, attr_none};
        int i;

        attrs[0] = g_new0(struct attr, 1);
        attrs[0]->type = attr_type;
        attrs[0]->u.str = "csv";

        attrs[1] = g_new0(struct attr, 1);
        attrs[1]->type = attr_name;
        attrs[1]->u.str = "search_results";

        attrs[2] = g_new0(struct attr, 1);
        attrs[2]->type = attr_charset;
        attrs[2]->u.str = "utf-8";

        attrs[3] = g_new0(struct attr, 1);
        attrs[3]->type = attr_item_type;
        attrs[3]->u.num = type_found_item;

        attrs[4] = g_new0(struct attr, 1);
        attrs[4]->type = attr_attr_types;
        attrs[4]->u.attr_types = types;
        attrs[5] = NULL;

        attrmap.type = attr_map;
        map = attrmap.u.map = map_new(NULL, attrs);
        if (map)
            mapset_add_attr(ms, &attrmap);

        for (i = 0; attrs[i]; i++)
            g_free(attrs[i]);
    }
    return map;
}

/**
 * @brief Populate a map containing one or more search result points
 *
 * These search results will be displayed as an overlay on the top of the geographic map.
 *
 * @warning Each call to this function will replace currently displayed results, it will not add to them
 *
 * @param this_ The navit instance
 * @param search_results A GList storing {@code struct lcoord} elements to display on the result map
 *                       If this argument in NULL, all existing results will be removed from the map
 * @param[in,out] coord_rect An optional rectangular zone that will be extended to contain all result points
 *                           or NULL if no zone needs to be computed
 * @return The number of results actually added to the map
 */
int Navit::populate_search_results_map(GList *search_results, struct coord_rect *r)
{
    struct map *map;
    struct map_rect *mr;
    struct item *item;
    GList *curr_result = search_results;
    int count;
    char *name_label;

    map = get_search_results_map();
    if (!map)
        return 0;

    mr = map_rect_new(map, NULL);

    if (!mr)
        return 0;

    /* Clean the map */
    while ((item = map_rect_get_item(mr)) != NULL)
    {
        item_type_set(item, type_none);
    }

    if (!search_results)
    {
        map_rect_destroy(mr);
        dbg(lvl_warning, "NULL result table - only map clean up is done.");
        return 0;
    }

    /* Populate the map with search results*/
    for (curr_result = search_results, count = 0; curr_result; curr_result = g_list_next(curr_result))
    {
        struct lcoord *point = (struct lcoord *)curr_result->data;
        struct item *it;
        if (point->label == NULL)
            continue;
        dbg(lvl_info, "%s", point->label);
        it = map_rect_create_item(mr, type_found_item);
        if (it)
        {
            struct attr a;
            item_coord_set(it, &(point->c), 1, change_mode_modify);
            a.type = attr_label;
            name_label = g_strdup(point->label);
            square_shape_str(name_label);
            a.u.str = name_label;
            item_attr_set(it, &a, change_mode_modify);
            if (r)
            {
                if (!count++)
                    r->lu = r->rl = point->c;
                else
                    coord_rect_extend(r, &(point->c));
            }
        }
    }
    map_rect_destroy(mr);
    return count;
}

struct tracking *Navit::get_tracking()
{
    return m_tracking;
}

/**
 * @brief	Get the user data directory.
 * @param[in]	 create	- create the directory if it does not exist
 *
 * @return	char * to the data directory string.
 *
 * returns the directory used to store user data files (center.txt,
 * destination.txt, bookmark.txt, ...)
 *
 */
char *Navit::get_user_data_directory(int create)
{
    char *dir;
    dir = getenv("NAVIT_USER_DATADIR");
    if (create && !file_exists(dir))
    {
        dbg(lvl_debug, "creating dir %s", dir);
        if (file_mkdir(dir, 1))
        {
            dbg(lvl_error, "failed creating dir %s", dir);
            return NULL;
        }
    }
    return dir;
}

void Navit::draw_async(int async)
{

    if (m_blocked)
    {
        m_blocked |= 2;
        return;
    }
    transform_setup_source_rect(m_trans);
    m_displaylist.draw_graphics((mapset *)m_mapsets->data, m_trans, m_layout_current, async, NULL, m_graphics_flags | 1);
}

void Navit::draw()
{
    if (m_ready == 3)
        draw_async(0);
}

int Navit::get_ready()
{
    return m_ready;
}

void Navit::draw_displaylist()
{
    if (m_ready == 3)
        m_displaylist.draw(m_trans, m_layout_current, m_graphics_flags | 1);
}

void Navit::map_progress()
{
    struct map *map;
    struct mapset *ms;
    struct mapset_handle *msh;
    struct attr attr;
    struct point p;
    if (m_ready != 3)
        return;
    p.x = 10;
    p.y = 32;

    ms = (struct mapset *)m_mapsets->data;
    msh = mapset_open(ms);
    while (msh && (map = mapset_next(msh, 0)))
    {
        if (map_get_attr(map, attr_progress, &attr, NULL))
        {
            char *str = g_strdup_printf("%s           ", attr.u.str);
            m_graphics.draw_mode(draw_mode_begin);
            m_graphics.draw_text_std(16, str, &p);
            g_free(str);
            p.y += 32;
            m_graphics.draw_mode(draw_mode_end);
        }
    }
    mapset_close(msh);
}

void Navit::redraw_route(struct route *route, struct attr *attr)
{
    int updated;
    if (attr->type != attr_route_status)
        return;
    updated = attr->u.num;
    if (m_ready != 3)
        return;
    if (updated != route_status_path_done_new)
        return;
    if (m_vehicle)
    {
        if (m_vehicle->follow_curr == 1)
            return;
        if (m_vehicle->follow_curr <= m_vehicle->follow)
            m_vehicle->follow_curr = m_vehicle->follow;
    }
    draw();
}

void Navit::handle_resize(int w, int h)
{
    struct map_selection sel;
    int callback = (m_ready == 1);
    m_ready |= 2;
    memset(&sel, 0, sizeof(sel));
    int firstcall = 0;
    struct attr attr;

    /* Fix for #1135: Check if pitch was set while w and h were 0. In this case set pitch
     * again so transformation value is set correctly */
    if (m_w == 0 && m_h == 0)
    {
        firstcall = 1;
    }

    m_w = w;
    m_h = h;

    /* Fix for #1135: Now w and h are set initially, we can set pitch value again
     *
     */
    if (firstcall)
    {
        attr.type = attr_pitch;
        attr.u.num = m_pitch;
        set_attr(&attr); // Set pitch again
    }

    sel.u.p_rect.rl.x = w;
    sel.u.p_rect.rl.y = h;
    transform_set_screen_selection(m_trans, &sel);
    m_graphics.init();
    m_graphics.set_rect(&sel.u.p_rect);
    if (callback)
        callback_list_call_attr_1(m_attr_cbl, attr_graphics_ready, this);
    if (m_ready == 3)
    {
        /* About to resize. Cancel drawing whatever it is */
        m_displaylist.draw_cancel();
        /* draw again even if we did not cancel anything */
        draw_async(1);
    }
}

int Navit::get_width()
{
    return m_w;
}

int Navit::get_height()
{
    return m_h;
}

void Navit::ignore_graphics_events(int ignore)
{
    m_ignore_graphics_events = ignore;
}

int Navit::restrict_to_range(int value, int min, int max)
{
    if (value > max)
    {
        value = max;
    }
    if (value < min)
    {
        value = min;
    }
    return value;
}

void Navit::restrict_map_center_to_world_boundingbox(struct transformation *tr, struct coord *new_center)
{
    new_center->x = restrict_to_range(new_center->x, WORLD_BOUNDINGBOX_MIN_X, WORLD_BOUNDINGBOX_MAX_X);
    new_center->y = restrict_to_range(new_center->y, WORLD_BOUNDINGBOX_MIN_Y, WORLD_BOUNDINGBOX_MAX_Y);
}

/**
 * @brief Change map center position by translating from "old" to "new".
 */
void Navit::update_transformation(struct transformation *tr, struct point *old, struct point *new_)
{
    /* Code for rotation was removed in rev. 5252; see Trac #1078. */
    struct coord coord_old, coord_new;
    struct coord center_new, *center_old;
    if (!transform_reverse(tr, old, &coord_old))
        return;
    if (!transform_reverse(tr, new_, &coord_new))
        return;
    center_old = transform_get_center(tr);
    center_new.x = center_old->x + coord_old.x - coord_new.x;
    center_new.y = center_old->y + coord_old.y - coord_new.y;
    restrict_map_center_to_world_boundingbox(tr, &center_new);
    dbg(lvl_debug, "change center from 0x%x,0x%x to 0x%x,0x%x", center_old->x, center_old->y, center_new.x, center_new.y);
    transform_set_center(tr, &center_new);
}

void Navit::set_timeout()
{
    struct attr follow;
    follow.type = attr_follow;
    follow.u.num = m_center_timeout;
    set_attr(&follow);
}

void Navit::motion_timeout()
{
    int dx, dy;

    if (m_drag_bitmap)
    {
        struct point point;
        point.x = (m_current.x - m_pressed.x);
        point.y = (m_current.y - m_pressed.y);
        if (m_graphics.draw_drag(&point))
        {
            m_graphics.overlay_disable(1);
            m_graphics.draw_mode(draw_mode_end);
            m_motion_timeout = NULL;
            return;
        }
    }
    dx = (m_current.x - m_last.x);
    dy = (m_current.y - m_last.y);
    if (dx || dy)
    {
        struct transformation *tr;
        m_last = m_current;
        m_graphics.overlay_disable(1);
        tr = transform_dup(m_trans);
        update_transformation(tr, &m_pressed, &m_current);
        m_displaylist.draw_cancel();
        m_displaylist.draw(tr, m_layout_current, m_graphics_flags | 512);
        transform_destroy(tr);
    }
    m_motion_timeout = NULL;
    return;
}

static void motion_timeout_callback(void *data)
{
    if (data != nullptr)
    {
        Navit *navitPtr = static_cast<Navit *>(data);
        navitPtr->motion_timeout();
    }
}

void Navit::handle_motion(struct point *p)
{
    int dx, dy;

    dx = (p->x - m_pressed.x);
    dy = (p->y - m_pressed.y);
    if (dx < -8 || dx > 8 || dy < -8 || dy > 8)
    {
        if (m_button_timeout)
        {
            event_remove_timeout(m_button_timeout);
            m_button_timeout = NULL;
        }
        m_current = *p;
        if (!m_motion_timeout_callback)
            m_motion_timeout_callback = callback_new_1(callback_cast(motion_timeout_callback), this);
        if (!m_motion_timeout)
            m_motion_timeout = event_add_timeout(m_drag_bitmap ? 10 : 100, 0, m_motion_timeout_callback);
    }
}

void Navit::scale(long scale, struct point *p, int draw_)
{
    struct coord c1, c2, *center;
    if (scale < m_zoom_min)
        scale = m_zoom_min;
    if (scale > m_zoom_max)
        scale = m_zoom_max;
    if (p)
        transform_reverse(m_trans, p, &c1);
    transform_set_scale(m_trans, scale);
    if (p)
    {
        transform_reverse(m_trans, p, &c2);
        center = transform_center(m_trans);
        center->x += c1.x - c2.x;
        center->y += c1.y - c2.y;
    }
    if (draw_)
        draw();
}

/**
 * @brief Automatically adjusts zoom level
 *
 * This function automatically adjusts the current
 * zoom level according to the current speed.
 *
 * @param this_ The navit struct
 * @param center The "immovable" point - i.e. the vehicles position if we're centering on the vehicle
 * @param speed The vehicles speed in meters per second
 * @param dir The direction into which the vehicle moves
 */
void Navit::autozoom(struct coord *center, int speed)
{
    struct point pc;
    int distance, w, h;
    double new_scale;
    long scale_;

    if (!m_autozoom_active)
    {
        return;
    }

    if (m_autozoom_paused)
    {
        m_autozoom_paused--;
        return;
    }

    distance = speed * m_autozoom_secs;

    transform_get_size(m_trans, &w, &h);
    transform_point(m_trans, transform_get_projection(m_trans), center, &pc);
    scale_ = transform_get_scale(m_trans);

    /* We make sure that the point we want to see is within a certain range
     * around the vehicle. The radius of this circle is the size of the
     * screen. This doesn't necessarily mean the point is visible because of
     * perspective etc. Quite rough, but should be enough. */

    if (w > h)
    {
        new_scale = (double)distance / h * 16;
    }
    else
    {
        new_scale = (double)distance / w * 16;
    }

    if (abs((int)new_scale - (int)scale_) < 2)
    {
        return; // Smoothing
    }
    if (new_scale > m_autozoom_max)
        new_scale = m_autozoom_max;
    if (new_scale < m_autozoom_min)
        new_scale = m_autozoom_min;
    if (new_scale != scale_)
        scale((long)new_scale, &pc, 0);
}

/**
 * Change the current zoom level, zooming closer to the ground
 *
 * @param navit The navit instance
 * @param level The zoom level between 0-18
 * @param p The invariant point (if set to NULL, default to center)
 * @returns nothing
 */

void Navit::zoom_level(int level, struct point *p)
{
    if (level > 18)
    {
        level = 18;
    }
    if (level < 0)
    {
        level = 0;
    }

    long scale_ = 2 << level;

    if (m_autozoom_active)
    {
        m_autozoom_paused = 10;
    }
    scale(scale_, p, 1);
}

/**
 * Change the current zoom level, zooming closer to the ground
 *
 * @param navit The navit instance
 * @param factor The zoom factor, usually 2
 * @param p The invariant point (if set to NULL, default to center)
 * @returns nothing
 */
void Navit::zoom_in(int factor, struct point *p)
{
    long scale_ = transform_get_scale(m_trans) / factor;
    if (m_autozoom_active)
    {
        m_autozoom_paused = 10;
    }
    if (scale_ < 1)
        scale_ = 1;
    scale(scale_, p, 1);
}

/**
 * Change the current zoom level, further to the ground
 *
 * @param navit The navit instance
 * @param factor The zoom factor, usually 2
 * @param p The invariant point (if set to NULL, default to center)
 * @returns nothing
 */
void Navit::zoom_out(int factor, struct point *p)
{
    long scale_ = transform_get_scale(m_trans) * factor;
    if (m_autozoom_active)
    {
        m_autozoom_paused = 10;
    }
    scale(scale_, p, 1);
}

void Navit::zoom_in_cursor(int factor)
{
    struct point p;
    if (m_vehicle && m_vehicle->follow_curr <= 1 && get_cursor_pnt(&p, 0, NULL))
    {
        zoom_in(factor, &p);
        m_vehicle->follow_curr = m_vehicle->follow;
    }
    else
        zoom_in(factor, NULL);
}

void Navit::zoom_out_cursor(int factor)
{
    struct point p;
    if (m_vehicle && m_vehicle->follow_curr <= 1 && get_cursor_pnt(&p, 0, NULL))
    {
        zoom_out(2, &p);
        m_vehicle->follow_curr = m_vehicle->follow;
    }
    else
        zoom_out(2, NULL);
}

void Navit::add_message(const char *message)
{
    message_new(m_messages, message);
}

struct message *Navit::get_messages()
{
    return message_get(m_messages);
}

void Navit::predraw()
{
    GList *l;
    struct navit_vehicle *nv;
    transform_copy(m_trans, m_trans_cursor);
    l = m_vehicles;
    while (l)
    {
        nv = (struct navit_vehicle *)l->data;
        draw_vehicle(nv, NULL);
        l = g_list_next(l);
    }
}

static void navit_resize(void *data, int w, int h)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    if (!navit->m_ignore_graphics_events)
        navit->handle_resize(w, h);
}

static void navit_motion(void *data, struct point *p)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    if (!navit->m_ignore_graphics_events)
        navit->handle_motion(p);
}

static void navit_predraw(void *data)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);

    navit->predraw();
}

int Navit::set_graphics(Graphics &gra)
{
    m_graphics = gra;
    m_resize_callback = callback_new_attr_1(callback_cast(navit_resize), attr_resize, this);
    m_graphics.add_callback(m_resize_callback);
    m_motion_callback = callback_new_attr_1(callback_cast(navit_motion), attr_motion, this);
    m_graphics.add_callback(m_motion_callback);
    m_predraw_callback = callback_new_attr_1(callback_cast(navit_predraw), attr_predraw, this);
    m_graphics.add_callback(m_predraw_callback);
    return 1;
}

struct vehicleprofile *Navit::get_vehicleprofile()
{
    return m_vehicleprofile;
}

GList *Navit::get_vehicleprofiles()
{
    return m_vehicleprofiles;
}

void Navit::projection_set(enum projection pro, int draw_)
{
    struct coord_geo g;
    struct coord *c;

    c = transform_center(m_trans);
    transform_to_geo(transform_get_projection(m_trans), c, &g);
    transform_set_projection(m_trans, pro);
    transform_from_geo(pro, &g, c);
    if (draw_)
        draw();
}

void Navit::mark_navigation_stopped(char *former_destination_file)
{
    FILE *f;
    f = fopen(former_destination_file, "a");
    if (f)
    {
        fprintf(f, "%s", TEXTFILE_COMMENT_NAVI_STOPPED);
        fclose(f);
    }
    else
    {
        dbg(lvl_error, "Error setting mark in destination file %s: %s", former_destination_file, strerror(errno));
    }
}

/**
 * Start or add a given set of coordinates for route computing
 *
 * @param navit The navit instance
 * @param c The coordinate to start routing to
 * @param description A label which allows the user to later identify this destination in the former destinations selection
 * @param async Set to 1 to do route calculation asynchronously
 * @return nothing
 */
void Navit::set_destination(struct pcoord *c, const char *description, int async)
{
    char *destination_file;
    destination_file = bookmarks_get_destination_file(TRUE);
    if (c)
    {
        m_destination = *c;
        m_destination_valid = 1;

        dbg(lvl_debug, "c=(%i,%i)", c->x, c->y);
        bookmarks_append_destinations(m_former_destination, destination_file, c, 1, type_former_destination, description,
                                      m_recentdest_count);
    }
    else
    {
        m_destination_valid = 0;
        bookmarks_append_destinations(m_former_destination, destination_file, NULL, 0, type_former_destination, NULL,
                                      m_recentdest_count);
        mark_navigation_stopped(destination_file);
    }
    g_free(destination_file);

    if (m_route)
    {
        struct attr attr;
        int dstcount;
        struct pcoord *pc;

        get_attr(attr_waypoints_flag, &attr, NULL);
        if (m_waypoints_flag == 0 || route_get_destination_count(m_route) == 0)
        {
            route_set_destination(m_route, c, async);
        }
        else
        {
            route_append_destination(m_route, c, async);
        }

        dstcount = route_get_destination_count(m_route);
        if (dstcount > 0)
        {
            destination_file = bookmarks_get_destination_file(TRUE);
            pc = g_new(struct pcoord, dstcount);
            route_get_destinations(m_route, pc, dstcount);
            bookmarks_append_destinations(m_former_destination, destination_file, pc, dstcount, type_former_itinerary,
                                          description, m_recentdest_count);
            g_free(pc);
            g_free(destination_file);
        }
    }

    callback_list_call_attr_0(m_attr_cbl, attr_destination);

    if (m_route && m_ready == 3 && !(m_flags & 4))
        draw();
}

/**
 * Add destination description to the recent dest file. Doesn't start routing.
 *
 * @param navit The navit instance
 * @param c The coordinate to start routing to
 * @param description A label which allows the user to later identify this destination in the former destinations selection
 * @returns nothing
 */
void Navit::add_destination_description(struct pcoord *c, const char *description)
{
    char *destination_file;
    if (c)
    {
        destination_file = bookmarks_get_destination_file(TRUE);
        bookmarks_append_destinations(m_former_destination, destination_file, c, 1, type_former_destination, description,
                                      m_recentdest_count);
        g_free(destination_file);
    }
}

/**
 * Start the route computing to a given set of coordinates including waypoints
 *
 * @param this_ The navit instance
 * @param c The coordinate to start routing to
 * @param count Number of items in {@code dst}
 * @param description A label which allows the user to later identify this destination in the former destinations selection
 * @param async If routing should be done asynchronously
 * @returns nothing
 */
void Navit::set_destinations(struct pcoord *c, int count, const char *description, int async)
{
    char *destination_file;
    if (c && count)
    {
        m_destination = c[count - 1];
        m_destination_valid = 1;

        destination_file = bookmarks_get_destination_file(TRUE);
        bookmarks_append_destinations(m_former_destination, destination_file, c, count, type_former_itinerary, description,
                                      m_recentdest_count);
        g_free(destination_file);
    }
    else
        m_destination_valid = 0;
    if (m_route)
        route_set_destinations(m_route, c, count, async);

    callback_list_call_attr_0(m_attr_cbl, attr_destination);
    if (m_route && m_ready == 3)
        draw();
}

/**
 * @brief Retrieves destinations from the route
 *
 * Prior to calling this method, you may want to retrieve the number of destinations by calling
 * {@link navit_get_destination_count(NavitHandle)} and assigning a buffer of sufficient capacity.
 *
 * If the return value equals `count`, the buffer was either just large enough or too small to hold the
 * entire list of destinations; there is no way to tell from the result which is the case.
 *
 * If the Navit instance does not have a route, the result is 0.
 *
 * @param this_ The Navit instance
 * @param pc Pointer to an array of projected coordinates which will receive the destination coordinates
 * @param count Capacity of `pc`
 * @return The number of destinations stored in `pc`, never greater than `count`
 */
int Navit::get_destinations(struct pcoord *pc, int count)
{
    if (!m_route)
        return 0;
    return route_get_destinations(m_route, pc, count);
}

/**
 * @brief Get the destinations count for the route
 *
 * @param this The Navit instance
 * @return destination count for the route, or 0 if the Navit instance has no route
 */
int Navit::get_destination_count()
{
    if (!m_route)
        return 0;
    return route_get_destination_count(m_route);
}

char *Navit::get_destination_description(int n)
{
    if (!m_route)
        return NULL;
    return route_get_destination_description(m_route, n);
}

void Navit::remove_nth_waypoint(int n)
{
    if (!m_route)
        return;
    if (route_get_destination_count(m_route) > 1)
    {
        route_remove_nth_waypoint(m_route, n);
    }
    else
    {
        set_destination(NULL, NULL, 0);
    }
}

void Navit::remove_waypoint()
{
    if (!m_route)
        return;
    if (route_get_destination_count(m_route) > 1)
    {
        route_remove_waypoint(m_route);
    }
    else
    {
        set_destination(NULL, NULL, 0);
    }
}

/**
 * @brief Checks if a route is calculated
 *
 * This function checks if a route is calculated.
 *
 * @param this_ The navit struct whose route should be checked.
 * @return True if the route is set, false otherwise.
 */
int Navit::check_route()
{
    if (m_route)
    {
        return route_get_path_set(m_route);
    }

    return 0;
}

int Navit::former_destinations_active()
{
    char *destination_file_name = bookmarks_get_destination_file(FALSE);
    FILE *destination_file;
    int active = 0;
    char lastline[100];
    destination_file = fopen(destination_file_name, "r");
    if (destination_file)
    {
        while (fgets(lastline, sizeof(lastline), destination_file))
            ;
        fclose(destination_file);
        /*forcefully terminate the string, there is no proper fgets error handling.*/
        lastline[sizeof(lastline) - 1] = 0;
        if (strcmp(lastline, TEXTFILE_COMMENT_NAVI_STOPPED))
        {
            active = 1;
        }
    }
    g_free(destination_file_name);
    return active;
}

struct map *read_former_destinations_from_file()
{
    struct attr type, data, no_warn, flags, *attrs[5];
    char *destination_file = bookmarks_get_destination_file(FALSE);
    struct map *m;

    type.type = attr_type;
    type.u.str = "textfile";

    data.type = attr_data;
    data.u.str = destination_file;

    no_warn.type = attr_no_warning_if_map_file_missing;
    no_warn.u.num = 1;

    flags.type = attr_flags;
    flags.u.num = 1;

    attrs[0] = &type;
    attrs[1] = &data;
    attrs[2] = &flags;
    attrs[3] = &no_warn;
    attrs[4] = NULL;

    m = map_new(NULL, attrs);
    g_free(destination_file);
    return m;
}

void Navit::add_former_destinations_from_file()
{
    struct item *item;
    int i, valid = 0, count = 0, maxcount = 1;
    struct coord *c = g_new(struct coord, maxcount);
    struct pcoord *pc;
    struct map_rect *mr;

    m_former_destination = read_former_destinations_from_file();
    if (!m_route || !former_destinations_active() || !m_vehicle)
        return;
    mr = map_rect_new(m_former_destination, NULL);
    while ((item = map_rect_get_item(mr)))
    {
        if (item->type == type_former_itinerary || item->type == type_former_itinerary_part)
        {
            count = item_coord_get(item, c, maxcount);
            while (count == maxcount)
            {
                maxcount *= 2;
                c = (struct coord *)g_realloc(c, sizeof(struct coord) * maxcount);
                count += item_coord_get(item, &c[count], maxcount - count);
            }
            if (count)
                valid = 1;
        }
    }
    map_rect_destroy(mr);
    if (valid && count > 0)
    {
        pc = g_new(struct pcoord, count);
        for (i = 0; i < count; i++)
        {
            pc[i].pro = map_projection(m_former_destination);
            pc[i].x = c[i].x;
            pc[i].y = c[i].y;
        }
        if (count == 1)
            route_set_destination(m_route, &pc[0], 1);
        else
            route_set_destinations(m_route, pc, count, 1);
        m_destination = pc[count - 1];
        m_destination_valid = 1;
        g_free(pc);
    }
    g_free(c);
}

void Navit::textfile_debug_log(const char *fmt, ...)
{
    va_list ap;
    char *str1, *str2;
    va_start(ap, fmt);
    if (m_textfile_debug_log && m_vehicle)
    {
        str1 = g_strdup_vprintf(fmt, ap);
        str2 = g_strdup_printf("0x%x 0x%x%s%s\n", m_vehicle->coord.x, m_vehicle->coord.y, strlen(str1) ? " " : "",
                               str1);
        log_write(m_textfile_debug_log, str2, strlen(str2), (log_flags)0);
        g_free(str2);
        g_free(str1);
    }
    va_end(ap);
}

void Navit::textfile_debug_log_at(struct pcoord *pc, const char *fmt, ...)
{
    va_list ap;
    char *str1, *str2;
    va_start(ap, fmt);
    if (m_textfile_debug_log && m_vehicle)
    {
        str1 = g_strdup_vprintf(fmt, ap);
        str2 = g_strdup_printf("0x%x 0x%x%s%s\n", pc->x, pc->y, strlen(str1) ? " " : "", str1);
        log_write(m_textfile_debug_log, str2, strlen(str2), (log_flags)0);
        g_free(str2);
        g_free(str1);
    }
    va_end(ap);
}

void Navit::say(const char *text)
{
    struct attr attr;
    if (m_speech)
    {
        if (!speech_get_attr(m_speech, attr_active, &attr, NULL))
            attr.u.num = 1;
        dbg(lvl_debug, "this_.speech->active %ld", attr.u.num);
        if (attr.u.num)
            speech_say(m_speech, text);
    }
}

void Navit::speak()
{
    struct navigation *nav = m_navigation;
    struct map *map = NULL;
    struct map_rect *mr = NULL;
    struct item *item;
    struct attr attr;

    if (!speech_get_attr(m_speech, attr_active, &attr, NULL))
        attr.u.num = 1;
    dbg(lvl_debug, "this_.speech->active %ld", attr.u.num);
    if (!attr.u.num)
        return;

    if (nav)
        map = navigation_get_map(nav);
    if (map)
        mr = map_rect_new(map, NULL);
    if (mr)
    {
        while ((item = map_rect_get_item(mr)) && (item->type == type_nav_position || item->type == type_nav_none))
            ;
        if (item && item_attr_get(item, attr_navigation_speech, &attr))
        {
            if (*attr.u.str != '\0')
            {
                speech_say(m_speech, attr.u.str);
                add_message(attr.u.str);
            }
            textfile_debug_log("type=announcement label=\"%s\"", attr.u.str);
        }
        map_rect_destroy(mr);
    }
}

void Navit::window_roadbook_update()
{
    struct navigation *nav = m_navigation;
    struct map *map = NULL;
    struct map_rect *mr = NULL;
    struct item *item;
    struct attr attr;
    struct param_list param[5];
    int secs;

    /* Respect the Imperial attribute as we enlighten the user. */
    int imperial = FALSE; /* default to using metric measures. */
    if (get_attr(attr_imperial, &attr, NULL))
        imperial = attr.u.num;

    dbg(lvl_debug, "enter");
    datawindow_mode(m_roadbook_window, 1);
    if (nav)
        map = navigation_get_map(nav);
    if (map)
        mr = map_rect_new(map, NULL);
    dbg(lvl_debug, "nav=%p map=%p mr=%p", nav, map, mr);
    if (mr)
    {
        dbg(lvl_debug, "while loop");
        while ((item = map_rect_get_item(mr)))
        {
            dbg(lvl_debug, "item=%p", item);
            attr.u.str = NULL;
            if (item->type != type_nav_position)
            {
                item_attr_get(item, attr_navigation_long, &attr);
                if (attr.u.str == NULL)
                {
                    continue;
                }
                dbg(lvl_info, "Command='%s'", attr.u.str);
                param[0].value = g_strdup(attr.u.str);
            }
            else
                param[0].value = _("Position");
            param[0].name = _("Command");

            /* Distance to the next maneuver. */
            item_attr_get(item, attr_length, &attr);
            dbg(lvl_info, "Length=%ld in meters", attr.u.num);
            param[1].name = _("Length");

            if (attr.u.num >= 2000)
            {
                param[1].value = g_strdup_printf("%5.1f %s",
                                                 imperial == TRUE ? (float)attr.u.num * METERS_TO_MILES : (float)attr.u.num / 1000,
                                                 imperial == TRUE ? _("mi") : _("km"));
            }
            else
            {
                param[1].value = g_strdup_printf("%7.0f %s",
                                                 imperial == TRUE ? (attr.u.num * FEET_PER_METER) : attr.u.num,
                                                 imperial == TRUE ? _("feet") : _("m"));
            }

            /* Time to next maneuver. */
            item_attr_get(item, attr_time, &attr);
            dbg(lvl_info, "Time=%ld", attr.u.num);
            secs = attr.u.num / 10;
            param[2].name = _("Time");
            if (secs >= 3600)
            {
                param[2].value = g_strdup_printf("%d:%02d:%02d", secs / 60, (secs / 60) % 60, secs % 60);
            }
            else
            {
                param[2].value = g_strdup_printf("%d:%02d", secs / 60, secs % 60);
            }

            /* Distance from next maneuver to destination. */
            item_attr_get(item, attr_destination_length, &attr);
            dbg(lvl_info, "Destlength=%ld in meters.", attr.u.num);
            param[3].name = _("Destination Length");
            if (attr.u.num >= 2000)
            {
                param[3].value = g_strdup_printf("%5.1f %s",
                                                 imperial == TRUE ? (float)attr.u.num * METERS_TO_MILES : (float)attr.u.num / 1000,
                                                 imperial == TRUE ? _("mi") : _("km"));
            }
            else
            {
                param[3].value = g_strdup_printf("%7.0f %s",
                                                 imperial == TRUE ? (attr.u.num * FEET_PER_METER) : attr.u.num,
                                                 imperial == TRUE ? _("feet") : _("m"));
            }

            /* Time from next maneuver to destination. */
            item_attr_get(item, attr_destination_time, &attr);
            dbg(lvl_info, "Desttime=%ld", attr.u.num);
            secs = attr.u.num / 10;
            param[4].name = _("Destination Time");
            if (secs >= 3600)
            {
                param[4].value = g_strdup_printf("%d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
            }
            else
            {
                param[4].value = g_strdup_printf("%d:%02d", secs / 60, secs % 60);
            }
            datawindow_add(m_roadbook_window, param, 5);
        }
        map_rect_destroy(mr);
    }
    datawindow_mode(m_roadbook_window, 0);
}

void Navit::window_roadbook_destroy()
{
    dbg(lvl_debug, "enter");
    navigation_unregister_callback(m_navigation, attr_navigation_long, m_roadbook_callback);
    callback_destroy(m_roadbook_callback);
    m_roadbook_window = NULL;
    m_roadbook_callback = NULL;
}

static void navit_window_roadbook_update(void *data)
{
    if (data != nullptr)
    {
        Navit *navitPtr = static_cast<Navit *>(data);
        navitPtr->motion_timeout();
    }
}

void Navit::window_roadbook_new()
{
    if (m_roadbook_callback || m_roadbook_window)
    {
        return;
    }

    m_roadbook_callback = callback_new_1(callback_cast(navit_window_roadbook_update), this);
    navigation_register_callback(m_navigation, attr_navigation_long, m_roadbook_callback);
    window_roadbook_update();
}

static void navit_map_progress(void *data)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);

    navit->map_progress();
}

static void navit_redraw_route(void *data, struct route *route, struct attr *attr)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    if (!navit->m_ignore_graphics_events)
        navit->redraw_route(route, attr);
}

static void navit_speak_callback(void *data)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    if (!navit->m_ignore_graphics_events)
        navit->speak();
}

int Navit::init()
{
    struct mapset *ms;
    struct map *map;
    int callback;
    char *center_file;
    struct attr_iter *iter;
    struct attr *attr;
    struct traffic *traffic;

    m_w = 0;
    m_h = 0;

    dbg(lvl_info, "enter graphics %p", &m_graphics);

    // if (!m_gra && !(m_flags & 1)) {
    //     dbg(lvl_error,"FATAL: No graphics subsystem available.");
    //     exit(1);
    // }

    if (m_speech && m_navigation)
    {
        struct attr speech;
        speech.type = attr_speech;
        speech.u.speech = m_speech;
        navigation_set_attr(m_navigation, &speech);
    }
    dbg(lvl_info, "Initializing graphics");
    dbg(lvl_info, "Setting Vehicle");
    set_vehicle(m_vehicle);
    dbg(lvl_info, "Adding dynamic maps to mapset %p", m_mapsets);
    if (m_mapsets)
    {
        struct mapset_handle *msh;
        ms = (struct mapset *)m_mapsets->data;
        msh = mapset_open(ms);
        while (msh && (map = mapset_next(msh, 0)))
        {
            // pass new callback instance for each map in the mapset to make map callback list destruction work correctly
            struct callback *pcb = callback_new_attr_1(callback_cast(navit_map_progress), attr_progress, this);
            map_add_callback(map, pcb);
        }
        mapset_close(msh);

        if (m_route)
        {
            if ((map = route_get_map(m_route)))
            {
                struct attr map_a;
                map_a.type = attr_map;
                map_a.u.map = map;
                mapset_add_attr(ms, &map_a);
            }
            if ((map = route_get_graph_map(m_route)))
            {
                struct attr map_a, active;
                map_a.type = attr_map;
                map_a.u.map = map;
                active.type = attr_active;
                active.u.num = 0;
                mapset_add_attr(ms, &map_a);
                map_set_attr(map, &active);
            }
            route_set_mapset(m_route, ms);
            route_set_projection(m_route, transform_get_projection(m_trans));
        }
        if (m_tracking)
        {
            tracking_set_mapset(m_tracking, ms);
            if (m_route)
                tracking_set_route(m_tracking, m_route);
        }

        attr = g_new0(struct attr, 1);
        iter = attr_iter_new();
        map = NULL;
        while (get_attr(attr_traffic, attr, iter))
        {
            traffic = (struct traffic *)attr->u.navit_object;
            traffic_set_mapset(traffic, ms);
            if (m_route)
                traffic_set_route(traffic, m_route);
            /* add the first map found */
            if (!map && (map = traffic_get_map(traffic)))
            {
                struct attr map_a;
                map_a.type = attr_map;
                map_a.u.map = map;
                mapset_add_attr(ms, &map_a);
            }
        }
        attr_iter_destroy(iter);
        g_free(attr);

        if (m_navigation)
        {
            if ((map = navigation_get_map(m_navigation)))
            {
                struct attr map_a, active;
                map_a.type = attr_map;
                map_a.u.map = map;
                active.type = attr_active;
                active.u.num = 0;
                mapset_add_attr(ms, &map_a);
                map_set_attr(map, &active);
            }
        }
        if (m_tracking)
        {
            if ((map = tracking_get_map(m_tracking)))
            {
                struct attr map_a, active;
                map_a.type = attr_map;
                map_a.u.map = map;
                active.type = attr_active;
                active.u.num = 0;
                mapset_add_attr(ms, &map_a);
                map_set_attr(map, &active);
            }
        }
        add_former_destinations_from_file();
    }
    else
    {
        dbg(lvl_error, "FATAL: No mapset available. Please add a (valid) mapset to your configuration.");
        exit(1);
    }
    if (m_route)
    {
        struct attr callback;
        m_route_cb = callback_new_attr_1(callback_cast(navit_redraw_route), attr_route_status, this);
        callback.type = attr_callback;
        callback.u.callback = m_route_cb;
        route_add_attr(m_route, &callback);
    }
    if (m_navigation)
    {
        if (m_speech)
        {
            m_nav_speech_cb = callback_new_1(callback_cast(navit_speak_callback), this);
            navigation_register_callback(m_navigation, attr_navigation_speech, m_nav_speech_cb);
        }
        if (m_route)
            navigation_set_route(m_navigation, m_route);
    }
    dbg(lvl_info, "Setting Center");
    center_file = bookmarks_get_center_file(FALSE);
    bookmarks_set_center_from_file(m_bookmarks, center_file);
    g_free(center_file);

    messagelist_init(m_messages);

    set_cursors();

    callback_list_call_attr_1(m_attr_cbl, attr_navit, this);
    callback = (m_ready == 2);
    m_ready |= 1;
    dbg(lvl_info, "ready=%d", m_ready);
    if (m_ready == 3)
        draw_async(1);
    if (callback)
        callback_list_call_attr_1(m_attr_cbl, attr_graphics_ready, this);
    return 0;
}

void Navit::zoom_to_rect(struct coord_rect *r)
{
    struct coord c;
    int w, h, scale = 16;

    c.x = (r->rl.x + r->lu.x) / 2;
    c.y = (r->rl.y + r->lu.y) / 2;
    transform_set_center(m_trans, &c);
    transform_get_size(m_trans, &w, &h);
    dbg(lvl_debug, "center 0x%x,0x%x w %d h %d", c.x, c.y, w, h);
    dbg(lvl_debug, "%x,%x-%x,%x", r->lu.x, r->lu.y, r->rl.x, r->rl.y);
    while (scale < 1 << 20)
    {
        struct point p1, p2;
        transform_set_scale(m_trans, scale);
        transform_setup_source_rect(m_trans);
        transform_point(m_trans, transform_get_projection(m_trans), &r->lu, &p1);
        transform_point(m_trans, transform_get_projection(m_trans), &r->rl, &p2);
        dbg(lvl_debug, "%d,%d-%d,%d", p1.x, p1.y, p2.x, p2.y);
        if (p1.x < 0 || p2.x < 0 || p1.x > w || p2.x > w ||
            p1.y < 0 || p2.y < 0 || p1.y > h || p2.y > h)
            scale *= 2;
        else
            break;
    }
    dbg(lvl_debug, "scale=%d (0x%x) of %d (0x%x)", scale, scale, 1 << 20, 1 << 20);
    if (m_ready == 3)
        draw_async(0);
}

void Navit::zoom_to_route(int orientation)
{
    struct map *map;
    struct map_rect *mr = NULL;
    struct item *item;
    struct coord c;
    struct coord_rect r;
    int count = 0;
    if (!m_route)
        return;
    dbg(lvl_debug, "enter");
    map = route_get_map(m_route);
    dbg(lvl_debug, "map=%p", map);
    if (map)
        mr = map_rect_new(map, NULL);
    dbg(lvl_debug, "mr=%p", mr);
    if (mr)
    {
        while ((item = map_rect_get_item(mr)))
        {
            dbg(lvl_debug, "item=%s", item_to_name(item->type));
            while (item_coord_get(item, &c, 1))
            {
                dbg(lvl_debug, "coord");
                if (!count)
                    r.lu = r.rl = c;
                else
                    coord_rect_extend(&r, &c);
                count++;
            }
        }
        map_rect_destroy(mr);
    }
    if (!count)
        return;
    if (orientation != -1)
        transform_set_yaw(m_trans, orientation);
    zoom_to_rect(&r);
}

/**
 * Change the current zoom level
 *
 * @param navit The navit instance
 * @param center The point where to center the map, including its projection
 * @returns nothing
 */
void Navit::set_center(struct pcoord *center, int set_timeout_)
{
    struct coord *c = transform_center(m_trans);
    struct coord c1, c2;
    enum projection pro = transform_get_projection(m_trans);
    if (pro != center->pro)
    {
        c1.x = center->x;
        c1.y = center->y;
        transform_from_to(&c1, center->pro, &c2, pro);
    }
    else
    {
        c2.x = center->x;
        c2.y = center->y;
    }
    *c = c2;
    if (set_timeout_)
        set_timeout();
    if (m_ready == 3)
        draw();
}

void Navit::set_center_coord_screen(struct coord *c, struct point *p, int set_timeout_)
{
    int width, height;
    struct point po;
    transform_set_center(m_trans, c);
    transform_get_size(m_trans, &width, &height);
    po.x = width / 2;
    po.y = height / 2;
    update_transformation(m_trans, &po, p);
    if (set_timeout_)
        set_timeout();
}

/**
 * Links all vehicles to a cursor depending on the current profile.
 *
 * @param this_ A navit instance
 * @author Ralph Sennhauser (10/2009)
 */
void Navit::set_cursors()
{
    struct attr name;
    struct navit_vehicle *nv;
    struct cursor *c;
    GList *v;

    v = g_list_first(m_vehicles); // GList of navit_vehicles
    while (v)
    {
        nv = (struct navit_vehicle *)v->data;
        if (vehicle_get_attr(nv->vehicle, attr_cursorname, &name, NULL))
        {
            if (!strcmp(name.u.str, "none"))
                c = NULL;
            else
                c = layout_get_cursor(m_layout_current, name.u.str);
        }
        else
            c = layout_get_cursor(m_layout_current, "default");
        vehicle_set_cursor(nv->vehicle, c, 0);
        v = g_list_next(v);
    }
    return;
}

/**
 * @brief Calculates the position of the cursor on the screen.
 *
 * This method considers padding if supported by the graphics plugin. In that case, the inner rectangle
 * (i.e. screen size minus padding) will be used to center the cursor and to determine cursor offset (as
 * specified in `m_radius`).
 *
 * @param this_ The navit object
 * @param p Receives the screen coordinates for the cursor
 * @param keep_orientation Whether to maintain the current map orientation. If false, the map will be
 * rotated so that the bearing of the vehicle is up.
 * @param dir Receives the new map orientation as requested by `screen_orientation` (can be `NULL`)
 *
 * @return Always 1
 */
int Navit::get_cursor_pnt(struct point *p, int keep_orientation, int *dir)
{
    int width, height;
    struct navit_vehicle *nv = m_vehicle;
    struct padding *padding = NULL;

    float offset = m_radius; // Cursor offset from the center of the screen (percent).
#if 0                        /* Better improve track.c to get that issue resolved or make it configurable with being off the default, the jumping back to the center is a bit annoying */
    float min_offset = 0.;      // Percent offset at min_offset_speed.
    float max_offset = 30.;     // Percent offset at max_offset_speed.
    int min_offset_speed = 2;   // Speed in km/h
    int max_offset_speed = 50;  // Speed in km/h
    // Calculate cursor offset from the center of the screen, upon speed.
    if (nv->speed <= min_offset_speed) {
        offset = min_offset;
    } else if (nv->speed > max_offset_speed) {
        offset = max_offset;
    } else {
        offset = (max_offset - min_offset) / (max_offset_speed - min_offset_speed) * (nv->speed - min_offset_speed);
    }
#endif

    padding = (struct padding *)m_graphics.get_data("padding");

    transform_get_size(m_trans, &width, &height);
    dbg(lvl_debug, "width=%d height=%d", width, height);

    if (padding)
    {
        width -= (padding->left + padding->right);
        height -= (padding->top + padding->bottom);
        dbg(lvl_debug, "corrected for padding: width=%d height=%d", width, height);
    }

    if (m_orientation == -1 || keep_orientation)
    {
        p->x = 50 * width / 100;
        p->y = (50 + offset) * height / 100;
        if (dir)
            *dir = keep_orientation ? m_orientation : nv->dir;
    }
    else
    {
        int mdir;
        if (m_tracking && m_tracking_flag)
        {
            mdir = tracking_get_angle(m_tracking) - m_orientation;
        }
        else
        {
            mdir = nv->dir - m_orientation;
        }

        p->x = (50 - offset * sin(M_PI * mdir / 180.)) * width / 100;
        p->y = (50 + offset * cos(M_PI * mdir / 180.)) * height / 100;
        if (dir)
            *dir = m_orientation;
    }

    if (padding)
    {
        p->x += padding->left;
        p->y += padding->top;
    }

    dbg(lvl_debug, "x=%d y=%d, offset=%f", p->x, p->y, offset);

    return 1;
}

/**
 * @brief Recalculates the map view so that the vehicle cursor is visible
 *
 * This function recalculates the parameters which control the visible map area, zoom and orientation. The
 * caller is responsible for redrawing the map after the function returns.
 *
 * If the vehicle supplies a {@code position_valid} attribute and it is {@code attr_position_valid_invalid},
 * the map position is not changed.
 *
 * @param this_ The navit object
 * @param autozoom Whether to set zoom based on current speed. If false, current zoom will be maintained.
 * @param keep_orientation Whether to maintain the current map orientation. If false, the map will be rotated
 * so that the bearing of the vehicle is up.
 */
void Navit::set_center_cursor(int autozoom_, int keep_orientation)
{
    int dir;
    struct point pn;
    struct navit_vehicle *nv = m_vehicle;
    struct attr attr;
    if (!nv || !nv->vehicle)
    {
        return;
    }
    if (vehicle_get_attr(nv->vehicle, attr_position_valid, &attr, NULL) && (attr.u.num == attr_position_valid_invalid))
        return;
    get_cursor_pnt(&pn, keep_orientation, &dir);
    transform_set_yaw(m_trans, dir);
    set_center_coord_screen(&nv->coord, &pn, 0);
    if (autozoom_)
        autozoom(&nv->coord, nv->speed);
}

/**
 * @brief Drags (moves) the map
 *
 * Drags (moves) the map from origin point to the destination point
 *
 * @param navit The navit instance
 * @param origin The point point where the drag starts
 * @param destination The point where to map should be dragged to
 * @returns nothing
 */

void Navit::drag_map(struct point *origin, struct point *destination)
{
    update_transformation(m_trans, origin, destination);
    m_graphics.draw_drag(NULL);
    transform_copy(m_trans, m_trans_cursor);
    m_graphics.overlay_disable(0);
    set_timeout();
    draw();
}

/**
 * @brief Recenters the map so that the vehicle cursor is visible
 *
 * This function first calls {@code navit_set_center_cursor()} to recalculate the map display, then
 * triggers a redraw of the map.
 *
 *@param this_ The navit object
 */
void Navit::set_center_cursor_draw()
{
    set_center_cursor(1, 0);
    if (m_ready == 3)
        draw_async(1);
}

void Navit::set_center_screen(struct point *p, int set_timeout)
{
    struct coord c;
    struct pcoord pc;
    transform_reverse(m_trans, p, &c);
    pc.x = c.x;
    pc.y = c.y;
    pc.pro = transform_get_projection(m_trans);
    set_center(&pc, set_timeout);
}

int Navit::set_attr_do(struct attr *attr, int init)
{
    int dir = 0, orient_old = 0, attr_updated = 0;
    struct coord co;
    long zoom;
    GList *l;
    struct navit_vehicle *nv;
    struct layout *lay;
    struct attr active;
    active.type = attr_active;
    active.u.num = 0;

    dbg(lvl_debug, "enter, this_=%p, attr=%p (%s), init=%d", this, attr, attr_to_name(attr->type), init);

    switch (attr->type)
    {
    case attr_autozoom:
        attr_updated = (m_autozoom_secs != attr->u.num);
        m_autozoom_secs = attr->u.num;
        break;
    case attr_autozoom_active:
        attr_updated = (m_autozoom_active != attr->u.num);
        m_autozoom_active = attr->u.num;
        break;
    case attr_center:
        transform_from_geo(transform_get_projection(m_trans), attr->u.coord_geo, &co);
        dbg(lvl_debug, "0x%x,0x%x", co.x, co.y);
        transform_set_center(m_trans, &co);
        break;
    case attr_drag_bitmap:
        attr_updated = (m_drag_bitmap != !!attr->u.num);
        m_drag_bitmap = !!attr->u.num;
        break;
    case attr_flags:
        attr_updated = (m_flags != attr->u.num);
        m_flags = attr->u.num;
        break;
    case attr_flags_graphics:
        attr_updated = (m_graphics_flags != attr->u.num);
        m_graphics_flags = attr->u.num;
        break;
    case attr_follow:
        if (!m_vehicle)
            return 0;
        attr_updated = (m_vehicle->follow_curr != attr->u.num);
        m_vehicle->follow_curr = attr->u.num;
        break;
    case attr_default_layout:
        if (!attr->u.str)
            return 0;
        if (m_default_layout_name)         /* There is already a default layout, ignore this new value */
            g_free(m_default_layout_name); /* Drop the previous layout name, use the this one instead */
        m_default_layout_name = g_strdup(attr->u.str);
        attr_updated = 1;
        break;
    case attr_layout:
        if (!attr->u.layout)
            return 0;
        dbg(lvl_debug, "setting attr_layout to %s", attr->u.layout->name);
        if (m_layout_current != attr->u.layout)
        {
            update_current_layout(attr->u.layout);
            m_graphics.font_destroy_all();
            set_cursors();
            if (m_ready == 3)
                draw();
            attr_updated = 1;
        }
        break;
    case attr_layout_name:
        if (!attr->u.str)
            return 0;
        dbg(lvl_debug, "setting attr_layout_name to %s", attr->u.str);
        l = m_layouts;
        while (l)
        {
            lay = (struct layout *)l->data;
            if (!strcmp(lay->name, attr->u.str))
            {
                struct attr attr;
                attr.type = attr_layout;
                attr.u.layout = lay;
                return set_attr_do(&attr, init);
            }
            l = g_list_next(l);
        }
        return 0;
    case attr_map_border:
        if (m_border != attr->u.num)
        {
            m_border = attr->u.num;
            attr_updated = 1;
        }
        break;
    case attr_orientation:
        orient_old = m_orientation;
        m_orientation = attr->u.num;
        if (!init)
        {
            if (m_orientation != -1)
            {
                dir = m_orientation;
            }
            else
            {
                if (m_vehicle)
                {
                    dir = m_vehicle->dir;
                }
            }
            transform_set_yaw(m_trans, dir);
            if (orient_old != m_orientation)
            {
#if 0
                if (m_ready == 3)
                    draw();
#endif
                attr_updated = 1;
            }
            if (attr_updated && m_ready == 3)
                draw();
        }
        break;
    case attr_pitch:
        attr_updated = (m_pitch != attr->u.num);
        m_pitch = attr->u.num;
        transform_set_pitch(m_trans, round(m_pitch * sqrt(240 * 320) / sqrt(m_w * m_h))); // Pitch corrected for window resolution
        if (!init && attr_updated && m_ready == 3)
            draw();
        break;
    case attr_projection:
        if (m_trans && transform_get_projection(m_trans) != attr->u.projection)
        {
            projection_set(attr->u.projection, !init);
            attr_updated = 1;
        }
        break;
    case attr_radius:
        attr_updated = (m_radius != attr->u.num);
        m_radius = attr->u.num;
        break;
    case attr_recent_dest:
        attr_updated = (m_recentdest_count != attr->u.num);
        m_recentdest_count = attr->u.num;
        break;
    case attr_speech:
        if (m_speech && m_speech != attr->u.speech)
        {
            attr_updated = 1;
            m_speech = attr->u.speech;
        }
        break;
    case attr_timeout:
        attr_updated = (m_center_timeout != attr->u.num);
        m_center_timeout = attr->u.num;
        break;
    case attr_tracking:
        attr_updated = (m_tracking_flag != !!attr->u.num);
        m_tracking_flag = !!attr->u.num;
        break;
    case attr_transformation:
        m_trans = attr->u.transformation;
        break;
    case attr_use_mousewheel:
        attr_updated = (m_use_mousewheel != !!attr->u.num);
        m_use_mousewheel = !!attr->u.num;
        break;
    case attr_vehicle:
        if (!attr->u.vehicle)
        {
            if (m_vehicle)
            {
                vehicle_set_attr(m_vehicle->vehicle, &active);
                set_vehicle(NULL);
                attr_updated = 1;
            }
            break;
        }
        l = m_vehicles;
        while (l)
        {
            nv = (struct navit_vehicle *)l->data;
            if (nv->vehicle == attr->u.vehicle)
            {
                if (!m_vehicle || m_vehicle->vehicle != attr->u.vehicle)
                {
                    if (m_vehicle)
                        vehicle_set_attr(m_vehicle->vehicle, &active);
                    active.u.num = 1;
                    vehicle_set_attr(nv->vehicle, &active);
                    attr_updated = 1;
                }
                set_vehicle(nv);
            }
            l = g_list_next(l);
        }
        break;
    case attr_vehicleprofile:
        attr_updated = set_vehicleprofile(attr->u.vehicleprofile);
        break;
    case attr_zoom:
        zoom = transform_get_scale(m_trans);
        attr_updated = (zoom != attr->u.num);
        transform_set_scale(m_trans, attr->u.num);
        if (attr_updated && !init)
            draw();
        break;
    case attr_zoom_min:
        attr_updated = (attr->u.num != m_zoom_min);
        m_zoom_min = attr->u.num;
        break;
    case attr_zoom_max:
        attr_updated = (attr->u.num != m_zoom_max);
        m_zoom_max = attr->u.num;
        break;
    case attr_message:
        add_message(attr->u.str);
        break;
    case attr_follow_cursor:
        attr_updated = (m_follow_cursor != !!attr->u.num);
        m_follow_cursor = !!attr->u.num;
        break;
    case attr_imperial:
        attr_updated = (m_imperial != attr->u.num);
        m_imperial = attr->u.num;
        break;
    case attr_waypoints_flag:
        attr_updated = (m_waypoints_flag != !!attr->u.num);
        m_waypoints_flag = !!attr->u.num;
        break;
    case attr_tunnel_nightlayout:
        attr_updated = (m_tunnel_nightlayout != !!attr->u.num);
        m_tunnel_nightlayout = !!attr->u.num;
        break;
    case attr_layout_daynightauto:
        attr_updated = (m_auto_switch != !!attr->u.num);
        m_auto_switch = !!attr->u.num;
        break;
    case attr_sunrise_degrees:
        attr_updated = (m_sunrise_degrees != attr->u.num);
        m_sunrise_degrees = attr->u.num;
        break;
    default:
        dbg(lvl_debug, "calling generic setter method for attribute type %s", attr_to_name(attr->type)) return navit_object_set_attr((struct navit_object *)this, attr);
    }
    if (attr_updated && !init)
    {
        callback_list_call_attr_2(m_attr_cbl, attr->type, this, attr);
        // if (attr->type == attr_osd_configuration)
        //     graphics_draw_mode(m_gra, draw_mode_end);
    }
    return 1;
}

int Navit::set_attr(struct attr *attr)
{
    return set_attr_do(attr, 0);
}

int Navit::get_attr(enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
    struct message *msg;
    struct coord *c;
    int len, offset;
    int ret = 1;

    switch (type)
    {
    case attr_message:
        msg = get_messages();

        if (!msg)
        {
            return 0;
        }

        len = 0;
        while (msg)
        {
            len += strlen(msg->text) + 1;
            msg = msg->next;
        }
        attr->u.str = (char *)g_malloc(len + 1);

        msg = get_messages();
        offset = 0;
        while (msg)
        {
            g_stpcpy((attr->u.str + offset), msg->text);
            offset += strlen(msg->text);
            attr->u.str[offset] = '\n';
            offset++;

            msg = msg->next;
        }

        attr->u.str[len] = '\0';
        break;
    case attr_imperial:
        attr->u.num = m_imperial;
        break;
    case attr_bookmark_map:
        attr->u.map = bookmarks_get_map(m_bookmarks);
        break;
    case attr_bookmarks:
        attr->u.bookmarks = m_bookmarks;
        break;
    case attr_callback_list:
        attr->u.callback_list = m_attr_cbl;
        break;
    case attr_center:
        c = transform_get_center(m_trans);
        transform_to_geo(transform_get_projection(m_trans), c, &m_center);
        attr->u.coord_geo = &m_center;
        break;
    case attr_destination:
        if (!m_destination_valid)
            return 0;
        attr->u.pcoord = &m_destination;
        break;
    case attr_displaylist:
        attr->u.displaylist = &m_displaylist;
        return (attr->u.displaylist != NULL);
    case attr_follow:
        if (!m_vehicle)
            return 0;
        attr->u.num = m_vehicle->follow_curr;
        break;
    case attr_former_destination_map:
        attr->u.map = m_former_destination;
        break;
    case attr_graphics:
        attr->u.graphics = &m_graphics;
        ret = (attr->u.graphics != NULL);
        break;
    case attr_gui:
        break;
    case attr_layer:
        ret = attr_generic_get_attr(m_attrs, NULL, type, attr, iter ? (struct attr_iter *)&iter->iter : NULL);
        break;
    case attr_layout:
        if (iter)
        {
            if (iter->u.list)
            {
                iter->u.list = g_list_next(iter->u.list);
            }
            else
            {
                iter->u.list = m_layouts;
            }
            if (!iter->u.list)
                return 0;
            attr->u.layout = (struct layout *)iter->u.list->data;
        }
        else
        {
            attr->u.layout = m_layout_current;
        }
        break;
    case attr_map:
        if (iter && m_mapsets)
        {
            if (!iter->u.mapset_handle)
            {
                iter->u.mapset_handle = mapset_open((struct mapset *)m_mapsets->data);
            }
            attr->u.map = mapset_next(iter->u.mapset_handle, 0);
            if (!attr->u.map)
            {
                mapset_close(iter->u.mapset_handle);
                return 0;
            }
        }
        else
        {
            return 0;
        }
        break;
    case attr_mapset:
        attr->u.mapset = (struct mapset *)m_mapsets->data;
        ret = (attr->u.mapset != NULL);
        break;
    case attr_navigation:
        attr->u.navigation = m_navigation;
        break;
    case attr_orientation:
        attr->u.num = m_orientation;
        break;
    case attr_pitch:
        attr->u.num = round(transform_get_pitch(m_trans) * sqrt(m_w * m_h) / sqrt(240 * 320)); // Pitch corrected for window resolution
        break;
    case attr_projection:
        if (m_trans)
        {
            attr->u.num = transform_get_projection(m_trans);
        }
        else
        {
            return 0;
        }
        break;
    case attr_route:
        attr->u.route = m_route;
        break;
    case attr_speech:
        if (m_speech)
        {
            attr->u.speech = m_speech;
        }
        else
        {
            return 0;
        }
        break;
    case attr_timeout:
        attr->u.num = m_center_timeout;
        break;
    case attr_tracking:
        attr->u.num = m_tracking_flag;
        break;
    case attr_trackingo:
        attr->u.tracking = m_tracking;
        break;
    case attr_transformation:
        attr->u.transformation = m_trans;
        break;
    case attr_vehicle:
        if (iter)
        {
            if (iter->u.list)
            {
                iter->u.list = g_list_next(iter->u.list);
            }
            else
            {
                iter->u.list = m_vehicles;
            }
            if (!iter->u.list)
                return 0;
            attr->u.vehicle = ((struct navit_vehicle *)iter->u.list->data)->vehicle;
        }
        else
        {
            if (m_vehicle)
            {
                attr->u.vehicle = m_vehicle->vehicle;
            }
            else
            {
                return 0;
            }
        }
        break;
    case attr_vehicleprofile:
        if (iter)
        {
            if (iter->u.list)
            {
                iter->u.list = g_list_next(iter->u.list);
            }
            else
            {
                iter->u.list = m_vehicleprofiles;
            }
            if (!iter->u.list)
                return 0;
            attr->u.vehicleprofile = (struct vehicleprofile *)iter->u.list->data;
        }
        else
        {
            attr->u.vehicleprofile = m_vehicleprofile;
        }
        break;
    case attr_zoom:
        attr->u.num = transform_get_scale(m_trans);
        break;
    case attr_autozoom_active:
        attr->u.num = m_autozoom_active;
        break;
    case attr_follow_cursor:
        attr->u.num = m_follow_cursor;
        break;
    case attr_waypoints_flag:
        attr->u.num = m_waypoints_flag;
        break;
    case attr_tunnel_nightlayout:
        attr->u.num = m_tunnel_nightlayout;
        break;
    case attr_layout_daynightauto:
        attr->u.num = m_auto_switch;
        break;
    case attr_sunrise_degrees:
        attr->u.num = m_sunrise_degrees;
        break;
    default:
        dbg(lvl_debug, "calling generic getter method for attribute type %s", attr_to_name(type)) return navit_object_get_attr((struct navit_object *)this, type, attr, iter);
    }
    attr->type = type;
    return ret;
}

/**
 * @brief Select the default layout by name
 *
 * @param this_ The navit instance
 * @param name The new default layout's name
 *
 * @return The first layout match (if any), or NULL if there was no match
 */
struct layout *Navit::get_layout_by_name(const char *layout_name)
{
    struct attr_iter *iter;
    struct attr layout_attr;
    struct layout *result = NULL;

    if (!layout_name)
        return NULL;
    iter = attr_iter_new();
    while (get_attr(attr_layout, &layout_attr, iter))
    {
        if (strcmp(layout_attr.u.layout->name, layout_name) == 0)
        {
            result = layout_attr.u.layout;
        }
    }
    attr_iter_destroy(iter);
    return result;
}

/**
 * @brief Set the current layout
 *
 * @param this_ The navit instance
 * @param layout The layout to set as default (if NULL, we will set the current layout according to the default name stored in m_default_layout_name
 *
 * @note If argument @p layout is NULL and the default layout name in the config file does not exist or has not been provided in the config file, the default layout is unchanged
 */
void Navit::update_current_layout(struct layout *layout)
{
    struct layout *default_layout = NULL;

    if (layout)
    {
        m_layout_current = layout;
    }
    else
    {
        if (m_default_layout_name)
        { /* If a default layout name was provided */
            default_layout = get_layout_by_name(m_default_layout_name);
            if (default_layout)
            {
                dbg(lvl_debug, "Found the config-specified default layout '%s'", m_default_layout_name);
                m_layout_current = default_layout;
                return;
            }
            else
            {
                dbg(lvl_warning, "No definition exists in config for specified default layout '%s'", m_default_layout_name);
            }
        }
    }
}

int Navit::add_log(struct log *log)
{
    struct attr type_attr;
    if (!log_get_attr(log, attr_type, &type_attr, NULL))
        return 0;
    if (!strcmp(type_attr.u.str, "textfile_debug"))
    {
        char *header = "type=track_tracked\n";
        if (m_textfile_debug_log)
            return 0;
        log_set_header(log, header, strlen(header));
        m_textfile_debug_log = log;
        return 1;
    }
    return 0;
}

int Navit::add_layout(struct layout *layout)
{
    struct attr active;
    int is_default = 0;
    int is_active = 0;
    m_layouts = g_list_append(m_layouts, layout);
    /** check if we want to immediately activate this layout.
     * Unfortunately we have concurring conditions about when to activate
     * a layout:
     * - A layout could bear the "active" property
     * - A layout's name could match m_default_layout_name
     * This cannot be fully resolved, as we cannot predict the future, so
     * lets set the last parsed layout active, which either matches default_layout_name or
     * bears the "active" tag, or is the first layout ever parsed.
     */
    if ((layout->name != NULL) && (m_default_layout_name != NULL))
    {
        if (strcmp(layout->name, m_default_layout_name) == 0)
            is_default = 1;
    }
    layout_get_attr(layout, attr_active, &active, NULL);
    if (active.u.num)
        is_active = 1;
    dbg(lvl_debug, "add layout '%s' is_default %d, is_active %d", layout->name, is_default, is_active);
    if (is_default || is_active || !m_layout_current)
    {
        m_layout_current = layout;
        return 1;
    }
    return 0;
}

int Navit::add_attr(struct attr *attr)
{
    int ret = 1;
    switch (attr->type)
    {
    case attr_callback:
        add_callback(attr->u.callback);
        break;
    case attr_log:
        ret = add_log(attr->u.log);
        break;
    case attr_gui:
        break;
    case attr_graphics:
    {
        Graphics *graphics = static_cast<Graphics *>(attr->u.graphics);
        if (graphics == nullptr)
        {
            ret = set_graphics(*graphics);
        }
        break;
    }
    case attr_layout:
        add_layout(attr->u.layout);
        break;
    case attr_route:
        m_route = attr->u.route;
        break;
    case attr_mapset:
        m_mapsets = g_list_append(m_mapsets, attr->u.mapset);
        break;
    case attr_navigation:
        m_navigation = attr->u.navigation;
        break;
    case attr_recent_dest:
        m_recentdest_count = attr->u.num;
        break;
    case attr_speech:
        m_speech = attr->u.speech;
        break;
    case attr_trackingo:
        m_tracking = attr->u.tracking;
        break;
    case attr_vehicle:
        ret = add_vehicle(attr->u.vehicle);
        break;
    case attr_vehicleprofile:
        m_vehicleprofiles = g_list_append(m_vehicleprofiles, attr->u.vehicleprofile);
        break;
    case attr_autozoom_min:
        m_autozoom_min = attr->u.num;
        break;
    case attr_autozoom_max:
        m_autozoom_max = attr->u.num;
        break;
    case attr_layer:
    case attr_script:
    case attr_traffic:
        break;
    default:
        return 0;
    }
    if (ret)
        m_attrs = attr_generic_add_attr(m_attrs, attr);
    callback_list_call_attr_2(m_attr_cbl, attr->type, this, attr);
    return ret;
}

int Navit::remove_attr(struct attr *attr)
{
    int ret = 1;
    switch (attr->type)
    {
    case attr_callback:
        remove_callback(attr->u.callback);
        break;
    case attr_vehicle:
        m_attrs = attr_generic_remove_attr(m_attrs, attr);
        return 1;
    default:
        return 0;
    }
    return ret;
}

struct attr_iter *Navit::attr_iter_new()
{
    return g_new0(struct attr_iter, 1);
}

void Navit::attr_iter_destroy(struct attr_iter *iter)
{
    g_free(iter);
}

void Navit::add_callback(struct callback *cb)
{
    callback_list_add(m_attr_cbl, cb);
}

void Navit::remove_callback(struct callback *cb)
{
    callback_list_remove(m_attr_cbl, cb);
}

static int coord_not_set(struct coord c)
{
    return !(c.x || c.y);
}

/**
 * Toggle the cursor update : refresh the map each time the cursor has moved (instead of only when it reaches a border)
 *
 * @param this_ The navit instance
 * @param nv vehicle to draw
 * @param pnt Screen coordinates of the vehicle. If NULL, position stored in nv is used.
 * @returns nothing
 */

void Navit::draw_vehicle(struct navit_vehicle *nv, struct point *pnt)
{
    struct point cursor_pnt;
    enum projection pro;

    if (m_blocked || coord_not_set(nv->coord))
        return;
    if (pnt)
        cursor_pnt = *pnt;
    else
    {
        pro = transform_get_projection(m_trans_cursor);
        if (!pro)
            return;
        transform_point(m_trans_cursor, pro, &nv->coord, &cursor_pnt);
    }
    vehicle_draw(nv->vehicle, m_gra, &cursor_pnt, nv->dir - transform_get_yaw(m_trans_cursor), nv->speed);
}

/**
 * @brief Called when the position of a vehicle changes.
 *
 * This function is called when the position of any configured vehicle changes and triggers all actions
 * that need to happen in response, such as:
 * <ul>
 * <li>Switching between day and night layout (based on the new position timestamp)</li>
 * <li>Updating position, bearing and speed of {@code nv} with the data of the active vehicle
 * (which may be different from the vehicle reporting the update)</li>
 * <li>Invoking callbacks for {@code navit}'s {@code attr_position} and {@code attr_position_coord_geo}
 * attributes</li>
 * <li>Triggering an update of the vehicle's position on the map and, if needed, an update of the
 * visible map area ad orientation</li>
 * <li>Logging a new track point, if enabled</li>
 * <li>Updating the position on the route</li>
 * <li>Stopping navigation if the destination has been reached</li>
 * </ul>
 *
 * @param this_ The navit object
 * @param nv The {@code navit_vehicle} which reported a new position
 */
void Navit::vehicle_update_position(struct navit_vehicle *nv)
{
    struct attr attr_valid, attr_dir, attr_speed, attr_pos;
    struct pcoord cursor_pc;
    struct point cursor_pnt, *pnt = &cursor_pnt;
    struct tracking *tracking = NULL;
    struct pcoord *pc;
    enum projection pro = transform_get_projection(m_trans_cursor);
    int count;
    int (*get_attr)(void *, enum attr_type, struct attr *, struct attr_iter *);
    void *attr_object;
    char *destination_file;
    char *description;

    profile(0, NULL);
    if (m_ready == 3)
        layout_switch();
    if (m_vehicle == nv && m_tracking_flag)
        tracking = m_tracking;
    if (tracking)
    {
        tracking_update(tracking, nv->vehicle, m_vehicleprofile, pro);
        attr_object = tracking;
        get_attr = (int (*)(void *, enum attr_type, struct attr *, struct attr_iter *))tracking_get_attr;
    }
    else
    {
        attr_object = nv->vehicle;
        get_attr = (int (*)(void *, enum attr_type, struct attr *, struct attr_iter *))vehicle_get_attr;
    }
    if (get_attr(attr_object, attr_position_valid, &attr_valid, NULL))
        if (!attr_valid.u.num != attr_position_valid_invalid)
            return;
    if (!get_attr(attr_object, attr_position_direction, &attr_dir, NULL) ||
        !get_attr(attr_object, attr_position_speed, &attr_speed, NULL) ||
        !get_attr(attr_object, attr_position_coord_geo, &attr_pos, NULL))
    {
        profile(0, "return 2\n");
        return;
    }
    nv->dir = *attr_dir.u.numd;
    nv->speed = *attr_speed.u.numd;
    transform_from_geo(pro, attr_pos.u.coord_geo, &nv->coord);
    if (nv != m_vehicle)
    {
        if (m_ready == 3)
            draw_vehicle(nv, NULL);
        profile(0, "return 3\n");
        return;
    }
    cursor_pc.x = nv->coord.x;
    cursor_pc.y = nv->coord.y;
    cursor_pc.pro = pro;
    if (m_route)
    {
        if (tracking)
            route_set_position_from_tracking(m_route, tracking, pro);
        else
            route_set_position(m_route, &cursor_pc);
    }
    callback_list_call_attr_0(m_attr_cbl, attr_position);
    textfile_debug_log("type=trackpoint_tracked");
    if (m_ready == 3)
    {
        transform_point(m_trans_cursor, pro, &nv->coord, &cursor_pnt);
        if (m_follow_cursor && nv->follow_curr <= nv->follow &&
            (nv->follow_curr == 1 || !transform_within_border(m_trans_cursor, &cursor_pnt, m_border)))
            set_center_cursor_draw();
        else
            draw_vehicle(nv, pnt);

        if (nv->follow_curr > 1)
            nv->follow_curr--;
        else
            nv->follow_curr = nv->follow;
    }
    callback_list_call_attr_2(m_attr_cbl, attr_position_coord_geo, this, nv->vehicle);

    /* Finally, if we reached our destination, stop navigation. */
    if (m_route)
    {
        switch (route_destination_reached(m_route))
        {
        case 1:
            description = route_get_destination_description(m_route, 0);
            route_remove_waypoint(m_route);
            count = route_get_destination_count(m_route);
            pc = (struct pcoord *)g_alloca(sizeof(*pc) * count);
            route_get_destinations(m_route, pc, count);
            destination_file = bookmarks_get_destination_file(TRUE);
            bookmarks_append_destinations(m_former_destination, destination_file, pc, count, type_former_itinerary_part,
                                          description, m_recentdest_count);
            g_free(destination_file);
            g_free(description);
            break;
        case 2:
            destination_file = bookmarks_get_destination_file(TRUE);
            bookmarks_append_destinations(m_former_destination, destination_file, NULL, 0, type_former_itinerary_part, NULL,
                                          m_recentdest_count);
            set_destination(NULL, NULL, 0);
            g_free(destination_file);
            break;
        }
    }
    profile(0, "return 5\n");
}

/**
 * @brief Called when a status attribute of a vehicle changes.
 *
 * This function is called when the {@code position_fix_type}, {@code position_sats_used} or {@code position_hdop}
 * attribute of any configured vehicle changes.
 *
 * The function checks if {@code nv} refers to the active vehicle and if {@code type} is one of the above types.
 * If this is the case, it invokes the callback functions for {@code navit}'s respective attributes.
 *
 * Future actions that need to happen when one of these three attribute changes for any vehicle should be
 * implemented here.
 *
 * @param this_ The navit object
 * @param nv The {@code navit_vehicle} which reported a new status attribute
 * @param type The type of attribute with has changed
 */
void Navit::vehicle_update_status(struct navit_vehicle *nv, enum attr_type type)
{
    if (m_vehicle != nv)
        return;
    switch (type)
    {
    case attr_position_fix_type:
    case attr_position_sats_used:
    case attr_position_hdop:
        callback_list_call_attr_2(m_attr_cbl, type, this, nv->vehicle);
        break;
    default:
        return;
    }
}

/**
 * Set the position of the vehicle
 *
 * @param navit The navit instance
 * @param c The coordinate to set as position
 * @returns nothing
 */

void Navit::set_position(struct pcoord *c)
{
    if (m_route)
    {
        route_set_position(m_route, c);
        callback_list_call_attr_0(m_attr_cbl, attr_position);
    }
    if (m_ready == 3)
        draw();
}

int Navit::set_vehicleprofile(struct vehicleprofile *vp)
{
    if (m_vehicleprofile == vp)
        return 0;
    m_vehicleprofile = vp;
    if (m_route)
        route_set_profile(m_route, m_vehicleprofile);
    return 1;
}

int Navit::set_vehicleprofile_name(char *name)
{
    struct attr attr;
    GList *l;
    l = m_vehicleprofiles;
    while (l)
    {
        if (vehicleprofile_get_attr((vehicleprofile *)l->data, attr_name, &attr, NULL))
        {
            if (!strcmp(attr.u.str, name))
            {
                set_vehicleprofile((vehicleprofile *)l->data);
                return 1;
            }
        }
        l = g_list_next(l);
    }
    return 0;
}

void Navit::set_vehicle(struct navit_vehicle *nv)
{
    struct attr attr;
    m_vehicle = nv;
    if (nv && vehicle_get_attr(nv->vehicle, attr_profilename, &attr, NULL))
    {
        if (set_vehicleprofile_name(attr.u.str))
            return;
    }
    if (!m_vehicleprofile)
    { // When deactivating vehicle, keep the last profile if any
        if (!set_vehicleprofile_name("car"))
        {
            /* We do not have a fallback "car" profile
             * so lets set any profile */
            GList *l;
            l = m_vehicleprofiles;
            if (l)
            {
                m_vehicleprofile = (vehicleprofile *)l->data;
                if (m_route)
                    route_set_profile(m_route, m_vehicleprofile);
            }
        }
    }
    else
    {
        if (m_route)
            route_set_profile(m_route, m_vehicleprofile);
    }
}

static void navit_vehicle_update_position(void *data, struct navit_vehicle *nv)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    if (!navit->m_ignore_graphics_events)
        navit->vehicle_update_position(nv);
}

static void navit_vehicle_update_status(void *data, struct navit_vehicle *nv, enum attr_type type)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    if (!navit->m_ignore_graphics_events)
        navit->vehicle_update_status(nv, type);
}

/**
 * @brief Registers a new vehicle.
 *
 * @param this_ The navit instance
 * @param v The vehicle to register
 * @return True for success
 */
int Navit::add_vehicle(struct vehicle *v)
{
    struct navit_vehicle *nv = g_new0(struct navit_vehicle, 1);
    struct attr follow, active, animate;
    nv->vehicle = v;
    nv->follow = 0;
    nv->last.x = 0;
    nv->last.y = 0;
    nv->animate_cursor = 0;
    if ((vehicle_get_attr(v, attr_follow, &follow, NULL)))
        nv->follow = follow.u.num;
    nv->follow_curr = nv->follow;
    m_vehicles = g_list_append(m_vehicles, nv);
    if ((vehicle_get_attr(v, attr_active, &active, NULL)) && active.u.num)
        set_vehicle(nv);
    if ((vehicle_get_attr(v, attr_animate, &animate, NULL)))
        nv->animate_cursor = animate.u.num;
    nv->callback.type = attr_callback;
    nv->callback.u.callback = callback_new_attr_2(callback_cast(navit_vehicle_update_position), attr_position_coord_geo,
                                                  this, nv);
    vehicle_add_attr(nv->vehicle, &nv->callback);
    nv->callback.u.callback = callback_new_attr_3(callback_cast(navit_vehicle_update_status), attr_position_fix_type, this,
                                                  nv, attr_position_fix_type);
    vehicle_add_attr(nv->vehicle, &nv->callback);
    nv->callback.u.callback = callback_new_attr_3(callback_cast(navit_vehicle_update_status), attr_position_sats_used, this,
                                                  nv, attr_position_sats_used);
    vehicle_add_attr(nv->vehicle, &nv->callback);
    nv->callback.u.callback = callback_new_attr_3(callback_cast(navit_vehicle_update_status), attr_position_hdop, this, nv,
                                                  attr_position_hdop);
    vehicle_add_attr(nv->vehicle, &nv->callback);
    vehicle_set_attr(nv->vehicle, &m_self);
    return 1;
}

struct gui *Navit::get_gui()
{
    return NULL;
}

struct transformation *Navit::get_trans()
{
    return m_trans;
}

struct route *Navit::get_route()
{
    return m_route;
}

struct navigation *Navit::get_navigation()
{
    return m_navigation;
}

void Navit::layout_switch()
{

    int currTs = 0;
    struct attr iso8601_attr, geo_attr, valid_attr, layout_attr;
    double trise, tset;
    struct layout *l;
    int year, month, day;
    int after_sunrise = FALSE;
    int after_sunset = FALSE;
    int tunnel = tracking_get_current_tunnel(m_tracking);

    if (get_attr(attr_layout, &layout_attr, NULL) != 1)
    {
        return; // No layout - nothing to switch
    }
    if (!m_vehicle)
        return;
    l = layout_attr.u.layout;

    if (l->dayname || l->nightname)
    {
        // Ok, we know that we have profile to switch

        // Check that we aren't calculating too fast
        if (vehicle_get_attr(m_vehicle->vehicle, attr_position_time_iso8601, &iso8601_attr, NULL) == 1)
        {
            currTs = iso8601_to_secs(iso8601_attr.u.str);
            dbg(lvl_debug, "currTs: %02u:%02u", currTs % 86400 / 3600, ((currTs % 86400) % 3600) / 60);
        }
        dbg(lvl_debug, "prevTs: %02u:%02u", m_prevTs % 86400 / 3600, ((m_prevTs % 86400) % 3600) / 60);

        if (m_auto_switch == FALSE)
            return;

        if (m_tunnel_nightlayout)
        {
            if (tunnel)
            {
                // store the current layout name
                if (!strcmp(m_layout_before_tunnel, ""))
                    m_layout_before_tunnel = m_layout_current->name;

                // We are in a tunnel and if we have a nightlayout -> switch to nightlayout
                if (l->nightname)
                {
                    set_layout_by_name(l->nightname);
                    dbg(lvl_debug, "tunnel -> nightlayout");
                }
                return;
            }
            else
            {
                if (l->dayname)
                {
                    if (!strcmp(l->dayname, m_layout_before_tunnel))
                    {
                        // restore previous layout
                        set_layout_by_name(l->dayname);
                        dbg(lvl_debug, "tunnel end -> daylayout");
                    }

                    // We were in nightlayout before the tunnel, keep it
                    m_layout_before_tunnel = "";
                }
            }
        }

        if (currTs - (m_prevTs) < 60)
        {
            // We've have to wait a little
            return;
        }

        if (sscanf(iso8601_attr.u.str, "%d-%02d-%02dT", &year, &month, &day) != 3)
            return;
        if (vehicle_get_attr(m_vehicle->vehicle, attr_position_valid, &valid_attr, NULL) && valid_attr.u.num == attr_position_valid_invalid)
        {
            return; // No valid fix yet
        }

        if (vehicle_get_attr(m_vehicle->vehicle, attr_position_coord_geo, &geo_attr, NULL) != 1)
        {
            // No position - no sun
            return;
        }

        // We calculate sunrise anyway, cause it is needed both for day and for night
        if (__sunriset__(year, month, day, geo_attr.u.coord_geo->lng, geo_attr.u.coord_geo->lat, m_sunrise_degrees, 1, &trise,
                         &tset) != 0)
        {
            dbg(lvl_debug, "near the pole sun never rises/sets, so we should never switch profiles");
            dbg(lvl_debug, "trise: %02u:%02u", HOURS(trise), MINUTES(trise));
            dbg(lvl_debug, "tset: %02u:%02u", HOURS(tset), MINUTES(tset));
            m_prevTs = currTs;
            return;
        }

        dbg(lvl_debug, "trise: %02u:%02u", HOURS(trise), MINUTES(trise));
        dbg(lvl_debug, "tset: %02u:%02u", HOURS(tset), MINUTES(tset));
        dbg(lvl_debug, "dayname = %s, name =%s ", l->dayname, l->name);
        dbg(lvl_debug, "nightname = %s, name = %s ", l->nightname, l->name);

        // We want any times to be in [0;1439].
        if (trise < 0)
            trise += 24;
        if (tset >= 24)
            tset -= 24;

        int trmin = HOURS(trise) * 60 + MINUTES(trise); // rise time in minutes
        int tsmin = HOURS(tset) * 60 + MINUTES(tset);   // set time in minutes
        int tcur = (currTs % 86400) / 60;               // minutes elapsed today

        if (trmin <= tcur)
        { // use <= to have defined values for after_sunrise for any value of tcur
            after_sunrise = TRUE;
        }

        if (tsmin > trmin)
        { // set > rise
            if (trmin <= tcur)
            {
                after_sunrise = TRUE;
            }
            if (tsmin <= tcur || trmin > tcur)
            {
                after_sunset = TRUE;
            }
        }
        else
        { // rise > set
            if ((tcur <= tsmin) || (tcur >= trmin))
            {
                after_sunrise = TRUE;
            }
            if (tcur > tsmin && tcur < trmin)
            {
                after_sunset = TRUE;
            }
        }

        if (after_sunrise && !after_sunset && l->dayname)
        {
            set_layout_by_name(l->dayname);
            dbg(lvl_debug, "layout set to day");
        }
        else if (after_sunset && l->nightname)
        {
            set_layout_by_name(l->nightname);
            dbg(lvl_debug, "layout set to night");
        }
        m_prevTs = currTs;
    }
}

int Navit::set_vehicle_by_name(const char *name)
{
    struct vehicle *v;
    struct attr_iter *iter;
    struct attr vehicle_attr, name_attr;

    iter = attr_iter_new();

    while (get_attr(attr_vehicle, &vehicle_attr, iter))
    {
        v = vehicle_attr.u.vehicle;
        vehicle_get_attr(v, attr_name, &name_attr, NULL);
        if (name_attr.type == attr_name)
        {
            if (!strcmp(name, name_attr.u.str))
            {
                set_attr(&vehicle_attr);
                attr_iter_destroy(iter);
                return 1;
            }
        }
    }
    attr_iter_destroy(iter);
    return 0;
}

int Navit::set_layout_by_name(const char *name)
{
    struct layout *l;
    struct attr_iter iter;
    struct attr layout_attr;

    iter.u.list = 0x00;

    if (get_attr(attr_layout, &layout_attr, &iter) != 1)
    {
        return 0; // No layouts - nothing to do
    }
    if (iter.u.list == NULL)
    {
        return 0;
    }

    iter.u.list = g_list_first(iter.u.list);

    while (iter.u.list)
    {
        l = (struct layout *)iter.u.list->data;
        if (!strcmp(name, l->name))
        {
            layout_attr.u.layout = l;
            layout_attr.type = attr_layout;
            set_attr(&layout_attr);
            iter.u.list = g_list_first(iter.u.list);
            return 1;
        }
        iter.u.list = g_list_next(iter.u.list);
    }

    iter.u.list = g_list_first(iter.u.list);
    return 0;
}

/**
 * @brief Blocks or unblocks redraw operations.
 *
 * The {@code block} parameter specifies the operation to carry out:
 *
 * {@code block > 0} cancels all draw operations in progress and blocks future operations. It sets flag 1 of the
 * {@code blocked} member. If draw operations in progress were canceled, flag 2 is also set.
 *
 * {@code block = 0} unblocks redraw operations, resetting {@code blocked} to 0. If flag 2 was previously set,
 * indicating that draw operations had been previously canceled, a redraw is triggered.
 *
 * {@code block < 0} unblocks redraw operations and forces a redraw. As above, {@code blocked} is reset to 0.
 *
 * @param this_ The navit instance
 * @param block The operation to perform, see description
 *
 * @return {@code true} if a redraw operation was triggered, {@code false} if not
 */
int Navit::block(int block)
{
    if (block > 0)
    {
        m_blocked |= 1;
        if (m_displaylist.draw_cancel())
            m_blocked |= 2;
        return 0;
    }
    if ((m_blocked & 2) || block < 0)
    {
        m_blocked = 0;
        draw();
        return 1;
    }
    m_blocked = 0;
    return 0;
}

/**
 * @brief Returns whether redraw operations are currently blocked.
 */
int Navit::get_blocked()
{
    return m_blocked;
}

void Navit::destroy()
{
    dbg(lvl_debug, "enter %p", this);
    GList *mapsets;
    struct map *map;
    struct attr attr;
    m_displaylist.draw_cancel();

    mapsets = m_mapsets;
    while (mapsets)
    {
        GList *maps = NULL;
        struct mapset_handle *msh;
        msh = mapset_open((mapset *)mapsets->data);
        while (msh && (map = mapset_next(msh, 0)))
        {
            /* Add traffic map (identified by the `attr_traffic` attribute) to list of maps to remove */
            if (map_get_attr(map, attr_traffic, &attr, NULL))
                maps = g_list_append(maps, map);
        }
        mapset_close(msh);

        /* Remove traffic maps, if any */
        while (maps)
        {
            attr.type = attr_map;
            attr.u.map = (struct map *)maps->data;
            mapset_remove_attr((mapset *)mapsets->data, &attr);
            attr_free_content(&attr);
            maps = g_list_next(maps);
        }
        if (maps)
            g_list_free(maps);
        mapsets = g_list_next(mapsets);
    }

    callback_list_call_attr_1(m_attr_cbl, attr_destroy, this);
    attr_list_free(m_attrs);

    if (m_bookmarks)
    {
        char *center_file = bookmarks_get_center_file(TRUE);
        bookmarks_write_center_to_file(m_bookmarks, center_file);
        g_free(center_file);
        bookmarks_destroy(m_bookmarks);
    }

    callback_destroy(m_nav_speech_cb);
    callback_destroy(m_roadbook_callback);
    callback_destroy(m_motion_timeout_callback);

    m_graphics.remove_callback(m_resize_callback);
    m_graphics.remove_callback(m_motion_callback);
    m_graphics.remove_callback(m_predraw_callback);

    callback_destroy(m_resize_callback);
    callback_destroy(m_motion_callback);
    callback_destroy(m_predraw_callback);

    callback_destroy(m_route_cb);
    if (m_route)
        route_destroy(m_route);

    map_destroy(m_former_destination);

    m_displaylist.destroy();

    m_graphics.free();

    // g_free();
}

/** @} */

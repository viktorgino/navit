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

#include "navit.h"

extern "C"
{
#include "debug.h"
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
#include "vehicle_wrapper.h"
#include "log.h"
#include "event.h"
#include "file.h"
#include "command.h"
#include "navit_nls.h"
#include "map.h"
#include "util.h"
#include "messages.h"
#include "vehicleprofile.h"
#include "sunriset.h"
#include "bookmarks.h"
#include "attr.h"
#include "plugin.h"
#ifdef HAVE_API_WIN32_BASE
#include <windows.h>
#include "util.h"
#endif
#ifdef HAVE_API_WIN32_CE
#include "libc.h"
#endif
}

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

GraphicsFunctions &getGraphicsFunctions()
{
    auto ret = reinterpret_cast<GraphicsFunctions *(*)()>(plugin_get_category(plugin_category_graphics, "qt5"));
    return *static_cast<GraphicsFunctions *>(ret());
}

Navit::Navit(NavitConfig &navitConfig, QObject *parent) : QObject(parent),
                                                          m_config(navitConfig),
                                                          m_plugins(m_config, this, this),
                                                          m_graphics(*this, getGraphicsFunctions(), this),
                                                          m_displaylist(m_graphics)
{
    struct pcoord center;
    struct coord co;
    struct coord_geo g;
    enum projection pro = projection_mg;
    int zoom = 256;
    g.lat = 53.13;
    g.lng = 11.70;

    m_self.type = attr_navit;
    m_self.u.navit = this;

    m_plugins.loadModules();

    add_vehicle(m_plugins.getVehicle());

    m_attr_cbl = callback_list_new();

    m_autozoom_active = 0;
    m_autozoom_paused = 0;
    m_layout_before_tunnel = "";

    transform_from_geo(pro, &g, &co);
    center.x = co.x;
    center.y = co.y;
    center.pro = pro;
    m_trans = transform_new(&center, zoom, (m_config.orientation != -1) ? m_config.orientation : 0);
    m_trans_cursor = transform_new(&center, zoom, (m_config.orientation != -1) ? m_config.orientation : 0);

    m_bookmarks = bookmarks_new(&m_self, NULL, m_trans);
    m_center = m_config.center;

    m_prevTs = 0;

    Layout *modern_layout = new Layout();
    ConfigLoader::loadLayout("navit_layout_car_modern.json", *modern_layout);
    add_layout(modern_layout);
    update_current_layout(modern_layout);

    // for (; *attrs; attrs++)
    // {
    //     set_attr_do(*attrs, 1);
    // }

    // TODO: Only used by traffic
    // m_messages = messagelist_new(attrs);

    // Init graphics callbacks
    set_graphics();
    m_plugins.getVehicle()->set_cursor(modern_layout->getCursors().first(), 1);
    dbg(lvl_debug, "return %p", this);

    init();

    coord *trans_c = transform_get_center(m_trans);
    transform_to_geo(transform_get_projection(m_trans), trans_c, &m_center);
}

// m_plugins.getMapsets().first()

/**
 * @brief Get the current mapset
 *
 * @param this_ The navit instance
 *
 * @return A pointer to the current mapset
 */
struct mapset *Navit::get_mapset()
{
    if (m_plugins.getMapsets().size() > 0)
    {
        return m_plugins.getMapsets().first();
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

        char csv_str[] = "csv";
        char search_results_str[] = "search_results";
        char utf_8_str[] = "utf-8";

        attrs[0] = g_new0(struct attr, 1);
        attrs[0]->type = attr_type;
        attrs[0]->u.str = csv_str;

        attrs[1] = g_new0(struct attr, 1);
        attrs[1]->type = attr_name;
        attrs[1]->u.str = search_results_str;

        attrs[2] = g_new0(struct attr, 1);
        attrs[2]->type = attr_charset;
        attrs[2]->u.str = utf_8_str;

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
    return m_plugins.getTracking();
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
    m_displaylist.draw_graphics(m_plugins.getMapsets().first(), m_trans, m_layout_current, async, NULL, m_graphics_flags | 1);
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
    LayoutCoord p(10, 32);

    if (m_ready != 3)
        return;

    ms = m_plugins.getMapsets().first();
    msh = mapset_open(ms);
    while (msh && (map = mapset_next(msh, 0)))
    {
        if (map_get_attr(map, attr_progress, &attr, NULL))
        {
            QString str = QString("%s           ").arg(attr.u.str);
            m_graphics.draw_mode(draw_mode_begin);
            m_graphics.draw_text_std(16, str, &p);
            p.set(p.getX(), p.getY() + 32);
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
    if (m_plugins.getVehicle())
    {
        if (m_plugins.getVehicle()->getFollowCursor() == 1)
            return;
        if (m_plugins.getVehicle()->getFollowCursor() <= m_plugins.getVehicle()->getFollow())
            m_plugins.getVehicle()->setFollowCursor(m_plugins.getVehicle()->getFollow());
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
        attr.u.num = m_config.pitch;
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
    follow.u.num = m_config.center_timeout;
    set_attr(&follow);
}

void Navit::motion_timeout()
{
    int dx, dy;

    if (m_config.drag_bitmap)
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
            m_motion_timeout = event_add_timeout(m_config.drag_bitmap ? 10 : 100, 0, m_motion_timeout_callback);
    }
}

void Navit::scale(long scale, struct point *p, int draw_)
{
    struct coord c1, c2, *center;
    if (scale < m_config.zoom_min)
        scale = m_config.zoom_min;
    if (scale > m_config.zoom_max)
        scale = m_config.zoom_max;
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

void transform_point(transformation *t, projection required_projection, LayoutCoord *coord, LayoutCoord *result)
{
    QVector<LayoutCoord *> coords;
    QVector<LayoutCoord *> results;

    coords.append(coord);
    coords.append(result);
    transform_point_buf(t, required_projection, coords, results, 0, 0, nullptr);
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
void Navit::autozoom(struct coord *c, int speed)
{
    struct point pc;
    LayoutCoord centerTrans;
    LayoutCoord center(c);
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

    distance = speed * m_config.autozoom_secs;

    transform_get_size(m_trans, &w, &h);
    transform_point(m_trans, transform_get_projection(m_trans), &center, &centerTrans);
    pc.x = centerTrans.getX();
    pc.y = centerTrans.getY();

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
    if (new_scale > m_config.autozoom_max)
        new_scale = m_config.autozoom_max;
    if (new_scale < m_config.autozoom_min)
        new_scale = m_config.autozoom_min;
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
    if (m_plugins.getVehicle() && m_plugins.getVehicle()->getFollowCursor() <= 1 && get_cursor_pnt(&p, 0, NULL))
    {
        zoom_in(factor, &p);
        m_plugins.getVehicle()->setFollowCursor(m_plugins.getVehicle()->getFollow());
    }
    else
        zoom_in(factor, NULL);
}

void Navit::zoom_out_cursor(int factor)
{
    struct point p;
    if (m_plugins.getVehicle() && m_plugins.getVehicle()->getFollowCursor() <= 1 && get_cursor_pnt(&p, 0, NULL))
    {
        zoom_out(2, &p);
        m_plugins.getVehicle()->setFollowCursor(m_plugins.getVehicle()->getFollow());
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
    transform_copy(m_trans, m_trans_cursor);
    for (Vehicle *vehicle : m_plugins.getVehicles())
    {
        draw_vehicle(vehicle, NULL);
    }
}

static void navit_resize(void *data, int w, int h)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    navit->handle_resize(w, h);
}

static void navit_motion(void *data, struct point *p)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
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

int Navit::set_graphics()
{
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
                                      m_config.recentdest_count);
    }
    else
    {
        m_destination_valid = 0;
        bookmarks_append_destinations(m_former_destination, destination_file, NULL, 0, type_former_destination, NULL,
                                      m_config.recentdest_count);
        mark_navigation_stopped(destination_file);
    }
    g_free(destination_file);

    if (m_plugins.getRoute())
    {
        struct attr attr;
        int dstcount;
        struct pcoord *pc;

        get_attr(attr_waypoints_flag, &attr, NULL);
        if (m_config.waypoints_flag == 0 || route_get_destination_count(m_plugins.getRoute()) == 0)
        {
            route_set_destination(m_plugins.getRoute(), c, async);
        }
        else
        {
            route_append_destination(m_plugins.getRoute(), c, async);
        }

        dstcount = route_get_destination_count(m_plugins.getRoute());
        if (dstcount > 0)
        {
            destination_file = bookmarks_get_destination_file(TRUE);
            pc = g_new(struct pcoord, dstcount);
            route_get_destinations(m_plugins.getRoute(), pc, dstcount);
            bookmarks_append_destinations(m_former_destination, destination_file, pc, dstcount, type_former_itinerary,
                                          description, m_config.recentdest_count);
            g_free(pc);
            g_free(destination_file);
        }
    }

    callback_list_call_attr_0(m_attr_cbl, attr_destination);

    if (m_plugins.getRoute() && m_ready == 3 && !(m_config.flags & 4))
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
                                      m_config.recentdest_count);
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
                                      m_config.recentdest_count);
        g_free(destination_file);
    }
    else
        m_destination_valid = 0;
    if (m_plugins.getRoute())
        route_set_destinations(m_plugins.getRoute(), c, count, async);

    callback_list_call_attr_0(m_attr_cbl, attr_destination);
    if (m_plugins.getRoute() && m_ready == 3)
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
    if (!m_plugins.getRoute())
        return 0;
    return route_get_destinations(m_plugins.getRoute(), pc, count);
}

/**
 * @brief Get the destinations count for the route
 *
 * @param this The Navit instance
 * @return destination count for the route, or 0 if the Navit instance has no route
 */
int Navit::get_destination_count()
{
    if (!m_plugins.getRoute())
        return 0;
    return route_get_destination_count(m_plugins.getRoute());
}

char *Navit::get_destination_description(int n)
{
    if (!m_plugins.getRoute())
        return NULL;
    return route_get_destination_description(m_plugins.getRoute(), n);
}

void Navit::remove_nth_waypoint(int n)
{
    if (!m_plugins.getRoute())
        return;
    if (route_get_destination_count(m_plugins.getRoute()) > 1)
    {
        route_remove_nth_waypoint(m_plugins.getRoute(), n);
    }
    else
    {
        set_destination(NULL, NULL, 0);
    }
}

void Navit::remove_waypoint()
{
    if (!m_plugins.getRoute())
        return;
    if (route_get_destination_count(m_plugins.getRoute()) > 1)
    {
        route_remove_waypoint(m_plugins.getRoute());
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
    if (m_plugins.getRoute())
    {
        return route_get_path_set(m_plugins.getRoute());
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

struct map *Navit::read_former_destinations_from_file()
{
    struct attr type, data, no_warn, flags, *attrs[5];
    char *destination_file = bookmarks_get_destination_file(FALSE);
    struct map *m;

    char textfile_str[] = "textfile";
    type.type = attr_type;
    type.u.str = textfile_str;

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
    if (!m_plugins.getRoute() || !former_destinations_active() || !m_plugins.getVehicle())
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
            route_set_destination(m_plugins.getRoute(), &pc[0], 1);
        else
            route_set_destinations(m_plugins.getRoute(), pc, count, 1);
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
    if (m_textfile_debug_log && m_plugins.getVehicle())
    {
        coord position = get_vehicle_cursor_coords(m_plugins.getVehicle());
        str1 = g_strdup_vprintf(fmt, ap);
        str2 = g_strdup_printf("0x%x 0x%x%s%s\n", position.x, position.y, strlen(str1) ? " " : "",
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
    if (m_textfile_debug_log && m_plugins.getVehicle())
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
    struct navigation *nav = m_plugins.getNavigation();
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
    struct navigation *nav = m_plugins.getNavigation();
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
    navigation_unregister_callback(m_plugins.getNavigation(), attr_navigation_long, m_roadbook_callback);
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
    navigation_register_callback(m_plugins.getNavigation(), attr_navigation_long, m_roadbook_callback);
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
    navit->redraw_route(route, attr);
}

static void navit_speak_callback(void *data)
{
    if (data == nullptr)
    {
        return;
    }
    Navit *navit = static_cast<Navit *>(data);
    navit->speak();
}

int Navit::init()
{
    int callback;
    char *center_file;

    m_w = 0;
    m_h = 0;

    dbg(lvl_info, "enter graphics %p", &m_graphics);

    // if (!m_gra && !(m_config.flags & 1)) {
    //     dbg(lvl_error,"FATAL: No graphics subsystem available.");
    //     exit(1);
    // }

    if (m_speech && m_plugins.getNavigation())
    {
        struct attr speech;
        speech.type = attr_speech;
        speech.u.speech = m_speech;
        navigation_set_attr(m_plugins.getNavigation(), &speech);
    }
    dbg(lvl_info, "Initializing graphics");
    dbg(lvl_info, "Setting Vehicle");
    set_vehicle(m_plugins.getVehicle());
    load_dynamic_mapsets();
    if (m_plugins.getRoute())
    {
        struct attr callback;
        m_route_cb = callback_new_attr_1(callback_cast(navit_redraw_route), attr_route_status, this);
        callback.type = attr_callback;
        callback.u.callback = m_route_cb;
        route_add_attr(m_plugins.getRoute(), &callback);
    }
    if (m_plugins.getNavigation())
    {
        if (m_speech)
        {
            m_nav_speech_cb = callback_new_1(callback_cast(navit_speak_callback), this);
            navigation_register_callback(m_plugins.getNavigation(), attr_navigation_speech, m_nav_speech_cb);
        }
        if (m_plugins.getRoute())
            navigation_set_route(m_plugins.getNavigation(), m_plugins.getRoute());
    }
    dbg(lvl_info, "Setting Center");
    center_file = bookmarks_get_center_file(FALSE);
    bookmarks_set_center_from_file(m_bookmarks, center_file);
    g_free(center_file);

    // TODO: maybe fix
    // messagelist_init(m_messages);

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

void Navit::load_dynamic_mapsets()
{
    dbg(lvl_info, "Adding dynamic maps to mapset");
    if (m_plugins.getMapsets().size() == 0)
    {
        dbg(lvl_error, "FATAL: No mapset available. Please add a (valid) mapset to your configuration.");
        exit(1);
    }

    for (struct map *m : getMaps())
    {
        // pass new callback instance for each map in the mapset to make map callback list destruction work correctly
        struct callback *pcb = callback_new_attr_1(callback_cast(navit_map_progress), attr_progress, this);
        map_add_callback(m, pcb);
    }

    struct map *map;
    struct mapset *mapset = m_plugins.getMapsets().first();
    if (m_plugins.getRoute())
    {
        if ((map = route_get_map(m_plugins.getRoute())))
        {
            struct attr map_a;
            map_a.type = attr_map;
            map_a.u.map = map;
            mapset_add_attr(mapset, &map_a);
        }
        if ((map = route_get_graph_map(m_plugins.getRoute())))
        {
            struct attr map_a, active;
            map_a.type = attr_map;
            map_a.u.map = map;
            active.type = attr_active;
            active.u.num = 0;
            mapset_add_attr(mapset, &map_a);
            map_set_attr(map, &active);
        }
        route_set_mapset(m_plugins.getRoute(), mapset);
        route_set_projection(m_plugins.getRoute(), transform_get_projection(m_trans));
    }
    if (m_plugins.getTracking())
    {
        tracking_set_mapset(m_plugins.getTracking(), mapset);
        if (m_plugins.getRoute())
            tracking_set_route(m_plugins.getTracking(), m_plugins.getRoute());
    }

    // TODO: fix traffic
    // struct attr *attr_ = g_new0(attr, 1);
    // struct attr_iter *iter = attr_iter_new();
    // map = NULL;
    // while (get_attr(attr_traffic, attr_, iter))
    // {
    //     traffic = (struct traffic *)attr_->u.navit_object;
    //     traffic_set_mapset(traffic, ms);
    //     if (m_plugins.getRoute())
    //         traffic_set_route(traffic, m_plugins.getRoute());
    //     /* add the first map found */
    //     if (!map && (map = traffic_get_map(traffic)))
    //     {
    //         struct attr map_a;
    //         map_a.type = attr_map;
    //         map_a.u.map = map;
    //         mapset_add_attr(ms, &map_a);
    //     }
    // }
    // attr_iter_destroy(iter);
    // g_free(attr_);

    if (m_plugins.getNavigation())
    {
        if ((map = navigation_get_map(m_plugins.getNavigation())))
        {
            struct attr map_a, active;
            map_a.type = attr_map;
            map_a.u.map = map;
            active.type = attr_active;
            active.u.num = 0;
            mapset_add_attr(mapset, &map_a);
            map_set_attr(map, &active);
        }
    }
    if (m_plugins.getTracking())
    {
        if ((map = tracking_get_map(m_plugins.getTracking())))
        {
            struct attr map_a, active;
            map_a.type = attr_map;
            map_a.u.map = map;
            active.type = attr_active;
            active.u.num = 0;
            mapset_add_attr(mapset, &map_a);
            map_set_attr(map, &active);
        }
    }
    add_former_destinations_from_file();
}

const QVector<map *> Navit::getMaps()
{
    QVector<map *> ret;
    if (m_plugins.getMapsets().size() > 0)
    {
        struct mapset *mapset = m_plugins.getMapsets().first();
        struct mapset_handle *handle = mapset_open(mapset);
        struct map *map;

        while (handle && (map = mapset_next(handle, 0)))
        {
            ret.append(map);
        }
        mapset_close(handle);
    }
    return ret;
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
        LayoutCoord p1, p2;
        LayoutCoord lu(&r->lu);
        LayoutCoord rl(&r->rl);
        transform_set_scale(m_trans, scale);
        transform_setup_source_rect(m_trans);
        transform_point(m_trans, transform_get_projection(m_trans), &lu, &p1);
        transform_point(m_trans, transform_get_projection(m_trans), &rl, &p2);
        dbg(lvl_debug, "%d,%d-%d,%d", p1.getX(), p1.getY(), p2.getX(), p2.getY());
        if (p1.getX() < 0 || p2.getX() < 0 || p1.getX() > w || p2.getX() > w ||
            p1.getY() < 0 || p2.getY() < 0 || p1.getY() > h || p2.getY() > h)
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
    if (!m_plugins.getRoute())
        return;
    dbg(lvl_debug, "enter");
    map = route_get_map(m_plugins.getRoute());
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

LayoutCursor *Navit::get_layout_cursor(const QString &name)
{
    for (LayoutCursor *cursor : m_layout_current->getCursors())
    {
        if (cursor->getName() == name)
        {
            return cursor;
        }
    }
    return nullptr;
}
/**
 * Links all vehicles to a cursor depending on the current profile.
 *
 * @param this_ A navit instance
 * @author Ralph Sennhauser (10/2009)
 */
void Navit::set_cursors()
{
    LayoutCursor *cursor = nullptr;
    for (Vehicle *vehicle : m_plugins.getVehicles())
    {
        QString cursorName = vehicle->getCursorName();

        if (cursorName == "none")
            cursor = nullptr;
        else
            cursor = get_layout_cursor(vehicle->getCursorName());
        vehicle->set_cursor(cursor, 0);
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
    assert(m_plugins.getVehicle());

    int width, height;
    struct padding *padding = NULL;

    float offset = m_config.radius; // Cursor offset from the center of the screen (percent).

    padding = (struct padding *)m_graphics.get_data("padding");

    transform_get_size(m_trans, &width, &height);
    dbg(lvl_debug, "width=%d height=%d", width, height);

    if (padding)
    {
        width -= (padding->left + padding->right);
        height -= (padding->top + padding->bottom);
        dbg(lvl_debug, "corrected for padding: width=%d height=%d", width, height);
    }

    if (m_config.orientation == -1 || keep_orientation)
    {
        p->x = 50 * width / 100;
        p->y = (50 + offset) * height / 100;
        if (dir)
            *dir = keep_orientation ? m_config.orientation : getDirection(m_plugins.getVehicle());
    }
    else
    {
        int mdir;
        if (m_plugins.getTracking() && m_config.tracking_flag)
        {
            mdir = tracking_get_angle(m_plugins.getTracking()) - m_config.orientation;
        }
        else
        {
            mdir = getDirection(m_plugins.getVehicle()) - m_config.orientation;
        }

        p->x = (50 - offset * sin(M_PI * mdir / 180.)) * width / 100;
        p->y = (50 + offset * cos(M_PI * mdir / 180.)) * height / 100;
        if (dir)
            *dir = m_config.orientation;
    }

    if (padding)
    {
        p->x += padding->left;
        p->y += padding->top;
    }

    dbg(lvl_debug, "x=%d y=%d, offset=%f", p->x, p->y, offset);

    return 1;
}

coord Navit::get_vehicle_cursor_coords(Vehicle *vehicle)
{
    coord cursorPosition;
    coord_geo position = getPosition(vehicle);
    transform_from_geo(transform_get_projection(m_trans), &position, &cursorPosition);
    return cursorPosition;
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
    if (!m_plugins.getVehicle())
    {
        return;
    }
    if (!isPositionValid(m_plugins.getVehicle()))
        return;

    coord cursorPosition = get_vehicle_cursor_coords(m_plugins.getVehicle());
    get_cursor_pnt(&pn, keep_orientation, &dir);
    transform_set_yaw(m_trans, dir);
    set_center_coord_screen(&cursorPosition, &pn, 0);
    if (autozoom_)
    {
        autozoom(&cursorPosition, getSpeed(m_plugins.getVehicle()));
    }
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
    m_graphics.draw_drag(destination);
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

    dbg(lvl_debug, "enter, this_=%p, attr=%p (%s), init=%d", this, attr, attr_to_name(attr->type), init);

    switch (attr->type)
    {
    case attr_autozoom:
        attr_updated = (m_config.autozoom_secs != attr->u.num);
        m_config.autozoom_secs = attr->u.num;
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
        attr_updated = (m_config.drag_bitmap != !!attr->u.num);
        m_config.drag_bitmap = !!attr->u.num;
        break;
    case attr_flags:
        attr_updated = (m_config.flags != attr->u.num);
        m_config.flags = attr->u.num;
        break;
    case attr_flags_graphics:
        attr_updated = (m_graphics_flags != attr->u.num);
        m_graphics_flags = attr->u.num;
        break;
    case attr_follow:
        m_plugins.getVehicle()->setFollowCursor(attr->u.num);
        return 0;
    case attr_default_layout:
        if (!attr->u.str)
            return 0;
        m_config.default_layout = QString(attr->u.str);
        attr_updated = 1;
        break;
    case attr_layout:
        qWarning() << "Trying to set layout";
        break;
    case attr_layout_name:
        qWarning() << "Trying to set layout by name!";
        return 0;
    case attr_map_border:
        if (m_config.border != attr->u.num)
        {
            m_config.border = attr->u.num;
            attr_updated = 1;
        }
        break;
    case attr_orientation:
        orient_old = m_config.orientation;
        m_config.orientation = attr->u.num;
        if (!init)
        {
            if (m_config.orientation != -1)
            {
                dir = m_config.orientation;
            }
            else
            {
                if (m_plugins.getVehicle())
                {
                    dir = getDirection(m_plugins.getVehicle());
                }
            }
            transform_set_yaw(m_trans, dir);
            if (orient_old != m_config.orientation)
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
        attr_updated = (m_config.pitch != attr->u.num);
        m_config.pitch = attr->u.num;
        transform_set_pitch(m_trans, round(m_config.pitch * sqrt(240 * 320) / sqrt(m_w * m_h))); // Pitch corrected for window resolution
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
        attr_updated = (m_config.radius != attr->u.num);
        m_config.radius = attr->u.num;
        break;
    case attr_recent_dest:
        attr_updated = (m_config.recentdest_count != attr->u.num);
        m_config.recentdest_count = attr->u.num;
        break;
    case attr_speech:
        if (m_speech && m_speech != attr->u.speech)
        {
            attr_updated = 1;
            m_speech = attr->u.speech;
        }
        break;
    case attr_timeout:
        attr_updated = (m_config.center_timeout != attr->u.num);
        m_config.center_timeout = attr->u.num;
        break;
    case attr_tracking:
        attr_updated = (m_config.tracking_flag != !!attr->u.num);
        m_config.tracking_flag = !!attr->u.num;
        break;
    case attr_transformation:
        m_trans = attr->u.transformation;
        break;
    case attr_use_mousewheel:
        attr_updated = (m_config.use_mousewheel != !!attr->u.num);
        m_config.use_mousewheel = !!attr->u.num;
        break;
    case attr_vehicle:
        qDebug() << "Can't set vehicle";
        return 0;
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
        attr_updated = (attr->u.num != m_config.zoom_min);
        m_config.zoom_min = attr->u.num;
        break;
    case attr_zoom_max:
        attr_updated = (attr->u.num != m_config.zoom_max);
        m_config.zoom_max = attr->u.num;
        break;
    case attr_message:
        add_message(attr->u.str);
        break;
    case attr_follow_cursor:
        attr_updated = (m_config.follow_cursor != !!attr->u.num);
        m_config.follow_cursor = !!attr->u.num;
        break;
    case attr_imperial:
        attr_updated = (m_config.imperial != attr->u.num);
        m_config.imperial = attr->u.num;
        break;
    case attr_waypoints_flag:
        attr_updated = (m_config.waypoints_flag != !!attr->u.num);
        m_config.waypoints_flag = !!attr->u.num;
        break;
    case attr_tunnel_nightlayout:
        attr_updated = (m_config.tunnel_nightlayout != !!attr->u.num);
        m_config.tunnel_nightlayout = !!attr->u.num;
        break;
    case attr_layout_daynightauto:
        attr_updated = (m_config.auto_switch != !!attr->u.num);
        m_config.auto_switch = !!attr->u.num;
        break;
    case attr_sunrise_degrees:
        attr_updated = (m_config.sunrise_degrees != attr->u.num);
        m_config.sunrise_degrees = attr->u.num;
        break;
    default:
        qCritical() << "Calling generic attribute setter: " << attr_to_name(attr->type) << attr->type;
        dbg(lvl_debug, "calling generic setter method for attribute type %s", attr_to_name(attr->type));
        // return navit_object_set_attr(&m_navit_object, attr);
        return 1;
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
        attr->u.num = m_config.imperial;
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
        if (!m_plugins.getVehicle())
            return 0;
        attr->u.num = m_plugins.getVehicle()->getFollowCursor();
        break;
    case attr_former_destination_map:
        attr->u.map = m_former_destination;
        break;
    case attr_gui:
        break;
    case attr_layer:
        qWarning() << "Trying to get layers";
        break;
    case attr_layout:
        qWarning() << "Trying to get layouts";
        break;
    case attr_map:
        qWarning() << "Trying to get maps";
        return 0;
        break;
    case attr_mapset:
        if (m_plugins.getMapsets().size() > 0)
        {
            attr->u.mapset = m_plugins.getMapsets().first();
            ret = 1;
        }
        else
        {
            qWarning() << "No mapsets";
            return 0;
        }
        break;
    case attr_navigation:
        attr->u.navigation = m_plugins.getNavigation();
        break;
    case attr_orientation:
        attr->u.num = m_config.orientation;
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
        attr->u.route = m_plugins.getRoute();
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
        attr->u.num = m_config.center_timeout;
        break;
    case attr_tracking:
        attr->u.num = m_config.tracking_flag;
        break;
    case attr_trackingo:
        attr->u.tracking = m_plugins.getTracking();
        break;
    case attr_transformation:
        attr->u.transformation = m_trans;
        break;
    case attr_vehicle:
        qDebug() << "Cant get vehicle";
        return 0;
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
        attr->u.num = m_config.follow_cursor;
        break;
    case attr_waypoints_flag:
        attr->u.num = m_config.waypoints_flag;
        break;
    case attr_tunnel_nightlayout:
        attr->u.num = m_config.tunnel_nightlayout;
        break;
    case attr_layout_daynightauto:
        attr->u.num = m_config.auto_switch;
        break;
    case attr_sunrise_degrees:
        attr->u.num = m_config.sunrise_degrees;
        break;
    default:
        qCritical() << "Calling generic attribute setter: " << attr_to_name(attr->type) << attr->type;
        dbg(lvl_debug, "calling generic getter method for attribute type %s", attr_to_name(type));
        // return navit_object_get_attr(&m_navit_object, type, attr, iter);
        return 1;
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
Layout *Navit::get_layout_by_name(const QString &name)
{
    Layout *result = nullptr;

    if (!name.isEmpty())
        return nullptr;

    for (Layout *layout : m_layouts)
    {
        if (layout->getName() == name)
        {
            result = layout;
        }
    }
    return result;
}

Layout *Navit::getCurrentLayout()
{
    return m_layout_current;
}

const QVector<Layout *> &Navit::getLayouts()
{
    return m_layouts;
}

/**
 * @brief Set the current layout
 *
 * @param this_ The navit instance
 * @param layout The layout to set as default (if NULL, we will set the current layout according to the default name stored in m_default_layout_name
 *
 * @note If argument @p layout is NULL and the default layout name in the config file does not exist or has not been provided in the config file, the default layout is unchanged
 */
void Navit::update_current_layout(Layout *layout)
{
    Layout *default_layout = NULL;

    if (layout)
    {
        m_layout_current = layout;
    }
    else
    {
        if (!m_config.default_layout.isEmpty())
        { /* If a default layout name was provided */
            default_layout = get_layout_by_name(m_config.default_layout);
            if (default_layout)
            {
                dbg(lvl_debug, "Found the config-specified default layout '%s'", m_config.default_layout.toLocal8Bit().data());
                m_layout_current = default_layout;
                return;
            }
            else
            {
                dbg(lvl_warning, "No definition exists in config for specified default layout '%s'", m_config.default_layout.toLocal8Bit().data());
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
        char header[] = "type=track_tracked\n";
        if (m_textfile_debug_log)
            return 0;
        log_set_header(log, header, strlen(header));
        m_textfile_debug_log = log;
        return 1;
    }
    return 0;
}

int Navit::add_layout(Layout *layout)
{
    int is_default = 0;
    int is_active = 0;
    layout->setParent(this);

    m_layouts.append(layout);
    /** check if we want to immediately activate this layout.
     * Unfortunately we have concurring conditions about when to activate
     * a layout:
     * - A layout could bear the "active" property
     * - A layout's name could match m_default_layout_name
     * This cannot be fully resolved, as we cannot predict the future, so
     * lets set the last parsed layout active, which either matches default_layout_name or
     * bears the "active" tag, or is the first layout ever parsed.
     */
    if ((!layout->getName().isEmpty()) && (!m_config.default_layout.isEmpty()))
    {
        if (layout->getName() == m_config.default_layout)
            is_default = 1;
    }

    is_active = layout->getActive();
    qDebug("add layout '%s' is_default %d, is_active %d", layout->getName().toLocal8Bit().data(), is_default, is_active);
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
    case attr_layout:
        qWarning() << "Can't add layout";
        break;
    case attr_route:
        qWarning() << "Can't add route";
        break;
    case attr_mapset:
        qWarning() << "Can't add mapset";
        break;
    case attr_navigation:
        qWarning() << "Can't add navigation";
        break;
    case attr_recent_dest:
        m_config.recentdest_count = attr->u.num;
        break;
    case attr_speech:
        m_speech = attr->u.speech;
        break;
    case attr_trackingo:
        qWarning() << "Can't add tracking";
        break;
    case attr_vehicle:
        qWarning() << "Can't add vehicle";
        break;
    case attr_vehicleprofile:
        m_vehicleprofiles = g_list_append(m_vehicleprofiles, attr->u.vehicleprofile);
        break;
    case attr_autozoom_min:
        m_config.autozoom_min = attr->u.num;
        break;
    case attr_autozoom_max:
        m_config.autozoom_max = attr->u.num;
        break;
    case attr_layer:
        qWarning() << "Can't add layer";
        break;
    case attr_script:
    case attr_traffic:
        break;
    default:
        return 0;
    }
    if (ret)
    {
        qCritical() << "Error adding attrs";
        // m_navit_object.attrs = attr_generic_add_attr(m_navit_object.attrs, attr);
        return 1;
    }
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
        qCritical() << "Error adding attrs";
        // m_navit_object.attrs = attr_generic_remove_attr(m_navit_object.attrs, attr);
        return 1;
    default:
        return 0;
    }
    return ret;
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

void Navit::draw_vehicle(Vehicle *vehicle, struct point *pnt)
{
    struct point cursor_pnt;
    enum projection pro;

    coord cursorCoord = get_vehicle_cursor_coords(vehicle);

    if (m_blocked || coord_not_set(cursorCoord))
        return;
    if (pnt)
        cursor_pnt = *pnt;
    else
    {
        LayoutCoord c(&cursorCoord);
        LayoutCoord pnt;
        pro = transform_get_projection(m_trans_cursor);
        if (!pro)
            return;
        transform_point(m_trans_cursor, pro, &c, &pnt);
        cursor_pnt.x = pnt.getX();
        cursor_pnt.y = pnt.getY();
    }
    vehicle->draw(&m_graphics, &cursor_pnt, getDirection(vehicle) - transform_get_yaw(m_trans_cursor), getSpeed(vehicle));
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
 * <li>Invoking callbacks for {@code navit}'s {@code attr_position_coord_geo}
 * attributes</li>
 * <li>Triggering an update of the vehicle's position on the map and, if needed, an update of the
 * visible map area ad orientation</li>
 * <li>Logging a new track point, if enabled</li>
 * <li>Updating the position on the route</li>
 * <li>Stopping navigation if the destination has been reached</li>
 * </ul>
 *
 * @param this_ The navit object
 */
void Navit::onVehiclePositionUpdated(const coord_geo &position)
{
    struct pcoord cursor_pc;
    struct point cursor_pnt, *pnt = &cursor_pnt;
    struct tracking *tracking = NULL;
    struct pcoord *pc;

    enum projection pro = transform_get_projection(m_trans_cursor);
    int count;

    char *destination_file;
    char *description;

    Vehicle *vehicle = qobject_cast<Vehicle *>(sender());
    assert(vehicle);

    // profile(0, NULL);
    if (m_ready == 3)
        layout_switch();
    if (m_plugins.getVehicle() == vehicle && m_config.tracking_flag)
        tracking = m_plugins.getTracking();
    if (tracking)
    {
        tracking_update(tracking, vehicle, m_vehicleprofile, pro);
    }

    if (!isPositionValid(vehicle))
        return;

    if (vehicle != m_plugins.getVehicle())
    {
        if (m_ready == 3)
            draw_vehicle(vehicle, NULL);
        // profile(0, "return 3\n");
        return;
    }
    coord cursorCoord = get_vehicle_cursor_coords(vehicle);

    cursor_pc.x = cursorCoord.x;
    cursor_pc.y = cursorCoord.y;
    cursor_pc.pro = pro;
    if (m_plugins.getRoute())
    {
        if (tracking)
            route_set_position_from_tracking(m_plugins.getRoute(), tracking, pro);
        else
            route_set_position(m_plugins.getRoute(), &cursor_pc);
    }
    // callback_list_call_attr_0(m_attr_cbl, attr_position);
    textfile_debug_log("type=trackpoint_tracked");
    if (m_ready == 3)
    {
        LayoutCoord c(&cursorCoord);
        LayoutCoord p(&cursor_pnt);
        transform_point(m_trans_cursor, pro, &c, &p);
        if (m_config.follow_cursor && vehicle->getFollowCursor() <= vehicle->getFollow() &&
            (vehicle->getFollowCursor() == 1 || !transform_within_border(m_trans_cursor, &cursor_pnt, m_config.border)))
            set_center_cursor_draw();
        else
            draw_vehicle(vehicle, pnt);

        if (vehicle->getFollowCursor() > 1)
            vehicle->setFollowCursor(vehicle->getFollowCursor() - 1);
        else
            vehicle->setFollowCursor(vehicle->getFollow());
    }
    emit positionChanged(getPosition(vehicle));

    /* Finally, if we reached our destination, stop navigation. */
    if (m_plugins.getRoute())
    {
        switch (route_destination_reached(m_plugins.getRoute()))
        {
        case 1:
            description = route_get_destination_description(m_plugins.getRoute(), 0);
            route_remove_waypoint(m_plugins.getRoute());
            count = route_get_destination_count(m_plugins.getRoute());
            pc = (struct pcoord *)g_alloca(sizeof(*pc) * count);
            route_get_destinations(m_plugins.getRoute(), pc, count);
            destination_file = bookmarks_get_destination_file(TRUE);
            bookmarks_append_destinations(m_former_destination, destination_file, pc, count, type_former_itinerary_part,
                                          description, m_config.recentdest_count);
            g_free(destination_file);
            g_free(description);
            break;
        case 2:
            destination_file = bookmarks_get_destination_file(TRUE);
            bookmarks_append_destinations(m_former_destination, destination_file, NULL, 0, type_former_itinerary_part, NULL,
                                          m_config.recentdest_count);
            set_destination(NULL, NULL, 0);
            g_free(destination_file);
            break;
        }
    }
    // profile(0, "return 5\n");
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
    if (m_plugins.getRoute())
    {
        route_set_position(m_plugins.getRoute(), c);
    }
    if (m_ready == 3)
        draw();
}

int Navit::set_vehicleprofile(struct vehicleprofile *vp)
{
    if (m_vehicleprofile == vp)
        return 0;
    m_vehicleprofile = vp;
    if (m_plugins.getRoute())
        route_set_profile(m_plugins.getRoute(), m_vehicleprofile);
    return 1;
}

int Navit::set_vehicleprofile_name(const QString &name)
{
    struct attr attr;
    GList *l;
    l = m_vehicleprofiles;
    while (l)
    {
        if (vehicleprofile_get_attr((vehicleprofile *)l->data, attr_name, &attr, NULL))
        {
            if (attr.u.str != name)
            {
                set_vehicleprofile((vehicleprofile *)l->data);
                return 1;
            }
        }
        l = g_list_next(l);
    }
    return 0;
}

void Navit::set_vehicle(Vehicle *vehicle)
{
    m_plugins.setVehicle(vehicle);

    if (vehicle != nullptr)
    {
        if (set_vehicleprofile_name(vehicle->getProfileName()))
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
                if (m_plugins.getRoute())
                    route_set_profile(m_plugins.getRoute(), m_vehicleprofile);
            }
        }
    }
    else
    {
        if (m_plugins.getRoute())
            route_set_profile(m_plugins.getRoute(), m_vehicleprofile);
    }
}

/**
 * @brief Registers a new vehicle.
 *
 * @param this_ The navit instance
 * @param v The vehicle to register
 * @return True for success
 */
int Navit::add_vehicle(Vehicle *vehicle)
{
    m_plugins.getVehicles().append(vehicle);

    vehicle->connect(vehicle, &Vehicle::positionValidChanged, this, &Navit::positionValidChanged);
    vehicle->connect(vehicle, &Vehicle::positionChanged, this, &Navit::onVehiclePositionUpdated);
    vehicle->connect(vehicle, &Vehicle::fixTypeChanged, this, &Navit::fixTypeChanged);
    vehicle->connect(vehicle, &Vehicle::hdopChanged, this, &Navit::hdopChanged);
    vehicle->connect(vehicle, &Vehicle::satellitesChanged, this, &Navit::satellitesChanged);
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
    return m_plugins.getRoute();
}

struct navigation *Navit::get_navigation()
{
    return m_plugins.getNavigation();
}

void Navit::layout_switch()
{
    int currTs = 0;
    double trise, tset;
    int year, month, day;
    int after_sunrise = FALSE;
    int after_sunset = FALSE;
    int tunnel = tracking_get_current_tunnel(m_plugins.getTracking());

    if (!m_plugins.getVehicle())
        return;

    if ((!m_layout_current->getDaylayout().isEmpty()) || (!m_layout_current->getNightlayout().isEmpty()))
    {
        // Ok, we know that we have profile to switch

        // Check that we aren't calculating too fast
        currTs = iso8601_to_secs(m_plugins.getVehicle()->getIso8601Time().toLocal8Bit().data());
        dbg(lvl_debug, "currTs: %02u:%02u", currTs % 86400 / 3600, ((currTs % 86400) % 3600) / 60);
        dbg(lvl_debug, "prevTs: %02u:%02u", m_prevTs % 86400 / 3600, ((m_prevTs % 86400) % 3600) / 60);

        if (m_config.auto_switch == FALSE)
            return;

        if (m_config.tunnel_nightlayout)
        {
            if (tunnel)
            {
                // store the current layout name
                if (m_layout_before_tunnel != "")
                    m_layout_before_tunnel = m_layout_current->getName();

                // We are in a tunnel and if we have a nightlayout -> switch to nightlayout

                set_layout_by_name(m_layout_current->getNightlayout());
                dbg(lvl_debug, "tunnel -> nightlayout");
                return;
            }
            else
            {
                if (m_layout_current->getDaylayout() != m_layout_before_tunnel)
                {
                    // restore previous layout
                    set_layout_by_name(m_layout_current->getDaylayout());
                    dbg(lvl_debug, "tunnel end -> daylayout");
                }

                // We were in nightlayout before the tunnel, keep it
                m_layout_before_tunnel = "";
            }
        }

        if (currTs - (m_prevTs) < 60)
        {
            // We've have to wait a little
            return;
        }

        if (sscanf(m_plugins.getVehicle()->getIso8601Time().toLocal8Bit().data(), "%d-%02d-%02dT", &year, &month, &day) != 3)
            return;
        if (!isPositionValid(m_plugins.getVehicle()))
        {
            return; // No valid fix yet
        }

        // if (vehicle_get_attr(m_plugins.getVehicle()->vehicle, attr_position_coord_geo, &geo_attr, NULL) != 1)
        // {
        //     // No position - no sun
        //     return;
        // }
        coord_geo position = m_plugins.getVehicle()->getPosition();

        // We calculate sunrise anyway, cause it is needed both for day and for night
        if (__sunriset__(year, month, day, position.lng, position.lat, m_config.sunrise_degrees, 1, &trise,
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

        if (after_sunrise && !after_sunset && (!m_layout_current->getDaylayout().isEmpty()))
        {
            set_layout_by_name(m_layout_current->getDaylayout());
            dbg(lvl_debug, "layout set to day");
        }
        else if (after_sunset && (!m_layout_current->getNightlayout().isEmpty()))
        {
            set_layout_by_name(m_layout_current->getNightlayout());
            dbg(lvl_debug, "layout set to night");
        }
        m_prevTs = currTs;
    }
}

int Navit::set_vehicle_by_name(const QString &name)
{
    for (Vehicle *vehicle : m_plugins.getVehicles())
    {
        if (vehicle->getName() == name)
        {
            set_vehicle(vehicle);
        }
    }
    return 0;
}

int Navit::set_layout_by_name(const QString &name)
{
    if (name.isEmpty())
    {
        qWarning() << "Empty layout name!";
        return -1;
    }
    for (Layout *layout : m_layouts)
    {
        if (layout->getName() == name)
        {
            if (m_layout_current != layout)
            {
                update_current_layout(layout);
                m_graphics.font_destroy_all();
                set_cursors();
                if (m_ready == 3)
                    draw();
            }
        }
    }
    return 0;
}
bool Navit::isPositionValid(Vehicle *vehicle)
{
    struct tracking *tracking;
    if (m_plugins.getVehicle() == vehicle && m_config.tracking_flag)
        tracking = m_plugins.getTracking();
    if (tracking)
    {
        struct attr attr;
        tracking_get_attr(tracking, attr_position_valid, &attr, nullptr);
        return attr.u.num == attr_position_valid_valid;
    }
    else
    {
        return vehicle->isPositionValid();
    }
}
coord_geo Navit::getPosition(Vehicle *vehicle)
{
    struct tracking *tracking;
    if (m_plugins.getVehicle() == vehicle && m_config.tracking_flag)
        tracking = m_plugins.getTracking();
    if (tracking)
    {
        struct attr attr;
        tracking_get_attr(tracking, attr_position_coord_geo, &attr, nullptr);
        coord_geo ret = *attr.u.coord_geo;
        return ret;
    }
    else
    {
        return vehicle->getPosition();
    }
}
double Navit::getSpeed(Vehicle *vehicle)
{
    struct tracking *tracking;
    if (m_plugins.getVehicle() == vehicle && m_config.tracking_flag)
        tracking = m_plugins.getTracking();
    if (tracking)
    {
        struct attr attr;
        tracking_get_attr(tracking, attr_position_speed, &attr, nullptr);
        double ret = *attr.u.numd;
        return ret;
    }
    else
    {
        return vehicle->getSpeed();
    }
}
double Navit::getDirection(Vehicle *vehicle)
{
    struct tracking *tracking;
    if (m_plugins.getVehicle() == vehicle && m_config.tracking_flag)
        tracking = m_plugins.getTracking();
    if (tracking)
    {
        struct attr attr;
        tracking_get_attr(tracking, attr_position_direction, &attr, nullptr);
        double ret = *attr.u.numd;
        return ret;
    }
    else
    {
        return vehicle->getDirection();
    }
}

NavitVehicleInterface *Navit::getVehicle()
{
    return m_plugins.getVehicle();
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

Navit::~Navit()
{
    dbg(lvl_debug, "enter %p", this);
    GList *mapsets;
    struct map *map;
    struct attr attr;
    m_displaylist.draw_cancel();

    for (mapset *mapset : m_plugins.getMapsets())
    {
        GList *maps = NULL;
        struct mapset_handle *msh;
        msh = mapset_open(mapset);
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
            mapset_remove_attr(mapset, &attr);
            attr_free_content(&attr);
            maps = g_list_next(maps);
        }
        if (maps)
            g_list_free(maps);
        mapsets = g_list_next(mapsets);
    }

    callback_list_call_attr_1(m_attr_cbl, attr_destroy, this);
    // attr_list_free(m_navit_object.attrs);

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
    if (m_plugins.getRoute())
        route_destroy(m_plugins.getRoute());

    map_destroy(m_former_destination);

    // delete m_displaylist;
}

/** @} */

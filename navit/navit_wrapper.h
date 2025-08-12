/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef NAVIT_NAVITWRAPPER_H
#define NAVIT_NAVITWRAPPER_H

// defined in glib.h.
#ifndef __G_LIST_H__
struct _GList;
typedef struct _GList GList;
#endif

#include "attr.h"
#include "coord.h"
#include "point.h"
#include "includes/common.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void *NavitHandle;
    typedef void *GraphicsHandle;
    typedef void *GraphicsGCHandle;
    void navit_add_mapset(NavitHandle navit, struct mapset *ms);
    struct mapset *navit_get_mapset(NavitHandle navit);
    struct map *navit_get_search_results_map(NavitHandle navit);
    int navit_populate_search_results_map(NavitHandle navit, GList *search_results, struct coord_rect *r);
    struct tracking *navit_get_tracking(NavitHandle navit);
    char *navit_get_user_data_directory(int create);
    void navit_draw_async(NavitHandle navit, int async);
    void navit_draw(NavitHandle navit);
    int navit_get_ready(NavitHandle navit);
    void navit_draw_displaylist(NavitHandle navit);
    void navit_handle_resize(NavitHandle navit, int w, int h);
    int navit_get_width(NavitHandle navit);
    int navit_get_height(NavitHandle navit);
    void navit_set_timeout(NavitHandle navit);
    void navit_handle_motion(NavitHandle navit, struct point *p);
    void navit_zoom_in(NavitHandle navit, int factor, struct point *p);
    void navit_zoom_out(NavitHandle navit, int factor, struct point *p);
    void navit_zoom_in_cursor(NavitHandle navit, int factor);
    void navit_zoom_out_cursor(NavitHandle navit, int factor);
    NavitHandle navit_new(struct attr *parent, struct attr **attrs);
    void navit_add_message(NavitHandle navit, const char *message);
    struct message *navit_get_messages(NavitHandle navit);
    struct vehicleprofile *navit_get_vehicleprofile(NavitHandle navit);
    GList *navit_get_vehicleprofiles(NavitHandle navit);
    void navit_set_destination(NavitHandle navit, struct pcoord *c, const char *description, int async);
    void navit_set_destinations(NavitHandle navit, struct pcoord *c, int count, const char *description, int async);
    void navit_add_destination_description(NavitHandle navit, struct pcoord *c, const char *description);
    int navit_get_destinations(NavitHandle navit, struct pcoord *pc, int count);
    int navit_get_destination_count(NavitHandle navit);
    char *navit_get_destination_description(NavitHandle navit, int n);
    void navit_remove_nth_waypoint(NavitHandle navit, int n);
    void navit_remove_waypoint(NavitHandle navit);
    int navit_check_route(NavitHandle navit);
    void navit_say(NavitHandle navit, const char *text);
    void navit_speak(NavitHandle navit);
    void navit_window_roadbook_destroy(NavitHandle navit);
    void navit_window_roadbook_new(NavitHandle navit);
    int navit_init(NavitHandle navit);
    void navit_zoom_to_rect(NavitHandle navit, struct coord_rect *r);
    void navit_zoom_to_route(NavitHandle navit, int orientation);
    void navit_set_center(NavitHandle navit, struct pcoord *center, int set_timeout);
    void navit_set_center_cursor(NavitHandle navit, int autozoom, int keep_orientation);
    void navit_set_center_screen(NavitHandle navit, struct point *p, int set_timeout);
    int navit_set_attr(NavitHandle navit, struct attr *attr);
    int navit_get_attr(NavitHandle navit, enum attr_type type, struct attr *attr, struct attr_iter *iter);
    int navit_add_attr(NavitHandle navit, struct attr *attr);
    int navit_remove_attr(NavitHandle navit, struct attr *attr);
    struct attr_iter *navit_attr_iter_new(void);
    void navit_attr_iter_destroy(struct attr_iter *iter);
    void navit_add_callback(NavitHandle navit, struct callback *cb);
    void navit_remove_callback(NavitHandle navit, struct callback *cb);
    void navit_set_position(NavitHandle navit, struct pcoord *c);
    struct gui *navit_get_gui(NavitHandle navit);
    struct transformation *navit_get_trans(NavitHandle navit);
    struct route *navit_get_route(NavitHandle navit);
    struct navigation *navit_get_navigation(NavitHandle navit);
    void navit_layout_switch(NavitHandle navit);
    int navit_set_vehicle_by_name(NavitHandle navit, const char *name);
    int navit_set_vehicleprofile_name(NavitHandle navit, char *name);
    int navit_set_layout_by_name(NavitHandle navit, const char *name);
    int navit_block(NavitHandle navit, int block);
    int navit_get_blocked(NavitHandle navit);
    void navit_destroy(NavitHandle navit);

    GraphicsGCHandle graphics_gc_new(GraphicsHandle graphics);
    void graphics_gc_set_foreground(GraphicsGCHandle *gc, struct NavitColor *c);
    void graphics_gc_destroy(GraphicsGCHandle *gc);

    GraphicsHandle graphics_overlay_new(GraphicsHandle parent, struct point *p, int w, int h, int wraparound);
    void graphics_free(GraphicsHandle graphics);
    void graphics_overlay_disable(GraphicsHandle graphics, int disable);
    void graphics_init(GraphicsHandle graphics);
    void graphics_background_gc(GraphicsHandle graphics, GraphicsGCHandle *gc);
    void graphics_overlay_resize(GraphicsHandle graphics, struct point *p, int w, int h, int wraparound);
    void graphics_draw_mode(GraphicsHandle graphics, enum draw_mode_num mode);
    void graphics_draw_rectangle(GraphicsHandle graphics, GraphicsGCHandle *gc, struct point *p, int w, int h);
    void graphics_draw_itemgra(GraphicsHandle graphics, struct itemgra *itm, struct transformation *t, char *label);
    int graphics_draw_drag(GraphicsHandle graphics, struct point *p);
    char *graphics_icon_path(const char *icon);
#ifdef __cplusplus
}
#endif
/* end of prototypes */

#endif

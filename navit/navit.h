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

#ifndef NAVIT_NAVIT_H
#define NAVIT_NAVIT_H

#include "NavitInterfaces.h"
#include "coord.h"
#include "point.h"
#include "attr.h"
#include "graphics.h"
#include "graphics_displaylist.h"

// defined in glib.h.
#ifndef __G_LIST_H__
struct _GList;
typedef struct _GList GList;
#endif

#ifdef __cplusplus
extern "C"
{
#endif
    /* prototypes */
    enum attr_type;
    struct attr;
    struct attr_iter;
    struct callback;
    struct coord_rect;
    struct gui;
    struct layout;
    struct mapset;
    struct message;
    struct navigation;
    struct route;
    struct tracking;
    struct transformation;
    struct vehicleprofile;
    struct command_table;
    struct item;
    struct object_func;

#ifdef __cplusplus
}
#endif

class Navit : public NavitInterface
{
public:
    Navit(struct attr *parent, struct attr **attrs);
    void add_mapset(struct mapset *ms);
    struct mapset *get_mapset() override;
    struct map *get_search_results_map();
    int populate_search_results_map(GList *search_results, struct coord_rect *r) override;
    struct tracking *get_tracking() override;
    void draw_async(int async);
    void draw() override;
    int get_ready();
    void draw_displaylist();
    void handle_resize(int w, int h);
    int get_width() override;
    int get_height() override;
    void ignore_graphics_events(int ignore);
    void set_timeout();
    void handle_motion(struct point *p);
    void zoom_level(int level, struct point *p) override;
    void zoom_in(int factor, struct point *p) override;
    void zoom_out(int factor, struct point *p) override;
    void zoom_in_cursor(int factor);
    void zoom_out_cursor(int factor);
    void add_message(const char *message);
    struct message *get_messages();
    struct vehicleprofile *get_vehicleprofile() override;
    GList *get_vehicleprofiles();
    void set_destination(struct pcoord *c, const char *description, int async) override;
    void set_destinations(struct pcoord *c, int count, const char *description, int async) override;
    void add_destination_description(struct pcoord *c, const char *description) override;
    int get_destinations(struct pcoord *pc, int count);
    int get_destination_count();
    char *get_destination_description(int n);
    void remove_nth_waypoint(int n);
    void remove_waypoint();
    char *get_coord_description(struct pcoord *c);
    int check_route();
    struct map *read_former_destinations_from_file(void);
    void textfile_debug_log(const char *fmt, ...);
    void textfile_debug_log_at(struct pcoord *pc, const char *fmt, ...);
    void say(const char *text);
    void speak();
    void window_roadbook_destroy();
    void window_roadbook_new();
    int init();
    void zoom_to_rect(struct coord_rect *r);
    void zoom_to_route(int orientation) override;
    void set_center(struct pcoord *center, int set_timeout_) override;
    void set_center_cursor(int autozoom_, int keep_orientation);
    void set_center_screen(struct point *p, int set_timeout_);
    void drag_map(struct point *origin, struct point *destination) override;
    void set_center_cursor_draw() override;
    int set_attr(struct attr *attr) override;
    int get_attr(enum attr_type type, struct attr *attr, struct attr_iter *iter) override;
    struct layout *get_layout_by_name(const char *layout_name);
    void update_current_layout(struct layout *layout);
    int add_attr(struct attr *attr);
    int remove_attr(struct attr *attr);
    void add_callback(struct callback *cb) override;
    void remove_callback(struct callback *cb);
    void set_position(struct pcoord *c) override;
    struct gui *get_gui();
    struct transformation *get_trans() override;
    struct route *get_route() override;
    struct navigation *get_navigation() override;
    void layout_switch();
    int set_vehicle_by_name(const char *name);
    int set_vehicleprofile_name(char *name) override;
    int set_layout_by_name(const char *name) override;
    int block(int block);
    int get_blocked();
    void destroy();
    void motion_timeout();
    void predraw();
    void window_roadbook_update();
    void map_progress();
    void redraw_route(struct route *route, struct attr *attr);
    void vehicle_update_status(struct navit_vehicle *nv, enum attr_type type);
    void vehicle_update_position(struct navit_vehicle *nv);

    static char *get_user_data_directory(int create);

    int m_ignore_graphics_events;

private:
    Graphics m_graphics;
    GraphicsDisplayList m_displaylist;

    object_func *m_func;
    int m_refcount;
    struct attr **m_attrs;
    struct attr m_self;

    GList *m_mapsets;
    GList *m_layouts;
    char *m_default_layout_name;     /*!< The default layout indicated by the config file (if any) */
    struct layout *m_layout_current; /*!< The current layout theme used to display the map */
    struct action *m_action;
    struct transformation *m_trans, *m_trans_cursor;
    struct compass *m_compass;
    struct route *m_route;
    struct navigation *m_navigation;
    struct speech *m_speech;
    struct tracking *m_tracking;
    int m_ready;
    struct window *m_win;
    int m_tracking_flag;
    int m_orientation;
    int m_recentdest_count;
    GList *m_vehicles;
    GList *m_windows_items;
    struct navit_vehicle *m_vehicle;
    struct callback_list *m_attr_cbl;
    struct callback *m_nav_speech_cb, *m_roadbook_callback, *m_route_cb;
    struct datawindow *m_roadbook_window;
    struct map *m_former_destination;
    struct point m_pressed, m_last, m_current;
    int m_center_timeout;
    int m_autozoom_secs;
    int m_autozoom_min;
    int m_autozoom_max;
    int m_autozoom_active;
    int m_autozoom_paused;
    struct event_timeout *m_button_timeout, *m_motion_timeout;
    struct callback *m_motion_timeout_callback;
    struct log *m_textfile_debug_log;
    struct pcoord m_destination;
    int m_destination_valid;
    int m_blocked; /**< Whether draw operations are currently blocked. This can be a combination of the
                      following flags:
                      1: draw operations are blocked
                      2: draw operations are pending, requiring a redraw once draw operations are unblocked */
    int m_w, m_h;
    int m_drag_bitmap;
    int m_use_mousewheel;
    struct messagelist *m_messages;
    struct callback *m_resize_callback, *m_motion_callback, *m_predraw_callback;
    struct vehicleprofile *m_vehicleprofile;
    GList *m_vehicleprofiles;
    int m_pitch;
    int m_follow_cursor;
    int m_prevTs;
    int m_graphics_flags;
    int m_zoom_min, m_zoom_max;
    int m_radius;
    struct bookmarks *m_bookmarks;
    int m_flags;
    /* 1=No graphics ok */
    /* 2=No gui ok */
    int m_border;
    int m_imperial;
    int m_waypoints_flag;
    struct coord_geo m_center;
    int m_auto_switch;        /*auto switching between day/night layout enabled ?*/
    int m_tunnel_nightlayout; /* switch to nightlayout if we are in a tunnel? */
    char *m_layout_before_tunnel;
    int m_sunrise_degrees;

    void draw_vehicle(struct navit_vehicle *nv, struct point *pnt);
    int add_vehicle(struct vehicle *v);
    int set_attr_do(struct attr *attr, int init);
    int get_cursor_pnt(struct point *p, int keep_orientation, int *dir);
    void set_cursors();
    void set_vehicle(struct navit_vehicle *nv);
    int set_vehicleprofile(struct vehicleprofile *vp);

    int restrict_to_range(int value, int min, int max);
    void restrict_map_center_to_world_boundingbox(struct transformation *tr, struct coord *new_center);
    void update_transformation(struct transformation *tr, struct point *old, struct point *new_);

    void scale(long scale, struct point *p, int draw_);
    void autozoom(struct coord *center, int speed);
    int set_graphics();
    void projection_set(enum projection pro, int draw_);
    void mark_navigation_stopped(char *former_destination_file);
    int former_destinations_active();
    void add_former_destinations_from_file();
    void set_center_coord_screen(struct coord *c, struct point *p, int set_timeout_);
    int add_layout(struct layout *layout);
    int add_log(struct log *log);
};
/* end of prototypes */

#endif

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

#include <string>

#include <QObject>

#include "NavitInterfaces.h"
#include "graphics.h"
#include "config_loader.h"
#include "plugin_loader.h"
#include "transform_2.h"

extern "C"
{
#include "coord.h"
#include "point.h"
#include "attr.h"
#include "graphics_displaylist.h"
}

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

class Navit : public QObject, public NavitInterface
{
    Q_OBJECT
public:
    Navit(NavitConfig &navitConfig, QObject *parent = nullptr);
    ~Navit();

    void load_dynamic_mapsets();

    // Interfaces
    int set_vehicleprofile_name(const QString &name) override;
    struct vehicleprofile *get_vehicleprofile() override;
    int get_attr(enum attr_type type, struct attr *attr, struct attr_iter *iter) override;
    struct mapset *get_mapset() override;
    struct tracking *get_tracking() override;
    void set_center(struct pcoord *center, int set_timeout_) override;
    int populate_search_results_map(GList *search_results, struct coord_rect *r) override;
    int set_attr(struct attr *attr) override;
    void add_callback(struct callback *cb) override;
    struct navigation *get_navigation() override;
    struct route *get_route() override;
    void zoom_level(int level, struct point *p) override;
    void set_destination(struct pcoord *c, const char *description, int async) override;
    int get_width() override;
    int get_height() override;
    struct transformation *get_trans() override;
    void draw() override;
    void set_position(struct pcoord *c) override;
    int get_destination_count() override;
    int get_destinations(struct pcoord *pc, int count) override;
    void add_destination_description(struct pcoord *c, const char *description) override;
    void set_destinations(struct pcoord *c, int count, const char *description, int async) override;
    void drag_map(struct point *origin, struct point *destination) override;
    void zoom_in(int factor, struct point *p) override;
    void zoom_out(int factor, struct point *p) override;
    void zoom_to_route(int orientation) override;
    void set_center_cursor_draw() override;

    int set_layout_by_name(const QString &name) override;
    Layout *getCurrentLayout() override;
    const QVector<Layout *> &getLayouts() override;
    const QVector<map *> getMaps() override;

    NavitVehicleInterface *getVehicle() override;

    // Other public functions
    struct map *get_search_results_map();
    void draw_async(int async);
    int get_ready();
    void draw_displaylist();
    void handle_resize(int w, int h);
    void set_timeout();
    void handle_motion(struct point *p);
    void zoom_in_cursor(int factor);
    void zoom_out_cursor(int factor);
    void add_message(const char *message);
    struct message *get_messages();
    GList *get_vehicleprofiles();
    char *get_destination_description(int n);
    void remove_nth_waypoint(int n);
    void remove_waypoint();
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
    void set_center_cursor(int autozoom_, int keep_orientation);
    void set_center_screen(struct point *p, int set_timeout_);
    Layout *get_layout_by_name(const QString &name);
    LayoutCursor *get_layout_cursor(const QString &name);
    void update_current_layout(Layout *layout);
    int add_attr(struct attr *attr);
    int remove_attr(struct attr *attr);
    void remove_callback(struct callback *cb);
    struct gui *get_gui();
    void layout_switch();
    int set_vehicle_by_name(const QString &name);
    int block(int block);
    int get_blocked();
    void motion_timeout();
    void predraw();
    void window_roadbook_update();
    void map_progress();
    void redraw_route(struct route *route, struct attr *attr);

    static char *get_user_data_directory(int create);

signals:
    void positionValidChanged(const bool &isValid);
    void positionChanged(const coord_geo &position);
    void fixTypeChanged(double &hdop);
    void hdopChanged(double &hdop);
    void satellitesChanged(int satellites);

private slots:
    void onVehiclePositionUpdated(const coord_geo &position);

private:
    NavitConfig &m_config;
    PluginLoader m_plugins;
    Graphics m_graphics;
    GraphicsDisplayList m_displaylist;
    Layout *m_layout_current = nullptr; /*!< The current layout theme used to display the map */

    QVector<Layout *> m_layouts;
    QList<LayoutLayer *> m_layers;

    struct attr m_self;

    struct action *m_action = nullptr;
    struct transformation *m_trans, *m_trans_cursor = nullptr;
    struct compass *m_compass = nullptr;
    struct speech *m_speech = nullptr;
    struct callback_list *m_attr_cbl = nullptr;
    struct window *m_win = nullptr;
    struct callback *m_nav_speech_cb, *m_roadbook_callback, *m_route_cb = nullptr;
    struct datawindow *m_roadbook_window = nullptr;
    struct messagelist *m_messages = nullptr;
    struct callback *m_resize_callback, *m_motion_callback, *m_predraw_callback = nullptr;
    struct vehicleprofile *m_vehicleprofile = nullptr;
    struct bookmarks *m_bookmarks = nullptr;
    GList *m_vehicleprofiles = nullptr;
    GList *m_windows_items = nullptr;

    int m_ready;
    struct map *m_former_destination;
    struct point m_pressed, m_last, m_current;

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
    int m_prevTs;
    int m_graphics_flags;

    int m_autozoom_active;
    int m_autozoom_paused;

    struct coord_geo m_center;
    QString m_layout_before_tunnel;

    int add_layout(Layout *layout);
    int add_log(struct log *log);

    int set_attr_do(struct attr *attr, int init);
    int get_cursor_pnt(struct point *p, int keep_orientation, int *dir);
    void set_cursors();
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

    void draw_vehicle(Vehicle *nv, point *pnt);
    int add_vehicle(Vehicle *v);
    void set_vehicle(Vehicle *nv);

    coord get_vehicle_cursor_coords(Vehicle *vehicle);
    void get_tracking_attr(attr *_attr, const enum attr_type &type);
    bool isPositionValid(Vehicle *vehicle);
    coord_geo getPosition(Vehicle *vehicle);
    double getSpeed(Vehicle *vehicle);
    double getDirection(Vehicle *vehicle);
};
/* end of prototypes */

#endif

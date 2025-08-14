/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2009 Navit Team
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

#ifndef NAVIT_VEHICLE_H
#define NAVIT_VEHICLE_H

#include <QObject>
#include "NavitVehicleInterface.h"
#include "config_loader_layout.h"
#include "config_loader_navit.h"

#include "graphics.h"

extern "C"
{
#include "attr.h"
#include "point.h"
#include "glib.h"
}

typedef void *GraphicsHandle;
typedef void *GraphicsGCHandle;

class Vehicle : public QObject
{
    Q_OBJECT
public:
    Vehicle(const NavitVehicleConfig *config, NavitVehicleInterface *plugin, QObject *parent = nullptr);
    // struct vehicle *new(struct attr *parent, struct attr **attrs);
    void destroy();
    int get_attr(enum attr_type type, struct attr *attr, struct attr_iter *iter);
    int set_attr(struct attr *attr);
    int add_attr(struct attr *attr);
    int remove_attr(struct attr *attr);
    void set_cursor(LayoutCursor *cursor, int overwrite);
    void draw(Graphics *gra, struct point *pnt, int angle, int speed);
    int get_cursor_data(struct point *pnt, int *angle, int *speed);
    struct vehicle *ref();
    void unref();

    static attr_iter *attr_iter_new(void *unused);
    static void attr_iter_destroy(struct attr_iter *iter);
    static void log_gpx_add_tag(char *tag, char **logstr);

    void draw_do();

signals:
    void positionValidChanged(const bool &isValid);
    void positionChanged(const coord_geo &position);

private:
    NavitVehicleInterface *m_plugin;
    struct callback_list *m_cbl;
    struct log *m_nmea_log, *m_gpx_log;
    char *m_gpx_desc;

    QString m_name;

    // cursor
    LayoutCursor *m_cursor;
    int m_cursor_fixed;
    struct callback *m_animate_callback;
    struct event_timeout *m_animate_timer;
    struct point m_cursor_pnt;
    int m_need_resize;
    int m_real_w;
    int m_real_h;
    Graphics *m_gra;
    GraphicsContext *m_bg;
    struct transformation *m_trans;
    int m_angle;
    int m_speed;
    int m_sequence;
    GHashTable *m_log_to_cb;

    void set_default_name(const QString &name);
    void log_nmea(struct log *log);
    void log_gpx(struct log *log);
    void log_textfile(struct log *log);
    void log_binfile(struct log *log);
    int add_log(struct log *log);
};

#endif

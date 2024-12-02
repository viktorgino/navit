/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2017 Navit Team
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

#ifndef __graphics_qt_h
#define __graphics_qt_h

#include <QBrush>
#include <QGuiApplication>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <glib.h>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QDebug>
#include "navitinstance.h"
#include "NavitInterfaces.h"

#ifndef HAVE_FREETYPE
#define HAVE_FREETYPE 0
#endif

#if HAVE_FREETYPE
#include "navit/font/freetype/font_freetype.h"
#endif

struct graphics_gc_priv;
struct graphics_priv;

struct graphics_image_priv
{
    QPixmap *pixmap;
};

class GraphicsContextQt5 : public QObject, public NavitGraphicsContextInterface
{
    Q_OBJECT
public:
    GraphicsContextQt5();
    ~GraphicsContextQt5();
    void set_linewidth(int width) override;
    void set_dashes(int width, int offset, unsigned char dash_list[], int n) override;
    void set_foreground(struct color *c) override;
    void set_background(struct color *c) override;
    void set_texture(struct graphics_image *img) override;

    QPen &pen();
    QBrush &brush();

private:
    GraphicsQt5 *m_graphics;
    QPen m_pen;
    QBrush m_brush;
};

class GraphicsQt5 : public QObject, public NavitGraphicsInterface
{
    Q_OBJECT
public:
#if HAVE_FREETYPE
    GraphicsQt5(Navit &nav, graphics_methods *meth, attr **attrs, callback_list *cbl);
#error "GraphicsQt5 doesn't implement freetype"
#else
    GraphicsQt5(NavitInterface &navit, attr **attrs, callback_list *cbl, QObject *parent = 0);
    GraphicsQt5(struct point *p, int w, int h, int wraparound, NavitGraphicsInterface &parent);
#endif
    ~GraphicsQt5();
    void emit_update();

    void draw_mode(enum draw_mode_num mode) override;
    void draw_lines(NavitGraphicsContextInterface *gc, struct point *p, int count) override;
    void draw_polygon(NavitGraphicsContextInterface *gc, struct point *p, int count) override;
    void draw_rectangle(NavitGraphicsContextInterface *gc, struct point *p, int w, int h) override;
    void draw_circle(NavitGraphicsContextInterface *gc, struct point *p, int r) override;
    void draw_text(NavitGraphicsContextInterface *fg, NavitGraphicsContextInterface *bg, struct graphics_font_priv *font, char *text, struct point *p, int dx, int dy) override;
    void draw_image(NavitGraphicsContextInterface *fg, struct point *p, struct graphics_image_priv *img) override;
    void draw_image_warp(NavitGraphicsContextInterface *fg, struct point *p, int count, struct graphics_image_priv *img) override;
    void draw_polygon_with_holes(NavitGraphicsContextInterface *gc, struct point *p, int count, int hole_count, int *ccount, struct point **holes) override;
    void draw_drag(struct point *p) override;
    struct graphics_font_priv *font_new(struct graphics_font_methods *meth, char *font, int size, int flags) override;
    void background_gc(NavitGraphicsContextInterface *gc) override;
    struct graphics_priv *overlay_new(struct graphics_methods *meth, struct point *p, int w, int h, int wraparound) override;
    void overlay_disable(int disable) override;
    void overlay_resize(struct point *p, int w, int h, int wraparound) override;
    struct graphics_image_priv *image_new(struct graphics_image_methods *meth, char *path, int *w, int *h, struct point *hot, int rotation) override;
    void image_free(struct graphics_image_priv *priv) override;
    void *get_data(const char *type) override;
    void get_text_bbox(struct graphics_font_priv *font, char *text, int dx, int dy, struct point *ret, int estimate) override;
    int set_attr(struct attr *attr) override;
    int show_native_keyboard(struct graphics_keyboard *kbd) override;
    void hide_native_keyboard(struct graphics_keyboard *kbd) override;
    navit_float get_dpi() override;

    int fullscreen(int on);
    static int static_fullscreen(struct window *w, int on);
    callback_list *get_callbacks();

    NavitInstance &get_navit_instance();
signals:
    void update();

private:
    GraphicsQt5 *m_parent;
    callback_list *m_callbacks;
    NavitInstance m_navitInstance;

    QElapsedTimer m_elapsedTimer;
    QPixmap *m_pixmap = nullptr;
    QPainter *m_painter = nullptr;
    int m_use_count = 0;
    bool m_disable = false;
    int m_x = 0;
    int m_y = 0;
    int m_scroll_x = 0;
    int m_scroll_y = 0;
    NavitGraphicsContextInterface *m_background_graphics_gc_priv;
#if HAVE_FREETYPE
    struct font_priv *(*font_freetype_new)(void *meth);
    struct font_freetype_methods freetype_methods;
#endif
    bool m_root;

    void *get_data(struct graphics_priv *this_priv, char const *type);
    void resize_callback(int w, int h);
};

// void resize_callback(struct graphics_priv *gr, int w, int h);
#endif

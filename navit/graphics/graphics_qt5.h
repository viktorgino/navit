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
#include "navitinstance.h"

#ifndef HAVE_FREETYPE
#define HAVE_FREETYPE 0
#endif

#if HAVE_FREETYPE
#include "navit/font/freetype/font_freetype.h"
#endif

struct graphics_gc_priv;
struct graphics_priv;

class GraphicsPriv : public QObject {
    Q_OBJECT
public:
    GraphicsPriv(struct graphics_priv* gp);
    ~GraphicsPriv();
    void emit_update();

    struct graphics_priv* gp;

signals:
    void update();
};

struct graphics_priv {
    QQmlApplicationEngine* engine;
    GraphicsPriv* GPriv;
    QQuickWindow* window;
    NavitInstance *navitInstance;
    QPixmap* pixmap;
    QPainter* painter;
    int use_count;
    int disable;
    int x;
    int y;
    int scroll_x;
    int scroll_y;
    struct graphics_gc_priv* background_graphics_gc_priv;
#if HAVE_FREETYPE
    struct font_priv* (*font_freetype_new)(void* meth);
    struct font_freetype_methods freetype_methods;
#endif
    struct callback_list* callbacks;
    GHashTable* overlays;
    struct graphics_priv* parent;
    bool root;
    int argc;
    char* argv[4];
};

struct graphics_gc_priv {
    struct graphics_priv* graphics_priv;
    QPen* pen;
    QBrush* brush;
};
/* central exported application info */
extern QGuiApplication* navit_app;

void resize_callback(struct graphics_priv* gr, int w, int h);

#endif

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

#include <glib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
extern "C"
{
#include "../config.h"
#include "item.h" /* needs to be first, as attr.h depends on it */

#include "callback.h"
#include "color.h"
#include "debug.h"
#include "event.h"

#include "point.h" /* needs to be before graphics.h */

#include "plugin.h"
#include "window.h"
}

#include "event_qt5.h"
#include "graphics_qt5.h"
// #include <QDBusConnection>
// #include <QDBusInterface>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScreen>
#include <QSvgRenderer>

#include "QNavitQuick_2.h"
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QElapsedTimer>

#if defined(WINDOWS) || defined(WIN32) || defined(HAVE_API_WIN32_CE)
#include <windows.h>
#endif

#include "gui/navitpoimodel.h"
#include "gui/navitrecentsmodel.h"
#include "gui/navitfavouritesmodel.h"
#include "gui/navitsearchmodel.h"
#include "gui/navitroute.h"
#include "gui/navitlayoutsmodel.h"
#include "gui/navitlayersmodel.h"
#include "gui/navitvehiclesmodel.h"
#include "gui/navitmapsmodel.h"

static NavitInstance *navitInst;

struct graphics_font_priv
{
    QFont *font;
};

static void image_destroy(struct graphics_image_priv *img)
{
    //    dbg(lvl_debug, "enter");
    if (img->pixmap != nullptr)
        delete (img->pixmap);
    g_free(img);
}

struct graphics_image_methods image_methods = {
    image_destroy};

static QObject *navit_singletontype_provider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    // TODO: remove singleton provider
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    return navitInst;
}

#if HAVE_FREETYPE
static bool setup_freetype(bool is_root)
{
    if (is_root)
    {
        struct font_priv *(*font_freetype_new)(void *meth);
        /* get font plugin if present */
        font_freetype_new = (struct font_priv * (*)(void *)) plugin_get_category(plugin_category_font, "freetype");
        if (!font_freetype_new)
        {
            dbg(lvl_error, "no freetype");
            return NULL;
        }
        m_font_freetype_new = font_freetype_new;
        font_freetype_new(&m_freetype_methods);
        meth->font_new = (struct graphics_font_priv * (*)(struct graphics_priv *, struct graphics_font_methods *, char *, int,
                                                          int)) m_freetype_methods.font_new;
        meth->get_text_bbox = (void (*)(struct graphics_priv *, struct graphics_font_priv *, char *, int, int, struct point *,
                                        int))m_freetype_methods.get_text_bbox;
    }
    else
    {
        if (m_font_freetype_new)
        {
            m_font_freetype_new = m_font_freetype_new;
            m_font_freetype_new(&m_freetype_methods);
            meth->font_new = (struct graphics_font_priv * (*)(struct graphics_priv *, struct graphics_font_methods *, char *, int,
                                                              int)) m_freetype_methods.font_new;
            meth->get_text_bbox = (void (*)(struct graphics_priv *, struct graphics_font_priv *, char *, int, int, struct point *,
                                            int))m_freetype_methods.get_text_bbox;
        }
    }
}
#endif

GraphicsQt5::GraphicsQt5(NavitInterface &navit, callback_list *cbl, QObject *parent) : m_callbacks(cbl),
                                                                                       m_navitInstance(navit, *this),
                                                                                       QObject(parent)
{
    qDebug() << "graphics_qt5_new";
    navitInst = &m_navitInstance;

    /* no event system requested by config. Default to our own */
    if (!event_request_system("qt5", "graphics_qt5"))
    {
        qFatal("Unable to request default event system for graphics_qt5");
        return;
    }

    /* generate initial pixmap same size as window */
    // TODO: get window size
    m_root = true;
    m_pixmap = new QPixmap(200, 200);
    m_pixmap->fill(Qt::black);

    // tell Navit our geometry
    resize_callback(m_pixmap->width(), m_pixmap->height());

    navit.draw();
}
GraphicsQt5::GraphicsQt5(point p, int w, int h, int wraparound, NavitGraphicsInterface &parent) : m_parent(&dynamic_cast<GraphicsQt5 &>(parent)),
                                                                                                  m_navitInstance(m_parent->get_navit_instance()),
                                                                                                  QObject(m_parent)

{
    qDebug() << "graphics_qt5_new::overlay";

    m_root = false;
    m_x = p.x;
    m_y = p.y;
    m_callbacks = m_parent->get_callbacks();
    m_pixmap = new QPixmap(w, h);
    m_pixmap->fill(Qt::transparent);

#if HAVE_FREETYPE
    setup_freetype(m_root);
#endif
}

GraphicsQt5::~GraphicsQt5()
{
//        dbg(lvl_debug,"enter");
#if HAVE_FREETYPE
    m_freetype_methods.destroy();
#endif
    /* destroy painter */
    if (m_painter != nullptr)
        delete (m_painter);
    /* destroy pixmap */
    if (m_pixmap != nullptr)
        delete (m_pixmap);
}

NavitInstance &GraphicsQt5::get_navit_instance()
{
    return m_navitInstance;
}

void GraphicsQt5::emit_update()
{
    emit update();
}

callback_list *GraphicsQt5::get_callbacks()
{
    return m_callbacks;
}

struct graphics_image_priv *GraphicsQt5::image_new(struct graphics_image_methods *meth, char *path,
                                                   int *w, int *h, struct point *hot, int rotation)
{
    struct graphics_image_priv *image_priv;
    //        dbg(lvl_debug,"enter %s, %d %d", path, *w, *h);
    if (path[0] == 0)
    {
        dbg(lvl_debug, "Refuse to load image without path");
        return NULL;
    }
    QString key(path);
    QString renderer_key(key);
    int index = key.lastIndexOf(".");
    QString extension;
    if (index > 0)
    {
        extension = key.right(index);
    }
    QFile imagefile(key);
    if (!imagefile.exists())
    {
        /* file doesn't exit. Either navit wants us to guess file name by
         * ommitting exstension, or the file does really not exist.
         */
        if (extension != "")
        {
            /*file doesn't exist. give up */
            dbg(lvl_debug, "File %s does not exist", path);
            return NULL;
        }
        else
        {
            /* add ".svg" for renderer to try .svg file first in renderer */
            dbg(lvl_debug, "Guess extension on %s", path);
            renderer_key += ".svg";
        }
    }
    image_priv = g_new0(struct graphics_image_priv, 1);
    *meth = image_methods;

    /* check if this can be rendered */
    if (renderer_key.endsWith("svg"))
    {
        QSvgRenderer renderer(renderer_key);
        if (renderer.isValid())
        {
            dbg(lvl_debug, "render %s", path);
            /* try to render this */
            /* assume "standard" size if size is not given */
            if (*w <= 0)
                *w = renderer.defaultSize().width();
            if (*h <= 0)
                *h = renderer.defaultSize().height();
            image_priv->pixmap = new QPixmap(*w, *h);
            image_priv->pixmap->fill(Qt::transparent);
            QPainter painter(image_priv->pixmap);
            renderer.render(&painter);
        }
    }

    if (image_priv->pixmap == nullptr)
    {
        /*cannot be rendered. try to load it */
        dbg(lvl_debug, "cannot render %s", path);
        image_priv->pixmap = new QPixmap(key);
    }

    /* check if we got image */
    if ((image_priv->pixmap == nullptr) || (image_priv->pixmap->isNull()))
    {
        g_free(image_priv);
        return NULL;
    }
    else
    {
        /* check if we need to scale this */
        if ((*w > 0) && (*h > 0))
        {
            if ((image_priv->pixmap->width() != *w) || (image_priv->pixmap->height() != *h))
            {
                dbg(lvl_debug, "scale pixmap %s, %d->%d,%d->%d", path, image_priv->pixmap->width(), *w, image_priv->pixmap->height(),
                    *h);
                QPixmap *scaled = new QPixmap(image_priv->pixmap->scaled(*w, *h, Qt::IgnoreAspectRatio, Qt::FastTransformation));
                delete (image_priv->pixmap);
                image_priv->pixmap = scaled;
            }
        }
    }

    *w = image_priv->pixmap->width();
    *h = image_priv->pixmap->height();
    //        dbg(lvl_debug, "Got (%d,%d)", *w,*h);
    if (hot)
    {
        hot->x = *w / 2;
        hot->y = *h / 2;
    }

    return image_priv;
}

void GraphicsQt5::draw_lines(NavitGraphicsContextInterface *gc, struct point *p, int count)
{
    int i;
    QPolygon polygon;
    GraphicsContextQt5 *context = dynamic_cast<GraphicsContextQt5 *>(gc);
    //        dbg(lvl_debug,"enter gr=%p, gc=%p, (%d, %d)", gr, gc, p->x, p->y);
    if (m_painter == nullptr)
        return;

    for (i = 0; i < count; i++)
        polygon.putPoints(i, 1, p[i].x, p[i].y);
    m_painter->setPen(context->pen());
    m_painter->drawPolyline(polygon);
}

void GraphicsQt5::draw_polygon(NavitGraphicsContextInterface *gc, struct point *p, int count)
{
    int i;
    QPolygon polygon;
    GraphicsContextQt5 *context = dynamic_cast<GraphicsContextQt5 *>(gc);
    //        dbg(lvl_debug,"enter gr=%p, gc=%p, (%d, %d)", gr, gc, p->x, p->y);
    if (m_painter == nullptr)
        return;

    for (i = 0; i < count; i++)
        polygon.putPoints(i, 1, p[i].x, p[i].y);
    m_painter->setPen(context->pen());
    m_painter->setBrush(context->brush());

    m_painter->drawPolygon(polygon);
}

void GraphicsQt5::draw_polygon_with_holes(NavitGraphicsContextInterface *gc, struct point *p, int count,
                                          int hole_count, int *ccount, struct point **holes)
{
    int i;
    int j;
    QPainterPath path;
    QPainterPath inner;
    QPolygon polygon;
    GraphicsContextQt5 *context = dynamic_cast<GraphicsContextQt5 *>(gc);
    // dbg(lvl_error,"enter gr=%p, gc=%p, (%d, %d) holes %d", gr, gc, p->x, p->y, hole_count);
    if (m_painter == nullptr)
        return;
    m_painter->setPen(context->pen());
    m_painter->setBrush(context->brush());
    /* construct outer polygon */
    for (i = 0; i < count; i++)
        polygon.putPoints(i, 1, p[i].x, p[i].y);
    /* add it to outer path */
    path.addPolygon(polygon);
    /* construct the polygons for the holes and add them to inner */
    for (j = 0; j < hole_count; j++)
    {
        QPolygon hole;
        for (i = 0; i < ccount[j]; i++)
            hole.putPoints(i, 1, holes[j][i].x, holes[j][i].y);
        inner.addPolygon(hole);
    }
    /* intersect */
    if (hole_count > 0)
        path = path.subtracted(inner);

    m_painter->drawPath(path);
}

void GraphicsQt5::draw_rectangle(NavitGraphicsContextInterface *gc, struct point *p, int w, int h)
{
    //	dbg(lvl_debug,"gr=%p gc=%p %d,%d,%d,%d", gr, gc, p->x, p->y, w, h);
    GraphicsContextQt5 *context = dynamic_cast<GraphicsContextQt5 *>(gc);
    if (m_painter == nullptr)
        return;
    m_painter->fillRect(p->x, p->y, w, h, context->brush());
}

void GraphicsQt5::draw_circle(NavitGraphicsContextInterface *gc, struct point *p, int r)
{
    //        dbg(lvl_debug,"enter gr=%p, gc=%p, (%d,%d) r=%d", gr, gc, p->x, p->y, r);
    GraphicsContextQt5 *context = static_cast<GraphicsContextQt5 *>(gc);
    if (m_painter == nullptr)
        return;
    m_painter->setPen(context->pen());
    m_painter->drawArc(p->x - r / 2, p->y - r / 2, r, r, 0, 360 * 16);
}

/**
 * @brief	Render given text
 * @param	gr	own private context
 * @param	fg	foreground drawing context (for color)
 * @param	bg	background drawing context (for color)
 * @param	font	font context to use (allocated by font_new)
 * @param	text	String to calculate bbox for
 * @param	p	offset on gr context to place this text.
 * @param	dx	transformation matrix (16.16 fixpoint)
 * @param	dy	transformation matrix (16.16 fixpoint)
 *
 * Renders given text on gr surface. Draws nice contrast outline around text.
 */
void GraphicsQt5::draw_text(NavitGraphicsContextInterface *fg, NavitGraphicsContextInterface *bg, struct graphics_font_priv *font, char *text, struct point *p, int dx, int dy)
{
    dbg(lvl_debug, "enter gr=%p, fg=%p, bg=%p pos(%d,%d) d(%d, %d) %s", this, fg, bg, p->x, p->y, dx, dy, text);

    GraphicsContextQt5 *fgContext = dynamic_cast<GraphicsContextQt5 *>(fg);
    GraphicsContextQt5 *bgContext = dynamic_cast<GraphicsContextQt5 *>(bg);
    if (m_painter == nullptr)
        return;
#if HAVE_FREETYPE
    struct font_freetype_text *t;
    struct font_freetype_glyph *g, **gp;
    struct color transparent = {0x0000, 0x0000, 0x0000, 0x0000};
    struct color fgc;
    struct color bgc;
    QColor temp;

    int i, x, y;

    if (!font)
        return;
    /* extract colors */
    fgc.r = fg->pen->color().red() << 8;
    fgc.g = fg->pen->color().green() << 8;
    fgc.b = fg->pen->color().blue() << 8;
    fgc.a = fg->pen->color().alpha() << 8;
    if (bg != nullptr)
    {
        bgc.r = bg->pen->color().red() << 8;
        bgc.g = bg->pen->color().green() << 8;
        bgc.b = bg->pen->color().blue() << 8;
        bgc.a = bg->pen->color().alpha() << 8;
    }
    else
    {
        bgc = transparent;
    }

    t = m_freetype_methods.text_new(text, (struct font_freetype_font *)font, dx, dy);
    x = p->x << 6;
    y = p->y << 6;
    gp = t->glyph;
    i = t->glyph_count;
    if (bg)
    {
        while (i-- > 0)
        {
            g = *gp++;
            if (g->w && g->h)
            {
                unsigned char *data;
                QImage img(g->w + 2, g->h + 2, QImage::Format_ARGB32_Premultiplied);
                data = img.bits();
                m_freetype_methods.get_shadow(g, (unsigned char *)data, img.bytesPerLine(), &bgc, &transparent);

                painter->drawImage(((x + g->x) >> 6) - 1, ((y + g->y) >> 6) - 1, img);
            }
            x += g->dx;
            y += g->dy;
        }
    }
    x = p->x << 6;
    y = p->y << 6;
    gp = t->glyph;
    i = t->glyph_count;
    while (i-- > 0)
    {
        g = *gp++;
        if (g->w && g->h)
        {
            unsigned char *data;
            QImage img(g->w, g->h, QImage::Format_ARGB32_Premultiplied);
            data = img.bits();
            m_freetype_methods.get_glyph(g, (unsigned char *)data, img.bytesPerLine(), &fgc, &bgc, &transparent);
            painter->drawImage((x + g->x) >> 6, (y + g->y) >> 6, img);
        }
        x += g->dx;
        y += g->dy;
    }
    m_freetype_methods.text_destroy(t);
#else
    QString tmp = QString::fromUtf8(text);
    qreal m_dx = ((qreal)dx) / 65536.0;
    qreal m_dy = ((qreal)dy) / 65536.0;
    QTransform sav = m_painter->worldTransform();
    QTransform m(m_dx, m_dy, -m_dy, m_dx, p->x, p->y);
    m_painter->setWorldTransform(m, TRUE);
    m_painter->setFont(*font->font);

    // Paint bg
    QPen shadow;
    QPainterPath path;
    shadow.setColor(bgContext->pen().color());
    shadow.setWidth(3);
    m_painter->setPen(shadow);
    path.addText(0, 0, *font->font, tmp);
    m_painter->drawPath(path);

    // Paint fg
    m_painter->setPen(fgContext->pen());
    m_painter->drawText(0, 0, tmp);
    m_painter->setWorldTransform(sav);
#endif
}

void GraphicsQt5::draw_image(NavitGraphicsContextInterface *fg, struct point *p, struct graphics_image_priv *img)
{
    //        dbg(lvl_debug,"enter");
    if (m_painter != nullptr)
        m_painter->drawPixmap(p->x, p->y, *img->pixmap);
    else
        dbg(lvl_debug, "Try to draw image, but no painter");
}

/**
 * @brief	Drag layer.
 * @param   gr private handle
 * @param	p	vector the bitmap is moved from base, or NULL to indicate 0:0 vector
 *
 * Move layer to new position. If drag_bitmap is enabled this may also be
 * called for root layer. There the content of the root layer is to be moved
 * by given vector. On root layer, NULL indicates the end of a drag.
 */
void GraphicsQt5::draw_drag(struct point *p)
{
    struct point vector;

    if (p != nullptr)
    {
        dbg(lvl_debug, "enter %p (%d,%d)", this, p->x, p->y);
        vector = *p;
    }
    else
    {
        dbg(lvl_debug, "enter %p (NULL)", this);
        vector.x = 0;
        vector.y = 0;
    }
    if (m_root)
    {
        m_scroll_x = vector.x;
        m_scroll_y = vector.y;
    }
    else
    {
        m_x = vector.x;
        m_y = vector.y;
    }
}

void GraphicsQt5::background_gc(NavitGraphicsContextInterface *gc)
{
    //        dbg(lvl_debug,"register context %p on %p", gc, gr);
    m_background_graphics_gc_priv = gc;
}

void GraphicsQt5::draw_mode(enum draw_mode_num mode)
{
    switch (mode)
    {
    case draw_mode_begin:
        m_elapsedTimer.restart();
        dbg(lvl_debug, "Begin drawing on context %p (use == %d)", this, m_use_count);
        m_use_count++;
        if (!m_painter)
        {
            if (m_parent)
                m_pixmap->fill(QColor(0, 0, 0, 0));
            m_painter = new QPainter(m_pixmap);
        }
        else
            dbg(lvl_debug, "drawing on %p already active", this);
        break;
    case draw_mode_end:
        dbg(lvl_debug, "End drawing on context %p (use == %d)", this, m_use_count);
        m_use_count--;
        if (m_use_count < 0)
            m_use_count = 0;
        if (m_use_count > 0)
        {
            dbg(lvl_debug, "drawing on %p still in use", this);
        }
        else if (m_painter)
        {
            m_painter->end();
            delete (m_painter);
            m_painter = nullptr;
        }
        else
        {
            dbg(lvl_debug, "Context %p not active!", this)
        }
        m_navitInstance.emit_update();

        dbg(lvl_debug, "qt5 draw took : %lld milliseconds to complete", m_elapsedTimer.elapsed());
        break;
    default:
        dbg(lvl_debug, "Unknown drawing %d on context %p", mode, this);
        break;
    }
}

void GraphicsQt5::resize_callback(int w, int h)
{
    //        dbg(lvl_debug,"enter (%d, %d)", w, h);
    callback_list_call_attr_2(m_callbacks, attr_resize, GINT_TO_POINTER(w), GINT_TO_POINTER(h));
}

void *GraphicsQt5::get_data(char const *type)
{
    //        dbg(lvl_debug,"enter: %s", type);
    if (strcmp(type, "window") == 0)
    {
        struct window *win;
        //                dbg(lvl_debug,"window detected");
        win = g_new0(struct window, 1);
        win->priv = this;
        win->fullscreen = static_fullscreen;
        resize_callback(m_pixmap->width(), m_pixmap->height());
        return win;
    }
    if (strcmp(type, "engine") == 0)
    {
        // dbg(lvl_debug, "Hand over QQmlApplicationEngine");
        // return (m_engine);
        qWarning("Getting QQmlApplicationEngine is not implemented");
    }
    return NULL;
}

void GraphicsQt5::image_free(struct graphics_image_priv *priv)
{
    //        dbg(lvl_debug,"enter");
    delete (priv->pixmap);
    g_free(priv);
}

/**
 * @brief	Calculate pixel space required for font display.
 * @param	gr	own private context
 * @param	font	font context to use (allocated by font_new)
 * @param	text	String to calculate bbox for
 * @param	dx	transformation matrix (16.16 fixpoint)
 * @param	dy	transformation matrix (16.16 fixpoint)
 * @param	ret	point array to fill. (low left, top left, top right, low right)
 * @param	estimate	???
 *
 * Calculates the bounding box around the given text.
 */
void GraphicsQt5::get_text_bbox(struct graphics_font_priv *font, char *text, int dx, int dy,
                                struct point *ret, int estimate)
{
    int i;
    struct point pt;
    QString tmp = QString::fromUtf8(text);
    QRect r;
    //        dbg(lvl_debug,"enter %s %d %d", text, dx, dy);

    /* use QFontMetrix for bbox calculation as we do not always have a painter */
    QFontMetrics fm(*font->font);
    r = fm.boundingRect(tmp);

    /* low left */
    ret[0].x = r.left();
    ret[0].y = r.bottom();
    /* top left */
    ret[1].x = r.left();
    ret[1].y = r.top();
    /* top right */
    ret[2].x = r.right();
    ret[2].y = r.top();
    /* low right */
    ret[3].x = r.right();
    ret[3].y = r.bottom();
    /* transform bbox if rotated */
    if (dy != 0 || dx != 0x10000)
    {
        for (i = 0; i < 4; i++)
        {
            pt = ret[i];
            ret[i].x = (pt.x * dx - pt.y * dy) / 0x10000;
            ret[i].y = (pt.y * dx + pt.x * dy) / 0x10000;
        }
    }
}

void GraphicsQt5::overlay_disable(int disable)
{
    // dbg(lvl_error,"enter gr=%p, %d", gr, disable);
    m_disable = disable;
}

void GraphicsQt5::overlay_resize(struct point *p, int w, int h, int wraparound)
{
    //        dbg(lvl_debug,"enter %d %d %d %d %d", p->x, p->y, w, h, wraparound);
    m_x = p->x;
    m_y = p->y;
    if (m_painter != nullptr)
    {
        delete (m_painter);
    }
    /* replacing the pixmap clears the content. Only neccesary if size actually changes */
    if ((m_pixmap->height() != h) || (m_pixmap->width() != w))
    {
        delete (m_pixmap);
        m_pixmap = new QPixmap(w, h);
        m_pixmap->fill(Qt::transparent);
    }
    if (m_painter != nullptr)
        m_painter = new QPainter(m_pixmap);
    m_navitInstance.emit_update();
}

#pragma region Unimplemented

int GraphicsQt5::fullscreen(int on)
{
    qWarning("Full screen setting is currently not implemented");
    // if (on)
    // {
    //     m_window->setWindowState(Qt::WindowFullScreen);
    // }
    // else
    // {
    //     m_window->setWindowState(Qt::WindowMaximized);
    // }
    return 1;
}

int GraphicsQt5::static_fullscreen(struct window *w, int on)
{
    if (w && w->priv)
    {
        auto this_ = static_cast<GraphicsQt5 *>(w->priv);
        return this_->fullscreen(on);
    }
    return 0;
}

/**
 * @brief Return number of dots per inch
 * @param gr self handle
 * @return dpi value
 */
navit_float GraphicsQt5::get_dpi()
{
    qreal dpi = 96;
    //    QScreen* primary = navit_app->primaryScreen();
    //    if (primary != nullptr) {
    //        dpi = primary->physicalDotsPerInch();
    //    }
    return (navit_float)dpi;
}

void GraphicsQt5::draw_image_warp(NavitGraphicsContextInterface *fg, struct point *p, int count, struct graphics_image_priv *img)
{
}

int GraphicsQt5::set_attr(struct attr *attr)
{
    return 1;
}

int GraphicsQt5::show_native_keyboard(struct graphics_keyboard *kbd)
{
    return 1;
}

void GraphicsQt5::hide_native_keyboard(struct graphics_keyboard *kbd) {}

#pragma regionend
#pragma region GraphicsContext

GraphicsContextQt5::GraphicsContextQt5() : m_brush(Qt::SolidPattern)
{
}

GraphicsContextQt5::~GraphicsContextQt5()
{
}

QPen &GraphicsContextQt5::pen() { return m_pen; }

QBrush &GraphicsContextQt5::brush() { return m_brush; }

void GraphicsContextQt5::set_linewidth(int w)
{
    //        dbg(lvl_debug,"enter gc=%p, %d", gc, w);
    m_pen.setWidth(w);
}

void GraphicsContextQt5::set_dashes(int w, int offset, unsigned char *dash_list, int n)
{
    if (n <= 0)
    {
        dbg(lvl_error, "Refuse to set dashes without dash pattern");
    }
    /* use Qt dash feature */
    QVector<qreal> dashes;
    m_pen.setWidth(w);
    m_pen.setDashOffset(offset);
    for (int a = 0; a < n; a++)
    {
        dashes << dash_list[a];
    }
    /* Qt requires the pattern to have even element count. Add the last
     * element twice if n doesn't divide by two
     */
    if ((n % 2) != 0)
    {
        dashes << dash_list[n - 1];
    }
    m_pen.setDashPattern(dashes);
}

void GraphicsContextQt5::set_foreground(struct color *c)
{
    QColor col(c->r >> 8, c->g >> 8, c->b >> 8, c->a >> 8);
    //        dbg(lvl_debug,"context %p: color %02x%02x%02x",gc, c->r >> 8, c->g >> 8, c->b >> 8);
    m_pen.setColor(col);
    m_brush.setColor(col);
    // m_c=*c;
}

void GraphicsContextQt5::set_background(struct color *c)
{
    QColor col(c->r >> 8, c->g >> 8, c->b >> 8, c->a >> 8);
    //        dbg(lvl_debug,"context %p: color %02x%02x%02x",gc, c->r >> 8, c->g >> 8, c->b >> 8);
    // m_pen.setColor(col);
    // m_brush.setColor(col);
}

void GraphicsContextQt5::set_texture(struct graphics_image *img)
{
    if (!img)
    {
        // disable texture mode
        m_brush.setStyle(Qt::SolidPattern);
    }
    else
    {
        // set and enable texture
        // Use a new pixmap
        QPixmap background(img->priv->pixmap->size());
        // Use fill color
        background.fill(m_brush.color());
        // Get a painter
        QPainter painter(&background);
        // Blit the (transparent) image on pixmap.
        painter.drawPixmap(0, 0, *(img->priv->pixmap));
        // Set the texture to the brush.
        m_brush.setTexture(background);
    }
}

#pragma endregion
#pragma region Fonts

#if HAVE_FREETYPE
static void font_destroy(struct graphics_font_priv *font)
{
    //        dbg(lvl_debug,"enter");
    if (font->font != nullptr)
        delete (font->font);
    g_free(font);
}

/**
 * @brief	font interface structure
 * This structure is preset with all function pointers provided by this implemention
 * to be returned as interface.
 */
static struct graphics_font_methods font_methods = {
    font_destroy};

/**
 * List of font families to use, in order of preference
 */
static const char *fontfamilies[] = {
    "Liberation Sans",
    "Arial",
    "NcrBI4nh",
    "luximbi",
    "FreeSans",
    "DejaVu Sans",
    NULL,
};

/**
 * @brief	Allocate a font context
 * @param	gr	own private context
 * @param	meth	fill this structure with correct functions to be called with handle as interface to font
 * @param	font	font family e.g. "Arial"
 * @param	size	Font size in 16.6 fractional points @ 300dpi. This is bullsh***. The encoding is freetypes
 *          16.6 fixed point format usually giving points. One point is usually 72th part of an inch. But
 *          navit does not honor dpi correct. It's traditionally used freetype backend is fixed to 300 dpi.
 *          So this value is (300/72) pixels
 * @param	flags	Font flags (currently 1 if bold and 0 if not)
 *
 * @return	font handle
 *
 * Allocates a font handle and returnes filled interface stucture
 */

static struct graphics_font_priv *font_new(struct graphics_font_methods *meth, char *font,
                                           int size, int flags)
{
    int a = 0;
    struct graphics_font_priv *font_priv;
    dbg(lvl_debug, "enter (font %s, %d, 0x%x)", font, size, flags);
    font_priv = g_new0(struct graphics_font_priv, 1);
    font_priv->font = new QFont(fontfamilies[0]);
    if (font != nullptr)
        font_priv->font->setFamily(font);
    /* search for exact font match */
    while ((!font_priv->font->exactMatch()) && (fontfamilies[a] != nullptr))
    {
        font_priv->font->setFamily(fontfamilies[a]);
        a++;
    }
    if (font_priv->font->exactMatch())
    {
        dbg(lvl_debug, "Exactly matching font: %s", font_priv->font->family().toUtf8().data());
    }
    else
    {
        /* set any font*/
        if (font != nullptr)
        {
            font_priv->font->setFamily(font);
        }
        else
        {
            font_priv->font->setFamily(fontfamilies[0]);
        }
        dbg(lvl_debug, "No matching font. Resort to: %s", font_priv->font->family().toUtf8().data());
    }

    /* Convert silly font size to pixels. by 64 is to convert fixpoint to int. */
    dbg(lvl_debug, "(font %s, %d=%f, %d)", font, size, ((float)size) / 64.0, ((size * 300) / 72) / 64);
    font_priv->font->setPixelSize(((size * 300) / 72) / 64);
    // font_priv->font->setStyleStrategy(QFont::NoSubpixelAntialias);
    /* Check for bold font */
    if (flags)
    {
        font_priv->font->setBold(true);
    }

    *meth = font_methods;
    return font_priv;
}
#endif
#pragma endregion
#pragma region "Register plugin"

NavitGraphicsInterface &new_qt5_graphics(NavitInterface &navit, callback_list *cbl)
{
    auto ret = GraphicsQt5(navit, cbl);
    return ret;
}

NavitGraphicsInterface &new_qt5_graphics_overlay(point p, int w, int h, int wraparound, NavitGraphicsInterface &parent)
{
    auto ret = GraphicsQt5(p, w, h, wraparound, parent);
    return ret;
}

NavitGraphicsContextInterface &new_qt5_graphics_context()
{
    auto ret = GraphicsContextQt5();
    return ret;
}

GraphicsFunctions get_graphics_functions()
{
    return GraphicsFunctions{
        new_qt5_graphics,
        new_qt5_graphics_overlay,
        new_qt5_graphics_context,
    };
}

void plugin_init()
{
    qDebug() << "Graphics plugin init";
    Q_INIT_RESOURCE(graphics_qt5);
    dbg(lvl_debug, "Graphics plugin init");

    /* register our QtQuick widget to allow it's usage within QML */
    qmlRegisterType<QNavitQuick_2>("Navit.Graphics", 2, 0, "NavitMap");
    qmlRegisterType<NavitPOIModel>("Navit.POI", 1, 0, "NavitPOIModel");
    qmlRegisterType<NavitRecentsModel>("Navit.Recents", 1, 0, "NavitRecentsModel");
    qmlRegisterType<NavitFavouritesModel>("Navit.Favourites", 1, 0, "NavitFavouritesModel");
    qmlRegisterType<NavitSearchModel>("Navit.Search", 1, 0, "NavitSearchModel");
    qmlRegisterType<NavitRoute>("Navit.Route", 1, 0, "NavitRoute");
    qmlRegisterType<NavitLayoutsModel>("Navit.Layouts", 1, 0, "NavitLayouts");
    qmlRegisterType<NavitLayersModel>("Navit.Layers", 1, 0, "NavitLayers");
    qmlRegisterType<NavitVehiclesModel>("Navit.Vehicles", 1, 0, "NavitVehicles");
    qmlRegisterType<NavitMapsModel>("Navit.Maps", 1, 0, "NavitMaps");
    qmlRegisterSingletonType<NavitInstance>("Navit", 1, 0, "Navit", navit_singletontype_provider);

    plugin_register_category(plugin_category_graphics, "qt5", (void *)get_graphics_functions);
    qt5_event_init();
}

#pragma endregion
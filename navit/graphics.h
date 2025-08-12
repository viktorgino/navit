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

/** @file
 *
 * @brief Exported functions / structures for the graphics subsystem.
 */

#ifndef NAVIT_GRAPHICS_H
#define NAVIT_GRAPHICS_H
#include <optional>
#include <array>
#include <memory>
#include <cassert>

#include <QObject>
#include <QDebug>

#include "NavitInterfaces.h"
#include "config_loader_layout.h"

extern "C"
{
#include "coord.h"
#include "item.h"
#include "point.h"
#include <glib.h>
}

struct attr;
struct point;
struct container;
struct graphics_font;
struct graphics_image;
struct transformation;
struct display_list;
struct mapset;

/* This enum must be synchronized with the constants in NavitGraphics.java. */

struct graphics_priv;
struct graphics_font_priv;
struct graphics_image_priv;
struct graphics_font_methods;
struct graphics_image_methods;

struct graphics_keyboard_priv;

/**
 * Describes an instance of the native on-screen keyboard or other input method.
 */
struct graphics_keyboard
{
    int w; /**< The width of the area obscured by the keyboard (-1 for full width) */
    int h; /**< The height of the area obscured by the keyboard (-1 for full height) */
    /* TODO mode is currently a copy of the respective value in the internal GUI and uses the same values.
     * This may need to be changed to something with globally available enum, possibly with revised values.
     * The Android implementation (the first to support a native on-screen keyboard) does not use this field
     * due to limitations of the platform. */
    int mode;                                /**< Mode flags for the keyboard */
    char *lang;                              /**< The preferred language for text input, may be {@code NULL}. */
    struct graphics_keyboard_priv *gra_priv; /**< Private data determined by the graphics plugin. The
                                              *   graphics plugin is responsible for its management. If it
                                              *   uses this member, it must free the associated data in
                                              *   its {@code hide_native_keyboard} method. */
};

/** Magic value for unset/unspecified width/height. */
#define IMAGE_W_H_UNSET (-1)

/** @brief The functions to be implemented by graphics plugins.
 *
 * This struct lists the functions that Navit graphics plugins must implement.
 * The plugin must supply its list of function implementations from its plugin_init() function.
 * @see graphics_gtk_drawing_area#plugin_init()
 * @see graphics_android#plugin_init()
 */

/**
 * Describes areas at each edge of the application window which may be obstructed by the system UI.
 *
 * This allows the map to use all available space, including areas which may be obscured by system UI
 * elements, while constraining other elements such as OSDs or UI controls to an area that is guaranteed
 * to be visible as long as Navit is in the foreground.
 */
struct padding
{
    int left;
    int top;
    int right;
    int bottom;
};

struct graphics_font_methods
{
    void (*font_destroy)(struct graphics_font_priv *font);
};

struct graphics_font
{
    struct graphics_font_priv *priv;
    struct graphics_font_methods meth;
};

struct graphics_image_methods
{
    void (*image_destroy)(struct graphics_image_priv *img);
};

struct graphics_image
{
    struct graphics_image_priv *priv;
    struct graphics_image_methods meth;
    int width;
    int height;
    struct point hot;
};

struct graphics_data_image
{
    void *data;
    int size;
};

/**
 * @brief graphics display item structure
 *
 * The graphics item passes the ap items and other items with this structure
 * to the graphics drawing routines. The struct is only a stub. It is allocated
 * including "count -1" struct coord's following c[0], if "holes" not NULL, by a
 * polygon hole structure, and if label != NULL, a series of zero terminated
 * strings followed by another zero for label.
 */
struct displayitem
{
    struct displayitem *next;
    struct item item;
    char *label;
    struct displayitem_poly_holes *holes;
    int z_order;
    int flags;
    int count;
    struct coord c[0];
};

/* prototypes */
enum attr_type;
enum item_type;
struct attr;
struct attr_iter;
struct callback;
struct displaylist;
struct displaylist_handle;
struct graphics_font;
struct graphics_image;
struct item;
struct itemgra;
struct layout;
struct mapset;
struct point;
struct point_rect;
struct transformation;

class Graphics;

class GraphicsContext
{
public:
    GraphicsContext(NavitGraphicsContextInterface &contextInterface, Graphics *graphics);
    ~GraphicsContext();
    void set_foreground(QColor *c);
    void set_background(QColor *c);
    void set_texture(struct graphics_image *img);
    void set_linewidth(int width);
    void set_dashes(int width, int offset, QVector<int> &dashes);

    NavitGraphicsContextInterface &get_context_interface();

private:
    NavitGraphicsContextInterface &m_contextInterface;
    Graphics *m_graphics;
};
struct display_context
{
    Graphics *gra;
    LayoutItemGraphElement *element;
    GraphicsContext *gc;
    GraphicsContext *gc_background;
    struct graphics_image *img;
    enum projection pro;
    int mindist;
    struct transformation *trans;
    enum item_type type;
    int maxlen;
};

class Graphics : public QObject
{
    Q_OBJECT
public:
    Graphics(NavitInterface &navit, GraphicsFunctions &graphicsFunctions, QObject *parent = nullptr);
    Graphics(NavitInterface &navit, Graphics &parent_graphics, point *p, int w, int h, int wraparound, QObject *parent = nullptr);
    ~Graphics();
    GraphicsFunctions &get_graphics_functions();
    NavitGraphicsInterface &get_graphics_interface();
    NavitInterface &get_navit_interface();

    int set_attr(struct attr *attr);
    void set_rect(struct point_rect *pr);
    int get_attr(enum attr_type type, struct attr *attr, struct attr_iter *iter);
    void overlay_resize(struct point *p, int w, int h, int wraparound);
    void gc_init();
    void init();
    void *get_data(const char *type);
    void add_callback(struct callback *cb);
    void remove_callback(struct callback *cb);
    struct graphics_font *font_new(int size, int flags);
    struct graphics_font *named_font_new(char *font, int size, int flags);
    void font_destroy(struct graphics_font *gra_font);
    struct graphics_image *image_new_scaled(char *path, int w, int h);
    struct graphics_image *image_new_scaled_rotated(char *path, int w, int h, int rotate);
    struct graphics_image *image_new(char *path);
    void image_free(struct graphics_image *img);
    void draw_mode(enum draw_mode_num mode, bool call_callback = false);
    void draw_lines(GraphicsContext *gc, struct point *p, int count);
    void draw_circle(GraphicsContext *gc, struct point *p, int r);
    void draw_rectangle(GraphicsContext *gc, struct point *p, int w, int h);
    void draw_rectangle_rounded(GraphicsContext *gc, struct point *plu, int w, int h, int r, int fill);
    void draw_text(GraphicsContext *gc1, GraphicsContext *gc2, struct graphics_font *font, char *text, struct point *p, int dx, int dy);
    void draw_polygon(GraphicsContext *gc, struct point *pin, int count_in);
    void get_text_bbox(struct graphics_font *font, char *text, int dx, int dy, struct point *ret, int estimate);
    void overlay_disable(int disable);
    int is_disabled();
    void draw_image(GraphicsContext *gc, struct point *p, struct graphics_image *img);
    void draw_image_warp(GraphicsContext *gc, struct point *p, int count, struct graphics_image *img);
    int draw_drag(struct point *p);
    void background_gc(GraphicsContext *gc);
    void draw_text_std(int text_size, char *text, struct point *p);
    static QString icon_path(QString icon);
    char *texture_path(const char *texture);
    void draw_itemgra(LayoutItemGraph *itemGraph, struct transformation *t, char *label);

    void display_draw_arrow(struct point *p, navit_float dx, navit_float dy, navit_float width, struct display_context *dc, int filled);

    struct item *displayitem_get_item(struct displayitem *di);
    int displayitem_get_coord_count(struct displayitem *di);
    char *displayitem_get_label(struct displayitem *di);
    int displayitem_get_displayed(struct displayitem *di);
    int displayitem_get_z_order(struct displayitem *di);

    int show_native_keyboard(struct graphics_keyboard *kbd);
    int hide_native_keyboard(struct graphics_keyboard *kbd);
    void draw_polygon_clipped(GraphicsContext *gc, struct point *pin, int count_in);
    void draw_polyline_clipped(GraphicsContext *gc, struct point *pa, int count, int *width, int poly);
    navit_float get_dpi();
    void dpi_patch(struct callback_list *l, enum attr_type type, int pcount, void **p);
    int dpi_scale(int p);
    struct point dpi_scale_point(struct point *p);
    void label_line(GraphicsContext *fg, GraphicsContext *bg, struct graphics_font *font, struct point *p, int count, char *label);
    void display_add(struct hash_entry *entry, struct item *item, int count, struct coord *c, char **label, int label_count);
    static void static_dpi_patch(struct callback_list *l, enum attr_type type, int pcount, void **p, void *context);

    GHashTable *getImageCacheHash();
    void set_layout(Layout *layout);
    void draw_background();
    void set_z_order(int z_order);
    void display_context_free(struct display_context *dc);
    void displayitem_draw(struct displayitem *di, Layout *layout, struct display_context *dc);
    void font_destroy_all();
    int get_dpi_factor();

private:
    std::optional<Graphics *> m_parent;
    NavitInterface &m_navit;
    GraphicsFunctions &m_graphics_functions;

    callback_list *m_callbacks;

    NavitGraphicsInterface &m_graphicsInterface;
    NavitGraphicsContextInterface &m_contextInterface;
    GraphicsContext m_gcBackground;
    GraphicsContext m_gcMiddground;
    GraphicsContext m_gcForeground;

    QString m_default_font;
    int m_font_len;
    graphics_font **m_font;

    point_rect m_r;
    int m_font_size;
    int m_disabled;
    /*
     * Counter for z_order of displayitems;
     */
    int m_current_z_order;
    GHashTable *m_image_cache_hash;
    /* for dpi compensation */
    int m_dpi_factor;

    int dpi_unscale(int p);
    struct point dpi_unscale_point(struct point *p);
    int set_attr_do(struct attr *attr);
    void image_new_helper(graphics_image *image, char *path, char *name, int width, int height, int rotate);
    struct graphics_font *get_font(int size);
    void draw_polygon_with_holes_clipped(GraphicsContext *gc, struct point *pin, int count_in, int hole_count, int *ccount, struct point **holes);
    void clip_polygon(struct point_rect *r, struct point *in, int count_in, struct point *out, int *count_out);
    void draw_polyline_as_polygon(GraphicsContext *gc, struct point *pnt, int count, int *width);
    int limit_count(struct coord *c, int count);
    void multiline_label_draw(GraphicsContext *fg, GraphicsContext *bg, struct graphics_font *font, struct point pref, const char *label, int line_spacing);
    void displayitem_transform_holes(struct transformation *trans, enum projection pro, struct displayitem_poly_holes *in, struct displayitem_poly_holes *out, int mindist);
    void displayitem_free_holes(struct displayitem_poly_holes *holes);
    void displayitem_draw_polygon(struct display_context *dc, struct point *pa, int count, struct displayitem_poly_holes *holes);
    void draw_polygon_with_holes(GraphicsContext *gc, struct point *pin, int count_in, int hole_count, int *ccount, struct point **holes);
    void displayitem_draw_polyline(struct display_context *dc, LayoutPolyline *element, struct point *pa, int count, int *width);
    void displayitem_draw_circle(struct displayitem *di, struct display_context *dc, LayoutCircle *element, struct point *pa, int count);
    void displayitem_draw_text(struct displayitem *di, struct display_context *dc, LayoutText *element, struct point *pa, int count, struct displayitem_poly_holes *holes);
    void displayitem_draw_icon(struct displayitem *di, struct display_context *dc, LayoutIcon *element, struct point *pa, int count, Layout *layout);
    void display_draw_arrows(struct display_context *dc, struct point *pnt, int count, int *width, int filled);
    void display_draw_spike(struct point *p, navit_float dx, navit_float dy, navit_float width, struct display_context *dc);
    void display_draw_spikes(struct display_context *dc, struct point *pnt, int count, int *width, int distance);
    void displayitem_draw_image(struct displayitem *di, struct display_context *dc, struct point *pa, int count);
};

#endif

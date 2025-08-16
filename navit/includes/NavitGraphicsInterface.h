#pragma once

#include <QObject>

#include "NavitInterface.h"

#include "coord.h"
#include "item.h"
#include "point.h"
#include "attr.h"
#include "includes/common.h"

class NavitGraphicsContextInterface
{
public:
    virtual void set_foreground(const QColor &c) = 0;
    virtual void set_background(const QColor &c) = 0;
    virtual void set_texture(struct graphics_image *img) = 0;
    virtual void set_linewidth(int width) = 0;
    virtual void set_dashes(int width, int offset, unsigned char dash_list[], int n) = 0;
};

class NavitGraphicsInterface
{
public:
    NavitGraphicsInterface() = default;
    explicit NavitGraphicsInterface(NavitInterface &navit, callback_list *cbl) {};
    explicit NavitGraphicsInterface(struct point *p, int w, int h, int wraparound, NavitGraphicsInterface &parent) {};
    virtual void draw_mode(enum draw_mode_num mode) = 0;
    virtual void draw_lines(NavitGraphicsContextInterface *gc, struct point *p, int count) = 0;
    virtual void draw_polygon(NavitGraphicsContextInterface *gc, struct point *p, int count) = 0;
    virtual void draw_rectangle(NavitGraphicsContextInterface *gc, struct point *p, int w, int h) = 0;
    virtual void draw_circle(NavitGraphicsContextInterface *gc, struct point *p, int r) = 0;
    virtual void draw_text(NavitGraphicsContextInterface *fg, NavitGraphicsContextInterface *bg, struct graphics_font_priv *font, QString &text, struct point *p, int dx, int dy) = 0;
    virtual void draw_image(NavitGraphicsContextInterface *fg, struct point *p, struct graphics_image_priv *img) = 0;
    virtual void draw_image_warp(NavitGraphicsContextInterface *fg, struct point *p, int count, struct graphics_image_priv *img) = 0;
    virtual void draw_polygon_with_holes(NavitGraphicsContextInterface *gc, struct point *p, int count, int hole_count, int *ccount, struct point **holes) = 0;
    virtual void draw_drag(struct point *p) = 0;
    virtual graphics_font_priv *font_new(char *font, int size, int flags) = 0;
    virtual void font_destroy(struct graphics_font_priv *font) = 0;
    virtual void background_gc(NavitGraphicsContextInterface *gc) = 0;
    virtual void overlay_disable(int disable) = 0;
    virtual void overlay_resize(struct point *p, int w, int h, int wraparound) = 0;
    virtual struct graphics_image_priv *image_new(struct graphics_image_methods *meth, QString &path, int *w, int *h, struct point *hot, int rotation) = 0;
    virtual void image_free(struct graphics_image_priv *priv) = 0;
    virtual void *get_data(const char *type) = 0;
    virtual void get_text_bbox(struct graphics_font_priv *font, QString &text, int dx, int dy, struct point *ret, int estimate) = 0;
    virtual int set_attr(struct attr *attr) = 0;
    virtual int show_native_keyboard(struct graphics_keyboard *kbd) = 0;
    virtual void hide_native_keyboard(struct graphics_keyboard *kbd) = 0;
    virtual navit_float get_dpi() = 0;
    virtual void resize_callback(int w, int h) = 0;
};

struct GraphicsFunctions
{
    NavitGraphicsInterface *(*new_graphics)(NavitInterface &, callback_list *);
    NavitGraphicsInterface *(*new_graphics_overlay)(point, int, int, int, NavitGraphicsInterface &parent);
    NavitGraphicsContextInterface *(*new_graphics_context)();
};
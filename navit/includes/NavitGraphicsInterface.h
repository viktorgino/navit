#ifndef NAVIT_GRAPHICS_INTERFACE_H
#define NAVIT_GRAPHICS_INTERFACE_H
#include "coord.h"
#include "item.h"
#include "point.h"
#include <glib.h>
class NavitGraphicsContextInterface {
public:
    NavitGraphicsContextInterface() {}

    virtual void draw_mode(enum draw_mode_num mode);
    virtual void draw_lines(struct point *p, int count);
    virtual void draw_polygon(struct point *p, int count);
    virtual void draw_rectangle(struct point *p, int w, int h);
    virtual void draw_circle(struct point *p, int r);
    virtual void draw_text(struct graphics_gc_priv *bg, struct graphics_font_priv *font, char *text, struct point *p, int dx, int dy);
    virtual void draw_image(struct point *p, struct graphics_image_priv *img);
    virtual void draw_image_warp(struct point *p, int count, struct graphics_image_priv *img);
    virtual void draw_drag(struct point *p);
    virtual struct graphics_font_priv *font_new(struct graphics_font_methods *meth, char *font, int size, int flags);
}
class NavitGraphicsInterface {
public:
    virtual NavitGraphicsInterface(char *path, int *w, int *h, struct point *hot, int rotation)
    ~NavitGraphicsInterface() ()

    virtual void background_gc(struct graphics_gc_priv *gc);
    virtual struct graphics_priv *overlay_new(struct point *p, int w, int h, int wraparound);
    virtual void *get_data(const char *type);
    virtual void image_free(struct graphics_image_priv *priv);
    virtual void get_text_bbox(struct graphics_font_priv *font, char *text, int dx, int dy, struct point *ret, int estimate);
    virtual void overlay_disable(int disable);
    virtual void overlay_resize(struct point *p, int w, int h, int wraparound);
    virtual int set_attr(struct attr *attr);
    virtual int show_native_keyboard(struct graphics_keyboard *kbd);
    virtual void hide_native_keyboard(struct graphics_keyboard *kbd);
    virtual navit_float get_dpi();
    virtual void draw_polygon_with_holes (struct point *p, int count, int hole_count, int* ccount, struct point **holes);
};

#endif // NAVIT_GRAPHICS_INTERFACE_H
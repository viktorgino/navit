#ifndef NAVIT_GRAPHICS_INTERFACE_H
#define NAVIT_GRAPHICS_INTERFACE_H
#include <functional>

#include "coord.h"
#include "item.h"
#include "point.h"
#include "attr.h"
#include "includes/common.h"
#include <glib.h>

struct attr_iter
{
    void *iter;
    union
    {
        GList *list;
        struct mapset_handle *mapset_handle;
    } u;
};
class NavitInterface
{
public:
    virtual int set_vehicleprofile_name(char *name) = 0;
    virtual struct vehicleprofile *get_vehicleprofile() = 0;
    virtual int get_attr(enum attr_type type, struct attr *attr, struct attr_iter *iter) = 0;
    virtual struct mapset *get_mapset() = 0;
    virtual struct tracking *get_tracking() = 0;
    virtual void set_center(struct pcoord *center, int set_timeout_) = 0;
    virtual int populate_search_results_map(GList *search_results, struct coord_rect *r) = 0;
    virtual int set_attr(struct attr *attr) = 0;
    virtual void add_callback(struct callback *cb) = 0;
    virtual struct navigation *get_navigation() = 0;
    virtual struct route *get_route() = 0;
    virtual void zoom_level(int level, struct point *p) = 0;
    virtual void set_destination(struct pcoord *c, const char *description, int async) = 0;
    virtual int get_width() = 0;
    virtual int get_height() = 0;
    virtual struct transformation *get_trans() = 0;
    virtual void draw() = 0;
    virtual int set_layout_by_name(const char *name) = 0;
    virtual void set_position(struct pcoord *c) = 0;
    virtual int get_destination_count() = 0;
    virtual int get_destinations(struct pcoord *pc, int count) = 0;
    virtual void add_destination_description(struct pcoord *c, const char *description) = 0;
    virtual void set_destinations(struct pcoord *c, int count, const char *description, int async) = 0;
    virtual void drag_map(struct point *origin, struct point *destination) = 0;
    virtual void zoom_in(int factor, struct point *p) = 0;
    virtual void zoom_out(int factor, struct point *p) = 0;
    virtual void zoom_to_route(int orientation) = 0;
    virtual void set_center_cursor_draw() = 0;
    static struct attr_iter *attr_iter_new()
    {
        return g_new0(struct attr_iter, 1);
    }

    static void attr_iter_destroy(struct attr_iter *iter)
    {
        g_free(iter);
    }
};

class NavitGraphicsContextInterface
{
public:
    virtual void set_foreground(struct color *c) = 0;
    virtual void set_background(struct color *c) = 0;
    virtual void set_texture(struct graphics_image *img) = 0;
    virtual void set_linewidth(int width) = 0;
    virtual void set_dashes(int width, int offset, unsigned char dash_list[], int n) = 0;
};

class NavitGraphicsInterface
{
public:
    NavitGraphicsInterface() = default;
    explicit NavitGraphicsInterface(NavitInterface &navit, callback_list *cbl) {};
    explicit NavitGraphicsInterface(struct point *p, int w, int h, int wraparound) {};
    virtual void draw_mode(enum draw_mode_num mode) = 0;
    virtual void draw_lines(NavitGraphicsContextInterface *gc, struct point *p, int count) = 0;
    virtual void draw_polygon(NavitGraphicsContextInterface *gc, struct point *p, int count) = 0;
    virtual void draw_rectangle(NavitGraphicsContextInterface *gc, struct point *p, int w, int h) = 0;
    virtual void draw_circle(NavitGraphicsContextInterface *gc, struct point *p, int r) = 0;
    virtual void draw_text(NavitGraphicsContextInterface *fg, NavitGraphicsContextInterface *bg, struct graphics_font_priv *font, char *text, struct point *p, int dx, int dy) = 0;
    virtual void draw_image(NavitGraphicsContextInterface *fg, struct point *p, struct graphics_image_priv *img) = 0;
    virtual void draw_image_warp(NavitGraphicsContextInterface *fg, struct point *p, int count, struct graphics_image_priv *img) = 0;
    virtual void draw_polygon_with_holes(NavitGraphicsContextInterface *gc, struct point *p, int count, int hole_count, int *ccount, struct point **holes) = 0;
    virtual void draw_drag(struct point *p) = 0;
    virtual struct graphics_font_priv *font_new(struct graphics_font_methods *meth, char *font, int size, int flags) = 0;
    virtual void background_gc(NavitGraphicsContextInterface *gc) = 0;
    virtual struct graphics_priv *overlay_new(struct graphics_methods *meth, struct point *p, int w, int h, int wraparound) = 0;
    virtual void overlay_disable(int disable) = 0;
    virtual void overlay_resize(struct point *p, int w, int h, int wraparound) = 0;
    virtual struct graphics_image_priv *image_new(struct graphics_image_methods *meth, char *path, int *w, int *h, struct point *hot, int rotation) = 0;
    virtual void image_free(struct graphics_image_priv *priv) = 0;
    virtual void *get_data(const char *type) = 0;
    virtual void get_text_bbox(struct graphics_font_priv *font, char *text, int dx, int dy, struct point *ret, int estimate) = 0;
    virtual int set_attr(struct attr *attr) = 0;
    virtual int show_native_keyboard(struct graphics_keyboard *kbd) = 0;
    virtual void hide_native_keyboard(struct graphics_keyboard *kbd) = 0;
    virtual navit_float get_dpi() = 0;
    virtual void resize_callback(int w, int h) = 0;
};

struct GraphicsFunctions
{
    std::function<NavitGraphicsInterface &(NavitInterface &, callback_list *)> new_graphics;
    std::function<NavitGraphicsInterface &(point, int, int, int, NavitGraphicsInterface &parent)> new_graphics_overlay;
    std::function<NavitGraphicsContextInterface &()> new_graphics_context;
};

#endif // NAVIT_GRAPHICS_INTERFACE_H
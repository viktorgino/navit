#include "navit.h"
#include "navit_wrapper.h"

void navit_add_mapset(NavitHandle navit, struct mapset *ms)
{
    return ((Navit *)navit)->add_mapset(ms);
}
struct mapset *navit_get_mapset(NavitHandle navit)
{
    return ((Navit *)navit)->get_mapset();
}
struct map *navit_get_search_results_map(NavitHandle navit)
{
    return ((Navit *)navit)->get_search_results_map();
}
int navit_populate_search_results_map(NavitHandle navit, GList *search_results, struct coord_rect *r)
{
    return ((Navit *)navit)->populate_search_results_map(search_results, r);
}
struct tracking *navit_get_tracking(NavitHandle navit)
{
    return ((Navit *)navit)->get_tracking();
}
char *navit_get_user_data_directory(int create)
{
    return Navit::get_user_data_directory(create);
}
void navit_draw_async(NavitHandle navit, int async)
{
    return ((Navit *)navit)->draw_async(async);
}
void navit_draw(NavitHandle navit)
{
    return ((Navit *)navit)->draw();
}
int navit_get_ready(NavitHandle navit)
{
    return ((Navit *)navit)->get_ready();
}
void navit_draw_displaylist(NavitHandle navit)
{
    return ((Navit *)navit)->draw_displaylist();
}
void navit_handle_resize(NavitHandle navit, int w, int h)
{
    return ((Navit *)navit)->handle_resize(w, h);
}
int navit_get_width(NavitHandle navit)
{
    return ((Navit *)navit)->get_width();
}
int navit_get_height(NavitHandle navit)
{
    return ((Navit *)navit)->get_height();
}
void navit_set_timeout(NavitHandle navit)
{
    return ((Navit *)navit)->set_timeout();
}
void navit_handle_motion(NavitHandle navit, struct point *p)
{
    return ((Navit *)navit)->handle_motion(p);
}
void navit_zoom_in(NavitHandle navit, int factor, struct point *p)
{
    return ((Navit *)navit)->zoom_in(factor, p);
}
void navit_zoom_out(NavitHandle navit, int factor, struct point *p)
{
    return ((Navit *)navit)->zoom_out(factor, p);
}
void navit_zoom_in_cursor(NavitHandle navit, int factor)
{
    return ((Navit *)navit)->zoom_in_cursor(factor);
}
void navit_zoom_out_cursor(NavitHandle navit, int factor)
{
    return ((Navit *)navit)->zoom_out_cursor(factor);
}
NavitHandle navit_new(struct attr *parent, struct attr **attrs)
{
    return (NavitHandle) new Navit(parent, attrs);
}
void navit_add_message(NavitHandle navit, const char *message)
{
    return ((Navit *)navit)->add_message(message);
}
struct message *navit_get_messages(NavitHandle navit)
{
    return ((Navit *)navit)->get_messages();
}
struct vehicleprofile *navit_get_vehicleprofile(NavitHandle navit)
{
    return ((Navit *)navit)->get_vehicleprofile();
}
GList *navit_get_vehicleprofiles(NavitHandle navit)
{
    return ((Navit *)navit)->get_vehicleprofiles();
}
void navit_set_destination(NavitHandle navit, struct pcoord *c, const char *description, int async)
{
    return ((Navit *)navit)->set_destination(c, description, async);
}
void navit_set_destinations(NavitHandle navit, struct pcoord *c, int count, const char *description, int async)
{
    return ((Navit *)navit)->set_destinations(c, count, description, async);
}
void navit_add_destination_description(NavitHandle navit, struct pcoord *c, const char *description)
{
    return ((Navit *)navit)->add_destination_description(c, description);
}
int navit_get_destinations(NavitHandle navit, struct pcoord *pc, int count)
{
    return ((Navit *)navit)->get_destinations(pc, count);
}
int navit_get_destination_count(NavitHandle navit)
{
    return ((Navit *)navit)->get_destination_count();
}
char *navit_get_destination_description(NavitHandle navit, int n)
{
    return ((Navit *)navit)->get_destination_description(n);
}
void navit_remove_nth_waypoint(NavitHandle navit, int n)
{
    return ((Navit *)navit)->remove_nth_waypoint(n);
}
void navit_remove_waypoint(NavitHandle navit)
{
    return ((Navit *)navit)->remove_waypoint();
}
int navit_check_route(NavitHandle navit)
{
    return ((Navit *)navit)->check_route();
}
void navit_say(NavitHandle navit, const char *text)
{
    return ((Navit *)navit)->say(text);
}
void navit_speak(NavitHandle navit)
{
    return ((Navit *)navit)->speak();
}
void navit_window_roadbook_destroy(NavitHandle navit)
{
    return ((Navit *)navit)->window_roadbook_destroy();
}
void navit_window_roadbook_new(NavitHandle navit)
{
    return ((Navit *)navit)->window_roadbook_new();
}
int navit_init(NavitHandle navit)
{
    return ((Navit *)navit)->init();
}
void navit_zoom_to_rect(NavitHandle navit, struct coord_rect *r)
{
    return ((Navit *)navit)->zoom_to_rect(r);
}
void navit_zoom_to_route(NavitHandle navit, int orientation)
{
    return ((Navit *)navit)->zoom_to_route(orientation);
}
void navit_set_center(NavitHandle navit, struct pcoord *center, int set_timeout)
{
    return ((Navit *)navit)->set_center(center, set_timeout);
}
void navit_set_center_cursor(NavitHandle navit, int autozoom, int keep_orientation)
{
    return ((Navit *)navit)->set_center_cursor(autozoom, keep_orientation);
}
void navit_set_center_screen(NavitHandle navit, struct point *p, int set_timeout)
{
    return ((Navit *)navit)->set_center_screen(p, set_timeout);
}
int navit_set_attr(NavitHandle navit, struct attr *attr)
{
    return ((Navit *)navit)->set_attr(attr);
}
int navit_get_attr(NavitHandle navit, enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
    return ((Navit *)navit)->get_attr(type, attr, iter);
}
struct layout *navit_get_layout_by_name(NavitHandle navit, const char *layout_name)
{
    return ((Navit *)navit)->get_layout_by_name(layout_name);
}
void navit_update_current_layout(NavitHandle navit, struct layout *layout)
{
    return ((Navit *)navit)->update_current_layout(layout);
}
int navit_add_attr(NavitHandle navit, struct attr *attr)
{
    return ((Navit *)navit)->add_attr(attr);
}
int navit_remove_attr(NavitHandle navit, struct attr *attr)
{
    return ((Navit *)navit)->remove_attr(attr);
}
struct attr_iter *navit_attr_iter_new()
{
    return Navit::attr_iter_new();
}
void navit_attr_iter_destroy(struct attr_iter *iter)
{
    return Navit::attr_iter_destroy(iter);
}
void navit_add_callback(NavitHandle navit, struct callback *cb)
{
    return ((Navit *)navit)->add_callback(cb);
}
void navit_remove_callback(NavitHandle navit, struct callback *cb)
{
    return ((Navit *)navit)->remove_callback(cb);
}
void navit_set_position(NavitHandle navit, struct pcoord *c)
{
    return ((Navit *)navit)->set_position(c);
}
struct gui *navit_get_gui(NavitHandle navit)
{
    return ((Navit *)navit)->get_gui();
}
struct transformation *navit_get_trans(NavitHandle navit)
{
    return ((Navit *)navit)->get_trans();
}
struct route *navit_get_route(NavitHandle navit)
{
    return ((Navit *)navit)->get_route();
}
struct navigation *navit_get_navigation(NavitHandle navit)
{
    return ((Navit *)navit)->get_navigation();
}
void navit_layout_switch(NavitHandle navit)
{
    return ((Navit *)navit)->layout_switch();
}
int navit_set_vehicle_by_name(NavitHandle navit, const char *name)
{
    return ((Navit *)navit)->set_vehicle_by_name(name);
}
int navit_set_vehicleprofile_name(NavitHandle navit, char *name)
{
    return ((Navit *)navit)->set_vehicleprofile_name(name);
}
int navit_set_layout_by_name(NavitHandle navit, const char *name)
{
    return ((Navit *)navit)->set_layout_by_name(name);
}

int navit_block(NavitHandle navit, int block)
{
    return ((Navit *)navit)->block(block);
}
int navit_get_blocked(NavitHandle navit)
{
    return ((Navit *)navit)->get_blocked();
}
void navit_destroy(NavitHandle navit)
{
    return ((Navit *)navit)->destroy();
}

GraphicsGCHandle graphics_gc_new(GraphicsHandle graphics)
{
    GraphicsFunctions &graphics_functions = ((Graphics *)graphics)->get_graphics_functions();

    return (GraphicsGCHandle) new GraphicsContext(*graphics_functions.new_graphics_context(), (Graphics *)graphics);
}
void graphics_gc_set_foreground(GraphicsGCHandle *gc, struct color *c) {}
void graphics_gc_destroy(GraphicsGCHandle *gc)
{
    delete gc;
}
GraphicsHandle graphics_overlay_new(GraphicsHandle parent, struct point *p, int w, int h, int wraparound)
{
    Graphics &parent_graphics = *static_cast<Graphics *>(parent);
    return (GraphicsHandle) new Graphics(parent_graphics.get_navit_interface(), parent_graphics, p, w, h, wraparound);
}
void graphics_free(GraphicsHandle graphics)
{
    delete ((Graphics *)graphics);
    graphics = nullptr;
}
void graphics_overlay_disable(GraphicsHandle graphics, int disable)
{
    ((Graphics *)graphics)->overlay_disable(disable);
}
void graphics_init(GraphicsHandle graphics)
{
    ((Graphics *)graphics)->init();
}
void graphics_background_gc(GraphicsHandle graphics, GraphicsGCHandle *gc)
{
    ((Graphics *)graphics)->background_gc((GraphicsContext *)gc);
}
void graphics_overlay_resize(GraphicsHandle graphics, struct point *p, int w, int h, int wraparound)
{
    ((Graphics *)graphics)->overlay_resize(p, w, h, wraparound);
}
void graphics_draw_mode(GraphicsHandle graphics, enum draw_mode_num mode)
{
    ((Graphics *)graphics)->draw_mode(mode);
}
void graphics_draw_rectangle(GraphicsHandle graphics, GraphicsGCHandle *gc, struct point *p, int w, int h)
{
    ((Graphics *)graphics)->draw_rectangle((GraphicsContext *)gc, p, w, h);
}
void graphics_draw_itemgra(GraphicsHandle graphics, struct itemgra *itm, struct transformation *t, char *label)
{
    ((Graphics *)graphics)->draw_itemgra(itm, t, label);
}
int graphics_draw_drag(GraphicsHandle graphics, struct point *p)
{
    return ((Graphics *)graphics)->draw_drag(p);
}
char *graphics_icon_path(const char *icon)
{
    return Graphics::icon_path(icon);
}

struct object_func navit_func = {
    attr_navit,
    (object_func_new)navit_new,
    (object_func_get_attr)navit_get_attr,
    (object_func_iter_new)navit_attr_iter_new,
    (object_func_iter_destroy)navit_attr_iter_destroy,
    (object_func_set_attr)navit_set_attr,
    (object_func_add_attr)navit_add_attr,
    (object_func_remove_attr)navit_remove_attr,
    (object_func_init)navit_init,
    (object_func_destroy)navit_destroy,
    (object_func_dup)NULL,
    (object_func_ref)navit_object_ref,
    (object_func_unref)navit_object_unref,
};
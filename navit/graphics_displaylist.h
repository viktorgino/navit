#ifndef NAVIT_DISPLAYLIST_GRAPHICS_H
#define NAVIT_DISPLAYLIST_GRAPHICS_H

#include <array>

extern "C"
{
#include "coord.h"
#include "item.h"
#include "point.h"
#include "layout.h"
#include "transform.h"
#include "callback.h"
#include "event.h"
#include "debug.h"
#include "map.h"
#include "mapset.h"
#include <glib.h>
}

#include "graphics.h"

#define HASH_SIZE 1024
struct hash_entry
{
    enum item_type type;
    struct displayitem *di;
};
struct displaylist_icon_cache
{
    unsigned int seq;
};

class GraphicsDisplayList
{
public:
    GraphicsDisplayList(Graphics &graphics);
    void update_layers(GList *layers, int order);
    void update_hash();
    void clear_hash();
    struct hash_entry *get_hash_entry(enum item_type type);
    struct hash_entry *set_hash_entry(enum item_type type);
    /**
     * @brief Returns selection structure based on displaylist transform, projection and order.
     * Use this function to get map selection if you are going to fetch complete item data from the map based on displayitem reference.
     * @param displaylist
     * @returns Pointer to selection structure
     */
    map_selection *get_selection();
    /**
     * @brief Compare displayitems based on their zorder values.
     * Use with g_list_insert_sorted to sort less shaded items to be before more shaded ones in the result list.
     */
    static int cmp_zorder(const struct displayitem *a, const struct displayitem *b);

    /**
     * @brief Returns list of displayitems clicked at given coordinates. The deeper item is in current layout, the deeper it will be in the list.
     * @param displaylist
     * @param p clicked point
     * @param radius radius of clicked area
     * @returns GList of displayitems
     */
    GList *get_clicked_list(struct point *p, int radius);
    struct displaylist_handle *open();
    struct displayitem *next(struct displaylist_handle *dlh);
    void close(struct displaylist_handle *dlh);
    void destroy();
    void draw(struct transformation *trans, struct layout *l, int flags);
    void do_draw(int cancel, int flags);
    void load_mapset(struct mapset *mapset, struct transformation *trans, struct layout *l, int async, struct callback *cb, int flags);

    static void static_do_draw(int cancel, int flags, void *context);
    void draw_graphics(struct mapset *mapset, struct transformation *trans, struct layout *l, int async, struct callback *cb, int flags);
    void xdisplay_draw(struct layout *l, int order);
    int draw_cancel();
    void xdisplay_draw_layer(struct layer *lay, int order, struct layout *l);
    void xdisplay_draw_elements(struct itemgra *itm, struct layout *l);
    void xdisplay_free();

private:
    Graphics &m_graphics;
    int busy;
    int m_workload;
    struct callback *cb;
    struct layout *layout, *layout_hashed;
    struct display_context dc;
    int order, order_hashed, max_offset;
    struct mapset *ms;
    struct mapset_handle *msh;
    struct map *m;
    int conv;
    struct map_selection *sel;
    struct map_rect *mr;
    struct callback *idle_cb;
    struct event_idle *idle_ev;
    unsigned int seq;
    std::array<hash_entry, HASH_SIZE> hash_entries;
    GList *m_selection;

    int displayitem_within_dist(struct displayitem *di, struct point *p, int dist);
    int within_dist_line(struct point *p, struct point *line_p0, struct point *line_p1, int dist);
    int within_dist_point(struct point *p0, struct point *p1, int dist);
    int within_dist_polyline(struct point *p, struct point *line_pnt, int count, int dist, int close);
    int within_dist_polygon(struct point *p, struct point *poly_pnt, int count, int dist);

    void process_selection();
    void process_selection_item(struct item *item);
    void clear_selection();
    void remove_selection(struct item *item, enum item_type type);
    void add_selection(struct item *item, enum item_type type);

    struct displayitem_poly_holes *display_add_holes(struct item *item, int hole_count, char **p);
    void display_add(struct hash_entry *entry, struct item *item, int count, struct coord *c, char **label, int label_count);
};
#endif
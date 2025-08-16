#ifndef NAVIT_DISPLAYLIST_GRAPHICS_H
#define NAVIT_DISPLAYLIST_GRAPHICS_H

#include <array>

#include <QList>
#include "graphics.h"
#include "config_loader_layout.h"
#include "transform_2.h"

extern "C"
{
#include "coord.h"
#include "item.h"
#include "point.h"
#include "transform.h"
#include "callback.h"
#include "event.h"
#include "debug.h"
#include "map.h"
#include "mapset.h"
#include <glib.h>
}

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
    ~GraphicsDisplayList();

    void update_layers(QVector<LayoutLayer *> layers, int order);
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
    GList *get_clicked_list(LayoutCoord *p, int radius);
    struct displaylist_handle *open();
    struct displayitem *next(struct displaylist_handle *dlh);
    void close(struct displaylist_handle *dlh);
    void draw(struct transformation *trans, Layout *layout, int flags);
    void do_draw(int cancel, int flags);
    void load_mapset(struct mapset *mapset, struct transformation *trans, Layout *layout, int async, struct callback *cb, int flags);

    static void static_do_draw(int cancel, int flags, void *context);
    void draw_graphics(struct mapset *mapset, struct transformation *trans, Layout *layout, int async, struct callback *cb, int flags);
    void xdisplay_draw(Layout *layout, int order);
    int draw_cancel();
    void xdisplay_draw_layer(LayoutLayer *layer, int order, Layout *layout);
    void xdisplay_draw_elements(LayoutItemGraph *itemGraph, Layout *layout);
    void xdisplay_free();

private:
    Graphics &m_graphics;
    int busy;
    int m_workload;
    struct callback *cb;
    Layout *m_layout, *m_layout_hashed;
    struct display_context m_display_context;
    int m_order, m_order_hashed, m_max_offset;
    struct mapset *m_mapset;
    struct mapset_handle *m_mapset_handle;
    struct map *m;
    int conv;
    struct map_selection *m_map_selection;
    struct map_rect *m_map_rect;
    struct callback *idle_cb;
    struct event_idle *idle_ev;
    unsigned int seq;
    std::array<hash_entry, HASH_SIZE> m_hash_entries;
    GList *m_selection;

    int displayitem_within_dist(struct displayitem *di, LayoutCoord *p, int dist);
    int within_dist_line(LayoutCoord *p, LayoutCoord *line_p0, LayoutCoord *line_p1, int dist);
    int within_dist_point(LayoutCoord *p0, LayoutCoord *p1, int dist);
    int within_dist_polyline(LayoutCoord *p, QVector<LayoutCoord *> &line_pnt, int dist, int close);
    int within_dist_polygon(LayoutCoord *p, QVector<LayoutCoord *> &poly_pnt, int dist);

    void process_selection();
    void process_selection_item(struct item *item);
    void clear_selection();
    void remove_selection(struct item *item, enum item_type type);
    void add_selection(struct item *item, enum item_type type);

    void display_add_holes(struct displayitem *di, struct item *item, char **p);
    void display_add(struct hash_entry *entry, struct item *item, int count, struct coord *c, char **label, int label_count);
};
#endif
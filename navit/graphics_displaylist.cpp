#include "graphics_displaylist.h"

extern void *route_selection;

struct displaylist_handle
{
    GraphicsDisplayList *dl;
    struct displayitem *di;
    int hashidx;
};

/**
 * @brief maximum amount of coordinates to allocate on stack using g_alloca
 */
#define ALLOCA_COORD_LIMIT 16384

#pragma region Init

GraphicsDisplayList::GraphicsDisplayList(Graphics &graphics) : m_graphics(graphics)
{
    m_display_context.maxlen = ALLOCA_COORD_LIMIT;

    for (auto &hash_entry : m_hash_entries)
    {
        hash_entry.di = nullptr;
        hash_entry.type = type_none;
    }
}

void GraphicsDisplayList::update_layers(QVector<LayoutLayer *> layers, int order)
{
    for (LayoutLayer *layer : layers)
    {
        for (LayoutItemGraph *itemgra : layer->getItemgraphs())
        {
            if (itemgra->getOrder()->getMin() <= order && itemgra->getOrder()->getMax() >= order)
            {
                for (item_type type : itemgra->getItemTypes())
                {
                    set_hash_entry(type);
                }
            }
        }
    }
}

void GraphicsDisplayList::load_mapset(struct mapset *mapset, struct transformation *trans, Layout *layout, int async, struct callback *cb, int flags)
{
    int order = transform_get_order(trans);

    dbg(lvl_debug, "enter");
    if (busy)
    {
        if (async == 1)
            return;
        do_draw(1, flags);
    }
    xdisplay_free();
    dbg(lvl_debug, "order=%d", order);

    m_display_context.gra = &m_graphics;
    m_mapset = mapset;
    if (m_display_context.trans && m_display_context.trans != trans)
        transform_destroy(m_display_context.trans);
    if (m_display_context.trans != trans)
        m_display_context.trans = transform_dup(trans);
    m_workload = async ? 100 : 0;
    cb = cb;
    seq++;
    m_order = order > 0 ? order : 0;
    busy = 1;
    m_layout = layout;
    if (async)
    {
        if (!idle_cb)
            idle_cb = callback_new_3(callback_cast(GraphicsDisplayList::static_do_draw), 0, flags, this);
        idle_ev = event_add_idle(50, idle_cb);
    }
    else
        do_draw(0, flags);
}

#pragma endregion
#pragma region Hash

void GraphicsDisplayList::clear_hash()
{
    for (auto &hash_entry : m_hash_entries)
    {
        hash_entry.type = type_none;
    }
}

void GraphicsDisplayList::update_hash()
{
    m_max_offset = 0;
    clear_hash();
    update_layers(m_layout->getLayers(), m_order);
    // dbg(lvl_debug, "max offset %d", max_offset);
}

struct hash_entry *GraphicsDisplayList::get_hash_entry(enum item_type type)
{
    int hashidx = (type * 2654435761UL) & (HASH_SIZE - 1);
    int offset = m_max_offset;
    do
    {
        if (!m_hash_entries[hashidx].type)
            return nullptr;
        if (m_hash_entries[hashidx].type == type)
            return &m_hash_entries[hashidx];
        hashidx = (hashidx + 1) & (HASH_SIZE - 1);
    } while (offset-- > 0);
    return nullptr;
}

struct hash_entry *GraphicsDisplayList::set_hash_entry(enum item_type type)
{
    int hashidx = (type * 2654435761UL) & (HASH_SIZE - 1);
    int offset = 0;
    for (;;)
    {
        if (!m_hash_entries[hashidx].type)
        {
            m_hash_entries[hashidx].type = type;
            if (m_max_offset < offset)
                m_max_offset = offset;
            return &m_hash_entries[hashidx];
        }
        if (m_hash_entries[hashidx].type == type)
            return &m_hash_entries[hashidx];
        hashidx = (hashidx + 1) & (HASH_SIZE - 1);
        offset++;
    }
    return nullptr;
}

#pragma endregion
#pragma region Handle

struct displaylist_handle *GraphicsDisplayList::open()
{
    struct displaylist_handle *ret;

    ret = g_new0(struct displaylist_handle, 1);
    ret->dl = this;

    return ret;
}

struct displayitem *GraphicsDisplayList::next(struct displaylist_handle *dlh)
{
    struct displayitem *ret;
    if (!dlh)
        return NULL;
    for (;;)
    {
        if (dlh->di)
        {
            ret = dlh->di;
            dlh->di = ret->next;
            break;
        }
        if (dlh->hashidx == HASH_SIZE)
        {
            ret = NULL;
            break;
        }
        if (m_hash_entries[dlh->hashidx].type)
            dlh->di = m_hash_entries[dlh->hashidx].di;
        dlh->hashidx++;
    }
    return ret;
}

void GraphicsDisplayList::close(struct displaylist_handle *dlh)
{
    g_free(dlh);
}

void GraphicsDisplayList::destroy()
{
    if (m_display_context.trans)
        transform_destroy(m_display_context.trans);
}

#pragma endregion
#pragma region XDisplay

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void GraphicsDisplayList::xdisplay_free()
{
    for (auto &hash_entry : m_hash_entries)
    {
        struct displayitem *di = hash_entry.di;
        while (di)
        {
            struct displayitem *next = di->next;
            g_free(di);
            di = next;
        }
        hash_entry.di = NULL;
    }
}

void GraphicsDisplayList::xdisplay_draw_elements(LayoutItemGraph *itemGraph, Layout *layout)
{
    struct hash_entry *entry;
    for (LayoutItemGraphElement *element : itemGraph->getElements())
    {
        m_display_context.element = element;
        for (item_type type : itemGraph->getItemTypes())
        {
            m_display_context.type = type;
            entry = get_hash_entry(type);
            m_graphics.display_context_free(&m_display_context);
        }
    }
}

void GraphicsDisplayList::xdisplay_draw_layer(LayoutLayer *layer, int order, Layout *layout)
{
    for (LayoutItemGraph *itemGraph : layer->getItemgraphs())
    {
        if (order >= itemGraph->getOrder()->getMin() && order <= itemGraph->getOrder()->getMax())
            xdisplay_draw_elements(itemGraph, layout);
    }
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void GraphicsDisplayList::xdisplay_draw(Layout *layout, int order)
{

    m_graphics.set_z_order(0);
    for (LayoutLayer *layer : layout->getLayers())
    {
        if (layer->getActive())
        {
            // TODO: fix layer references
            // if (lay->ref)
            // lay = lay->ref;
            xdisplay_draw_layer(layer, order, layout);
        }
    }
}

#pragma endregion
#pragma region Draw

void GraphicsDisplayList::static_do_draw(int cancel, int flags, void *context)
{
    GraphicsDisplayList *graphics = static_cast<GraphicsDisplayList *>(context);
    if (graphics == nullptr)
        return;
    graphics->do_draw(cancel, flags);
}

void GraphicsDisplayList::do_draw(int cancel, int flags)
{
    struct item *item;
    int count;
    int max = m_display_context.maxlen;
    int workload = 0;
    int used = 0;
    struct coord *ca;
    struct attr attr, attr2;
    enum projection pro;
    int need_free = 0;

    if (max < ALLOCA_COORD_LIMIT)
    {
        ca = (coord *)g_alloca(sizeof(struct coord) * max);
        need_free = 0;
    }
    else
    {
        ca = (coord *)g_malloc(sizeof(struct coord) * max);
        need_free = 1;
    }

    if (m_order != m_order_hashed || m_layout != m_layout_hashed)
    {
        update_hash();
        m_order_hashed = m_order;
        m_layout_hashed = m_layout;
    }
    if (m_display_context.trans)
    {
        pro = transform_get_projection(m_display_context.trans);
    }
    while (!cancel)
    {
        if (!m_mapset_handle)
            m_mapset_handle = mapset_open(m_mapset);
        if (!m)
        {
            m = mapset_next(m_mapset_handle, 1);
            if (!m)
            {
                mapset_close(m_mapset_handle);
                m_mapset_handle = NULL;
                break;
            }
            m_display_context.pro = map_projection(m);
            conv = map_requires_conversion(m);
            if (route_selection)
                m_map_selection = (map_selection *)route_selection;
            else
                m_map_selection = get_selection();
            m_map_rect = map_rect_new(m, m_map_selection);
        }
        if (m_map_rect)
        {
            while ((item = map_rect_get_item(m_map_rect)))
            {
                int label_count = 0;
                char *labels[2];
                struct hash_entry *entry;
                int coords_left;
                if (item == &busy_item)
                {
                    if (m_workload)
                    {
                        if (need_free)
                        {
                            g_free(ca);
                        }
                        return;
                    }
                    else
                        continue;
                }
                entry = get_hash_entry(item->type);
                if (!entry)
                    continue;
                count = item_coord_get_within_selection(item, ca, item->type < type_line ? 1 : max, m_map_selection);
                /* abort if no coordinates within selection at all */
                if (!count)
                    continue;
                /* handle overflow */
                if (count == max)
                {
                    /* get required space */
                    item_coord_rewind(item);
                    coords_left = item_coords_left(item);
                    /* increase to required space, or double space if we couldn't get required space */
                    if (coords_left > 0)
                    {
                        m_display_context.maxlen = (coords_left + 2);
                    }
                    else
                    {
                        m_display_context.maxlen = max * 2;
                    }
                    dbg(lvl_error, "point count overflow %d for %s " ITEM_ID_FMT ". Increase to %d", count, item_to_name(item->type),
                        ITEM_ID_ARGS(*item), m_display_context.maxlen);
                    /* remember the new maximum */
                    max = m_display_context.maxlen;
                    /* get more memory */
                    if (need_free)
                        g_free(ca);
                    ca = (coord *)g_malloc(sizeof(struct coord) * (m_display_context.maxlen));
                    need_free = 1;
                    /* try again to get coordinates */
                    item_coord_rewind(item);
                    count = item_coord_get_within_selection(item, ca, item->type < type_line ? 1 : max, m_map_selection);
                    /* check if we got valid coordinates in second attempt. If not don't try to draw this at all */
                    if (count <= 0)
                    {
                        continue;
                    }
                }
                /* transform the coordinates */
                if (m_display_context.pro != pro)
                    transform_from_to_count(ca, m_display_context.pro, ca, pro, count);

                /* remember the peak coordinates actually used */
                if (used < count)
                    used = count;

                if (item_is_custom_poi(*item))
                {
                    if (item_attr_get(item, attr_icon_src, &attr2))
                        labels[1] = map_convert_string(m, attr2.u.str);
                    else
                        labels[1] = NULL;
                    label_count = 2;
                }
                else
                {
                    labels[1] = NULL;
                    label_count = 0;
                }
                if (item_attr_get(item, attr_label, &attr))
                {
                    labels[0] = attr.u.str;
                    if (!label_count)
                        label_count = 2;
                }
                else
                    labels[0] = NULL;
                if (conv && label_count)
                {
                    labels[0] = map_convert_string(m, labels[0]);
                    display_add(entry, item, count, ca, labels, label_count);
                    map_convert_free(labels[0]);
                }
                else
                    display_add(entry, item, count, ca, labels, label_count);
                if (labels[1])
                    map_convert_free(labels[1]);
                workload++;
                if (workload == m_workload)
                {
                    if (need_free)
                    {
                        g_free(ca);
                    }
                    return;
                }
            }
            map_rect_destroy(m_map_rect);
        }
        if (!route_selection)
            map_selection_destroy(m_map_selection);
        m_map_rect = NULL;
        m_map_selection = NULL;
        m = NULL;
    }
    if (idle_ev)
        event_remove_idle(idle_ev);
    idle_ev = NULL;
    callback_destroy(idle_cb);
    idle_cb = NULL;
    busy = 0;
    process_selection();
    if (!cancel)
        draw(m_display_context.trans, m_layout, flags);
    map_rect_destroy(m_map_rect);
    if (!route_selection)
        map_selection_destroy(m_map_selection);
    mapset_close(m_mapset_handle);
    m_map_rect = NULL;
    m_map_selection = NULL;
    m = NULL;
    m_mapset_handle = NULL;
    callback_call_1(cb, cancel);
    /* check if we can shrink item buffer next time */
    if ((m_display_context.maxlen > ALLOCA_COORD_LIMIT) && (used < ALLOCA_COORD_LIMIT))
    {
        dbg(lvl_debug, "Shrink memory. %d actually used", used);
        m_display_context.maxlen = ALLOCA_COORD_LIMIT;
    }
    /* clean up if required */
    if (need_free)
    {
        g_free(ca);
    }
}

void GraphicsDisplayList::draw(struct transformation *trans, Layout *layout, int flags)
{
    int order = transform_get_order(trans);
    if (m_display_context.trans && m_display_context.trans != trans)
        transform_destroy(m_display_context.trans);
    if (m_display_context.trans != trans)
        m_display_context.trans = transform_dup(trans);
    m_display_context.gra = &m_graphics;
    m_display_context.mindist = flags & 512 ? 15 : 2;
    // FIXME find a better place to set the background color
    m_graphics.set_layout(layout);
    m_graphics.draw_mode((flags & 8) ? draw_mode_begin_clear : draw_mode_begin, flags & 1);
    if (!(flags & 2))
    {
        m_graphics.draw_background();
    }
    if (layout)
    {
        xdisplay_draw(layout, order > 0 ? order : 0);
    }
    if (!(flags & 4))
        m_graphics.draw_mode(draw_mode_end, flags & 1);
}

void GraphicsDisplayList::draw_graphics(struct mapset *mapset, struct transformation *trans, Layout *layout, int async, struct callback *cb, int flags)
{
    load_mapset(mapset, trans, layout, async, cb, flags);
}

int GraphicsDisplayList::draw_cancel()
{
    if (!busy)
        return 0;
    do_draw(1, 0);
    return 1;
}

#pragma endregion
#pragma region Display Add

/**
 * @brief add the holes structure into preallocated area after displayitem
 *
 * @param item to extract holes from
 * @param hole_count precounted number of holes
 * @param p changeable pointer to buffer. Advanced by the size used
 * @returns pointer to newly created holes structure
 */
struct displayitem_poly_holes *GraphicsDisplayList::display_add_holes(struct item *item, int hole_count, char **p)
{
    struct attr attr;
    struct displayitem_poly_holes *holes;
    holes = (struct displayitem_poly_holes *)*p;
    *p += sizeof(*holes);
    holes->count = 0;
    holes->ccount = (int *)*p;
    *p += hole_count * sizeof(int);
    holes->coords = (struct coord **)*p;
    *p += hole_count * sizeof(struct coord *);
    item_attr_rewind(item);
    while (item_attr_get(item, attr_poly_hole, &attr))
    {
        holes->coords[holes->count] = (struct coord *)*p;
        holes->ccount[holes->count] = attr.u.poly_hole->coord_count;
        memcpy(holes->coords[holes->count], attr.u.poly_hole->coord, holes->ccount[holes->count] * sizeof(struct coord));
        *p += holes->ccount[holes->count] * sizeof(struct coord);
        holes->count++;
    }
    return holes;
}

void GraphicsDisplayList::display_add(struct hash_entry *entry, struct item *item, int count, struct coord *c, char **label, int label_count)
{
    struct displayitem *di;
    int len, i;
    char *p;
    struct attr attr;
    int hole_count = 0;
    int hole_total_coords = 0;
    int holes_length;
    int flags = 0;

    /* calculate number of bytes required */
    /* own length */
    len = sizeof(*di) + count * sizeof(*c);
    /* add length of lables including closing zero */
    if (label && label_count)
    {
        for (i = 0; i < label_count; i++)
        {
            if (label[i])
                len += strlen(label[i]) + 1;
            else
                len++;
        }
    }
    /* check for and remember flags (for underground drawing) */
    item_attr_rewind(item);
    if (item_attr_get(item, attr_flags, &attr))
    {
        flags = attr.u.num;
    }
    /* add length for holes */
    item_attr_rewind(item);
    while (item_attr_get(item, attr_poly_hole, &attr))
    {
        hole_count++;
        hole_total_coords += attr.u.poly_hole->coord_count;
    }
    holes_length = sizeof(struct displayitem_poly_holes) + hole_count * sizeof(int) + hole_count * sizeof(struct coord *) + hole_total_coords * sizeof(struct coord);
    if (hole_count > 0)
        dbg(lvl_debug, "got %d holes with %d coords total", hole_count, hole_total_coords);
    len += holes_length;

    p = (char *)g_malloc(len);

    di = (struct displayitem *)p;
    p += sizeof(*di) + count * sizeof(*c);
    di->item = *item;
    di->z_order = 0;
    di->flags = flags;
    di->holes = NULL;
    if (hole_count > 0)
    {
        di->holes = display_add_holes(item, hole_count, &p);
    }
    if (label && label_count)
    {
        di->label = p;
        for (i = 0; i < label_count; i++)
        {
            if (label[i])
            {
                strcpy(p, label[i]);
                p += strlen(label[i]) + 1;
            }
            else
                *p++ = '\0';
        }
    }
    else
        di->label = NULL;
    di->count = count;
    memcpy(di->c, c, count * sizeof(*c));
    di->next = entry->di;
    entry->di = di;
}

#pragma endregion
#pragma region Selection

struct map_selection *GraphicsDisplayList::get_selection()
{
    return transform_get_selection(m_display_context.trans, m_display_context.pro, m_order);
}

void GraphicsDisplayList::process_selection()
{
    GList *curr;

    curr = m_selection;
    while (curr)
    {
        struct item *item = (struct item *)curr->data;
        process_selection_item(item);
        curr = g_list_next(curr);
    }
}

void GraphicsDisplayList::process_selection_item(struct item *item)
{
#if 0 /* FIXME */
    struct displayitem di,*di_res;
    GHashTable *h;
    int count,max=dl->dc.maxlen;
    struct coord ca[max];
    struct attr attr;
    struct map_rect *mr;

    di.item=*item;
    di.label=NULL;
    di.count=0;
    h=g_hash_table_lookup(dl->dl, GINT_TO_POINTER(di.item.type));
    if (h) {
        di_res=g_hash_table_lookup(h, &di);
        if (di_res) {
            di.item.type=(enum item_type)item->priv_data;
            display_add(dl, &di.item, di_res->count, di_res->c, NULL, 0);
            return;
        }
    }
    mr=map_rect_new(item->map, NULL);
    item=map_rect_get_item_byid(mr, item->id_hi, item->id_lo);
    count=item_coord_get(item, ca, item->type < type_line ? 1: max);
    if (!item_attr_get(item, attr_label, &attr))
        attr.u.str=NULL;
    if (dl->conv && attr.u.str && attr.u.str[0]) {
        char *str=map_convert_string(item->map, attr.u.str);
        display_add(dl, item, count, ca, &str, 1);
        map_convert_free(str);
    } else
        display_add(dl, item, count, ca, &attr.u.str, 1);
    map_rect_destroy(mr);
#endif
}

void GraphicsDisplayList::add_selection(struct item *item, enum item_type type)
{
    struct item *item_dup = g_new(struct item, 1);
    *item_dup = *item;
    item_dup->priv_data = (void *)type;
    m_selection = g_list_append(m_selection, item_dup);
    process_selection_item(item_dup);
}

void GraphicsDisplayList::remove_selection(struct item *item, enum item_type type)
{
    GList *curr;
    int found;

    for (;;)
    {
        curr = m_selection;
        found = 0;
        while (curr)
        {
            struct item *sitem = (struct item *)curr->data;
            if (item_is_equal(*item, *sitem))
            {
#if 0 /* FIXME */
                if (dl) {
                    struct displayitem di;
                    GHashTable *h;
                    di.item=*sitem;
                    di.label=NULL;
                    di.count=0;
                    di.item.type=type;
                    h=g_hash_table_lookup(dl->dl, GINT_TO_POINTER(di.item.type));
                    if (h)
                        g_hash_table_remove(h, &di);
                }
#endif
                g_free(sitem);
                m_selection = g_list_remove(m_selection, curr->data);
                found = 1;
                break;
            }
        }
        if (!found)
            return;
    }
}

void GraphicsDisplayList::clear_selection()
{
    while (m_selection)
    {
        struct item *item = (struct item *)m_selection->data;
        remove_selection(item, *(enum item_type *)item->priv_data);
    }
}

#pragma endregion
#pragma region Distance

int GraphicsDisplayList::within_dist_point(struct point *p0, struct point *p1, int dist)
{
    if (p0->x == 32767 || p0->y == 32767 || p1->x == 32767 || p1->y == 32767)
        return 0;
    if (p0->x == -32768 || p0->y == -32768 || p1->x == -32768 || p1->y == -32768)
        return 0;
    if ((p0->x - p1->x) * (p0->x - p1->x) + (p0->y - p1->y) * (p0->y - p1->y) <= dist * dist)
    {
        return 1;
    }
    return 0;
}

int GraphicsDisplayList::within_dist_line(struct point *p, struct point *line_p0, struct point *line_p1, int dist)
{
    int vx, vy, wx, wy;
    int c1, c2;
    struct point line_p;

    if (line_p0->x < line_p1->x)
    {
        if (p->x < line_p0->x - dist)
            return 0;
        if (p->x > line_p1->x + dist)
            return 0;
    }
    else
    {
        if (p->x < line_p1->x - dist)
            return 0;
        if (p->x > line_p0->x + dist)
            return 0;
    }
    if (line_p0->y < line_p1->y)
    {
        if (p->y < line_p0->y - dist)
            return 0;
        if (p->y > line_p1->y + dist)
            return 0;
    }
    else
    {
        if (p->y < line_p1->y - dist)
            return 0;
        if (p->y > line_p0->y + dist)
            return 0;
    }

    vx = line_p1->x - line_p0->x;
    vy = line_p1->y - line_p0->y;
    wx = p->x - line_p0->x;
    wy = p->y - line_p0->y;

    c1 = vx * wx + vy * wy;
    if (c1 <= 0)
        return within_dist_point(p, line_p0, dist);
    c2 = vx * vx + vy * vy;
    if (c2 <= c1)
        return within_dist_point(p, line_p1, dist);

    line_p.x = line_p0->x + vx * c1 / c2;
    line_p.y = line_p0->y + vy * c1 / c2;
    return within_dist_point(p, &line_p, dist);
}

int GraphicsDisplayList::within_dist_polyline(struct point *p, struct point *line_pnt, int count, int dist, int close)
{
    int i;
    for (i = 0; i < count - 1; i++)
    {
        if (within_dist_line(p, line_pnt + i, line_pnt + i + 1, dist))
        {
            return 1;
        }
    }
    if (close)
        return (within_dist_line(p, line_pnt, line_pnt + count - 1, dist));
    return 0;
}

int GraphicsDisplayList::within_dist_polygon(struct point *p, struct point *poly_pnt, int count, int dist)
{
    int i, j, c = 0;
    for (i = 0, j = count - 1; i < count; j = i++)
    {
        if ((((poly_pnt[i].y <= p->y) && (p->y < poly_pnt[j].y)) ||
             ((poly_pnt[j].y <= p->y) && (p->y < poly_pnt[i].y))) &&
            (p->x < (poly_pnt[j].x - poly_pnt[i].x) * (p->y - poly_pnt[i].y) / (poly_pnt[j].y - poly_pnt[i].y) + poly_pnt[i].x))
            c = !c;
    }
    if (!c)
        return within_dist_polyline(p, poly_pnt, count, dist, 1);
    return c;
}

int GraphicsDisplayList::displayitem_within_dist(struct displayitem *di, struct point *p, int dist)
{
    int result;
    struct point *pa;
    int count;
    long pa_buf_size = sizeof(struct point) * m_display_context.maxlen;

    if (m_display_context.maxlen < ALLOCA_COORD_LIMIT)
    {
        pa = (struct point *)g_alloca(pa_buf_size);
    }
    else
    {
        pa = (struct point *)g_malloc(pa_buf_size);
    }

    count = transform_point_buf(m_display_context.trans, m_display_context.pro, di->c, pa, pa_buf_size, di->count, 0, 0, NULL);

    if (di->item.type < type_line)
    {
        result = within_dist_point(p, &pa[0], dist);
    }
    else if (di->item.type < type_area)
    {
        result = within_dist_polyline(p, pa, count, dist, 0);
    }
    else
        result = within_dist_polygon(p, pa, count, dist);

    if (m_display_context.maxlen >= ALLOCA_COORD_LIMIT)
    {
        g_free(pa);
    }
    return result;
}

#pragma endregion
#pragma region Misc

int GraphicsDisplayList::cmp_zorder(const struct displayitem *a, const struct displayitem *b)
{
    if (a->z_order > b->z_order)
        return -1;
    if (a->z_order < b->z_order)
        return 1;
    return 0;
}

GList *GraphicsDisplayList::get_clicked_list(struct point *p, int radius)
{
    GList *l = NULL;
    struct displayitem *di;
    struct displaylist_handle *dlh = open();

    while ((di = next(dlh)))
    {
        if (di->z_order > 0 && displayitem_within_dist(di, p, radius))
            l = g_list_insert_sorted(l, (gpointer)di, (GCompareFunc)GraphicsDisplayList::cmp_zorder);
    }

    close(dlh);

    return l;
}

#pragma endregion
#pragma once

#include <string>
#include <functional>

#include <QObject>

#include "coord.h"
#include "item.h"
#include "point.h"
#include "attr.h"
#include "includes/common.h"
#include <glib.h>
#include "config_loader_layout.h"

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
    virtual int set_vehicleprofile_name(const QString &name) = 0;
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
    virtual int set_layout_by_name(const QString &name) = 0;

    virtual Layout *getCurrentLayout() = 0;
    virtual const QVector<Layout *> &getLayouts() = 0;

    static struct attr_iter *attr_iter_new()
    {
        return g_new0(struct attr_iter, 1);
    }

    static void attr_iter_destroy(struct attr_iter *iter)
    {
        g_free(iter);
    }
};
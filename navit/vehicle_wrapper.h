
#ifndef NAVIT_VEHICLEWRAPPER_H
#define NAVIT_VEHICLEWRAPPER_H

#include "attr.h"
#include "coord.h"
#include "point.h"
#include "includes/common.h"
#ifdef __cplusplus
extern "C"
{
#endif
    struct point;
    struct vehicle_priv;

    struct vehicle_methods
    {
        void (*destroy)(struct vehicle_priv *priv);
        int (*position_attr_get)(struct vehicle_priv *priv, enum attr_type type, struct attr *attr);
        int (*set_attr)(struct vehicle_priv *priv, struct attr *attr);
    };

    /* prototypes */
    enum attr_type;
    struct attr;
    struct attr_iter;
    struct cursor;
    struct point;

    typedef void *VehicleHandle;
    typedef void *GraphicsHandle;
    struct vehicle *vehicle_new(struct attr *parent, struct attr **attrs);
    void vehicle_destroy(struct vehicle *this_);
    struct attr_iter *vehicle_attr_iter_new(void *unused);
    void vehicle_attr_iter_destroy(struct attr_iter *iter);
    int vehicle_get_attr(struct vehicle *this_, enum attr_type type, struct attr *attr, struct attr_iter *iter);
    int vehicle_set_attr(struct vehicle *this_, struct attr *attr);
    int vehicle_add_attr(struct vehicle *this_, struct attr *attr);
    int vehicle_remove_attr(struct vehicle *this_, struct attr *attr);
    void vehicle_set_cursor(struct vehicle *this_, struct cursor *cursor, int overwrite);
    void vehicle_draw(struct vehicle *this_, GraphicsHandle gra, struct point *pnt, int angle, int speed);
    int vehicle_get_cursor_data(struct vehicle *this_, struct point *pnt, int *angle, int *speed);
    void vehicle_log_gpx_add_tag(char *tag, char **logstr);
    struct vehicle *vehicle_ref(struct vehicle *this_);
    void vehicle_unref(struct vehicle *this_);

#ifdef __cplusplus
}
#endif
/* end of prototypes */

#endif

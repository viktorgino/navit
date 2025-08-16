
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

    int vehicle_get_attr(VehicleHandle this_, enum attr_type type, struct attr *attr, struct attr_iter *iter);

#ifdef __cplusplus
}
#endif
/* end of prototypes */

#endif

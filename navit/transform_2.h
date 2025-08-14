#pragma once

#include <QVector>
#include <QVariant>
#include "config_loader_layout.h"

#include "coord.h"
#include "point.h"
#include "debug.h"

int transform_point(struct transformation *t, enum projection pro, LayoutCoord *coord, LayoutCoord *result);
int transform_point_buf(struct transformation *t, enum projection pro, QVector<LayoutCoord *> &coord, QVector<LayoutCoord *> &result, int mindist, int width, int *width_return);
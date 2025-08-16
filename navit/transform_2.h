#pragma once

#include <QVector>
#include <QVariant>
#include "config_loader_layout.h"

extern "C"
{
#include "coord.h"
#include "point.h"
#include "debug.h"
#include "transform.h"
}

void transform_point(transformation *t, projection required_projection, LayoutCoord *coord, LayoutCoord *result);
void transform_point_buf(transformation *t, projection required_projection, QVector<LayoutCoord *> &coords, QVector<LayoutCoord *> &result, int mindist, int width, QVector<int> *width_result = nullptr);
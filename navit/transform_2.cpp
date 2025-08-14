#include "transform_2.h"
#include "transform.h"

struct coord_3d
{
    int x;
    int y;
    int z;
};

struct z_clip_result
{
    struct coord_3d clipped_coord;
    int visible;
    int process_coord_again;
    int skip_coord;
};

#define POST_SHIFT 5

#ifdef ENABLE_ROLL
#define HOG(t) ((t).hog)
#else
#define HOG(t) 0
#endif

static struct point transform_project_onto_view_plane(struct transformation *t, struct coord_3d c)
{
    struct point result;
    result.x = (long long)c.x * t->xscale / c.z;
    result.y = (long long)c.y * t->yscale / c.z;
    return result;
}

static int transform_points_too_close(struct point screen_point, LayoutCoord *screen_point_old, int mindist)
{
    if (!mindist)
    {
        return 0;
    }
    // approximation of Euclidean distance
    return (abs(screen_point.x - screen_point_old->getX()) +
            abs(screen_point.y - screen_point_old->getY())) < mindist;
}

static const navit_float gar2geo_units = 360.0 / (1 << 24);
static const navit_float geo2gar_units = 1 / (360.0 / (1 << 24));

void transform_to_geo2(enum projection pro, LayoutCoord *coord, struct coord_geo *g)
{
    int x, y, northern, zone;
    switch (pro)
    {
    case projection_mg:
        g->lng = coord->getX() / 6371000.0 / M_PI * 180;
        g->lat = navit_atan(exp(coord->getY() / 6371000.0)) / M_PI * 360 - 90;
        break;
    case projection_garmin:
        g->lng = coord->getX() * gar2geo_units;
        g->lat = coord->getY() * gar2geo_units;
        break;
    case projection_utm:
        x = coord->getX();
        y = coord->getY();
        northern = y >= 0;
        if (!northern)
        {
            y += 10000000;
        }
        zone = (x / 1000000);
        x = x % 1000000;
        transform_utm_to_geo(x, y, zone, northern, g);
        break;
    default:
        break;
    }
}
void transform_from_geo2(enum projection pro, struct coord_geo *g, LayoutCoord *coord)
{
    switch (pro)
    {
    case projection_mg:
        coord->set(g->lng * 6371000.0 * M_PI / 180, log(navit_tan(M_PI_4 + g->lat * M_PI / 360)) * 6371000.0);
        break;
    case projection_garmin:
        coord->set(g->lng * geo2gar_units, g->lat * geo2gar_units);
        break;
    default:
        break;
    }
}

static void transform_correct_projection(struct transformation *t, enum projection required_projection,
                                         LayoutCoord *coord, LayoutCoord *result)
{
    struct coord_geo g;
    if (required_projection == t->pro)
    {
        result->set(coord->getX(), coord->getY());
    }
    else
    {
        transform_to_geo2(required_projection, coord, &g);
        transform_from_geo2(t->pro, &g, result);
    }
}

static void transform_shift_by_center_and_scale(struct transformation *t, LayoutCoord *coord, LayoutCoord *result)
{
    int x = coord->getX() - t->map_center.x;
    int y = coord->getY() - t->map_center.y;
    x >>= t->scale_shift;
    y >>= t->scale_shift;

    result->set(x, y);
}

static struct coord_3d transform_rotate(struct transformation *t, LayoutCoord *coord)
{
    struct coord_3d result;
    result.x = coord->getX() * t->m00 + coord->getY() * t->m01 + HOG(*t) * t->m02;
    result.y = coord->getX() * t->m10 + coord->getY() * t->m11 + HOG(*t) * t->m12;
    result.z = (coord->getX() * t->m20 + coord->getY() * t->m21 + HOG(*t) * t->m22);
    result.z += t->offz << POST_SHIFT;
    dbg(lvl_debug, "result: (%d,%d,%d)", result.x, result.y, result.z);
    return result;
}

static struct coord_3d transform_z_clip(struct coord_3d c, struct coord_3d c_old, int zlimit)
{
    struct coord_3d result;
    float clip_factor = ((float)zlimit - c.z) / (c_old.z - c.z);
    dbg(lvl_debug, "in (%d,%d,%d) - (%d,%d,%d)", c.x, c.y, c.z, c_old.x, c_old.y, c_old.z);
    result.x = c.x + (c_old.x - c.x) * clip_factor;
    result.y = c.y + (c_old.y - c.y) * clip_factor;
    result.z = zlimit;
    dbg(lvl_debug, "clip result: (%d,%d,%d)", result.x, result.y, result.z);
    return result;
}

static struct z_clip_result transform_z_clip_if_necessary(struct coord_3d coord, int zlimit,
                                                          struct z_clip_result clip_result_old)
{
    int visibility_changed;
    struct z_clip_result clip_result = {{0, 0}, 0, 0, 0};
    clip_result.visible = (coord.z < zlimit ? 0 : 1);
    visibility_changed = (clip_result_old.visible != -1) && (clip_result.visible != clip_result_old.visible);
    if (visibility_changed)
    {
        clip_result.clipped_coord = transform_z_clip(coord, clip_result_old.clipped_coord, zlimit);
    }
    else
    {
        clip_result.clipped_coord = coord;
    }
    if (clip_result.visible && visibility_changed)
    {
        // line was clipped, but current point
        // is visible -> process it again
        clip_result.process_coord_again = 1;
    }
    else if (!clip_result.visible && !visibility_changed)
    {
        clip_result.skip_coord = 1;
    }
    return clip_result;
}

int transform_point(struct transformation *t, enum projection required_projection, LayoutCoord *coord, LayoutCoord *result)
{
    QVector<LayoutCoord *> coords;
    coords.append(coord);

    QVector<LayoutCoord *> results;
    coords.append(result);

    int ret = transform_point_buf(t, required_projection, coords, results, 0, 0, NULL);

    return ret;
}

int transform_point_buf(struct transformation *t, enum projection required_projection, QVector<LayoutCoord *> &coords,
                        QVector<LayoutCoord *> &result, int mindist, int width, int *width_result)
{
    LayoutCoord projected_coord, shifted_coord;
    struct coord_3d rotated_coord;
    struct point screen_point;
    int zlimit = t->znear;
    struct z_clip_result clip_result, clip_result_old = {{0, 0}, -1, 0, 0};
    int i, result_idx = 0, result_idx_last = 0;
    long max_results = result.size() / sizeof(struct point);

    dbg(lvl_debug, "count=%d", coords.size());
    for (i = 0; i < coords.size(); i++)
    {
        int x, y;
        dbg(lvl_debug, "input coord %d: (%d, %d)", i, coords[i]->getX(), coords[i]->getY());

        transform_correct_projection(t, required_projection, coords[i], &projected_coord);
        transform_shift_by_center_and_scale(t, &projected_coord, &projected_coord);
        rotated_coord = transform_rotate(t, &projected_coord);

        if (t->ddd)
        {
            clip_result = transform_z_clip_if_necessary(rotated_coord, zlimit, clip_result_old);
            clip_result_old = clip_result;
            if (clip_result.process_coord_again)
            {
                /* if we repeat an interation, we have to make sure that there is enough space in the result buffer to
                   not overflow. */
                if (result_idx + 1 < max_results)
                {
                    i--;
                }
                else
                {
                    dbg(lvl_debug, "Not enough space in buf for transform_point_buf");
                    return TRANSFORM_ERR_BUF_SPACE;
                }
            }
            else if (clip_result.skip_coord)
            {
                continue;
            }
            struct point point_on = transform_project_onto_view_plane(t, clip_result.clipped_coord);
            x = point_on.x;
            y = point_on.y;
        }
        else
        {
            x = rotated_coord.x >> POST_SHIFT;
            y = rotated_coord.y >> POST_SHIFT;
        }
        x += t->offx;
        y += t->offy;
        dbg(lvl_debug, "result: (%d, %d)", x, y);

        if (i != 0 && i != coords.size() - 1 &&
            (coords[i + 1]->getX() != coords[0]->getX() || coords[i + 1]->getY() != coords[0]->getY()))
        {
            if (transform_points_too_close(point(x, y), result[result_idx_last], mindist))
            {
                continue;
            }
        }

        result.append(new LayoutCoord(x, y));

        if (width_result)
        {
            if (t->ddd)
            {
                dbg(lvl_debug, "width %d * %d / %d", width, t->wscale, clip_result.clipped_coord.z);
                width_result[result_idx] = width * t->wscale / clip_result.clipped_coord.z;
            }
            else
                width_result[result_idx] = width;
        }
        result_idx_last = result_idx;
        result_idx++;
    }
    return result_idx;
}
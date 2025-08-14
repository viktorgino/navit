#include "vehicle.h"
#include "vehicle_wrapper.h"

void vehicle_destroy(VehicleHandle vehicle)
{
    return ((Vehicle *)vehicle)->destroy();
}
struct attr_iter *vehicle_attr_iter_new(void *unused)
{
    return Vehicle::attr_iter_new(unused);
}
void vehicle_attr_iter_destroy(struct attr_iter *iter)
{
    return Vehicle::attr_iter_destroy(iter);
}
int vehicle_get_attr(VehicleHandle vehicle, enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
    return ((Vehicle *)vehicle)->get_attr(type, attr, iter);
}
int vehicle_set_attr(VehicleHandle vehicle, struct attr *attr)
{
    return ((Vehicle *)vehicle)->set_attr(attr);
}
int vehicle_add_attr(VehicleHandle vehicle, struct attr *attr)
{
    return ((Vehicle *)vehicle)->add_attr(attr);
}
int vehicle_remove_attr(VehicleHandle vehicle, struct attr *attr)
{
    return ((Vehicle *)vehicle)->remove_attr(attr);
}

void vehicle_draw(VehicleHandle vehicle, GraphicsHandle gra, struct point *pnt, int angle, int speed)
{
    return ((Vehicle *)vehicle)->draw(gra, pnt, angle, speed);
}
int vehicle_get_cursor_data(VehicleHandle vehicle, struct point *pnt, int *angle, int *speed)
{
    return ((Vehicle *)vehicle)->get_cursor_data(pnt, angle, speed);
}
void vehicle_log_gpx_add_tag(char *tag, char **logstr)
{
    return Vehicle::log_gpx_add_tag(tag, logstr);
}
struct vehicle *vehicle_ref(VehicleHandle vehicle)
{
    return ((Vehicle *)vehicle)->ref();
}
void vehicle_unref(VehicleHandle vehicle)
{
    return ((Vehicle *)vehicle)->unref();
}

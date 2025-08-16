#include "vehicle.h"
#include "vehicle_wrapper.h"

int vehicle_get_attr(VehicleHandle v, enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
    // return ((Vehicle *)vehicle)->get_attr(type, attr, iter);
    Vehicle *vehicle = static_cast<Vehicle *>(v);
    switch (type)
    {
    case attr_position_valid:
        attr->u.num = vehicle->isPositionValid() ? attr_position_valid_valid : attr_position_valid_invalid;
        break;
    case attr_position_speed:
    {
        double speed = vehicle->getSpeed();
        attr->u.numd = &speed;
    }
    break;
    case attr_position_direction:
    {
        double direction = vehicle->getDirection();
        attr->u.numd = &direction;
    }
    break;
    case attr_position_time_iso8601:
        attr->u.str = vehicle->getIso8601Time().toLocal8Bit().data();
        break;
    case attr_position_coord_geo:
    {
        coord_geo geo = vehicle->getPosition();
        attr->u.coord_geo = &geo;
        break;
    }
    case attr_position_fix_type:
        attr->u.num = vehicle->getFixType();
        break;
    case attr_lag:
        attr->u.num = vehicle->getLag();
        break;

    default:
        return 0;
        break;
    }
    return 1;
}
#pragma once

#include <QObject>

#include "NavitInterface.h"

#include "coord.h"
#include "attr.h"

class NavitVehiclePlugin
{
public:
    virtual int position_attr_get(enum attr_type type, struct attr *attr) = 0;
    virtual int set_attr(struct attr *attr) = 0;

    virtual bool isPositionValid() = 0;
    virtual coord_geo getPosition() = 0;
    virtual QString getIso8601Time() = 0;
    virtual double getSpeed() = 0;
    virtual double getDirection() = 0;
    virtual int getFixType() = 0;
    virtual int getLag() = 0;

    // Signals
    virtual void positionValidChanged(const bool &isValid) = 0;
    virtual void positionChanged(const coord_geo &position) = 0;
};

class NavitVehicleFactory
{
public:
    virtual NavitVehiclePlugin *instantiate(NavitInterface *navit) = 0;
};
#define NavitVehicleFactory_iid "navit.NavitVehicleFactory"
Q_DECLARE_INTERFACE(NavitVehicleFactory, NavitVehicleFactory_iid)

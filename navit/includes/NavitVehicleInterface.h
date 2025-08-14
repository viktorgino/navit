#pragma once

#include <QObject>

#include "NavitInterface.h"

#include "coord.h"
#include "attr.h"

struct NavitVehicleAttrs
{
    double config_speed;
    double height;
    int interval;
    struct coord_geo geo;
};

class NavitVehicleInterface : public QObject
{
    Q_OBJECT
public:
    NavitVehicleInterface(QObject *parent = nullptr);
    virtual void destroy() = 0;
    virtual int position_attr_get(enum attr_type type, struct attr *attr) = 0;
    virtual int set_attr(struct attr *attr) = 0;
signals:
    void positionValidChanged(const bool &isValid);
    void positionChanged(const coord_geo &position);
};

class NavitVehicleFactory : public QObject
{
    Q_OBJECT
public:
    virtual NavitVehicleInterface *newVehicle(NavitInterface &navit, NavitVehicleAttrs &attrs, callback_list *cbl) = 0;
};
#define NavitVehicleFactory_iid "navit.NavitVehicleFactory"
Q_DECLARE_INTERFACE(NavitVehicleFactory, NavitVehicleFactory_iid)

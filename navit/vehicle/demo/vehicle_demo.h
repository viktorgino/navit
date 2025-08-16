
#ifndef __vehicle_h
#define __vehicle_h

#include <QObject>

#include "NavitInterfaces.h"

extern "C"
{
#include <glib.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "debug.h"
#include "coord.h"
#include "item.h"
#include "navit_wrapper.h"
#include "map.h"
#include "route.h"
#include "callback.h"
#include "transform.h"
#include "plugin.h"
#include "vehicle_wrapper.h"
#include "event.h"
#include "util.h"
#include "track.h"
#include "attr_def.h"
}

class VehicleDemoFactory : public NavitVehicleFactory
{

    Q_PLUGIN_METADATA(IID NavitVehicleFactory_iid)
    Q_INTERFACES(NavitVehicleFactory)
public:
    NavitVehicleInterface *instantiate(NavitInterface *navit) override;
};

class VehicleDemo : public NavitVehicleInterface
{
public:
    explicit VehicleDemo(NavitInterface *navit, QObject *parent = 0);
    ~VehicleDemo();

    int position_attr_get(enum attr_type type, struct attr *attr) override;
    int set_attr(struct attr *attr) override;

    bool isPositionValid() override;
    coord_geo getPosition() override;
    QString getIso8601Time() override;
    double getSpeed() override;
    double getDirection() override;
    int getFixType() override;
    int getLag() override;

    void timer();

private:
    NavitInterface *m_navit;

    int set_attr_do(struct attr *attr);
    void nmea_chksum(char *nmea);
    int m_interval;
    int m_position_set;
    struct route *m_route;
    struct coord_geo m_geo;
    struct coord m_last;
    double m_config_speed;
    double m_speed;
    double m_direction;
    struct callback *m_timer_callback;
    struct event_timeout *m_timer;
    char *m_timep;
    char *m_nmea;
    bool m_valid; /**< Whether the vehicle has valid position data **/
    double m_height;
    QString m_currentIso;
};
#endif
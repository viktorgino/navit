
#ifndef __vehicle_h
#define __vehicle_h

#include <QObject>

#include "NavitInterfaces.h"

class VehicleDemoFactory : public NavitVehicleFactory
{

    Q_PLUGIN_METADATA(IID NavitVehicleFactory_iid)
    Q_INTERFACES(NavitVehicleInterface)
public:
    NavitVehicleInterface *newVehicle(NavitInterface &navit, NavitVehicleAttrs &attrs, callback_list *cbl) override;
};

class VehicleDemo : public NavitVehicleInterface
{

    Q_INTERFACES(NavitVehicleInterface)
public:
    explicit VehicleDemo(NavitInterface &navit, NavitVehicleAttrs &attrs, callback_list *cbl, QObject *parent = 0);

    void destroy() override;
    int position_attr_get(enum attr_type type, struct attr *attr) override;
    int set_attr(struct attr *attr) override;

    void timer();

private:
    int set_attr_do(struct attr *attr);
    void nmea_chksum(char *nmea);
    int m_interval;
    int m_position_set;
    struct callback_list *m_cbl;
    NavitHandle m_navit;
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
    enum attr_position_valid m_valid; /**< Whether the vehicle has valid position data **/
    double m_height;
};
#endif
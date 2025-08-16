#pragma once
#include <QObject>
#include <QVector>
#include <QMetaType>
#include <QMetaProperty>

#include "config_loader_layout.h"

class NavitLogConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled MEMBER enabled)
    Q_PROPERTY(QString type MEMBER type REQUIRED)
    Q_PROPERTY(QString data MEMBER data REQUIRED)
    Q_PROPERTY(int flush_size MEMBER flush_size)
    Q_PROPERTY(int flush_time MEMBER flush_time)
    Q_PROPERTY(QString attr_types MEMBER attr_types)

public:
    bool enabled;
    QString type;
    QString data;
    int flush_size;
    int flush_time;
    QString attr_types;
};

class NavitPluginConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString path MEMBER path REQUIRED)
    Q_PROPERTY(bool active MEMBER active)
    Q_PROPERTY(bool ondemand MEMBER ondemand)
    Q_PROPERTY(bool lazy MEMBER lazy)
public:
    QString path;
    bool active = 1;
    bool lazy;
    bool ondemand;
};

class NavitDebugConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER name REQUIRED)
    Q_PROPERTY(int level MEMBER level)
    Q_PROPERTY(QString dbg_level MEMBER dbg_level)
public:
    QString name = "error";
    int level;
    QString dbg_level;
};

class NavitVehicleConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER name REQUIRED)
    Q_PROPERTY(QString source MEMBER source REQUIRED)
    Q_PROPERTY(bool enabled MEMBER enabled)
    Q_PROPERTY(bool active MEMBER active)
    Q_PROPERTY(bool follow MEMBER follow)
    Q_PROPERTY(bool update MEMBER update)
    Q_PROPERTY(QString gpsd_query MEMBER gpsd_query)
    Q_PROPERTY(QString profilename MEMBER profilename)
    Q_PROPERTY(NavitLogConfig *log READ getLog CONSTANT)

public:
    QString name;
    QString source;
    bool enabled;
    bool active;
    bool follow;
    bool update;
    QString gpsd_query;
    QString profilename;
    NavitLogConfig log;

private:
    NavitLogConfig *getLog()
    {
        return &log;
    }
};

class NavitTrackingConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int cdf_histsize MEMBER cdf_histsize)
public:
    int cdf_histsize;
};

class NavitRouteConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int destination_distance MEMBER destination_distance)
public:
    int destination_distance;
};

class NavitAnnounceConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type MEMBER type REQUIRED)
    Q_PROPERTY(int level0 MEMBER level0)
    Q_PROPERTY(int level1 MEMBER level1)
    Q_PROPERTY(int level2 MEMBER level2)
    Q_PROPERTY(QString unit MEMBER unit REQUIRED)
public:
    QString type;
    int level0;
    int level1;
    int level2;
    QString unit;
};

class NavitNavigationConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector<NavitAnnounceConfig *> announce MEMBER announce CONSTANT)
public:
    QVector<NavitAnnounceConfig *> announce;

private:
    QVector<NavitAnnounceConfig *> getAnnounce()
    {
        return announce;
    }
};

class NavitMap : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString type MEMBER type REQUIRED)
    Q_PROPERTY(QString data MEMBER data REQUIRED)
    Q_PROPERTY(bool enabled MEMBER enabled)
public:
    QString type;
    QString data;
    bool enabled = true;
};

class NavitMapset : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector<NavitMap *> maps MEMBER maps)
public:
    QVector<NavitMap *> maps;
    QVector<NavitMap *> getMaps()
    {
        return maps;
    }
};

class NavitConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(coord_geo *center READ getCenter CONSTANT)
    Q_PROPERTY(int zoom MEMBER zoom REQUIRED)
    Q_PROPERTY(bool vehicle_tracking MEMBER vehicle_tracking REQUIRED)
    Q_PROPERTY(int orientation MEMBER orientation REQUIRED)
    Q_PROPERTY(int recent_dest MEMBER recent_dest)
    Q_PROPERTY(bool drag_bitmap MEMBER drag_bitmap)
    Q_PROPERTY(QString default_layout MEMBER default_layout)
    Q_PROPERTY(bool tunnel_nightlayout MEMBER tunnel_nightlayout)
    Q_PROPERTY(int sunrise_degrees MEMBER sunrise_degrees)
    Q_PROPERTY(NavitLogConfig *log READ getLog CONSTANT)
    Q_PROPERTY(QVector<NavitPluginConfig *> plugins MEMBER plugins)
    Q_PROPERTY(QVector<NavitDebugConfig *> debug MEMBER debug)
    Q_PROPERTY(QVector<NavitVehicleConfig *> vehicle MEMBER vehicle)
    Q_PROPERTY(NavitTrackingConfig *tracking READ getTracking CONSTANT)
    Q_PROPERTY(NavitRouteConfig *route READ getRoute CONSTANT)
    Q_PROPERTY(NavitNavigationConfig *navigation READ getNavigation CONSTANT)
    Q_PROPERTY(QVector<NavitMapset *> mapsets MEMBER mapsets)

public:
    coord_geo center;
    int zoom;
    bool vehicle_tracking;
    int orientation = -1;
    int recent_dest;
    bool drag_bitmap;
    QString default_layout;
    bool tunnel_nightlayout = false; /* switch to nightlayout if we are in a tunnel? */
    int sunrise_degrees = -5;
    NavitLogConfig log;
    QVector<NavitPluginConfig *> plugins;
    QVector<NavitDebugConfig *> debug;
    QVector<NavitVehicleConfig *> vehicle;
    NavitTrackingConfig tracking;
    NavitRouteConfig route;
    NavitNavigationConfig navigation;
    QVector<NavitMapset *> mapsets;

    int tracking_flag = 1;
    int recentdest_count = 10;
    int center_timeout = 10;
    int autozoom_secs = 10;
    int autozoom_min = 7;
    int autozoom_max = 2097152;
    int use_mousewheel = 1;
    int pitch;
    int follow_cursor = 1;
    int zoom_min = 1;
    int zoom_max = 2097152;
    int radius = 30;
    int flags;
    /* 1=No graphics ok */
    /* 2=No gui ok */
    int border = 16;
    int imperial;
    int waypoints_flag;
    bool auto_switch = true; /*auto switching between day/night layout enabled ?*/

private:
    NavitLogConfig *getLog()
    {
        return &log;
    }
    QVector<NavitPluginConfig *> getPlugins()
    {
        return plugins;
    }
    QVector<NavitDebugConfig *> getDebug()
    {
        return debug;
    }
    QVector<NavitVehicleConfig *> getVehicle()
    {
        return vehicle;
    }
    NavitTrackingConfig *getTracking()
    {
        return &tracking;
    }
    NavitRouteConfig *getRoute()
    {
        return &route;
    }
    NavitNavigationConfig *getNavigation()
    {
        return &navigation;
    }
    QVector<NavitMapset *> getMapsets()
    {
        return mapsets;
    }
    coord_geo *getCenter()
    {
        return &center;
    }
};

Q_DECLARE_METATYPE(NavitLogConfig *)
Q_DECLARE_METATYPE(QVector<NavitPluginConfig *>)
Q_DECLARE_METATYPE(QVector<NavitDebugConfig *>)
Q_DECLARE_METATYPE(QVector<NavitVehicleConfig *>)
Q_DECLARE_METATYPE(NavitTrackingConfig *)
Q_DECLARE_METATYPE(NavitRouteConfig *)
Q_DECLARE_METATYPE(QVector<NavitAnnounceConfig *>)
Q_DECLARE_METATYPE(NavitNavigationConfig *)
Q_DECLARE_METATYPE(QVector<NavitMap *>)
Q_DECLARE_METATYPE(QVector<NavitMapset *>)
Q_DECLARE_METATYPE(coord_geo *)
#pragma once
#include <QObject>
#include <QMetaType>
#include <QMetaProperty>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include "config_loader_layout.h"

struct NavitLogConfig
{
    Q_GADGET
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

struct NavitPluginConfig
{
    Q_GADGET
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

struct NavitDebugConfig
{
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name REQUIRED)
    Q_PROPERTY(int level MEMBER level)
    Q_PROPERTY(QString dbg_level MEMBER dbg_level)
public:
    QString name = "error";
    int level;
    QString dbg_level;
};

struct NavitVehicleConfig
{
    Q_GADGET
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

struct NavitTrackingConfig
{
    Q_GADGET
    Q_PROPERTY(int cdf_histsize MEMBER cdf_histsize)
public:
    int cdf_histsize;
};

struct NavitRouteConfig
{
    Q_GADGET
    Q_PROPERTY(int destination_distance MEMBER destination_distance)
public:
    int destination_distance;
};

struct NavitAnnounceConfig
{
    Q_GADGET
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

struct NavitNavigationConfig
{
    Q_GADGET
    Q_PROPERTY(QList<NavitAnnounceConfig> *announce READ getAnnounce CONSTANT)
public:
    QList<NavitAnnounceConfig> announce;

private:
    QList<NavitAnnounceConfig> *getAnnounce()
    {
        return &announce;
    }
};

struct NavitMap
{
    Q_GADGET
    Q_PROPERTY(QString type MEMBER type REQUIRED)
    Q_PROPERTY(QString data MEMBER data REQUIRED)
    Q_PROPERTY(bool enabled MEMBER enabled)
public:
    QString type;
    QString data;
    bool enabled = true;
};

struct NavitConfig
{
    Q_GADGET
    Q_PROPERTY(QString center MEMBER center REQUIRED)
    Q_PROPERTY(int zoom MEMBER zoom REQUIRED)
    Q_PROPERTY(bool vehicle_tracking MEMBER vehicle_tracking REQUIRED)
    Q_PROPERTY(int orientation MEMBER orientation REQUIRED)
    Q_PROPERTY(int recent_dest MEMBER recent_dest)
    Q_PROPERTY(bool drag_bitmap MEMBER drag_bitmap)
    Q_PROPERTY(QString default_layout MEMBER default_layout)
    Q_PROPERTY(bool tunnel_nightlayout MEMBER tunnel_nightlayout)
    Q_PROPERTY(int sunrise_degrees MEMBER sunrise_degrees)
    Q_PROPERTY(NavitLogConfig *log READ getLog CONSTANT)
    Q_PROPERTY(QList<NavitPluginConfig> *plugins READ getPlugins CONSTANT)
    Q_PROPERTY(QList<NavitDebugConfig> *debug READ getDebug CONSTANT)
    Q_PROPERTY(QList<NavitVehicleConfig> *vehicle READ getVehicle CONSTANT)
    Q_PROPERTY(NavitTrackingConfig *tracking READ getTracking CONSTANT)
    Q_PROPERTY(NavitRouteConfig *route READ getRoute CONSTANT)
    Q_PROPERTY(NavitNavigationConfig *navigation READ getNavigation CONSTANT)
    Q_PROPERTY(QList<NavitMap> *maps READ getMaps CONSTANT)

public:
    QString center;
    int zoom;
    bool vehicle_tracking;
    int orientation = -1;
    int recent_dest;
    bool drag_bitmap;
    QString default_layout;
    bool tunnel_nightlayout = false; /* switch to nightlayout if we are in a tunnel? */
    int sunrise_degrees = -5;
    NavitLogConfig log;
    QList<NavitPluginConfig> plugins;
    QList<NavitDebugConfig> debug;
    QList<NavitVehicleConfig> vehicle;
    NavitTrackingConfig tracking;
    NavitRouteConfig route;
    NavitNavigationConfig navigation;
    QList<NavitMap> maps;

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
    QList<NavitPluginConfig> *getPlugins()
    {
        return &plugins;
    }
    QList<NavitDebugConfig> *getDebug()
    {
        return &debug;
    }
    QList<NavitVehicleConfig> *getVehicle()
    {
        return &vehicle;
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
    QList<NavitMap> *getMaps()
    {
        return &maps;
    }
};

Q_DECLARE_METATYPE(NavitLogConfig *)
Q_DECLARE_METATYPE(QList<NavitPluginConfig> *)
Q_DECLARE_METATYPE(QList<NavitDebugConfig> *)
Q_DECLARE_METATYPE(QList<NavitVehicleConfig> *)
Q_DECLARE_METATYPE(NavitTrackingConfig *)
Q_DECLARE_METATYPE(NavitRouteConfig *)
Q_DECLARE_METATYPE(QList<NavitAnnounceConfig> *)
Q_DECLARE_METATYPE(NavitNavigationConfig *)
Q_DECLARE_METATYPE(QList<NavitMap> *)

class ConfigLoader : public QObject
{
    Q_OBJECT
public:
    ConfigLoader(QObject *parent = nullptr);
    NavitConfig &loadNavit(QString fileName);

private:
    void loadFromFile(const QString &fileName);
    void loadFromJson(const QVariant &configObject);
    NavitConfig m_navitConfig;
};

#ifndef NAVIT_PLUGIN_LOADER_H
#define NAVIT_PLUGIN_LOADER_H

#include <QObject>
#include "config_loader.h"

extern "C"
{
#include "debug.h"
#include "plugin.h"
#include "vehicle.h"
#include "track.h"
#include "route.h"
#include "navigation.h"
#include "mapset.h"
#include "map.h"
#include "navit_wrapper.h"
}

class PluginLoader : public QObject
{
    Q_OBJECT
public:
    PluginLoader(NavitConfig &navitConfig, NavitHandle navit, QObject *parent = nullptr);

    void loadModules();

    tracking *getTracking();
    route *getRoute();
    navigation *getNavigation();
    vehicle *getVehicle();
    mapset *getMapset();

private:
    NavitConfig &m_navitConfig;
    NavitHandle m_navit;

    QList<debug *> m_debugConfigs;
    QList<plugin *> m_plugins;
    vehicle *m_vehicle;
    tracking *m_tracking;
    route *m_route;
    navigation *m_navigation;
    mapset *m_mapset;

    void loadDebug(QList<NavitDebugConfig> &debugConfigs);
    void loadPlugins(QList<NavitPluginConfig> &plugins);
    void loadVehicles(QList<NavitVehicleConfig> &vehicles);
    void loadTracking(NavitTrackingConfig &tracking);
    void loadRoute(NavitRouteConfig &route);
    void loadNavigation(NavitNavigationConfig &navigation);
    void loadMaps(QList<NavitMap> &maps);
};

#endif
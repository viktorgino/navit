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
}

class PluginLoader : public QObject
{
    Q_OBJECT
public:
    PluginLoader(NavitConfig &navitConfig, QObject *parent = nullptr);
    void loadDebug(QList<NavitDebugConfig> &debugConfigs);
    void loadPlugins(QList<NavitPluginConfig> &plugins);
    void loadVehicles(QList<NavitVehicleConfig> &vehicles);
    void loadTracking(NavitTrackingConfig &tracking);
    void loadRoute(NavitRouteConfig &route);
    void loadNavigation(NavitNavigationConfig &navigation);

    tracking *getTracking();
    route *getRoute();
    navigation *getNavigation();

private:
    NavitConfig &m_navitConfig;

    QList<debug *> m_debugConfigs;
    QList<plugin *> m_plugins;
    QList<vehicle *> m_vehicles;
    tracking *m_tracking;
    route *m_route;
    navigation *m_navigation;
};

#endif
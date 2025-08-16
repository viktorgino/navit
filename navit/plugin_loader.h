#ifndef NAVIT_PLUGIN_LOADER_H
#define NAVIT_PLUGIN_LOADER_H

#include <QObject>
#include <QPluginLoader>
#include "NavitInterfaces.h"
#include "config_loader.h"
#include "vehicle.h"

extern "C"
{
#include "debug.h"
#include "plugin.h"
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
    PluginLoader(NavitConfig &navitConfig, NavitInterface *navit, QObject *parent = nullptr);

    void loadModules();

    tracking *getTracking();
    route *getRoute();
    navigation *getNavigation();
    Vehicle *getVehicle();
    mapset *getMapset();

private:
    NavitConfig &m_navitConfig;
    NavitInterface *m_navit;

    QVector<debug *> m_debugConfigs;
    QVector<plugin *> m_plugins;

    QVector<Vehicle *> m_vehicles;

    Vehicle *m_current_vehicle;
    tracking *m_current_tracking;
    route *m_current_route;
    navigation *m_current_navigation;
    mapset *m_current_mapset;

    QMap<QString, QString> m_vehiclePlugins;

    QObject *loadPlugin(QString path);

    void loadQtPlugins(const NavitPluginConfig *plugin);

    void loadDebug(QList<NavitDebugConfig *> &debugConfigs);
    void loadPlugins(QList<NavitPluginConfig *> &plugins);
    void loadVehicles(QList<NavitVehicleConfig *> &configs);
    void loadTracking(NavitTrackingConfig &tracking);
    void loadRoute(NavitRouteConfig &route);
    void loadNavigation(NavitNavigationConfig &navigation);
    void loadMaps(QList<NavitMap *> &maps);
};

#endif
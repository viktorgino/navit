#include "plugin_loader.h"

PluginLoader::PluginLoader(NavitConfig &navitConfig, NavitHandle navit, QObject *parent) : QObject(parent),
                                                                                           m_navitConfig(navitConfig),
                                                                                           m_navit(navit)
{
    loadDebug(m_navitConfig.debug);
    loadPlugins(m_navitConfig.plugins);
    loadVehicles(m_navitConfig.vehicle);
    loadTracking(m_navitConfig.tracking);
    loadRoute(m_navitConfig.route);
    loadNavigation(m_navitConfig.navigation);
}

void PluginLoader::loadDebug(QList<NavitDebugConfig> &debugConfigs)
{
    // for (const NavitDebugConfig &debug : debugConfigs)
    // {
    // }
}

void PluginLoader::loadPlugins(QList<NavitPluginConfig> &plugins)
{
    for (const NavitPluginConfig &plugin : plugins)
    {
        auto pl = plugin_new(plugin.path.toLocal8Bit().data(), plugin.active, plugin.lazy, plugin.ondemand);
        m_plugins.append(pl);
    }
}

void PluginLoader::loadVehicles(QList<NavitVehicleConfig> &vehicles)
{
    struct attr *a = g_new0(struct attr, 0);
    for (const NavitVehicleConfig &vehicle : vehicles)
    {
        vehicle_new(NULL);
    }
}

void PluginLoader::loadTracking(NavitTrackingConfig &tracking)
{
    struct attr *a = g_new0(struct attr, 1);
    a->type = attr_cdf_histsize;
    a->u.num = tracking.cdf_histsize;
    m_tracking = tracking_new(NULL, attr_generic_set_attr(NULL, a));
}

void PluginLoader::loadRoute(NavitRouteConfig &route)
{
    struct attr *a = g_new0(struct attr, 1);
    a->type = attr_destination_distance;
    a->u.num = route.destination_distance;
    m_route = route_new(NULL, &a);
}

void PluginLoader::loadNavigation(NavitNavigationConfig &navigation)
{
    struct attr *parent = g_new0(struct attr, 1);
    parent->u.navit = m_navit;

    struct attr *a = g_new0(struct attr, 0);

    m_navigation = navigation_new(parent, &a);

    for (const NavitAnnounceConfig &announce : navigation.announce)
    {
        for (const QString &type : announce.type.split(","))
        {
            item_type itemType = item_from_name(type.toLocal8Bit().data());
            int level[] = {
                announce.level0,
                announce.level1,
                announce.level2,
            };

            if (itemType != type_none)
            {
                navigation_set_announce(m_navigation, itemType, level);
            }
            else
            {
                qWarning() << "Invalid type for announcement: " << type;
            }
        }
    }
}

tracking *PluginLoader::getTracking() { return m_tracking; }

route *PluginLoader::getRoute() { return m_route; }

navigation *PluginLoader::getNavigation() { return m_navigation; }
#include "plugin_loader.h"

PluginLoader::PluginLoader(NavitConfig &navitConfig, NavitHandle navit, QObject *parent) : QObject(parent),
                                                                                           m_navitConfig(navitConfig),
                                                                                           m_navit(navit)
{
    loadDebug(m_navitConfig.debug);
    loadPlugins(m_navitConfig.plugins);
}
void PluginLoader::loadModules()
{
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
    bool vehicleFound = false;

    struct attr *a = g_new0(struct attr, 0);
    struct attr **vehicleAttrs = NULL;

    struct attr *navit = g_new0(struct attr, 1);
    struct attr *profilename = g_new0(struct attr, 1);
    struct attr *source = g_new0(struct attr, 1);
    struct attr *name = g_new0(struct attr, 1);
    struct attr *follow = g_new0(struct attr, 1);
    struct attr *active = g_new0(struct attr, 1);

    navit->type = attr_navit;
    profilename->type = attr_profilename;
    source->type = attr_source;
    name->type = attr_name;
    follow->type = attr_follow;
    active->type = attr_active;

    navit->u.navit = m_navit;

    for (const NavitVehicleConfig &vehicle : vehicles)
    {
        if (vehicle.active)
        {
            vehicleFound = true;
            profilename->u.str = vehicle.profilename.toLocal8Bit().data();
            source->u.str = vehicle.source.toLocal8Bit().data();
            name->u.str = vehicle.name.toLocal8Bit().data();
            follow->u.num = vehicle.follow;
            active->u.num = vehicle.active;
        }
    }

    if (!vehicleFound)
    {
        qWarning() << "No active vehicle found!";
        return;
    }

    vehicleAttrs = attr_generic_set_attr(vehicleAttrs, navit);
    vehicleAttrs = attr_generic_set_attr(vehicleAttrs, profilename);
    vehicleAttrs = attr_generic_set_attr(vehicleAttrs, source);
    vehicleAttrs = attr_generic_set_attr(vehicleAttrs, name);
    vehicleAttrs = attr_generic_set_attr(vehicleAttrs, follow);
    vehicleAttrs = attr_generic_set_attr(vehicleAttrs, active);

    vehicle_new(NULL, vehicleAttrs);
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
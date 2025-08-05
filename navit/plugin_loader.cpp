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
    loadMaps(m_navitConfig.maps);
}

void PluginLoader::loadDebug(QList<NavitDebugConfig *> &debugConfigs)
{
    // for (const NavitDebugConfig &debug : debugConfigs)
    // {
    // }
}

void PluginLoader::loadPlugins(QList<NavitPluginConfig *> &plugins)
{
    for (const NavitPluginConfig *plugin : plugins)
    {
        auto pl = plugin_new(plugin->path.toLocal8Bit().data(), plugin->active, plugin->lazy, plugin->ondemand);
        m_plugins.append(pl);
    }
}

void PluginLoader::loadVehicles(QList<NavitVehicleConfig *> &vehicles)
{
    bool vehicleFound = false;

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

    for (const NavitVehicleConfig *vehicle : vehicles)
    {
        if (vehicle->active)
        {
            vehicleFound = true;
            profilename->u.str = vehicle->profilename.toLocal8Bit().data();
            source->u.str = vehicle->source.toLocal8Bit().data();
            name->u.str = vehicle->name.toLocal8Bit().data();
            follow->u.num = vehicle->follow;
            active->u.num = vehicle->active;
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

    m_vehicle = vehicle_new(NULL, vehicleAttrs);
}

void PluginLoader::loadTracking(NavitTrackingConfig &tracking)
{
    struct attr *attrs = g_new0(struct attr, 1);
    attrs->type = attr_cdf_histsize;
    attrs->u.num = tracking.cdf_histsize;
    m_tracking = tracking_new(NULL, attr_generic_set_attr(NULL, attrs));
}

void PluginLoader::loadRoute(NavitRouteConfig &route)
{
    struct attr *attrs = g_new0(struct attr, 1);
    attrs->type = attr_destination_distance;
    attrs->u.num = route.destination_distance;
    m_route = route_new(NULL, &attrs);
}

void PluginLoader::loadNavigation(NavitNavigationConfig &navigation)
{
    struct attr *parent = g_new0(struct attr, 1);
    parent->u.navit = m_navit;

    struct attr *attrs = g_new0(struct attr, 0);

    m_navigation = navigation_new(parent, &attrs);

    for (const NavitAnnounceConfig *announce : navigation.announce)
    {
        for (const QString &type : announce->type.split(","))
        {
            item_type itemType = item_from_name(type.toLocal8Bit().data());
            int level[] = {
                announce->level0,
                announce->level1,
                announce->level2,
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

void PluginLoader::loadMaps(QList<NavitMap *> &maps)
{
    struct attr *attrs = g_new0(struct attr, 0);

    m_mapset = mapset_new(NULL, &attrs);

    for (const NavitMap *map : maps)
    {
        struct attr map_attr;
        struct attr *map_attrs = g_new0(struct attr, 2);

        map_attrs[0].type = attr_type;
        map_attrs[0].u.str = map->type.toLocal8Bit().data();
        map_attrs[1].type = attr_data;
        map_attrs[1].u.str = map->data.toLocal8Bit().data();

        map_attr.type = attr_map;
        map_attr.u.map = map_new(NULL, &map_attrs);
        mapset_add_attr(m_mapset, &map_attr);
    }
}

tracking *PluginLoader::getTracking() { return m_tracking; }

route *PluginLoader::getRoute() { return m_route; }

navigation *PluginLoader::getNavigation() { return m_navigation; }

vehicle *PluginLoader::getVehicle() { return m_vehicle; }

mapset *PluginLoader::getMapset() { return m_mapset; }
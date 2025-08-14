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

void PluginLoader::loadQtPlugins(const NavitPluginConfig *plugin)
{
    struct file_wordexp *we;
    int i, count;
    char **array;

    if (plugin->path.isEmpty())
    {
        dbg(lvl_error, "Invalid path_pattern");
        return;
    }

    dbg(lvl_debug, "path=\"%s\", active=%d, lazy=%d, ondemand=%d", plugin->path.toLocal8Bit().data(), plugin->active, plugin->lazy, plugin->ondemand);

    we = file_wordexp_new(plugin->path.toLocal8Bit().data());
    count = file_wordexp_get_count(we);
    array = file_wordexp_get_array(we);
    dbg(lvl_info, "expanded to %d words", count);
    if (count != 1 || file_exists(array[0]))
    {
        for (i = 0; i < count; i++)
        {
            QString path = array[i];
            QString name = path.split("/").last();
            name = name.replace(".so", "");
            name = name.replace(".dll", "");

            QString category = name.split("_")[0];
            if (category.startsWith("lib"))
            {
                category = category.replace(0, 3, "");
            }
            name = name.split("_")[1];
            if (category == "vehicle")
            {
                m_vehiclePlugins[name] = QString(array[i]);
            }
        }
    }
    file_wordexp_destroy(we);
}

QObject *PluginLoader::loadPlugin(QString path)
{
    QPluginLoader pluginLoader(path);
    QObject *plugin = pluginLoader.instance();
    assert(plugin);
    plugin->setParent(this);
    return plugin;
}

void PluginLoader::loadVehicles(QList<NavitVehicleConfig *> &configs)
{
    bool vehicleFound = false;

    for (const NavitVehicleConfig *config : configs)
    {
        assert(config);
        if (config->active)
        {
            if (m_vehiclePlugins.contains(config->name))
            {
                vehicleFound = true;
                NavitVehicleInterface *plugin = qobject_cast<NavitVehicleInterface *>(loadPlugin(m_vehiclePlugins[config->name]));
                assert(plugin);
                m_vehicle = new Vehicle(config, plugin, this);
            }
            break;
        }
    }

    if (!vehicleFound)
    {
        qWarning() << "No active vehicle found!";
    }
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

Vehicle *PluginLoader::getVehicle() { return m_vehicle; }

mapset *PluginLoader::getMapset() { return m_mapset; }
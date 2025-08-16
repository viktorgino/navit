#include "plugin_loader.h"

PluginLoader::PluginLoader(NavitConfig &navitConfig, NavitInterface *navit, QObject *parent) : QObject(parent),
                                                                                               m_navitConfig(navitConfig),
                                                                                               m_navit(navit)
{
    assert(m_navit);
    loadDebug(m_navitConfig.debug);
    loadPlugins(m_navitConfig.plugins);
}
void PluginLoader::loadModules()
{
    loadVehicles(m_navitConfig.vehicle);
    loadTracking(m_navitConfig.tracking);
    loadRoute(m_navitConfig.route);
    loadNavigation(m_navitConfig.navigation);
    loadMapsets(m_navitConfig.mapsets);
}

void PluginLoader::loadDebug(QVector<NavitDebugConfig *> &debugConfigs)
{
    // for (const NavitDebugConfig &debug : debugConfigs)
    // {
    // }
}

void PluginLoader::loadPlugins(QVector<NavitPluginConfig *> &plugins)
{
    for (const NavitPluginConfig *plugin : plugins)
    {
        struct plugin *pl = plugin_new(plugin->path.toLocal8Bit().data(), plugin->active, plugin->lazy, plugin->ondemand);
        if (pl && plugin_has_init(pl))
        {
            m_plugins.append(pl);
        }
        else
        {
            loadQtPlugins(plugin);
        }
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
                m_vehiclePlugins[name] = path;
            }
        }
    }
    file_wordexp_destroy(we);
}

QObject *PluginLoader::loadPlugin(QString path)
{
    QPluginLoader pluginLoader(path);
    pluginLoader.load();
    if (pluginLoader.isLoaded())
    {
        qDebug() << "loaded module " << path;
    }
    else
    {
        qWarning() << "Unable to load plugin: " << path << pluginLoader.errorString();
        return nullptr;
    }
    QObject *plugin = pluginLoader.instance();
    return plugin;
}

void PluginLoader::loadVehicles(QVector<NavitVehicleConfig *> &configs)
{
    for (NavitVehicleConfig *config : configs)
    {
        QString name = config->name.toLower();
        if (!config)
        {
            qWarning() << "Invalid vehicle config!";
            continue;
        }

        if (!m_vehiclePlugins.contains(name))
        {
            qWarning() << "No plugin for:" << name;
            continue;
        }

        NavitVehicleFactory *factory = qobject_cast<NavitVehicleFactory *>(loadPlugin(m_vehiclePlugins[name]));
        if (!factory)
        {
            qWarning() << "Unable to load vehicle factory for: " << config->name << m_vehiclePlugins[config->name];
            continue;
        }
        Vehicle *vehicle = new Vehicle(m_navit, config, factory, this);

        m_vehicles.append(vehicle);

        if (config->active)
        {
            m_current_vehicle = vehicle;
        }
    }

    if (!m_current_vehicle)
    {
        qWarning() << "No active vehicle found!";
    }
}

void PluginLoader::loadTracking(NavitTrackingConfig &tracking)
{
    struct attr *attrs = g_new0(struct attr, 1);
    attrs->type = attr_cdf_histsize;
    attrs->u.num = tracking.cdf_histsize;
    m_current_tracking = tracking_new(NULL, attr_generic_set_attr(NULL, attrs));
    g_free(attrs);
}

void PluginLoader::loadRoute(NavitRouteConfig &route)
{
    struct attr *attrs = g_new0(struct attr, 1);
    attrs->type = attr_destination_distance;
    attrs->u.num = route.destination_distance;
    m_current_route = route_new(NULL, &attrs);
    g_free(attrs);
}

void PluginLoader::loadNavigation(NavitNavigationConfig &navigation)
{
    struct attr *parent = g_new0(struct attr, 1);
    parent->u.navit = m_navit;

    struct attr *attrs = g_new0(struct attr, 0);

    m_current_navigation = navigation_new(parent, &attrs);

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
                navigation_set_announce(m_current_navigation, itemType, level);
            }
            else
            {
                qWarning() << "Invalid type for announcement: " << type;
            }
        }
    }
    g_free(attrs);
    g_free(parent);
}

void PluginLoader::loadMapsets(QVector<NavitMapset *> &mapsets)
{
    for (NavitMapset *mapset : mapsets)
    {
        struct attr *attrs = g_new0(struct attr, 0);

        struct mapset *ms = mapset_new(NULL, &attrs);
        for (const NavitMap *map : mapset->getMaps())
        {
            struct attr map_attr, type_at, data_at;
            struct attr **map_attrs = g_new0(struct attr *, 1);

            type_at.type = attr_type;
            type_at.u.str = map->type.toLocal8Bit().data();
            data_at.type = attr_data;
            data_at.u.str = map->data.toLocal8Bit().data();

            map_attrs[0] = &type_at;
            map_attrs[1] = &data_at;

            map_attr.type = attr_map;
            map_attr.u.map = map_new(NULL, map_attrs);
            mapset_add_attr(ms, &map_attr);
            g_free(map_attrs);
        }
        g_free(attrs);
        m_mapsets.append(ms);
    }
}

tracking *PluginLoader::getTracking() { return m_current_tracking; }

route *PluginLoader::getRoute() { return m_current_route; }

navigation *PluginLoader::getNavigation() { return m_current_navigation; }

Vehicle *PluginLoader::getVehicle() { return m_current_vehicle; }

QVector<mapset *> &PluginLoader::getMapsets() { return m_mapsets; }

QVector<Vehicle *> &PluginLoader::getVehicles() { return m_vehicles; }

void PluginLoader::setVehicle(Vehicle *vehicle) { m_current_vehicle = vehicle; }

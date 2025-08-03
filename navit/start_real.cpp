/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <glib.h>
#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include <XGetopt.h>
#endif
#include "config_.h"
#include "item.h"
#include "coord.h"
#include "main.h"
#include "route.h"
#include "navigation.h"
#include "track.h"
#include "debug.h"
#include "event.h"
#include "xmlconfig.h"
#include "file.h"
#include "search.h"
#include "start_real.h"
#include "linguistics.h"
#include "navit_nls.h"
#include "atom.h"
#include "command.h"
#include "geom.h"
#include "traffic.h"
#include "plugin.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QThread>
#include <QDebug>

#include "config_loader.h"
#include "plugin_loader.h"
#include "navit.h"

int main_argc;
char *const *main_argv;

#ifndef USE_PLUGINS
extern void builtin_init(void);
#endif /* USE_PLUGINS*/

int load_xml_config()
{
    GList *list = NULL, *li;
    xmlerror *error = NULL;
    char *config_file = NULL;

#ifdef HAVE_GETOPT_H
    opterr = 0; // don't bomb out on errors.
#endif          /* _MSC_VER */

    // if config file is explicitely given only look for it, otherwise try std paths
    if (config_file)
    {
        list = g_list_append(list, g_strdup(config_file));
    }
    else
    {
        list = g_list_append(list, g_strjoin(NULL, getenv("NAVIT_USER_DATADIR"), "/navit.xml", NULL));
        list = g_list_append(list, g_strdup("navit.xml.local"));
        list = g_list_append(list, g_strdup("navit.xml"));
        list = g_list_append(list, g_strjoin(NULL, getenv("NAVIT_SHAREDIR"), "/navit.xml.local", NULL));
        list = g_list_append(list, g_strjoin(NULL, getenv("NAVIT_SHAREDIR"), "/navit.xml", NULL));
#ifndef _WIN32
        list = g_list_append(list, g_strdup("/etc/navit/navit.xml"));
#endif
    }
    li = list;
    for (;;)
    {
        if (li == NULL)
        {
            // We have not found an existing config file from all possibilities
            dbg(lvl_error, "%s", _("No config file navit.xml, navit.xml.local found"));
            return 4;
        }
        // Try the next config file possibility from the list
        config_file = static_cast<char *>(li->data);
        dbg(lvl_debug, "trying %s", config_file);
        if (file_exists(config_file))
        {
            break;
        }
        g_free(config_file);
        li = g_list_next(li);
    }

    qDebug() << "Loading config from: " << config_file;
    if (!config_load(config_file, &error))
    {
        dbg(lvl_error, _("Error parsing config file '%s': %s"), config_file, error ? error->message : "");
    }
    else
    {
        dbg(lvl_info, _("Using config file '%s'"), config_file);
    }
    if (!config)
    {
        dbg(lvl_error, _("Error: No configuration found in config file '%s'"), config_file);
    }
    while (li)
    {
        g_free(li->data);
        li = g_list_next(li);
    }
    g_list_free(list);
    return 0;
}

int navit_enter(int argc, char *const *argv)
{
    char *cp;

    char program_name[] = "Navit";
    main_argc = argc;
    main_argv = argv;

    atom_init();
    main_init(argv[0]);
    navit_nls_main_init();
    //    debug_init(argv[0]);

    cp = getenv("NAVIT_LOGFILE");
    if (cp)
    {
        debug_set_logfile(cp);
    }

    file_init();
#ifndef USE_PLUGINS
    builtin_init();
#endif
    // Add plugins

    route_init();
    navigation_init();
    tracking_init();
    search_init();
    linguistics_init();
    geom_init();
    traffic_init();
    debug_init(program_name);

    // if (load_xml_config() > 0)
    // {
    //     return 4;
    // }

    // if (!config_get_attr(config, attr_plugins, &plugins, NULL))
    // {
    //     dbg(lvl_error, "Internal initialization failed, can't get plugins");
    //     exit(6);
    // }
    // dbg(lvl_error, "Loading graphics");

    // add_plugin(&navit, plugins.u.plugins, "graphics/libnavit_graphics");
    // plugins_init(plugins.u.plugins);
    // plugin_register_category(plugin_category_graphics, "qt5", );

    return 0;
}

int main(int argc, char **argv)
{

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QGuiApplication app(argc, argv);
    app.setApplicationName(QString("Navit"));
    app.setOrganizationName(QString("navit"));
    app.setOrganizationDomain(QString("navit-project.org"));

    navit_enter(argc, argv);

    QQmlApplicationEngine engine;
    ConfigLoader configLoader(&engine);
    NavitConfig &navitConfig = configLoader.loadNavit("navit.json");

    PluginLoader pluginLoader(navitConfig, &engine);
    pluginLoader.loadPlugins(navitConfig.plugins);

    Navit navit(navitConfig, pluginLoader, &engine);

    engine.addImportPath("navit/");
    engine.addImportPath("navit/graphics");

    engine.load(QUrl(QStringLiteral("qrc:/mainWindow.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    qDebug() << "Loading QML";
    int ret = app.exec();
    navit_exit();
    qDebug() << "Finished with : " << ret;
    return ret;
}

void navit_exit()
{
    /* TODO: Android actually has no event loop, so we can't free all allocated resources here. Have to find better place to
     *  free all allocations on program exit. And don't forget to free all the stuff allocated in the code above.
     */
    linguistics_free();
    debug_finished();
}

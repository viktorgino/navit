///**
// * Navit, a modular navigation system.
// * Copyright (C) 2005-2010 Navit Team
// *
// * This program is free software; you can redistribute it and/or
// * modify it under the terms of the GNU General Public License
// * version 2 as published by the Free Software Foundation.
// *
// * This program is distributed in the hope that it will be useful,
// * but WITHOUT ANY WARRANTY; without even the implied warranty of
// * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// * GNU General Public License for more details.
// *
// * You should have received a copy of the GNU General Public License
// * along with this program; if not, write to the
// * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
// * Boston, MA  02110-1301, USA.
// */
//// style with: clang-format -style=WebKit -i *

// #include <QQmlApplicationEngine>
// #include <QQmlContext>
// #include <QQmlEngine>

// #include <glib.h>

// extern "C" {
// #include "item.h" /* needs to be first, as attr.h depends on it */

// #include "debug.h"

// #include "point.h" /* needs to be before graphics.h */

// #include "navit.h"
// #include "graphics.h"
// }

// #include "navitinstance.h"
// #include "navitpoimodel.h"
// #include "navitrecentsmodel.h"
// #include "navitfavouritesmodel.h"
// #include "navitsearchmodel.h"
// #include "navitroute.h"
// #include "navitlayoutsmodel.h"
// #include "navitlayersmodel.h"
// #include "navitvehiclesmodel.h"
// #include "navitmapsmodel.h"

// static int init_qml_gui(struct navit* nav, struct graphics* gra) {
//     dbg(lvl_debug, "enter");

//    /* expect to have qt5 graphics. So get the qml engine prepared by graphics */
//    QQmlApplicationEngine* engine = (QQmlApplicationEngine*)graphics_get_data(gra, "engine");
//    if (engine == NULL) {
//        dbg(lvl_error, "Graphics doesn't seem to be qt5, or doesn't have QML. Cannot set graphics");
//        return 1;
//    }

//    qmlRegisterType<NavitPOIModel>("Navit.POI", 1, 0, "NavitPOIModel");
//    qmlRegisterType<NavitRecentsModel>("Navit.Recents", 1, 0, "NavitRecentsModel");
//    qmlRegisterType<NavitFavouritesModel>("Navit.Favourites", 1, 0, "NavitFavouritesModel");
//    qmlRegisterType<NavitSearchModel>("Navit.Search", 1, 0, "NavitSearchModel");
//    qmlRegisterType<NavitRoute>("Navit.Route", 1, 0, "NavitRoute");
//    qmlRegisterType<NavitLayoutsModel>("Navit.Layouts", 1, 0, "NavitLayouts");
//    qmlRegisterType<NavitLayersModel>("Navit.Layers", 1, 0, "NavitLayers");
//    qmlRegisterType<NavitVehiclesModel>("Navit.Vehicles", 1, 0, "NavitVehicles");
//    qmlRegisterType<NavitMapsModel>("Navit.Maps", 1, 0, "NavitMaps");

//    QObject* loader = engine->rootObjects().value(0)->findChild<QObject*>("navit_loader");
//    if (loader != NULL) {
//        dbg(lvl_debug, "navit_loader found");
//        /* load our root window into the loader component */
//        loader->setProperty("source", "qrc:/NavitGUI/MainLayout.qml");
//    }

//    navit_draw(nav);

//    return 0;
//}

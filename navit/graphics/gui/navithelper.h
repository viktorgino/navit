#ifndef NAVITHELPER_H
#define NAVITHELPER_H

#include <QString>
#include <QDebug>
#include <QVariant>

#include "navitinstance.h"

#include <glib.h>
extern "C"
{
#include "config.h"
#include "item.h" /* needs to be first, as attr.h depends on it */
#include "navit.h"

#include "coord.h"
#include "attr.h"
#include "xmlconfig.h" // for NAVIT_OBJECT
#include "layout.h"
#include "map.h"
#include "transform.h"

#include "mapset.h"
#include "search.h"
#include "bookmarks.h"

#include "event.h"
#include "callback.h"
#include "layout.h"
}

class NavitHelper
{
public:
    NavitHelper();

    static QString getAddress(Navit &navit, struct coord center, QString filter = "");
    static QVariantMap getPOI(Navit &navit, struct coord center, int distance = 2);
    static QString getClosest(QList<QVariantMap> items, int maxDistance = -1);
    static QString formatDist(int dist);
    static pcoord positionToPcoord(Navit &navit, int x, int y);
    static coord positionToCoord(Navit &navit, int x, int y);
    static pcoord coordToPcoord(Navit &navit, int x, int y);
    static void setDestination(Navit &navit, QString label, int x, int y);
    static void setPosition(Navit &navit, int x, int y);
    static void addBookmark(Navit &navit, QString label, int x, int y);
    static void addStop(Navit &navit, int position, QString label, int x, int y);
    static char *get_icon(Navit &navit, struct item *item);
};

#endif // NAVITHELPER_H

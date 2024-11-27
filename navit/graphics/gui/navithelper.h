#ifndef NAVITHELPER_H
#define NAVITHELPER_H

#include <QString>
#include <QDebug>
#include <QVariant>

#include "navit.h"
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

    static QString getAddress(NavitInterface &navit, struct coord center, QString filter = "");
    static QVariantMap getPOI(NavitInterface &navit, struct coord center, int distance = 2);
    static QString getClosest(QList<QVariantMap> items, int maxDistance = -1);
    static QString formatDist(int dist);
    static pcoord positionToPcoord(NavitInterface &navit, int x, int y);
    static coord positionToCoord(NavitInterface &navit, int x, int y);
    static pcoord coordToPcoord(NavitInterface &navit, int x, int y);
    static void setDestination(NavitInterface &navit, QString label, int x, int y);
    static void setPosition(NavitInterface &navit, int x, int y);
    static void addBookmark(NavitInterface &navit, QString label, int x, int y);
    static void addStop(NavitInterface &navit, int position, QString label, int x, int y);
    static char *get_icon(NavitInterface &navit, struct item *item);
};

#endif // NAVITHELPER_H

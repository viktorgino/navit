#ifndef NAVITINSTANCE_H
#define NAVITINSTANCE_H

#include <QObject>
#include "navit.h"
#include "NavitInterfaces.h"

extern "C"
{
#include "../config.h"
#include "item.h" /* needs to be first, as attr.h depends on it */

#include "callback.h"
#include "color.h"
#include "debug.h"
#include "event.h"

#include "point.h" /* needs to be before graphics.h */

#include "graphics.h"
#include "plugin.h"
#include "plugin_def.h"
#include "window.h"
}

class NavitInstance : public QObject
{
    Q_OBJECT
public:
    explicit NavitInstance(NavitInterface &nav, NavitGraphicsInterface &graphics) : QObject(&dynamic_cast<QObject &>(graphics)), m_graphics(graphics), m_navit(nav) {}
    explicit NavitInstance(NavitInstance const &n) : QObject(&dynamic_cast<QObject &>(n.m_graphics)), m_graphics(n.m_graphics), m_navit(n.m_navit) {}
    NavitInterface &getNavit()
    {
        return m_navit;
    }
    NavitGraphicsInterface &getGraphics()
    {
        return m_graphics;
    }
    void emit_update()
    {
        emit update();
    }
    struct graphics_priv *m_graphics_priv = nullptr;
    QPixmap getPixmap();
signals:
    void update();

private:
    NavitGraphicsInterface &m_graphics;
    NavitInterface &m_navit;
};

#endif // NAVITINSTANCE_H

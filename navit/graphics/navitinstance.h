#ifndef NAVITINSTANCE_H
#define NAVITINSTANCE_H

#include <QObject>
#include "navit.h"

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
    explicit NavitInstance(Navit &nav, struct graphics_priv *gp, QObject *parent = nullptr) : m_graphics_priv(gp), m_navit(nav) {}
    Navit &getNavit()
    {
        return m_navit;
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
    Navit &m_navit;
};

#endif // NAVITINSTANCE_H

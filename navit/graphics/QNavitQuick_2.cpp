/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2019 Navit Team
 * Copyright (C) 2019 Viktor Verebelyi <me@viktorgino.me>
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

#include <glib.h>
#include <unistd.h>

#include "config.h"
#include "item.h" /* needs to be first, as attr.h depends on it */
#include "navit.h"

#include "callback.h"
#include "color.h"
#include "debug.h"
#include "event.h"
#include "transform.h"

#include "point.h" /* needs to be before graphics.h */

#include "graphics.h"
#include "keys.h"
#include "plugin.h"
#include "window.h"

#include "bookmarks.h"

#if defined(WINDOWS) || defined(WIN32) || defined(HAVE_API_WIN32_CE)
#include <windows.h>
#endif
#include "QNavitQuick_2.h"
#include "graphics_qt5.h"
#include <QPainter>

#include <QOpenGLFramebufferObject>
QNavitQuick_2::QNavitQuick_2(QQuickItem *parent)
    : QQuickPaintedItem(parent),
      graphics_priv(nullptr),
      m_moveX(0),
      m_moveY(0)
{
    setAcceptedMouseButtons(Qt::AllButtons);

    connect(this, &QNavitQuick_2::onResizeEvent, qt5_timer, &Qt5GraphicsWorker::resizeEvent);
    connect(this, &QNavitQuick_2::onMapMove, qt5_timer, &Qt5GraphicsWorker::mapMove);
    connect(this, &QNavitQuick_2::onZoomIn, qt5_timer, &Qt5GraphicsWorker::zoomIn);
    connect(this, &QNavitQuick_2::onZoomOut, qt5_timer, &Qt5GraphicsWorker::zoomOut);
    connect(this, &QNavitQuick_2::onZoomToRoute, qt5_timer, &Qt5GraphicsWorker::zoomToRoute);
    connect(this, &QNavitQuick_2::onSetNumAttr, qt5_timer, &Qt5GraphicsWorker::setNumAttr);
    connect(this, &QNavitQuick_2::onCenterOnPosition, qt5_timer, &Qt5GraphicsWorker::centerOnPosition);
}

void QNavitQuick_2::paintOverlays(QPainter *painter, GraphicsQt5 *gp, QPaintEvent *event)
{
    foreach (auto &overlay, gp->overlay_get_all())
    {
        if (!overlay->disabled())
        {
            QRect rr = overlay->rect();
            if (event->rect().intersects(rr))
            {
                qDebug() << "Draw overlay" << rr.x(), rr.y(), rr.width(), rr.height();

                painter->drawPixmap(rr.x(), rr.y(), overlay->pixmap());
                paintOverlays(painter, overlay, event);
            }
        }
    }
}

void QNavitQuick_2::paint(QPainter *painter)
{
    QPaintEvent event = QPaintEvent(QRect(boundingRect().x(), boundingRect().y(), boundingRect().width(),
                                          boundingRect().height()));
    GraphicsContextQt5 *background = static_cast<GraphicsContextQt5 *>(graphics_priv->background());
    /* color background if any */
    if (background != nullptr)
    {
        painter->setPen(background->pen());
        painter->fillRect(boundingRect(), background->brush());
    }

    painter->drawPixmap(m_moveX, m_moveY, graphics_priv->pixmap(),
                        boundingRect().x(), boundingRect().y(),
                        boundingRect().width(), boundingRect().height());

    /* disable on root pane disables ALL overlays (for drag of background) */
    if (!graphics_priv->disabled())
    {
        paintOverlays(painter, graphics_priv, &event);
    }
    // qDebug() << "Painting thread : " << QThread::currentThread();
    updateZoomLevel();
}

void QNavitQuick_2::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    if (graphics_priv == nullptr)
    {
        qWarning("Context not set, aborting");
        return;
    }
    graphics_priv->resize(width(), height());
    /* if the root window got resized, tell navit about it */
    if (graphics_priv->is_root())
    {
        emit onResizeEvent(m_navitInstance, width(), height());
    }
}

void QNavitQuick_2::mousePressEvent(QMouseEvent *event)
{
    QPoint loc;
    loc.setX(event->x());
    loc.setY(event->y());
    if (event->button() == Qt::LeftButton)
    {
        m_originX = event->x();
        m_originY = event->y();
        emit leftButtonClicked(loc);
    }
    else if (event->button() == Qt::RightButton)
    {
        emit rightButtonClicked(loc);
    }
}

void QNavitQuick_2::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        mapMove(m_originX, m_originY, event->x(), event->y());
    }
}

void QNavitQuick_2::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton)
    {
        setFollowVehicle(false);
        if (event->modifiers() & Qt::ShiftModifier)
        {
            int pitch = qFloor((m_originY - event->y()) / 10);
            int orientation = m_orientation + (qFloor((event->x() - m_originX)) / 10);

            if (m_pitch + pitch < 0)
            {
                setPitch(0);
            }
            else if (m_pitch + pitch > 60)
            {
                setPitch(60);
            }
            else
            {
                setPitch(m_pitch + pitch);
            }

            setOrientation(orientation % 360);
        }
        else
        {
            m_moveX = event->x() - m_originX;
            m_moveY = event->y() - m_originY;
            update();
        }
        emit positionChanged(event);
    }
}

void QNavitQuick_2::wheelEvent(QWheelEvent *event)
{
    if (event->angleDelta().y() > 0)
    {
        zoomInToPoint(2, event->position().x(), event->position().y());
    }
    else
    {
        zoomOutFromPoint(2, event->position().x(), event->position().y());
    }
}

void QNavitQuick_2::draw()
{
    m_moveX = 0;
    m_moveY = 0;
    update();
}
void QNavitQuick_2::mapMove(int originX, int originY, int destinationX, int destinationY)
{
    struct point *origin = new struct point;
    origin->x = originX;
    origin->y = originY;
    struct point *destination = new struct point;
    destination->x = destinationX;
    destination->y = destinationY;

    emit onMapMove(m_navitInstance, origin, destination);
    // navit_drag_map(m_navitInstance->getNavit(), origin, destination);
}

void QNavitQuick_2::zoomIn(int zoomLevel, point *p)
{
    emit onZoomIn(m_navitInstance, zoomLevel, p);
    // navit_zoom_in(m_navitInstance->getNavit(), zoomLevel, p);
}
void QNavitQuick_2::zoomOut(int zoomLevel, point *p)
{
    emit onZoomOut(m_navitInstance, zoomLevel, p);
    // navit_zoom_out(m_navitInstance->getNavit(), zoomLevel, p);
}

void QNavitQuick_2::zoomInToPoint(int zoomLevel, int x, int y)
{
    if (m_navitInstance)
    {
        struct point *p = new struct point;
        p->x = x;
        p->y = y;
        zoomIn(zoomLevel, p);
    }
}

void QNavitQuick_2::zoomOutFromPoint(int zoomLevel, int x, int y)
{
    if (m_navitInstance)
    {
        struct point *p = new struct point;
        p->x = x;
        p->y = y;
        zoomOut(zoomLevel, p);
    }
}
void QNavitQuick_2::zoomToRoute()
{
    emit onZoomToRoute(m_navitInstance);
    // navit_zoom_to_route(m_navitInstance->getNavit(), 1);
}

void QNavitQuick_2::setNavitNumProperty(enum attr_type type, int value)
{
    if (m_navitInstance)
    {
        struct attr attr;

        attr.type = type;
        attr.u.num = value;

        emit onSetNumAttr(m_navitInstance, &attr);
        // navit_set_attr(m_navitInstance->getNavit(), &attr);
    }
}

int QNavitQuick_2::getNavitNumProperty(enum attr_type type)
{
    struct attr attr;
    if (m_navitInstance)
    {
        m_navitInstance->getNavit().get_attr(type, &attr, nullptr);
    }
    return attr.u.num;
}

void QNavitQuick_2::setPitch(int pitch)
{
    setNavitNumProperty(attr_pitch, pitch);
}

void QNavitQuick_2::setFollowVehicle(bool followVehicle)
{
    // qDebug() << "setFollowVehicle " << followVehicle;
    setNavitNumProperty(attr_follow, followVehicle);
    setNavitNumProperty(attr_follow_cursor, followVehicle);
}

void QNavitQuick_2::setTracking(bool tracking)
{
    setNavitNumProperty(attr_tracking, tracking);
}

void QNavitQuick_2::setAutozoom(bool autoZoom)
{
    setNavitNumProperty(attr_autozoom_active, (int)autoZoom);
}

void QNavitQuick_2::setOrientation(int orientation)
{
    setNavitNumProperty(attr_orientation, orientation);
}

void QNavitQuick_2::addBookmark(QString label, int x, int y)
{
    NavitHelper::addBookmark(m_navitInstance->getNavit(), label, x, y);
}
NavitInstance *QNavitQuick_2::navitInstance()
{
    return m_navitInstance;
}

void QNavitQuick_2::setNavitInstance(NavitInstance *navit)
{
    m_navitInstance = navit;
    if (m_navitInstance)
    {
        NavitInterface &navit_interface = m_navitInstance->getNavit();
        graphics_priv = static_cast<GraphicsQt5 *>(&m_navitInstance->getGraphics());

        QObject::connect(navit, SIGNAL(update()), this, SLOT(draw()));

        navit_interface.add_callback(callback_new_attr_1(callback_cast(QNavitQuick_2::attributeCallbackHandler),
                                                         attr_orientation, this));
        navit_interface.add_callback(callback_new_attr_1(callback_cast(QNavitQuick_2::attributeCallbackHandler),
                                                         attr_follow_cursor, this));
        navit_interface.add_callback(callback_new_attr_1(callback_cast(QNavitQuick_2::attributeCallbackHandler),
                                                         attr_tracking, this));
        navit_interface.add_callback(callback_new_attr_1(callback_cast(QNavitQuick_2::attributeCallbackHandler),
                                                         attr_autozoom_active, this));
        navit_interface.add_callback(callback_new_attr_1(callback_cast(QNavitQuick_2::attributeCallbackHandler),
                                                         attr_pitch, this));
        m_orientation = getNavitNumProperty(attr_orientation);
        m_followVehicle = getNavitNumProperty(attr_follow_cursor);
        m_tracking = getNavitNumProperty(attr_tracking);
        m_autoZoom = getNavitNumProperty(attr_autozoom_active);
        m_pitch = getNavitNumProperty(attr_pitch);

        setNavitNumProperty(attr_autozoom, 1);      // Sets auto zoom secs
        setNavitNumProperty(attr_autozoom_min, 0);  // Set auto zoom minimum distance
        setNavitNumProperty(attr_autozoom_max, 30); // Set auto zoom minimum distanceattr_timeout
        setNavitNumProperty(attr_timeout, 1);       // Set auto zoom minimum distance
        emit propertiesChanged();
    }
}

QString QNavitQuick_2::getAddress(int x, int y)
{
    if (m_navitInstance)
    {
        coord c = NavitHelper::positionToCoord(m_navitInstance->getNavit(), x, y);
        return NavitHelper::getAddress(m_navitInstance->getNavit(), c);
    }
    return "";
}

void QNavitQuick_2::centerOnPosition()
{
    emit onCenterOnPosition(m_navitInstance);
    // navit_set_center_cursor_draw(m_navitInstance->getNavit());
}

void QNavitQuick_2::updateZoomLevel()
{
    if (m_navitInstance)
    {
        struct transformation *trans = m_navitInstance->getNavit().get_trans();
        long scale = transform_get_scale(trans);

        int i = 0;
        while (scale >> i)
        {
            i++;
        }

        m_zoomLevel = 19 - i;
        emit zoomLevelChanged();
    }
}

void QNavitQuick_2::attributeCallbackHandler(QNavitQuick_2 *navitGraph, NavitHandle *this_, attr *attr)
{
    navitGraph->attributeCallback(attr);
}
void QNavitQuick_2::attributeCallback(attr *attr)
{
    switch (attr->type)
    {
    case attr_orientation:
        m_orientation = attr->u.num;
        break;
    case attr_follow_cursor:
        m_followVehicle = attr->u.num;
        break;
    case attr_tracking:
        m_tracking = attr->u.num;
        break;
    case attr_autozoom_active:
        m_autoZoom = attr->u.num;
        break;
    case attr_pitch:
        m_pitch = attr->u.num;
        break;
    default:
        return;
    }
    emit propertiesChanged();
}

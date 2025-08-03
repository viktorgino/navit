#pragma once

#include <QObject>
#include <cstdint>

// class NavitConfig : QObject
// {
//     // <!ATTLIST navit center CDATA #REQUIRED>
//     // <!ATTLIST navit zoom CDATA #REQUIRED>
//     // <!ATTLIST navit tracking CDATA #REQUIRED>
//     // <!ATTLIST navit orientation CDATA #REQUIRED>
//     // <!ATTLIST navit recent_dest CDATA #IMPLIED>
//     // <!ATTLIST navit drag_bitmap CDATA #IMPLIED>
//     // <!ATTLIST navit default_layout CDATA #IMPLIED>
//     // <!ATTLIST navit tunnel_nightlayout CDATA #IMPLIED>
//     // <!ATTLIST navit nightlayout_auto CDATA #IMPLIED>
//     // <!ATTLIST navit sunrise_degrees CDATA #IMPLIED>
//     Q_OBJECT
//     Q_PROPERTY(QString center MEMBER m_center NOTIFY centerChanged)
//     Q_PROPERTY(uint8_t zoom MEMBER m_zoom NOTIFY zoomChanged)
//     Q_PROPERTY(bool vehicle_tracking MEMBER m_vehicle_tracking NOTIFY vehicle_trackingChanged)
//     Q_PROPERTY(int32_t orientation MEMBER m_orientation NOTIFY orientationChanged)
//     Q_PROPERTY(uint32_t recent_dest MEMBER m_recent_dest NOTIFY recent_destChanged)
//     Q_PROPERTY(bool drag_bitmap MEMBER m_drag_bitmap NOTIFY drag_bitmapChanged)
//     Q_PROPERTY(QString default_layout MEMBER m_default_layout NOTIFY default_layoutChanged)
//     Q_PROPERTY(QString tunnel_nightlayout MEMBER m_tunnel_nightlayout NOTIFY default_layoutChanged)
//     Q_PROPERTY(bool nightlayout_auto MEMBER m_nightlayout_auto NOTIFY default_layoutChanged)
//     Q_PROPERTY(int32_t sunrise_degrees MEMBER m_sunrise_degrees NOTIFY default_layoutChanged)

// signals:
//     void centerChanged();
//     void zoomChanged();
//     void vehicle_trackingChanged();
//     void orientationChanged();
//     void recent_destChanged();
//     void drag_bitmapChanged();
//     void default_layoutChanged();
//     void tunnel_nightlayoutChanged();
//     void nightlayout_autoChanged();
//     void sunrise_degrees();

// private:
//     QString m_center;
//     uint8_t m_zoom;
//     bool m_vehicle_tracking;
//     int32_t m_orientation;
//     uint32_t m_recent_dest;
//     bool m_drag_bitmap;
//     QString m_default_layout;
//     QString m_tunnel_nightlayout;
//     bool m_nightlayout_auto;
//     int32_t m_sunrise_degrees;
// };

// class ItemGraphElement : QObject
// {
//     // <!ELEMENT itemgra (polygon|polyline|text|circle|icon|image|arrows|spikes)*>
//     // <!ATTLIST itemgra item_types CDATA #IMPLIED>
//     // <!ATTLIST itemgra order CDATA #IMPLIED>
//     // <!ATTLIST itemgra speed_range CDATA #IMPLIED>
// };
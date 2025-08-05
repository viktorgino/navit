#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QColor>

enum LayoutElementType
{
    LayoutElementPoint,
    LayoutElementPolyline,
    LayoutElementPolygon,
    LayoutElementCircle,
    LayoutElementText,
    LayoutElementIcon,
    LayoutElementImage,
    LayoutElementArrows,
    LayoutElementSpikes
};
class LayoutCoord : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int x MEMBER m_x REQUIRED)
    Q_PROPERTY(int y MEMBER m_y REQUIRED)

public:
    int getX()
    {
        return m_x;
    }
    int getY()
    {
        return m_y;
    }

private:
    int m_x;
    int m_y;
};

class LayoutItemGraphItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString color MEMBER m_color)
    Q_PROPERTY(int oneway MEMBER m_oneway)
    Q_PROPERTY(int text_size MEMBER m_text_size)
    Q_PROPERTY(QList<LayoutCoord *> *coords READ getCoords CONSTANT)
    Q_PROPERTY(LayoutElementType type MEMBER m_type)

public:
    explicit LayoutItemGraphItem(LayoutElementType type, QObject *parent = nullptr) : QObject(parent), m_type(type) {}
    LayoutElementType getType()
    {
        return m_type;
    }
    QString getColor()
    {
        return m_color;
    }
    int getOneway()
    {
        return m_oneway;
    }
    int getTextSize()
    {
        return m_text_size;
    }
    QList<LayoutCoord *> *getCoords()
    {
        return &m_coords;
    }

private:
    LayoutElementType m_type;
    QString m_color;
    int m_oneway;
    int m_text_size;
    QList<LayoutCoord *> m_coords;
    Q_ENUM(LayoutElementType)
};

class LayoutSpikes : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER m_width)
    Q_PROPERTY(int distance MEMBER m_distance)

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;
    int getWidth()
    {
        return m_width;
    }
    int getDistance()
    {
        return m_distance;
    }

private:
    int m_width;
    int m_distance;
};

class LayoutArrows : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER m_width)

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;
    int getWidth()
    {
        return m_width;
    }

private:
    int m_width;
};

class LayoutIcon : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(QString src MEMBER m_src REQUIRED)
    Q_PROPERTY(int w MEMBER m_w)
    Q_PROPERTY(int h MEMBER m_h)
    Q_PROPERTY(int x MEMBER m_x)
    Q_PROPERTY(int y MEMBER m_y)
    Q_PROPERTY(int rotation MEMBER m_rotation)

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;
    QString getSrc()
    {
        return m_src;
    }
    int getW()
    {
        return m_w;
    }
    int getH()
    {
        return m_h;
    }
    int getX()
    {
        return m_x;
    }
    int getY()
    {
        return m_y;
    }
    int getRotation()
    {
        return m_rotation;
    }

private:
    QString m_src;
    int m_w;
    int m_h;
    int m_x;
    int m_y;
    int m_rotation;
};

class LayoutCircle : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int radius MEMBER m_radius REQUIRED)
    Q_PROPERTY(int width MEMBER m_width)
    Q_PROPERTY(QString background_color MEMBER m_background_color)

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;
    int getRadius()
    {
        return m_radius;
    }
    int getWidth()
    {
        return m_width;
    }
    QString getBackgroundColor()
    {
        return m_background_color;
    }

private:
    int m_radius;
    int m_width;
    QString m_background_color;
};

class LayoutText : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(QString background_color MEMBER m_background_color)

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;
    QString getBackgroundColor()
    {
        return m_background_color;
    }

private:
    QString m_background_color;
};

class LayoutPolyline : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER m_width)
    Q_PROPERTY(QString dash MEMBER m_dash)
    Q_PROPERTY(int offset MEMBER m_offset)
    Q_PROPERTY(int directed MEMBER m_directed)
    Q_PROPERTY(int radius MEMBER m_radius)

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;
    int getWidth()
    {
        return m_width;
    }
    QString getDash()
    {
        return m_dash;
    }
    int getOffset()
    {
        return m_offset;
    }
    int getDirected()
    {
        return m_directed;
    }
    int getRadius()
    {
        return m_radius;
    }

private:
    int m_width;
    QString m_dash;
    int m_offset;
    int m_directed;
    int m_radius;
};

class LayoutPolygon : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(QString src MEMBER m_src)
    Q_PROPERTY(int w MEMBER m_w)
    Q_PROPERTY(int h MEMBER m_h)
    Q_PROPERTY(int x MEMBER m_x)
    Q_PROPERTY(int y MEMBER m_y)
    Q_PROPERTY(int rotation MEMBER m_rotation)

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;
    QString getSrc()
    {
        return m_src;
    }
    int getW()
    {
        return m_w;
    }
    int getH()
    {
        return m_h;
    }
    int getX()
    {
        return m_x;
    }
    int getY()
    {
        return m_y;
    }
    int getRotation()
    {
        return m_rotation;
    }

private:
    QString m_src;
    int m_w;
    int m_h;
    int m_x;
    int m_y;
    int m_rotation;
};

class LayoutImage : public LayoutItemGraphItem
{
    Q_OBJECT

public:
    using LayoutItemGraphItem::LayoutItemGraphItem;

private:
};

class LayoutItemGraph : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString item_types MEMBER m_item_types)
    Q_PROPERTY(QString order MEMBER m_order)
    Q_PROPERTY(QString speed_range MEMBER m_speed_range)
    Q_PROPERTY(QList<LayoutItemGraphItem *> *items READ getItems CONSTANT)

public:
    QString getItemTypes()
    {
        return m_item_types;
    }
    QString getOrder()
    {
        return m_order;
    }
    QString getSpeedRange()
    {
        return m_speed_range;
    }
    QList<LayoutItemGraphItem *> *getItems()
    {
        return &m_items;
    }

private:
    QString m_item_types;
    QString m_order;
    QString m_speed_range;
    QList<LayoutItemGraphItem *> m_items;
};

class LayoutCursor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int w MEMBER m_w REQUIRED)
    Q_PROPERTY(int h MEMBER m_h REQUIRED)
    Q_PROPERTY(QList<LayoutItemGraph *> *itemgra READ getItemgra CONSTANT)

public:
    int getW()
    {
        return m_w;
    }
    int getH()
    {
        return m_h;
    }
    QList<LayoutItemGraph *> *getItemgra()
    {
        return &m_itemgra;
    }

private:
    int m_w;
    int m_h;
    QList<LayoutItemGraph *> m_itemgra;
};

class LayoutLayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString enabled MEMBER m_enabled)
    Q_PROPERTY(QString name MEMBER m_name)
    Q_PROPERTY(QString details MEMBER m_details)
    Q_PROPERTY(QString order MEMBER m_order)
    Q_PROPERTY(QString ref MEMBER m_ref)
    Q_PROPERTY(QString active MEMBER m_active)
    Q_PROPERTY(QList<LayoutItemGraph *> *itemgraphs READ getItemgraphs CONSTANT)

public:
    QString getEnabled()
    {
        return m_enabled;
    }
    QString getName()
    {
        return m_name;
    }
    QString getDetails()
    {
        return m_details;
    }
    QString getOrder()
    {
        return m_order;
    }
    QString getRef()
    {
        return m_ref;
    }
    QString getActive()
    {
        return m_active;
    }
    QList<LayoutItemGraph *> *getItemgraphs()
    {
        return &m_itemgraphs;
    }

private:
    QString m_enabled;
    QString m_name;
    QString m_details;
    QString m_order;
    QString m_ref;
    QString m_active;
    QList<LayoutItemGraph *> m_itemgraphs;
};

class Layout : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER m_name REQUIRED)
    Q_PROPERTY(int active MEMBER m_active)
    Q_PROPERTY(QString color MEMBER m_color)
    Q_PROPERTY(QString font MEMBER m_font)
    Q_PROPERTY(QString daylayout MEMBER m_daylayout)
    Q_PROPERTY(QString nightlayout MEMBER m_nightlayout)
    Q_PROPERTY(int icon_w MEMBER m_icon_w)
    Q_PROPERTY(int icon_h MEMBER m_icon_h)
    Q_PROPERTY(QString underground_alpha MEMBER m_underground_alpha)
    Q_PROPERTY(QList<LayoutCursor *> *cursors READ getCursors CONSTANT)
    Q_PROPERTY(QList<LayoutLayer *> *layers READ getLayers CONSTANT)

public:
    QString getName()
    {
        return m_name;
    }
    int getActive()
    {
        return m_active;
    }
    QString getColor()
    {
        return m_color;
    }
    QString getFont()
    {
        return m_font;
    }
    QString getDaylayout()
    {
        return m_daylayout;
    }
    QString getNightlayout()
    {
        return m_nightlayout;
    }
    int getIconW()
    {
        return m_icon_w;
    }
    int getIconH()
    {
        return m_icon_h;
    }
    QString getUndergroundAlpha()
    {
        return m_underground_alpha;
    }
    QList<LayoutCursor *> *getCursors()
    {
        return &m_cursors;
    }
    QList<LayoutLayer *> *getLayers()
    {
        return &m_layers;
    }

private:
    QString m_name;
    int m_active;
    QString m_color;
    QString m_font;
    QString m_daylayout;
    QString m_nightlayout;
    int m_icon_w;
    int m_icon_h;
    QString m_underground_alpha;
    QList<LayoutCursor *> m_cursors;
    QList<LayoutLayer *> m_layers;
};

Q_DECLARE_METATYPE(LayoutElementType)
Q_DECLARE_METATYPE(QList<LayoutCoord *> *)
Q_DECLARE_METATYPE(QList<LayoutItemGraphItem *> *)
Q_DECLARE_METATYPE(QList<LayoutSpikes *> *)
Q_DECLARE_METATYPE(QList<LayoutArrows *> *)
Q_DECLARE_METATYPE(QList<LayoutIcon *> *)
Q_DECLARE_METATYPE(QList<LayoutCircle *> *)
Q_DECLARE_METATYPE(QList<LayoutText *> *)
Q_DECLARE_METATYPE(QList<LayoutPolyline *> *)
Q_DECLARE_METATYPE(QList<LayoutPolygon *> *)
Q_DECLARE_METATYPE(QList<LayoutImage *> *)
Q_DECLARE_METATYPE(QList<LayoutItemGraph *> *)
Q_DECLARE_METATYPE(QList<LayoutCursor *> *)
Q_DECLARE_METATYPE(QList<LayoutLayer *> *)
Q_DECLARE_METATYPE(QList<Layout *> *)
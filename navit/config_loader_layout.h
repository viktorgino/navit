#pragma once
#include <QObject>
#include <QVector>
#include <QString>
#include <QColor>

#include "item_type_def.h"

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
    int &getX()
    {
        return m_x;
    }
    int &getY()
    {
        return m_y;
    }

private:
    int m_x;
    int m_y;
};

class LayoutRange : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int min MEMBER m_min REQUIRED)
    Q_PROPERTY(int max MEMBER m_max REQUIRED)

public:
    int &getMin()
    {
        return m_min;
    }
    int &getMax()
    {
        return m_max;
    }

private:
    int m_min;
    int m_max;
};

class LayoutItemGraphElement : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QColor color MEMBER m_color)
    Q_PROPERTY(int oneway MEMBER m_oneway)
    Q_PROPERTY(int text_size MEMBER m_text_size)
    Q_PROPERTY(QVector<LayoutCoord *> coords MEMBER m_coords)
    Q_PROPERTY(LayoutElementType type MEMBER m_type)

public:
    explicit LayoutItemGraphElement(LayoutElementType type, QObject *parent = nullptr) : QObject(parent), m_type(type) {}
    LayoutElementType getType()
    {
        return m_type;
    }
    QColor &getColor()
    {
        return m_color;
    }
    int &getOneway()
    {
        return m_oneway;
    }
    int &getTextSize()
    {
        return m_text_size;
    }
    QVector<LayoutCoord *> getCoords()
    {
        return m_coords;
    }

private:
    LayoutElementType m_type;
    QColor m_color;
    int m_oneway;
    int m_text_size;
    QVector<LayoutCoord *> m_coords;
    Q_ENUM(LayoutElementType)
};

class LayoutSpikes : public LayoutItemGraphElement
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER m_width)
    Q_PROPERTY(int distance MEMBER m_distance)

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;
    int &getWidth()
    {
        return m_width;
    }
    int &getDistance()
    {
        return m_distance;
    }

private:
    int m_width;
    int m_distance;
};

class LayoutArrows : public LayoutItemGraphElement
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER m_width)

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;
    int &getWidth()
    {
        return m_width;
    }

private:
    int m_width;
};

class LayoutIcon : public LayoutItemGraphElement
{
    Q_OBJECT
    Q_PROPERTY(QString src MEMBER m_src REQUIRED)
    Q_PROPERTY(int w MEMBER m_w)
    Q_PROPERTY(int h MEMBER m_h)
    Q_PROPERTY(int x MEMBER m_x)
    Q_PROPERTY(int y MEMBER m_y)
    Q_PROPERTY(int rotation MEMBER m_rotation)

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;
    QString &getSrc()
    {
        return m_src;
    }
    int &getW()
    {
        return m_w;
    }
    int &getH()
    {
        return m_h;
    }
    int &getX()
    {
        return m_x;
    }
    int &getY()
    {
        return m_y;
    }
    int &getRotation()
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

class LayoutCircle : public LayoutItemGraphElement
{
    Q_OBJECT
    Q_PROPERTY(int radius MEMBER m_radius REQUIRED)
    Q_PROPERTY(int width MEMBER m_width)
    Q_PROPERTY(QColor background_color MEMBER m_background_color)

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;
    int &getRadius()
    {
        return m_radius;
    }
    int &getWidth()
    {
        return m_width;
    }
    QColor &getBackgroundColor()
    {
        return m_background_color;
    }

private:
    int m_radius;
    int m_width;
    QColor m_background_color;
};

class LayoutText : public LayoutItemGraphElement
{
    Q_OBJECT
    Q_PROPERTY(QColor background_color MEMBER m_background_color)

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;
    QColor &getBackgroundColor()
    {
        return m_background_color;
    }

private:
    QColor m_background_color;
};

class LayoutPolyline : public LayoutItemGraphElement
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER m_width)
    Q_PROPERTY(QVector<int> dash MEMBER m_dash)
    Q_PROPERTY(int offset MEMBER m_offset)
    Q_PROPERTY(int directed MEMBER m_directed)
    Q_PROPERTY(int radius MEMBER m_radius)

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;
    int &getWidth()
    {
        return m_width;
    }
    QVector<int> &getDash()
    {
        return m_dash;
    }
    int &getOffset()
    {
        return m_offset;
    }
    int &getDirected()
    {
        return m_directed;
    }
    int &getRadius()
    {
        return m_radius;
    }

private:
    int m_width;
    QVector<int> m_dash;
    int m_offset;
    int m_directed;
    int m_radius;
};

class LayoutPolygon : public LayoutItemGraphElement
{
    Q_OBJECT
    Q_PROPERTY(QString src MEMBER m_src)
    Q_PROPERTY(int w MEMBER m_w)
    Q_PROPERTY(int h MEMBER m_h)
    Q_PROPERTY(int x MEMBER m_x)
    Q_PROPERTY(int y MEMBER m_y)
    Q_PROPERTY(int rotation MEMBER m_rotation)

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;
    QString &getSrc()
    {
        return m_src;
    }
    int &getW()
    {
        return m_w;
    }
    int &getH()
    {
        return m_h;
    }
    int &getX()
    {
        return m_x;
    }
    int &getY()
    {
        return m_y;
    }
    int &getRotation()
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

class LayoutImage : public LayoutItemGraphElement
{
    Q_OBJECT

public:
    using LayoutItemGraphElement::LayoutItemGraphElement;

private:
};

class LayoutItemGraph : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVector<item_type> item_types MEMBER m_item_types)
    Q_PROPERTY(LayoutRange *order READ getOrder)
    Q_PROPERTY(LayoutRange *speed_range READ getSpeedRange)
    Q_PROPERTY(QVector<LayoutItemGraphElement *> elements MEMBER m_elements)

public:
    QVector<item_type> &getItemTypes()
    {
        return m_item_types;
    }
    LayoutRange *getOrder()
    {
        return &m_order;
    }
    LayoutRange *getSpeedRange()
    {
        return &m_speed_range;
    }
    QVector<LayoutItemGraphElement *> getElements()
    {
        return m_elements;
    }

private:
    QVector<item_type> m_item_types;
    LayoutRange m_order;
    LayoutRange m_speed_range;
    QVector<LayoutItemGraphElement *> m_elements;
};

class LayoutCursor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int w MEMBER m_w REQUIRED)
    Q_PROPERTY(int h MEMBER m_h REQUIRED)
    Q_PROPERTY(QVector<LayoutItemGraph *> itemgra MEMBER m_itemgra)
    Q_PROPERTY(QString name MEMBER m_name)

public:
    int &getW()
    {
        return m_w;
    }
    int &getH()
    {
        return m_h;
    }
    QVector<LayoutItemGraph *> getItemgra()
    {
        return m_itemgra;
    }
    QString &getName()
    {
        return m_name;
    }

private:
    int m_w;
    int m_h;
    QVector<LayoutItemGraph *> m_itemgra;
    QString m_name;
};

class LayoutLayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString enabled MEMBER m_enabled)
    Q_PROPERTY(QString name MEMBER m_name)
    Q_PROPERTY(QString details MEMBER m_details)
    Q_PROPERTY(LayoutRange *order READ getOrder)
    Q_PROPERTY(QString ref MEMBER m_ref)
    Q_PROPERTY(int active MEMBER m_active)
    Q_PROPERTY(QVector<LayoutItemGraph *> itemgraphs MEMBER m_itemgraphs)

public:
    QString &getEnabled()
    {
        return m_enabled;
    }
    QString &getName()
    {
        return m_name;
    }
    QString &getDetails()
    {
        return m_details;
    }
    LayoutRange *getOrder()
    {
        return &m_order;
    }
    QString &getRef()
    {
        return m_ref;
    }
    int &getActive()
    {
        return m_active;
    }
    QVector<LayoutItemGraph *> getItemgraphs()
    {
        return m_itemgraphs;
    }

private:
    QString m_enabled;
    QString m_name;
    QString m_details;
    LayoutRange m_order;
    QString m_ref;
    int m_active = 1;
    QVector<LayoutItemGraph *> m_itemgraphs;
};

class Layout : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER m_name REQUIRED)
    Q_PROPERTY(int active MEMBER m_active)
    Q_PROPERTY(QColor color MEMBER m_color)
    Q_PROPERTY(QString font MEMBER m_font)
    Q_PROPERTY(QString daylayout MEMBER m_daylayout)
    Q_PROPERTY(QString nightlayout MEMBER m_nightlayout)
    Q_PROPERTY(int icon_w MEMBER m_icon_w)
    Q_PROPERTY(int icon_h MEMBER m_icon_h)
    Q_PROPERTY(int underground_alpha MEMBER m_underground_alpha)
    Q_PROPERTY(QVector<LayoutCursor *> cursors MEMBER m_cursors)
    Q_PROPERTY(QVector<LayoutLayer *> layers MEMBER m_layers)

public:
    QString &getName()
    {
        return m_name;
    }
    int &getActive()
    {
        return m_active;
    }
    QColor &getColor()
    {
        return m_color;
    }
    QString &getFont()
    {
        return m_font;
    }
    QString &getDaylayout()
    {
        return m_daylayout;
    }
    QString &getNightlayout()
    {
        return m_nightlayout;
    }
    int &getIconW()
    {
        return m_icon_w;
    }
    int &getIconH()
    {
        return m_icon_h;
    }
    int &getUndergroundAlpha()
    {
        return m_underground_alpha;
    }
    QVector<LayoutCursor *> getCursors()
    {
        return m_cursors;
    }
    QVector<LayoutLayer *> getLayers()
    {
        return m_layers;
    }

private:
    QString m_name;
    int m_active = 1;
    QColor m_color;
    QString m_font;
    QString m_daylayout;
    QString m_nightlayout;
    int m_icon_w;
    int m_icon_h;
    int m_underground_alpha;
    QVector<LayoutCursor *> m_cursors;
    QVector<LayoutLayer *> m_layers;
};

Q_DECLARE_METATYPE(LayoutRange *)
Q_DECLARE_METATYPE(LayoutElementType)
Q_DECLARE_METATYPE(QVector<int>)
Q_DECLARE_METATYPE(QVector<item_type>)
Q_DECLARE_METATYPE(QVector<LayoutCoord *>)
Q_DECLARE_METATYPE(QVector<LayoutItemGraphElement *>)
Q_DECLARE_METATYPE(QVector<LayoutSpikes *>)
Q_DECLARE_METATYPE(QVector<LayoutArrows *>)
Q_DECLARE_METATYPE(QVector<LayoutIcon *>)
Q_DECLARE_METATYPE(QVector<LayoutCircle *>)
Q_DECLARE_METATYPE(QVector<LayoutText *>)
Q_DECLARE_METATYPE(QVector<LayoutPolyline *>)
Q_DECLARE_METATYPE(QVector<LayoutPolygon *>)
Q_DECLARE_METATYPE(QVector<LayoutImage *>)
Q_DECLARE_METATYPE(QVector<LayoutItemGraph *>)
Q_DECLARE_METATYPE(QVector<LayoutCursor *>)
Q_DECLARE_METATYPE(QVector<LayoutLayer *>)
Q_DECLARE_METATYPE(QVector<Layout *>)
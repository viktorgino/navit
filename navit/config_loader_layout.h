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
    Q_PROPERTY(int x MEMBER x REQUIRED)
    Q_PROPERTY(int y MEMBER y REQUIRED)

public:
    int x;
    int y;
};
class LayoutItemGraphItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString color MEMBER color)
    Q_PROPERTY(int oneway MEMBER oneway)
    Q_PROPERTY(int text_size MEMBER text_size)
    Q_PROPERTY(QList<LayoutCoord *> *coords READ getCoords CONSTANT)

public:
    LayoutElementType type;
    QString color;
    int oneway;
    int text_size;
    QList<LayoutCoord *> coords;

private:
    QList<LayoutCoord *> *getCoords()
    {
        return &coords;
    }
};
class LayoutSpikes : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(int distance MEMBER distance)

public:
    int width;
    int distance;
};
class LayoutArrows : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER width)

public:
    int width;
};
class LayoutIcon : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(QString src MEMBER src REQUIRED)
    Q_PROPERTY(int w MEMBER w)
    Q_PROPERTY(int h MEMBER h)
    Q_PROPERTY(int x MEMBER x)
    Q_PROPERTY(int y MEMBER y)
    Q_PROPERTY(int rotation MEMBER rotation)

public:
    QString src;
    int w;
    int h;
    int x;
    int y;
    int rotation;
};
class LayoutCircle : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int radius MEMBER radius REQUIRED)
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(QString background_color MEMBER background_color)

public:
    int radius;
    int width;
    QString background_color;
};
class LayoutText : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(QString background_color MEMBER background_color)

public:
    QString background_color;
};
class LayoutPolyline : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(QString dash MEMBER dash)
    Q_PROPERTY(int offset MEMBER offset)
    Q_PROPERTY(int directed MEMBER directed)
    Q_PROPERTY(int radius MEMBER radius)

public:
    int width;
    QString dash;
    int offset;
    int directed;
    int radius;
};
class LayoutPolygon : public LayoutItemGraphItem
{
    Q_OBJECT
    Q_PROPERTY(QString src MEMBER src)
    Q_PROPERTY(int w MEMBER w)
    Q_PROPERTY(int h MEMBER h)
    Q_PROPERTY(int x MEMBER x)
    Q_PROPERTY(int y MEMBER y)
    Q_PROPERTY(int rotation MEMBER rotation)

public:
    QString src;
    int w;
    int h;
    int x;
    int y;
    int rotation;
};
class LayoutImage : public LayoutItemGraphItem
{
    Q_OBJECT

public:
};
class LayoutItemGraph : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString item_types MEMBER item_types)
    Q_PROPERTY(QString order MEMBER order)
    Q_PROPERTY(QString speed_range MEMBER speed_range)
    Q_PROPERTY(QList<LayoutItemGraphItem *> *items READ getItems CONSTANT)

public:
    QString item_types;
    QString order;
    QString speed_range;
    QList<LayoutItemGraphItem *> items;

private:
    QList<LayoutItemGraphItem *> *getItems()
    {
        return &items;
    }
};
class LayoutCursor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int w MEMBER w REQUIRED)
    Q_PROPERTY(int h MEMBER h REQUIRED)
    Q_PROPERTY(QList<LayoutItemGraph *> *itemgra READ getItemgra CONSTANT)

public:
    int w;
    int h;
    QList<LayoutItemGraph *> itemgra;

private:
    QList<LayoutItemGraph *> *getItemgra()
    {
        return &itemgra;
    }
};
class LayoutLayer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString enabled MEMBER enabled)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString details MEMBER details)
    Q_PROPERTY(QString order MEMBER order)
    Q_PROPERTY(QString ref MEMBER ref)
    Q_PROPERTY(QString active MEMBER active)
    Q_PROPERTY(QList<LayoutItemGraph *> *itemgraphs READ getItemgraphs CONSTANT)

public:
    QString enabled;
    QString name;
    QString details;
    QString order;
    QString ref;
    QString active;
    QList<LayoutItemGraph *> itemgraphs;

private:
    QList<LayoutItemGraph *> *getItemgraphs()
    {
        return &itemgraphs;
    }
};
class Layout : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name MEMBER name REQUIRED)
    Q_PROPERTY(QString active MEMBER active)
    Q_PROPERTY(QString color MEMBER color)
    Q_PROPERTY(QString font MEMBER font)
    Q_PROPERTY(QString daylayout MEMBER daylayout)
    Q_PROPERTY(QString nightlayout MEMBER nightlayout)
    Q_PROPERTY(QString icon_w MEMBER icon_w)
    Q_PROPERTY(QString icon_h MEMBER icon_h)
    Q_PROPERTY(QString underground_alpha MEMBER underground_alpha)
    Q_PROPERTY(QList<LayoutCursor *> *cursors READ getCursors CONSTANT)
    Q_PROPERTY(QList<LayoutLayer *> *layers READ getLayers CONSTANT)

public:
    QString name;
    QString active;
    QString color;
    QString font;
    QString daylayout;
    QString nightlayout;
    QString icon_w;
    QString icon_h;
    QString underground_alpha;
    QList<LayoutCursor *> cursors;
    QList<LayoutLayer *> layers;

private:
    QList<LayoutCursor *> *getCursors()
    {
        return &cursors;
    }
    QList<LayoutLayer *> *getLayers()
    {
        return &layers;
    }
};

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
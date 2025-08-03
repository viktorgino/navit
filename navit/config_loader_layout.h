#pragma once
#include <QObject>
#include <QList>
#include <QString>
#include <QColor>

struct NavitCoord
{
    int x;
    int y;
};

struct NavitItemgraElement
{
    QColor color;
    int text_size;
    int oneway;
    int coord_count;
    NavitCoord *coord;
};

struct NavitItemgraElementPoint
{
    char stub;
};

struct NavitItemgraElementPolyline
{
    int width;
    int directed;
    int dash_num;
    int offset;
    QString dash_table;
};

struct NavitItemgraElementPolygon
{
    /* for texture */
    QString src;
    int width;
    int height;
    int rotation;
    int x;
    int y;
};

struct NavitItemgraElementCircle
{
    int width;
    int radius;
    QColor background_color;
};

struct NavitItemgraElementicon
{
    QString src;
    int width;
    int height;
    int rotation;
    int x;
    int y;
};

struct NavitItemgraElementText
{
    QColor background_color;
};

struct NavitItemgraElementArrows
{
    int width;
};

struct NavitItemgraElementSpikes
{
    int width;
    int distance;
};

struct NavitRange
{
    short min, max;
};

struct NavitItemgra
{
    NavitRange order;
    NavitRange sequence_range;
    NavitRange speed_range;
    NavitRange angle_range;
    QList<std::unique_ptr<NavitItemgraElement>> elements;
};

struct NavitLayer
{
    QString name;
    int details;
    QList<NavitItemgra> itemgras;
    int active;
    NavitLayer *ref; // TODO: What's this doing?
};

struct NavitCursor
{
    struct attr **attrs;
    NavitRange sequence_range;
    QString name;
    int w, h;
    int interval;
};

struct NavitLayout
{
    QString name;
    QString dayname;
    QString nightname;
    QString font;
    QColor color;
    int underground_alpha;
    int icon_w;
    int icon_h;
    QList<NavitLayer> layers;
    QList<NavitCursor> cursors;
    int order_delta;
    int active;
};
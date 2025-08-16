#include "config_loader.h"

typedef void (*NewTypeBuilder)(const QVariant &, QVariant &, QObject *);
typedef LayoutItemGraphElement *(*NewItemGraphItemBuilder)(const QVariant &, QObject *);

template <typename T>
void build_struct(const QVariant &jsonObject, QVariant &configVariant, QObject *parent);

template <typename T>
void build_list(const QVariant &jsonList, QVariant &propertyValue, QObject *parent);

void build_itemgraph_list(const QVariant &jsonList, QVariant &propertyValue, QObject *parent);

template <typename T>
LayoutItemGraphElement *build_itemgraph_item(const QVariant &jsonItem, QObject *parent);

void build_range(const QVariant &jsonObject, QVariant &propertyValue, QObject *parent)
{
    QString rangeStr = jsonObject.toString();

    LayoutRange *range = propertyValue.value<LayoutRange *>();
    assert(range);

    int min = 0;
    int max = 32767;
    if (!rangeStr.contains("-"))
    {
        min = rangeStr.toInt();
        max = min;
    }
    else if (rangeStr.startsWith("-"))
    {
        rangeStr = rangeStr.remove(0, 1);
        max = rangeStr.toInt();
    }
    else
    {
        QStringList minMax = rangeStr.split("-");
        assert(minMax.size() == 2);
        min = minMax[0].toInt();
        max = minMax[1].toInt();
    }

    range->setProperty("min", min);
    range->setProperty("max", max);
}

void build_item_type(const QVariant &jsonObject, QVariant &propertyValue, QObject *parent)
{
    QVector<item_type> *listPtr = static_cast<QVector<item_type> *>(propertyValue.data());
    assert(listPtr);

    QStringList itemTypes = jsonObject.toString().split(",");
    for (QString itemType : itemTypes)
    {
        listPtr->append(item_from_name(itemType.toLocal8Bit().data()));
    }
}
void build_int_list(const QVariant &jsonObject, QVariant &propertyValue, QObject *parent)
{
    QVector<int> *listPtr = static_cast<QVector<int> *>(propertyValue.data());
    assert(listPtr);

    QStringList itemTypes = jsonObject.toString().split(",");
    for (QString itemType : itemTypes)
    {
        listPtr->append(itemType.toInt());
    }

    // listPtr->append(0); // Don't think we need this
}

void build_coord_geo(const QVariant &jsonObject, QVariant &propertyValue, QObject *parent)
{
    struct coord c;
    QString coordStr = jsonObject.toString();
    coord_geo g;
    coord_geo *value = static_cast<coord_geo *>(propertyValue.data());

    coord_parse(coordStr.toLocal8Bit().data(), projection_mg, &c);
    transform_to_geo(projection_mg, &c, &g);

    value->lat = g.lat;
    value->lng = g.lng;
}

const static QMap<QString, NewTypeBuilder> typeBuilders{
    {"NavitLogConfig*", build_struct<NavitLogConfig>},
    {"NavitTrackingConfig*", build_struct<NavitTrackingConfig>},
    {"NavitRouteConfig*", build_struct<NavitRouteConfig>},
    {"NavitNavigationConfig*", build_struct<NavitNavigationConfig>},
    {"QVector<NavitPluginConfig*>", build_list<NavitPluginConfig>},
    {"QVector<NavitDebugConfig*>", build_list<NavitDebugConfig>},
    {"QVector<NavitVehicleConfig*>", build_list<NavitVehicleConfig>},
    {"QVector<NavitAnnounceConfig*>", build_list<NavitAnnounceConfig>},
    {"QVector<NavitMap*>", build_list<NavitMap>},
    {"QVector<NavitMapset*>", build_list<NavitMapset>},
    {"QVector<LayoutCoord*>", build_list<LayoutCoord>},
    {"QVector<LayoutItemGraphElement*>", build_itemgraph_list},
    {"QVector<LayoutItemGraph*>", build_list<LayoutItemGraph>},
    {"QVector<LayoutCursor*>", build_list<LayoutCursor>},
    {"QVector<LayoutLayer*>", build_list<LayoutLayer>},
    {"QVector<Layout*>", build_list<Layout>},
    {"LayoutRange*", build_range},
    {"QVector<item_type>", build_item_type},
    {"QVector<int>", build_int_list},
    {"coord_geo*", build_coord_geo},
};

const static QMap<QString, NewItemGraphItemBuilder> itemGraphItemBuilders{
    {"spikes", build_itemgraph_item<LayoutSpikes>},
    {"arrows", build_itemgraph_item<LayoutArrows>},
    {"icon", build_itemgraph_item<LayoutIcon>},
    {"circle", build_itemgraph_item<LayoutCircle>},
    {"text", build_itemgraph_item<LayoutText>},
    {"polyline", build_itemgraph_item<LayoutPolyline>},
    {"polygon", build_itemgraph_item<LayoutPolygon>},
    {"image", build_itemgraph_item<LayoutImage>},
};

void populateProperties(const QMetaObject *configMeta, QVariantMap &jsonObjectMap, QObject *configObject, QObject *parent)
{
    for (int i = configMeta->propertyOffset(); i < configMeta->propertyCount(); i++)
    {
        QMetaProperty property = configMeta->property(i);
        const char *name = property.name();
        const char *typeName = property.typeName();
        QVariant configValue = jsonObjectMap.value(name);

        // Make sure required properties are set from config
        if ((configValue.isNull() && property.isRequired()))
        {
            qDebug() << "Property " << name << " is required, but not set" << jsonObjectMap.value("name");
            assert(false);
        }
        if (configValue.isValid())
        {
            if (property.type() == QVariant::Type::UserType)
            {
                // Custom types
                QVariant propertyValue = configObject->property(name);
                if (!typeBuilders.contains(typeName))
                {
                    qWarning() << "No type builder for " << typeName << " on: " << name;
                    assert(false);
                }
                if (!propertyValue.isValid())
                {
                    qWarning() << "Invalid property value for " << typeName << " on: " << name;
                    assert(false);
                }
                typeBuilders.value(typeName)(configValue, propertyValue, parent);
                configObject->setProperty(name, propertyValue);
            }
            else
            {
                // Built-in simple types
                property.write(configObject, configValue);
            }
            // Remove already added properties so they don't get populated twice
            jsonObjectMap.remove(name);
        }
    }
    // If there's a parent populate its properties too
    if (configMeta->superClass())
    {
        populateProperties(configMeta->superClass(), jsonObjectMap, configObject, parent);
    }
}

template <typename T>
void build_struct(const QVariant &jsonObject, QVariant &configVariant, QObject *parent)
{
    T *configObject = configVariant.value<T *>();
    assert(configObject);
    assert(jsonObject.type() == QVariant::Type::Map);
    QVariantMap jsonObjectMap = jsonObject.toMap();
    const QMetaObject &configMeta = configObject->staticMetaObject;
    populateProperties(&configMeta, jsonObjectMap, configObject, configObject);
};

template <typename T>
void build_list(const QVariant &jsonList, QVariant &propertyValue, QObject *parent)
{
    assert(jsonList.type() == QVariant::Type::List);
    QVector<T *> *listPtr = static_cast<QVector<T *> *>(propertyValue.data());
    assert(listPtr);

    for (const QVariant &jsonItem : jsonList.toList())
    {
        T *newItem = new T;
        newItem->setParent(parent);
        QVariant newItemVariant = QVariant::fromValue(newItem);
        build_struct<T>(jsonItem, newItemVariant, parent);
        listPtr->append(newItem);
    }
}

LayoutElementType element_type_from_string(const QString type)
{
    if (type == "point")
        return LayoutElementType::LayoutElementPoint;
    else if (type == "polyline")
        return LayoutElementType::LayoutElementPolyline;
    else if (type == "polygon")
        return LayoutElementType::LayoutElementPolygon;
    else if (type == "circle")
        return LayoutElementType::LayoutElementCircle;
    else if (type == "text")
        return LayoutElementType::LayoutElementText;
    else if (type == "icon")
        return LayoutElementType::LayoutElementIcon;
    else if (type == "image")
        return LayoutElementType::LayoutElementImage;
    else if (type == "arrows")
        return LayoutElementType::LayoutElementArrows;
    else if (type == "spikes")
        return LayoutElementType::LayoutElementSpikes;
    else
        assert(false);
};

template <typename T>
LayoutItemGraphElement *build_itemgraph_item(const QVariant &jsonItem, QObject *parent)
{
    // Exctract type and remove it from map
    assert(jsonItem.type() == QVariant::Type::Map);
    QVariantMap jsonObjectMap = jsonItem.toMap();
    LayoutElementType type = element_type_from_string(jsonObjectMap["type"].toString());
    jsonObjectMap.remove("type");

    T *newItem = new T(type);
    newItem->setParent(parent);
    QVariant newItemVariant = QVariant::fromValue(newItem);
    build_struct<T>(jsonObjectMap, newItemVariant, parent);
    return newItem;
}

void build_itemgraph_list(const QVariant &jsonList, QVariant &propertyValue, QObject *parent)
{
    assert(jsonList.type() == QVariant::Type::List);
    QVector<LayoutItemGraphElement *> *configPtr = static_cast<QVector<LayoutItemGraphElement *> *>(propertyValue.data());
    assert(configPtr);
    for (const QVariant &jsonItem : jsonList.toList())
    {
        assert(jsonItem.type() == QVariant::Type::Map);
        QVariantMap itemProperties = jsonItem.toMap();

        QString type = itemProperties.value("type").toString();
        assert(itemGraphItemBuilders.contains(type));

        LayoutItemGraphElement *graphItem = itemGraphItemBuilders.value(type)(jsonItem, parent);
        configPtr->append(graphItem);
    }
}

template <typename T>
void ConfigLoader::loadFromJson(const QVariant &configJson, QObject &configObject)
{
    QVariant configVariant = QVariant::fromValue(&configObject);
    build_struct<T>(configJson, configVariant, &configObject);
}
template <typename T>
void ConfigLoader::loadFromFile(const QString &fileName, QObject &configObject)
{
    QFile file;
    file.setFileName(fileName);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QJsonDocument jsonDocument = QJsonDocument::fromJson(QString(file.readAll()).toUtf8());
    file.close();
    loadFromJson<T>(jsonDocument.toVariant(), configObject);
}

void ConfigLoader::loadNavit(QString fileName, NavitConfig &navitConfig)
{
    loadFromFile<NavitConfig>(fileName, navitConfig);
}

void ConfigLoader::loadLayout(QString fileName, Layout &layout)
{
    loadFromFile<Layout>(fileName, layout);
}
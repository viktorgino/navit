#include "config_loader.h"

typedef void (*NewTypeBuilder)(const QVariant &, QVariant &, QObject *);
typedef LayoutItemGraphItem *(*NewItemGraphItemBuilder)(const QVariant &, QObject *);

template <typename T>
void build_struct(const QVariant &jsonObject, QVariant &configVariant, QObject *parent);

template <typename T>
void build_list(const QVariant &jsonList, QVariant &propertyValue, QObject *parent);

void build_itemgraph_list(const QVariant &jsonList, QVariant &propertyValue, QObject *parent);

template <typename T>
LayoutItemGraphItem *build_itemgraph_item(const QVariant &jsonItem, QObject *parent);

const static QMap<QString, NewTypeBuilder> typeBuilders{
    {"NavitLogConfig*", build_struct<NavitLogConfig>},
    {"NavitTrackingConfig*", build_struct<NavitTrackingConfig>},
    {"NavitRouteConfig*", build_struct<NavitRouteConfig>},
    {"NavitNavigationConfig*", build_struct<NavitNavigationConfig>},
    {"QList<NavitPluginConfig*>*", build_list<NavitPluginConfig>},
    {"QList<NavitDebugConfig*>*", build_list<NavitDebugConfig>},
    {"QList<NavitVehicleConfig*>*", build_list<NavitVehicleConfig>},
    {"QList<NavitAnnounceConfig*>*", build_list<NavitAnnounceConfig>},
    {"QList<NavitMap>*", build_struct<NavitMap>},
    {"QList<LayoutCoord*>*", build_list<LayoutCoord>},
    {"QList<LayoutItemGraphItem*>*", build_itemgraph_list},
    {"QList<LayoutItemGraph*>*", build_list<LayoutItemGraph>},
    {"QList<LayoutCursor*>*", build_list<LayoutCursor>},
    {"QList<LayoutLayer*>*", build_list<LayoutLayer>},
    {"QList<Layout*>*", build_list<Layout>},
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
        QVariant configValue = jsonObjectMap.value(property.name());

        // Make sure required properties are set from config
        assert(!(configValue.isNull() && property.isRequired()));

        if (configValue.isValid())
        {
            if (property.type() == QVariant::Type::UserType)
            {
                // Custom types
                QVariant propertyValue = property.read(configObject);
                assert(typeBuilders.contains(property.typeName()) && propertyValue.isValid());
                typeBuilders.value(property.typeName())(configValue, propertyValue, parent);
            }
            else
            {
                // Built-in simple types
                property.write(configObject, configValue);
            }
            // Remove already added properties so they don't get populated twice
            jsonObjectMap.remove(property.name());
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
    QList<T *> *listPtr = propertyValue.value<QList<T *> *>();
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

template <typename T>
LayoutItemGraphItem *build_itemgraph_item(const QVariant &jsonItem, QObject *parent)
{
    T *newItem = new T;
    newItem->setParent(parent);
    QVariant newItemVariant = QVariant::fromValue(newItem);
    build_struct<T>(jsonItem, newItemVariant, parent);
    return newItem;
}

void build_itemgraph_list(const QVariant &jsonList, QVariant &propertyValue, QObject *parent)
{
    assert(jsonList.type() == QVariant::Type::List);
    QList<LayoutItemGraphItem *> *configPtr = propertyValue.value<QList<LayoutItemGraphItem *> *>();
    assert(configPtr != nullptr);
    for (const QVariant &jsonItem : jsonList.toList())
    {
        assert(jsonItem.type() == QVariant::Type::Map);
        QVariantMap itemProperties = jsonItem.toMap();

        QString type = itemProperties.value("type").toString();
        assert(itemGraphItemBuilders.contains(type));

        LayoutItemGraphItem *graphItem = itemGraphItemBuilders.value(type)(jsonItem, parent);
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
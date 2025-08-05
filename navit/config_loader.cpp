#include "config_loader.h"

typedef void (*NewTypeBuilder)(const QVariant &, QVariant &);
typedef LayoutItemGraphItem *(*NewItemGraphItemBuilder)(const QVariant &);

template <typename T>
void build_struct(const QVariant &configObject, QVariant &configStruct);

template <typename T>
void build_list(const QVariant &configObject, QVariant &configList);

void build_itemgraph_list(const QVariant &configObject, QVariant &configList);

template <typename T>
LayoutItemGraphItem *build_itemgraph_item(const QVariant &configItem);

const static QMap<QString, NewTypeBuilder> typeBuilders{
    {"NavitLogConfig*", build_struct<NavitLogConfig>},
    {"NavitTrackingConfig*", build_struct<NavitTrackingConfig>},
    {"NavitRouteConfig*", build_struct<NavitRouteConfig>},
    {"NavitNavigationConfig*", build_struct<NavitNavigationConfig>},
    {"QList<NavitPluginConfig>*", build_list<NavitPluginConfig>},
    {"QList<NavitDebugConfig>*", build_list<NavitDebugConfig>},
    {"QList<NavitVehicleConfig>*", build_list<NavitVehicleConfig>},
    {"QList<NavitAnnounceConfig>*", build_list<NavitAnnounceConfig>},
    {"QList<NavitMap>*", build_struct<NavitMap>},
    {"QList<LayoutCoord>*", build_list<LayoutCoord>},
    {"QList<LayoutItemGraphItem*>*", build_itemgraph_list},
    {"QList<LayoutItemGraph>*", build_list<LayoutItemGraph>},
    {"QList<LayoutCursor>*", build_list<LayoutCursor>},
    {"QList<LayoutLayer>*", build_list<LayoutLayer>},
    {"QList<Layout>*", build_list<Layout>},
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

void populateProperties(const QMetaObject *configMeta, QVariantMap &configObjectMap, void *configPtr)
{
    for (int i = configMeta->propertyOffset(); i < configMeta->propertyCount(); i++)
    {
        QMetaProperty property = configMeta->property(i);
        QVariant configValue = configObjectMap.value(property.name());

        // Make sure required properties are set from config
        assert(!(configValue.isNull() && property.isRequired()));

        if (configValue.isValid())
        {
            if (property.type() == QVariant::Type::UserType)
            {
                // Custom types
                QVariant propertyValue = property.readOnGadget(configPtr);
                assert(typeBuilders.contains(property.typeName()) && propertyValue.isValid());
                typeBuilders.value(property.typeName())(configValue, propertyValue);
            }
            else
            {
                // Built-in simple types
                property.writeOnGadget(configPtr, configValue);
                qDebug() << property.name() << property.typeName() << configValue;
            }
            // Remove already added properties so they don't get populated twice
            configObjectMap.remove(property.name());
        }
    }
    // If there's a parent populate its properties too
    if (configMeta->superClass())
    {
        populateProperties(configMeta->superClass(), configObjectMap, configPtr);
    }
}
template <typename T>
void build_struct(const QVariant &configObject, QVariant &configStruct)
{
    assert(configObject.type() == QVariant::Type::Map);
    QVariantMap configObjectMap = configObject.toMap();
    T *configPtr = configStruct.value<T *>();
    assert(configPtr != nullptr);
    const QMetaObject &configMeta = configPtr->staticMetaObject;
    populateProperties(&configMeta, configObjectMap, configPtr);
};

template <typename T>
void build_list(const QVariant &configObject, QVariant &configList)
{
    assert(configObject.type() == QVariant::Type::List);
    QList<T> *configPtr = configList.value<QList<T> *>();
    assert(configPtr != nullptr);
    for (const QVariant &configItem : configObject.toList())
    {
        T newItem = {};
        QVariant newItemVariant = QVariant::fromValue(&newItem);
        build_struct<T>(configItem, newItemVariant);
        configPtr->append(newItem);
    }
}

template <typename T>
LayoutItemGraphItem *build_itemgraph_item(const QVariant &configItem)
{
    T *newItem = new T;
    QVariant newItemVariant = QVariant::fromValue(newItem);
    build_struct<T>(configItem, newItemVariant);
    return newItem;
}

void build_itemgraph_list(const QVariant &configObject, QVariant &configList)
{
    assert(configObject.type() == QVariant::Type::List);
    QList<LayoutItemGraphItem *> *configPtr = configList.value<QList<LayoutItemGraphItem *> *>();
    assert(configPtr != nullptr);
    for (const QVariant &configItem : configObject.toList())
    {
        assert(configItem.type() == QVariant::Type::Map);
        QVariantMap itemProperties = configItem.toMap();

        QString type = itemProperties.value("type").toString();
        assert(itemGraphItemBuilders.contains(type));

        LayoutItemGraphItem *graphItem = itemGraphItemBuilders.value(type)(configItem);
        configPtr->append(graphItem);
    }
}

template <typename T>
void ConfigLoader::loadFromJson(const QVariant &configJson, QVariant &configObject)
{
    build_struct<T>(configJson, configObject);
}
template <typename T>
void ConfigLoader::loadFromFile(const QString &fileName, QVariant &configObject)
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
    QVariant configObject = QVariant::fromValue(&navitConfig);
    loadFromFile<NavitConfig>(fileName, configObject);
}

void ConfigLoader::loadLayout(QString fileName, Layout &layout)
{
    QVariant configObject = QVariant::fromValue(&layout);
    loadFromFile<Layout>(fileName, configObject);
}
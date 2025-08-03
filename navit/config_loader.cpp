#include "config_loader.h"

typedef void (*NewTypeBuilder)(const QVariant &, QVariant &);

template <typename T>
void build_struct(const QVariant &configObject, QVariant &configStruct);

template <typename T>
void build_list(const QVariant &configObject, QVariant &configList);

const static QMap<QString, NewTypeBuilder> typeBuilders{
    {
        "NavitLogConfig*",
        build_struct<NavitLogConfig>,
    },
    {
        "QList<NavitPluginConfig>*",
        build_list<NavitPluginConfig>,
    },
    {
        "QList<NavitDebugConfig>*",
        build_list<NavitDebugConfig>,
    },
    {
        "QList<NavitVehicleConfig>*",
        build_list<NavitVehicleConfig>,
    },
    {
        "NavitTrackingConfig*",
        build_struct<NavitTrackingConfig>,
    },
    {
        "NavitRouteConfig*",
        build_struct<NavitRouteConfig>,
    },
    {
        "QList<NavitAnnounceConfig>*",
        build_list<NavitAnnounceConfig>,
    },
    {
        "NavitNavigationConfig*",
        build_struct<NavitNavigationConfig>,
    },
    {
        "QList<NavitMap>*",
        build_struct<NavitMap>,
    },
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
void build_struct(const QVariant &configObject, QVariant &configStruct)
{
    assert(configObject.type() == QVariant::Type::Map);
    QVariantMap configObjectMap = configObject.toMap();
    T *configPtr = configStruct.value<T *>();
    assert(configPtr != nullptr);
    const QMetaObject &configMeta = configPtr->staticMetaObject;
    for (int i = configMeta.propertyOffset(); i < configMeta.propertyCount(); i++)
    {
        QMetaProperty property = configMeta.property(i);
        QVariant configValue = configObjectMap.value(property.name());

        // Make sure required properties are set from config
        assert(!(configValue.isNull() && property.isRequired()));

        if (configValue.isValid())
        {
            if (property.type() == QVariant::Type::UserType)
            {
                QVariant propertyValue = property.readOnGadget(configPtr);
                assert(typeBuilders.contains(property.typeName()) && propertyValue.isValid());
                typeBuilders.value(property.typeName())(configValue, propertyValue);
            }
            else
            {
                property.writeOnGadget(configPtr, configValue);
            }
        }
    }
};

ConfigLoader::ConfigLoader(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<NavitLogConfig>();
}

void ConfigLoader::loadFromJson(const QVariant &configObject)
{
    QVariant value = QVariant::fromValue(&m_navitConfig);
    build_struct<NavitConfig>(configObject, value);
}

void ConfigLoader::loadFromFile(const QString &fileName)
{
    QFile file;
    file.setFileName(fileName);
    file.open(QIODevice::ReadOnly | QIODevice::Text);
    QJsonDocument jsonDocument = QJsonDocument::fromJson(QString(file.readAll()).toUtf8());
    file.close();
    loadFromJson(jsonDocument.toVariant());
}

NavitConfig &ConfigLoader::loadNavit(QString fileName)
{
    loadFromFile(fileName);
    return m_navitConfig;
}
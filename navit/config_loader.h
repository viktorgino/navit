#pragma once
#include <QObject>
#include <QMetaType>
#include <QMetaProperty>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariant>

#include "config_loader_navit.h"
#include "config_loader_layout.h"

#include "item.h"

class ConfigLoader
{
public:
    static void loadNavit(QString fileName, NavitConfig &navitConfig);
    static void loadLayout(QString fileName, Layout &layout);

private:
    template <typename T>
    static void loadFromFile(const QString &fileName, QObject &configObject);
    template <typename T>
    static void loadFromJson(const QVariant &configJson, QObject &configObject);
};

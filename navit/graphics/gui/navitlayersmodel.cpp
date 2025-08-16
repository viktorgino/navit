#include "navitlayersmodel.h"

NavitLayersModel::NavitLayersModel(QObject *parent)
{
}

QHash<int, QByteArray> NavitLayersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[ActionRole] = "action";
    roles[ImageUrlRole] = "imageUrl";
    return roles;
}

QVariant NavitLayersModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_layers.count())
        return QVariant();

    const QVariantMap *poi = &m_layers.at(index.row());

    if (role == NameRole)
        return poi->value("name");
    else if (role == ActionRole)
        return poi->value("action");
    else if (role == ImageUrlRole)
        return poi->value("imageUrl");
    return QVariant();
}

int NavitLayersModel::rowCount(const QModelIndex &parent) const
{
    return m_layers.count();
}

Qt::ItemFlags NavitLayersModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool NavitLayersModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return false;
}

QModelIndex NavitLayersModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}

QModelIndex NavitLayersModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

int NavitLayersModel::columnCount(const QModelIndex &parent) const
{
    return 0;
}

void NavitLayersModel::setNavitLayers(NavitLayoutsModel *navitLayouts)
{
    m_navitLayoutsInstance = navitLayouts;
    update();
    connect(m_navitLayoutsInstance, &NavitLayoutsModel::layoutChanged, this, &NavitLayersModel::update);
}

void NavitLayersModel::update()
{
    if (m_navitLayoutsInstance && m_navitLayoutsInstance->m_navitInstance)
    {
        NavitInterface &navit = m_navitLayoutsInstance->m_navitInstance->getNavit();

        beginResetModel();
        m_layers.clear();
        endResetModel();

        Layout *layout = navit.getCurrentLayout();

        for (LayoutLayer *layer : layout->getLayers())
        {
            QVariantMap layerMap;
            layerMap.insert("name", layer->getName());
            layerMap.insert("action", "toggleLayer");
            if (layer->getActive())
            {
                layerMap.insert("imageUrl", "qrc:/NavitGUI/assets/ionicons/md-checkmark-circle-outline.svg");
            }
            else
            {
                layerMap.insert("imageUrl", "");
            }
            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            m_layers.append(layerMap);
            endInsertRows();
        }
    }
}

void NavitLayersModel::toggleLayer(QString name)
{
    if (m_navitLayoutsInstance && m_navitLayoutsInstance->m_navitInstance)
    {
        NavitInterface &navit = m_navitLayoutsInstance->m_navitInstance->getNavit();

        Layout *layout = navit.getCurrentLayout();

        for (LayoutLayer *layer : layout->getLayers())
        {
            if (layer->getName() == name)
            {
                // TODO: implement layer toggling
                qWarning() << "Layter toggling is not implemented";
                navit.draw();
            }
        }

        update();
    }
}

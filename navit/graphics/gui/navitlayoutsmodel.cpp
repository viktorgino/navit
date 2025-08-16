#include "navitlayoutsmodel.h"

NavitLayoutsModel::NavitLayoutsModel(QObject *parent)
{
}

QHash<int, QByteArray> NavitLayoutsModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[ActionRole] = "action";
    roles[ImageUrlRole] = "imageUrl";
    return roles;
}

QVariant NavitLayoutsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= m_layouts.count())
        return QVariant();

    const QVariantMap *poi = &m_layouts.at(index.row());

    if (role == NameRole)
        return poi->value("name");
    else if (role == ActionRole)
        return poi->value("action");
    else if (role == ImageUrlRole)
        return poi->value("imageUrl");
    return QVariant();
}

int NavitLayoutsModel::rowCount(const QModelIndex &parent) const
{
    return m_layouts.count();
}

Qt::ItemFlags NavitLayoutsModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool NavitLayoutsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    return false;
}

QModelIndex NavitLayoutsModel::index(int row, int column, const QModelIndex &parent) const
{
    return createIndex(row, column);
}

QModelIndex NavitLayoutsModel::parent(const QModelIndex &child) const
{
    return QModelIndex();
}

int NavitLayoutsModel::columnCount(const QModelIndex &parent) const
{
    return 0;
}

void NavitLayoutsModel::setNavit(NavitInstance *navit)
{
    m_navitInstance = navit;
    update();
}
void NavitLayoutsModel::update()
{
    if (m_navitInstance)
    {
        NavitInterface &navit = m_navitInstance->getNavit();

        beginResetModel();
        m_layouts.clear();
        endResetModel();

        Layout *activeLayout = navit.getCurrentLayout();

        for (Layout *layout : navit.getLayouts())
        {
            QVariantMap layouts;
            if (!layout)
            {
                qWarning() << "Got an invalid layout";
                continue;
            }

            layouts.insert("name", layout->getName());
            layouts.insert("action", "setLayout");
            if (activeLayout && activeLayout->getName() == layout->getName())
            {
                layouts.insert("imageUrl", "qrc:/NavitGUI/assets/ionicons/md-checkmark-circle-outline.svg");
            }
            else
            {
                layouts.insert("imageUrl", "");
            }

            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            m_layouts.append(layouts);
            endInsertRows();
        }
        emit layoutChanged();
    }
}

void NavitLayoutsModel::setLayout(QString name)
{
    if (m_navitInstance)
    {
        NavitInterface &navit = m_navitInstance->getNavit();
        navit.set_layout_by_name(name.toUtf8().data());
        update();
    }
}

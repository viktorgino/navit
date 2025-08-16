#include "navitroute.h"

NavitRoute::NavitRoute() : m_status(Invalid)
{
}
void listIcons(NavitInstance *navitInstance)
{
    if (!navitInstance)
    {
        qWarning() << "Invalid navit instance";
    }

    NavitInterface &navit = navitInstance->getNavit();

    for (LayoutLayer *layer : navit.getCurrentLayout()->getLayers())
    {
        if (layer->getName().contains("POI", Qt::CaseInsensitive))
        {
            for (LayoutItemGraph *itemgra : layer->getItemgraphs())
            {
                for (LayoutItemGraphElement *element : itemgra->getElements())
                {
                    if (element->getType() == LayoutElementType::LayoutElementIcon)
                    {
                        LayoutIcon *icon = qobject_cast<LayoutIcon *>(element);
                        qDebug() << "src : " << icon->getSrc() << "x : " << icon->getX() << "y : " << icon->getY();
                    }
                }
            }
        }
    }
}

void NavitRoute::setNavit(NavitInstance *navitInstance)
{
    m_navitInstance = navitInstance;

    NavitInterface &navit = m_navitInstance->getNavit();

    struct callback *cb = callback_new_attr_1(callback_cast(NavitRoute::routeCallbackHandler),
                                              attr_position_coord_geo, this);
    struct callback *cb2 = callback_new_attr_1(callback_cast(NavitRoute::destinationCallbackHandler),
                                               attr_destination, this);

    struct route *route = navit.get_route();
    m_destCount = route_get_destination_count(route);

    navit.add_callback(cb);
    navit.add_callback(cb2);

    struct attr route_attr;

    if (navit.get_attr(attr_route, &route_attr, nullptr))
    {
        struct attr callback;
        callback.type = attr_callback;
        callback.u.callback = callback_new_attr_1(callback_cast(NavitRoute::statusCallbackHandler), attr_route_status, this);
        route_add_attr(route_attr.u.route, &callback);
    }

    routeUpdate();
}

void NavitRoute::updateNextTurn(struct map *map)
{
    struct map_rect *mr = nullptr;
    struct item *item = nullptr;

    if (map)
        mr = map_rect_new(map, nullptr);
    if (mr)
    {
        while ((item = map_rect_get_item(mr)) && (item->type == type_nav_position || item->type == type_nav_none /* || level-- > 0*/))
            ;
        if (item)
        {
            QString url = QString("%1_bk.svg").arg(graphics_icon_path(item_to_name(item->type)));
            QUrl nextTurnIcon = QUrl::fromLocalFile(url);
            if (nextTurnIcon != m_nextTurnIcon)
            {
                m_nextTurnIcon = nextTurnIcon;
                emit nextTurnChanged();
            }
        }
    }
    map_rect_destroy(mr);
}

void NavitRoute::routeUpdate()
{
    NavitInterface &navit = m_navitInstance->getNavit();
    NavitVehicleInterface *vehicle;
    struct route *route;
    struct tracking *tracking;

    struct map *map = nullptr;
    struct map_rect *mr = nullptr;
    struct navigation *nav = nullptr;
    struct attr attr;
    struct item *item = nullptr;
    struct item *item2 = nullptr;

    nav = navit.get_navigation();
    if (!nav)
    {
        return;
    }
    map = navigation_get_map(nav);
    if (map)
        mr = map_rect_new(map, nullptr);
    if (mr)
    {
        m_directions.clear();
        while ((item = map_rect_get_item(mr)))
        {
            if (item_attr_get(item, attr_navigation_long, &attr))
            {
                m_directions << map_convert_string_tmp(item->map, attr.u.str);
            }
        }
    }

    map_rect_destroy(mr);
    if (map)
        mr = map_rect_new(map, nullptr);
    if (mr)
    {
        map_rect_get_item(mr);
        if ((item2 = map_rect_get_item(mr)))
        {
            struct attr length_attr, street_attr;
            if (item_attr_get(item2, attr_length, &length_attr))
            {
                m_nextTurnDistance = QString("%1 meters").arg(length_attr.u.num);
            }

            QString streetname;
            if (item_attr_get(item2, attr_street_name, &street_attr))
            {
                streetname = street_attr.u.str;
            }
            if (item_attr_get(item2, attr_street_name_systematic, &street_attr))
            {
                if (streetname.isEmpty())
                    streetname = street_attr.u.str;
                else
                    streetname.append(QString(" (%1)").arg(street_attr.u.str));
            }
            m_nextTurn = streetname;

            emit nextTurnChanged();
        }
    }
    map_rect_destroy(mr);

    route = navit.get_route();
    if (route)
    {
        struct attr destination_length, destination_time;

        if (route_get_attr(route, attr_destination_length, &destination_length, nullptr))
        {
            m_distance = attr_to_text_ext(&destination_length, nullptr, attr_format_with_units, attr_format_default, nullptr);
        }

        if (route_get_attr(route, attr_destination_time, &destination_time, nullptr))
        {
            char test[] = ": ads :";

            QStringList timeLeft = QString(attr_to_text_ext(&destination_time, test, attr_format_with_units, attr_format_default, nullptr)).split(":");
            QDateTime dt = QDateTime::currentDateTime();
            QTime time;

            switch (timeLeft.size())
            {
            case 4:
                //            m_timeLeft = QString("%1 days %2 hours %3%4").arg("");
                dt = dt.addDays(timeLeft[0].toInt());
                time.setHMS(timeLeft[1].toInt(), timeLeft[2].toInt(), timeLeft[3].toInt());
                m_timeLeft = QString("%1 day %2 h %3 min").arg(timeLeft[0].toInt()).arg(timeLeft[1].toInt()).arg(timeLeft[2].toInt());
                break;
            case 3:
                time.setHMS(timeLeft[0].toInt(), timeLeft[1].toInt(), timeLeft[2].toInt());
                m_timeLeft = QString("%1 h %2 min").arg(timeLeft[0].toInt()).arg(timeLeft[1].toInt());
                break;
            case 2:
                time.setHMS(0, timeLeft[0].toInt(), timeLeft[1].toInt());
                m_timeLeft = QString("%1 min").arg(timeLeft[0].toInt());
                break;
            case 1:
                time.setHMS(0, 0, timeLeft[0].toInt());
                m_timeLeft = QString("%1 seconds").arg(timeLeft[0].toInt());
                break;
            }

            dt = dt.addMSecs(time.msecsSinceStartOfDay());

            m_arrivalTime = dt.toString("hh:mm");
        }
    }

    vehicle = navit.getVehicle();
    if (vehicle)
    {
        m_speed = vehicle->getSpeed();
    }

    struct attr maxspeed_attr, street_name_attr;
    int routespeed = -1;
    tracking = navit.get_tracking();

    if (tracking)
    {

        if (tracking_get_attr(tracking, attr_maxspeed, &maxspeed_attr, nullptr))
        {
            routespeed = maxspeed_attr.u.num;
        }
        if (routespeed == -1)
        {
            struct vehicleprofile *prof = navit.get_vehicleprofile();
            struct roadprofile *rprof = nullptr;
            if (prof && item)
                rprof = vehicleprofile_get_roadprofile(prof, item->type);
            if (rprof)
            {
                if (rprof->maxspeed != 0)
                    routespeed = rprof->maxspeed;
            }
        }
        QString streetname;
        if (tracking_get_attr(tracking, attr_street_name, &street_name_attr, nullptr))
        {
            streetname = street_name_attr.u.str;
        }
        if (tracking_get_attr(tracking, attr_street_name_systematic, &street_name_attr, nullptr))
        {
            if (streetname.isEmpty())
                streetname = street_name_attr.u.str;
            else
                streetname.append(QString(" (%1)").arg(street_name_attr.u.str));
        }
        m_currentStreet = streetname;
    }
    m_speedLimit = routespeed;
    emit propertiesChanged();

    updateNextTurn(map);
    statusUpdate();
}

QString NavitRoute::getLastDestination(struct pcoord *pc)
{
    if (m_navitInstance)
    {
        NavitInterface &navit = m_navitInstance->getNavit();
        struct map *formerdests;
        struct map_rect *mr_formerdests;
        struct item *item;
        struct attr attr;

        if (!navit.get_attr(attr_former_destination_map, &attr, nullptr))
            return "";

        formerdests = attr.u.map;
        if (!formerdests)
            return "";

        mr_formerdests = map_rect_new(formerdests, nullptr);
        if (!mr_formerdests)
            return "";

        pc->pro = map_projection(formerdests);

        QString ret;
        while ((item = map_rect_get_item(mr_formerdests)))
        {
            struct coord c;
            if (item->type != type_former_destination)
                continue;
            if (!item_attr_get(item, attr_label, &attr))
                continue;

            if (item_coord_get(item, &c, 1))
            {
                pc->x = c.x;
                pc->y = c.y;
                ret = attr.u.str;
            }
        }
        map_rect_destroy(mr_formerdests);
        return ret;
    }

    return "";
}
void NavitRoute::destinationUpdate()
{
    NavitInterface &navit = m_navitInstance->getNavit();
    struct route *route = navit.get_route();
    int destCount = route_get_destination_count(route);

    if (destCount > m_destCount)
    {
        QString destination = getLastDestination(&m_lastDestinationCoord);

        navit.set_center(&m_lastDestinationCoord, 1);
        navit.zoom_level(4, nullptr);
        emit destinationAdded(destination);
    }
    else if (destCount < m_destCount)
    {
        emit destinationRemoved();
    }
    if (destCount == 0)
    {
        emit navigationFinished();
    }
    m_destCount = destCount;
}

void NavitRoute::statusUpdate()
{
    NavitInterface &navit = m_navitInstance->getNavit();
    struct navigation *nav = nullptr;
    nav = navit.get_navigation();
    if (!nav)
    {
        return;
    }
    struct attr attr;
    if (navigation_get_attr(nav, attr_nav_status, &attr, nullptr))
    {

        int status = (attr.u.num == status_recalculating) ? status_routing : attr.u.num;
        if (m_status != status)
        {
            m_status = static_cast<Status>(status);
            qDebug() << "status : " << nav_status_to_text(status);
            emit statusChanged();
        }
    }
}
void NavitRoute::routeCallbackHandler(NavitRoute *navitRoute)
{
    navitRoute->routeUpdate();
}
void NavitRoute::destinationCallbackHandler(NavitRoute *navitRoute)
{
    navitRoute->destinationUpdate();
}

void NavitRoute::statusCallbackHandler(NavitRoute *navitRoute, int status)
{
    navitRoute->statusUpdate();
}
void NavitRoute::cancelNavigation()
{
    if (m_navitInstance)
    {
        NavitInterface &navit = m_navitInstance->getNavit();
        navit.set_destination(nullptr, nullptr, 0);
    }
}

void NavitRoute::setDestination(QString label, int x, int y)
{
    if (m_navitInstance)
    {
        cancelNavigation();
        struct pcoord c = NavitHelper::positionToPcoord(m_navitInstance->getNavit(), x, y);
        NavitHelper::setDestination(m_navitInstance->getNavit(), label, c.x, c.y);
    }
}

void NavitRoute::setPosition(int x, int y)
{
    if (m_navitInstance)
    {
        struct pcoord c = NavitHelper::positionToPcoord(m_navitInstance->getNavit(), x, y);
        //        navit.set_position( &c);
        NavitHelper::setPosition(m_navitInstance->getNavit(), c.x, c.y);
    }
}

void NavitRoute::addStop(QString label, int x, int y, int position)
{
    if (m_navitInstance)
    {
        struct pcoord c = NavitHelper::positionToPcoord(m_navitInstance->getNavit(), x, y);
        NavitHelper::addStop(m_navitInstance->getNavit(), position, label, c.x, c.y);
    }
}

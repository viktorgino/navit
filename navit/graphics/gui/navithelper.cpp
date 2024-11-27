#include "navithelper.h"

NavitHelper::NavitHelper()
{
}

QString NavitHelper::formatDist(int dist)
{
    double distance = dist / 1000;
    if (dist > 10000)
        return QString::number(distance, 'f', 0);
    else if (dist > 0)
        return QString("%1.%2").arg(distance).arg((dist % 1000) / 100);
    return QString();
}

QString NavitHelper::getClosest(QList<QVariantMap> items, int maxDistance)
{
    QString ret;
    int retDistance = 1000000;
    for (QVariantMap item : items)
    {
        if (item.value("distance").toInt() < retDistance)
        {
            ret = item.value("label").toString();
            retDistance = item.value("distance").toInt();
        }
    }
    if (maxDistance > 0 && retDistance > maxDistance)
    {
        return QString();
    }

    return ret;
}

QVariantMap NavitHelper::getPOI(NavitInterface &navit, struct coord center, int distance)
{
    struct transformation *trans;

    struct map_selection *sel, *selm;
    struct coord c;
    struct pcoord pcenter;
    struct mapset_handle *h;
    struct map *m;
    struct map_rect *mr;
    struct item *item;

    enum projection pro;
    int idist = 0;

    trans = navit.get_trans();
    pro = transform_get_projection(trans);

    pcenter.x = center.x;
    pcenter.y = center.y;
    pcenter.pro = pro;

    int distanceSel = distance * transform_scale(abs(center.y) + distance * 1.5);
    sel = map_selection_rect_new(&(pcenter), distanceSel, 18);

    dbg(lvl_debug, "center is at %x, %x", center.x, center.y);

    h = mapset_open(navit.get_mapset());
    QList<QVariantMap> pois;
    while ((m = mapset_next(h, 1)))
    {
        selm = map_selection_dup_pro(sel, pro, map_projection(m));
        mr = map_rect_new(m, selm);
        dbg(lvl_debug, "mr=%p", mr);
        if (mr)
        {
            while ((item = map_rect_get_item(mr)))
            {
                if (item_is_poi(*item) &&
                    item_coord_get_pro(item, &c, 1, pro) &&
                    coord_rect_contains(&sel->u.c_rect, &c) &&
                    (idist = transform_distance(pro, &center, &c)) < distance)
                {
                    item_attr_rewind(item);
                    struct attr attr;
                    char *label;

                    if (item_attr_get(item, attr_label, &attr))
                    {
                        label = map_convert_string(item->map, attr.u.str);

                        QVariantMap poi;
                        poi.insert("label", label);
                        poi.insert("distance", idist);
                        pois << poi;
                    }
                }
            }
            map_rect_destroy(mr);
        }
        map_selection_destroy(selm);
    }
    map_selection_destroy(sel);
    mapset_close(h);

    QVariantMap ret;
    int retDistance = 1000000;
    for (QVariantMap poi : pois)
    {
        if (poi.value("distance").toInt() < retDistance)
        {
            ret = poi;
            retDistance = poi.value("distance").toInt();
        }
    }

    return ret;

    return QVariantMap();
}

QString NavitHelper::getAddress(NavitInterface &navit, struct coord center, QString filter)
{
    struct transformation *trans;

    struct map_selection *sel, *selm;
    struct coord c;
    struct pcoord pcenter;
    struct mapset_handle *h;
    struct map *m;
    struct map_rect *mr;
    struct item *item;

    enum projection pro;
    int idist = 0;
    int distance = 1000;

    trans = navit.get_trans();
    pro = transform_get_projection(trans);

    pcenter.x = center.x;
    pcenter.y = center.y;
    pcenter.pro = pro;

    int distanceSel = distance * transform_scale(abs(center.y) + distance * 1.5);
    sel = map_selection_rect_new(&(pcenter), distanceSel, 18);

    dbg(lvl_debug, "center is at %x, %x", center.x, center.y);

    h = mapset_open(navit.get_mapset());
    QList<QVariantMap> housenumbers;
    QList<QVariantMap> streets;
    QList<QVariantMap> districts;
    QList<QVariantMap> towns;
    while ((m = mapset_next(h, 1)))
    {
        selm = map_selection_dup_pro(sel, pro, map_projection(m));
        mr = map_rect_new(m, selm);
        dbg(lvl_debug, "mr=%p", mr);
        if (mr)
        {
            while ((item = map_rect_get_item(mr)))
            {
                if (!filter.isEmpty() && filter != item_to_name(item->type))
                {
                    continue;
                }

                if ((item_is_house_number(*item) || item_is_street(*item) || item_is_district(*item) || item_is_town(*item)) &&
                    item_coord_get_pro(item, &c, 1, pro) &&
                    coord_rect_contains(&sel->u.c_rect, &c) &&
                    (idist = transform_distance(pro, &center, &c)) < distance)
                {
                    item_attr_rewind(item);
                    struct attr attr;
                    char *label;

                    if (item_attr_get(item, attr_label, &attr))
                    {
                        label = map_convert_string(item->map, attr.u.str);
                        // TODO: District city and country search doesn't always work
                        if (item_is_house_number(*item))
                        {
                            QVariantMap street;
                            street.insert("label", label);
                            street.insert("distance", idist);
                            streets << street;
                        }
                        else if (item_is_street(*item))
                        {
                            QVariantMap street;
                            street.insert("label", QVariant(label));
                            street.insert("distance", idist);
                            streets << street;
                        }
                        else if (item_is_town(*item))
                        {
                            QVariantMap town;
                            town.insert("label", label);
                            town.insert("distance", idist);
                            towns << town;
                        }
                        else if (item_is_district(*item))
                        {
                            QVariantMap district;
                            district.insert("label", label);
                            district.insert("distance", idist);
                            districts << district;
                        }
                    }
                }
            }
            map_rect_destroy(mr);
        }
        map_selection_destroy(selm);
    }
    map_selection_destroy(sel);
    mapset_close(h);

    QStringList address;
    QString housenumber = getClosest(housenumbers, 15);
    QString street = getClosest(streets, 100);
    QString district = getClosest(districts);
    QString town = getClosest(towns);

    if (!housenumber.isEmpty())
    {
        address.append(housenumber);
    }
    else if (!street.isEmpty())
    {
        address.append(street);
    }

    if (street.isEmpty())
    {
        QString streetFar = getClosest(streets, 500);
        address.append(QString("%1 %2").arg("Near").arg(streetFar));
    }

    if (!district.isEmpty())
        address.append(district);

    if (!town.isEmpty())
        address.append(town);

    return address.join(", ");

    return QString();
}

coord NavitHelper::positionToCoord(NavitInterface &navit, int x, int y)
{
    struct coord co;

    struct point p;
    p.x = x;
    p.y = y;

    struct transformation *trans = navit.get_trans();
    transform_reverse(trans, &p, &co);

    return co;
}
pcoord NavitHelper::positionToPcoord(NavitInterface &navit, int x, int y)
{
    struct pcoord c;

    struct coord co = positionToCoord(navit, x, y);
    struct transformation *trans = navit.get_trans();

    c.pro = transform_get_projection(trans);
    c.x = co.x;
    c.y = co.y;

    return c;
}
pcoord NavitHelper::coordToPcoord(NavitInterface &navit, int x, int y)
{
    struct pcoord c;

    struct transformation *trans = navit.get_trans();

    c.pro = transform_get_projection(trans);
    c.x = x;
    c.y = y;

    return c;
}

static void setDestinationStatic(NavitInterface &navit, char *label, int x, int y)
{
    navit.set_destination(nullptr, nullptr, 0);
    struct pcoord c = NavitHelper::coordToPcoord(navit, x, y);
    navit.set_destination(&c, label, 1);
}

void NavitHelper::setDestination(NavitInterface &navit, QString label, int x, int y)
{
    // event_add_timeout(0, 0, callback_new_4(callback_cast(setDestinationStatic), navitInstance->getNavit(), label.toUtf8().data(), x ,y));
    setDestinationStatic(navit, label.toUtf8().data(), x, y);
}

static void setPositionStatic(NavitInterface &navit, int x, int y)
{
    struct pcoord c = NavitHelper::coordToPcoord(navit, x, y);
    navit.set_position(&c);
}

void NavitHelper::setPosition(NavitInterface &navit, int x, int y)
{
    // event_add_timeout(0, 0, callback_new_3(callback_cast(setPositionStatic), navitInstance->getNavit(), x ,y));
    setPositionStatic(navit, x, y);
}

static void addStopStatic(NavitInterface &navit, int position, char *label, pcoord *c)
{
    int dstcount = navit.get_destination_count() + 1;
    int pos, i;
    struct pcoord *dst = (pcoord *)g_alloca(dstcount * sizeof(struct pcoord));
    dstcount = navit.get_destinations(dst, dstcount);

    pos = position;
    if (pos < 0)
        pos = 0;

    for (i = dstcount; i > pos; i--)
        dst[i] = dst[i - 1];

    dst[pos] = *c;

    navit.add_destination_description(c, label);
    navit.set_destinations(dst, dstcount + 1, label, 1);
}

void NavitHelper::addStop(NavitInterface &navit, int position, QString label, int x, int y)
{
    struct pcoord c = NavitHelper::coordToPcoord(navit, x, y);
    event_add_timeout(0, 0, callback_new_4(callback_cast(addStopStatic), navit, position, label.toUtf8().data(), &c));
}
void addBookmarkStatic(NavitInterface &navit, QString label, int x, int y)
{
    struct attr attr;
    struct pcoord c = NavitHelper::positionToPcoord(navit, x, y);
    navit.get_attr(attr_bookmarks, &attr, nullptr);

    bookmarks_add_bookmark(attr.u.bookmarks, &c, label.toUtf8().data());
}
void NavitHelper::addBookmark(NavitInterface &navit, QString label, int x, int y)
{
    event_add_timeout(0, 0, callback_new_4(callback_cast(addBookmarkStatic), navit, label.toUtf8().data(), x, y));
}

char *NavitHelper::get_icon(NavitInterface &navit, struct item *item)
{

    struct attr layout;
    struct attr icon_src;
    GList *layer;
    navit.get_attr(attr_layout, &layout, NULL);
    layer = layout.u.layout->layers;

    while (layer)
    {
        GList *itemgra = ((struct layer *)layer->data)->itemgras;
        while (itemgra)
        {
            GList *types = ((struct itemgra *)itemgra->data)->type;
            while (types)
            {
                if ((long)types->data == item->type)
                {
                    GList *elementIter = ((struct itemgra *)itemgra->data)->elements;
                    while (elementIter)
                    {
                        struct element *el = (struct element *)elementIter->data;
                        if (el->type == element::element_icon)
                        {
                            char *src;
                            char *icon;
                            if (item_is_custom_poi(*item))
                            {
                                struct map_rect *mr = map_rect_new(item->map, NULL);
                                item = map_rect_get_item_byid(mr, item->id_hi, item->id_lo);
                                if (item_attr_get(item, attr_icon_src, &icon_src))
                                {
                                    src = el->u.icon.src;
                                    if (!src || !src[0])
                                        src = "%s";
                                    icon = g_strdup_printf(src, map_convert_string_tmp(item->map, icon_src.u.str));
                                }
                                else
                                {
                                    icon = g_strdup(el->u.icon.src);
                                }
                            }
                            else
                            {
                                icon = g_strdup(el->u.icon.src);
                            }
                            icon[strlen(icon) - 3] = 's';
                            icon[strlen(icon) - 2] = 'v';
                            icon[strlen(icon) - 1] = 'g';
                            return icon;
                            // FIXME
                            g_free(icon);
                        }
                        elementIter = g_list_next(elementIter);
                    }
                }
                types = g_list_next(types);
            }
            itemgra = g_list_next(itemgra);
        }
        layer = g_list_next(layer);
    }
    return "unknown.svg";
}

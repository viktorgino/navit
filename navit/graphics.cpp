/**
 * Navit, a modular navigation system.
 * Copyright (C) 2005-2008 Navit Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

// ##############################################################################################################
// #
// # File: graphics.c
// # Description:
// # Comment:
// # Authors: Martin Schaller (04/2008)
// #
// ##############################################################################################################

#include <stdlib.h>
#include <glib.h>
#include <stdio.h>
#include <math.h>

#include "graphics.h"

extern "C"
{
#include "config.h"
#include "debug.h"
#include "string.h"
#include "draw_info.h"
#include "point.h"
#include "projection.h"
#include "item.h"
#include "xmlconfig.h"
#include "map.h"
#include "coord.h"
#include "transform.h"
#include "plugin.h"
#include "profile.h"
#include "mapset.h"
#include "route.h"
#include "util.h"
#include "callback.h"
#include "file.h"
#include "event.h"
#include "util.h"
}

/**
 * @brief maximum amount of coordinates to allocate on stack using g_alloca
 */
#define ALLOCA_COORD_LIMIT 16384

constexpr QColor COLOR_WHITE_ = QColor(0xff, 0xff, 0xff, 0xff);
constexpr QColor COLOR_BLACK_ = QColor(0x00, 0x00, 0x00, 0xff);
constexpr QColor COLOR_BACKGROUND_ = QColor(0xFF, 0xEF, 0xB7, 0xFF);
constexpr QColor COLOR_TRANSPARENT = QColor(0x00, 0x00, 0x00, 0xff);

constexpr uint8_t UNDERGROUND_ALPHA_ = 0xFF;

// ##############################################################################################################
// # Description:
// # Comment:
// # Authors: Martin Schaller (04/2008)
// ##############################################################################################################
/**
 * @brief graphics object
 * A graphics object serves as the target for drawing operations.
 * It encapsulates various settings, and a drawing target, such as an image buffer or a window.
 * Currently, in Navit, there is always one main graphics object, which is used to draw the
 * map, and optionally additional graphics objects for overlays.
 * @see overlay_new()
 * @see GraphicsContext
 */

// static void
// circle_to_points(const struct point *center, int diameter, int scale, int start, int len, QVector<LayoutCoord *> &res,
//                  int *pos, int dir);

int Graphics::dpi_scale(int p)
{
    int result;
    result = p * m_dpi_factor;
    return result;
}
struct point Graphics::dpi_scale_point(LayoutCoord *point)
{
    struct point result = {-1, -1};
    result.x = dpi_scale(point->getX());
    result.y = dpi_scale(point->getY());
    return result;
}
struct point Graphics::dpi_scale_point(point *point)
{
    struct point result = {-1, -1};
    result.x = dpi_scale(point->x);
    result.y = dpi_scale(point->y);
    return result;
}
int Graphics::dpi_unscale(int p)
{
    int result;
    result = p / m_dpi_factor;
    return result;
}
struct point Graphics::dpi_unscale_point(struct point *p)
{
    struct point result = {-1, -1};
    result.x = dpi_unscale(p->x);
    result.y = dpi_unscale(p->y);
    return result;
}

/**
 * @brief Sets a generic attribute of the graphics instance
 *
 * This will only set one of the supported generic graphics attributes (currently {@code gamma},
 * {@code brightness}, {@code contrast} or {@code font_size}) and fail for other attribute types.
 *
 * To set an attribute provided by a graphics plugin, use {@link set_attr(struct graphics *, struct attr *)}
 * instead.
 *
 * @param gra The graphics instance
 * @param attr The attribute to set
 *
 * @return True if the attribute was set, false if not
 */
int Graphics::set_attr_do(struct attr *attr)
{
    switch (attr->type)
    {
    case attr_gamma:
        qDebug() << "Warning trying to set gamma";
        break;
    case attr_brightness:
        qDebug() << "Warning trying to set brightness";
        break;
    case attr_contrast:
        qDebug() << "Warning trying to set contrast";
        break;
    case attr_font_size:
        m_font_size = attr->u.num;
        return 1;
    default:
        return 0;
    }
    gc_init();
    return 1;
}

/**
 * @brief Sets an attribute of the graphics instance
 *
 * This method first tries to set one of the private attributes implemented by the current graphics
 * plugin. If this fails, it tries to set one of the generic attributes.
 *
 * If the graphics plugin does not supply a {@code set_attr} method, this method currently does nothing
 * and returns true, even if the attribute is a generic one.
 *
 * @param gra The graphics instance
 * @param attr The attribute to set
 *
 * @return True if the attribute was successfully set, false otherwise.
 */
int Graphics::set_attr(struct attr *attr)
{
    int ret = 1;
    /* FIXME if m_graphicsInterface.doesn't have a setter, we don't even try the generic attrs - is that what we want? */
    dbg(lvl_debug, "enter");
    ret = m_graphicsInterface.set_attr(attr);
    if (!ret)
        ret = set_attr_do(attr);
    return ret != 0;
}

void Graphics::set_rect(struct point_rect *pr)
{
    m_r = *pr;
}

/**
 * @brief unscale coordinates coming from the graphics backend via callback.
 *
 * @param l pointer to callback list
 * @param pcount number of parameters attached to this callback
 * @param p list of parameters
 * @param context context handed over by callback_list_add_patch_function, gra in this case.
 * @return nothing
 */
void Graphics::static_dpi_patch(struct callback_list *l, enum attr_type type, int pcount, void **p, void *context)
{
    Graphics *graphics = static_cast<Graphics *>(context);
    if (graphics == nullptr)
        return;
    graphics->dpi_patch(l, type, pcount, p);
}

void Graphics::dpi_patch(struct callback_list *l, enum attr_type type, int pcount, void **p)
{
    /* this is black magic. We scaled all coordinates to the graphics backend
     * to compensate screen dpi. Since the backends communicate back via the callback
     * list, we hook this function to unscale the coordinates coming back to
     * navit before actually calling the callbacks.
     */

    if ((type == attr_resize) && (pcount >= 2))
    {
        int w, h;
        w = GPOINTER_TO_INT(p[0]);
        h = GPOINTER_TO_INT(p[1]);
        dbg(lvl_debug, "scaling attr_resize %d, %d, %d", pcount, w, h);
        p[0] = GINT_TO_POINTER(dpi_unscale(w));
        p[1] = GINT_TO_POINTER(dpi_unscale(h));
    }
    if ((type == attr_button) && (pcount >= 3))
    {
        struct point *pnt;
        pnt = (struct point *)p[2];
        dbg(lvl_debug, "scaling attr_button %d, %d, %d", pcount, pnt->x, pnt->y);
        *pnt = dpi_unscale_point(pnt);
    }
    if ((type == attr_motion) && (pcount >= 1))
    {
        struct point *pnt;
        pnt = (struct point *)p[0];
        dbg(lvl_debug, "scaling attr_motion %d, %d, %d", pcount, pnt->x, pnt->y);
        *pnt = dpi_unscale_point(pnt);
    }
    /* any more?  attr_keypress doesn't come with coordinates */
}

Graphics::Graphics(NavitInterface &navit, GraphicsFunctions &graphicsFunctions,
                   QObject *parent) : QObject(parent),
                                      m_parent(std::nullopt),
                                      m_navit(navit),
                                      m_graphics_functions(graphicsFunctions),
                                      m_callbacks(callback_list_new()),
                                      m_graphicsInterface(*m_graphics_functions.new_graphics(navit, m_callbacks)),
                                      m_contextInterface(*m_graphics_functions.new_graphics_context()),
                                      m_gcBackground(m_contextInterface, this),
                                      m_gcMiddground(m_contextInterface, this),
                                      m_gcForeground(m_contextInterface, this)
{

    /* start with no scaling */
    m_dpi_factor = 1;
    // TODO: Add configurable DPI scaling

    m_font_size = 20;
    m_image_cache_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    // TODO: Add resize callback
    // if (m_dpi_factor != 1)
    //     callback_list_call_attr_2(m_callbacks, attr_resize, GINT_TO_POINTER(navit_get_width(parent->u.navit)),
    //                               GINT_TO_POINTER(navit_get_height(parent->u.navit)));
}

/**
 * @brief Create a new graphics overlay.
 * An overlay is a graphics object that is independent of the main graphics object. When
 * drawing everything to a window, the overlay will be shown on top of the main graphics
 * object. Navit uses overlays for OSD elements and for the vehicle on the map.
 * This allows updating OSD elements and the vehicle without redrawing the map.
 *
 * @param parent parent graphics context (should be the main graphics context as returned by
 *        new)
 * @param p drawing position for the overlay
 * @param w width of overlay
 * @param h height of overlay
 * @param wraparound use wraparound (0/1). If set, position, width and height "wrap around":
 * negative position coordinates wrap around the window, negative width/height specify
 * difference to window width/height.
 * @returns new overlay
 * @author Martin Schaller (04/2008)
 */

Graphics::Graphics(
    NavitInterface &navit, Graphics &parent_graphics,
    point *p, int w, int h, int wraparound, QObject *parent) : QObject(parent),
                                                               m_parent(&parent_graphics),
                                                               m_navit(navit),
                                                               m_graphics_functions(parent_graphics.get_graphics_functions()),
                                                               m_callbacks(callback_list_new()),
                                                               m_graphicsInterface(
                                                                   *m_graphics_functions.new_graphics_overlay(parent_graphics.dpi_scale_point(p),
                                                                                                              parent_graphics.dpi_scale(w),
                                                                                                              parent_graphics.dpi_scale(h),
                                                                                                              wraparound, parent_graphics.get_graphics_interface())),
                                                               m_contextInterface(*m_graphics_functions.new_graphics_context()),
                                                               m_gcBackground(m_contextInterface, this),
                                                               m_gcMiddground(m_contextInterface, this),
                                                               m_gcForeground(m_contextInterface, this)
{
    assert(m_parent);

    point_rect pr = {
        .lu = {
            .x = 0,
            .y = 0,
        },
        .rl = {
            .x = w,
            .y = h,

        },
    };

    m_dpi_factor = parent_graphics.get_dpi_factor();
    m_image_cache_hash = parent_graphics.getImageCacheHash();

    m_font_size = 20;
    set_rect(&pr);
}

Graphics::~Graphics()
{
    /* If it's not an overlay, free the image cache. */
    // TODO: do we need to destroy layouts?
    // if (!m_parent.has_value())
    // {
    //     struct graphics_image *img;
    //     GList *ll, *l;
    // /* We can't specify context (pointer to struct graphics) for g_hash_table_new to have it passed to free function
    //    so we have to free img->priv manually, the rest would be freed by g_hash_table_destroy. GHashTableIter isn't used because it
    //    broke n800 build at r5107.
    // */
    // for (ll = l = g_hash_to_list(m_image_cache_hash); l; l = g_list_next(l))
    // {
    //     img = (graphics_image *)layout->data;
    //     if (img)
    //         m_graphicsInterface.image_free(img->priv);
    // }
    // g_list_free(ll);
    // g_hash_table_destroy(m_image_cache_hash);
    // }

    // m_gcBackground->destroy();
    // m_gcMiddground->destroy();
    // m_gcForeground->destroy();
    font_destroy_all();
    g_free(m_font);
    // m_graphicsInterface.destroy();
}

GraphicsFunctions &Graphics::get_graphics_functions()
{
    return m_graphics_functions;
}

NavitGraphicsInterface &Graphics::get_graphics_interface()
{
    return m_graphicsInterface;
}

NavitInterface &Graphics::get_navit_interface()
{
    return m_navit;
}
/**
 * @brief Gets an attribute of the graphics instance
 *
 * This function searches the attribute list of the graphics object for an attribute of a given type and
 * stores it in the attr parameter.
 * <p>
 * Searching for attr_any or attr_any_xml is supported.
 * <p>
 * An iterator can be specified to get multiple attributes of the same type:
 * The first call will return the first match from attr; each subsequent call
 * with the same iterator will return the next match. If no more matching
 * attributes are found in either of them, false is returned.
 * <p>
 * Note that currently this will only return the generic attributes which can be set with
 * {@link set_attr_do(struct graphics *, struct attr *)}. Attributes implemented by a graphics
 * plugin cannot be retrieved with this method.
 *
 * @param this The graphics instance
 * @param type The attribute type to search for
 * @param attr Points to a {@code struct attr} which will receive the attribute
 * @param iter An iterator. This parameter may be NULL.
 *
 * @return True if a matching attribute was found, false if not.
 *
 * @author Martin Schaller (04/2008)
 */
int Graphics::get_attr(enum attr_type type, struct attr *attr, struct attr_iter *iter)
{
    // TODO: Add getting attrs
    return 0;
}

GHashTable *Graphics::getImageCacheHash()
{
    return m_image_cache_hash;
}

/**
 * @brief Alters the size, position and wraparound for an overlay
 *
 * @param this_ The overlay's graphics struct
 * @param p The new position of the overlay
 * @param w The new width of the overlay
 * @param h The new height of the overlay
 * @param wraparound The new wraparound of the overlay
 */
void Graphics::overlay_resize(struct point *p, int w, int h, int wraparound)
{
    struct point p_scaled;
    int w_scaled, h_scaled;

    p_scaled = dpi_scale_point(p);
    w_scaled = dpi_scale(w);
    h_scaled = dpi_scale(h);
    m_graphicsInterface.overlay_resize(&p_scaled, w_scaled, h_scaled, wraparound);
}

void Graphics::gc_init()
{
    QColor background = {COLOR_BACKGROUND_};
    QColor black = {COLOR_BLACK_};
    QColor white = {COLOR_WHITE_};

    m_gcBackground.set_background(background);
    m_gcBackground.set_foreground(background);
    m_gcMiddground.set_background(black);
    m_gcMiddground.set_foreground(white);
    m_gcForeground.set_background(white);
    m_gcForeground.set_foreground(black);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::init()
{

    gc_init();
    background_gc(&m_gcBackground);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void *Graphics::get_data(const char *type)
{
    return (m_graphicsInterface.get_data(type));
}

void Graphics::add_callback(struct callback *cb)
{
    callback_list_add(m_callbacks, cb);
}

void Graphics::remove_callback(struct callback *cb)
{
    callback_list_remove(m_callbacks, cb);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
struct graphics_font *Graphics::font_new(int size, int flags)
{

    return named_font_new(m_default_font.toLocal8Bit().data(), size, flags);
}

struct graphics_font *Graphics::named_font_new(char *font, int size, int flags)
{
    struct graphics_font *this_;

    this_ = g_new0(struct graphics_font, 1);
    this_->priv = m_graphicsInterface.font_new(font, dpi_scale(size), flags);
    return this_;
}

void Graphics::font_destroy(struct graphics_font *gra_font)
{
    if (!gra_font)
        return;
    m_graphicsInterface.font_destroy(gra_font->priv);
    g_free(gra_font);
}

/**
 * Free all loaded fonts.
 * Used when switching layouts.
 * @param gra The graphics instance
 * @returns nothing
 * @author Sarah Nordstrom (05/2008)
 */
void Graphics::font_destroy_all()
{
    int i;
    for (i = 0; i < m_font_len; i++)
    {
        if (!m_font[i])
            continue;
        m_graphicsInterface.font_destroy(m_font[i]->priv);
        g_free(m_font[i]);
        m_font[i] = NULL;
    }
}

/**
 * @brief Create a new image from file path, optionally scaled to w and h pixels.
 *
 * @param gra the graphics instance
 * @param path path of the image to load
 * @param w width to rescale to, or IMAGE_W_H_UNSET for original width
 * @param h height to rescale to, or IMAGE_W_H_UNSET for original height
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
struct graphics_image *Graphics::image_new_scaled(QString &path, int w, int h)
{
    return image_new_scaled_rotated(path, w, h, 0);
}

void Graphics::image_new_helper(graphics_image *image, QString &path, char *name, int width, int height, int rotate)
{
    int i = 0;
    int stdsizes[] = {8, 12, 16, 22, 24, 32, 36, 48, 64, 72, 96, 128, 192, 256};
    const int numstdsizes = sizeof(stdsizes) / sizeof(int);
    int sz;
    int mode = 1;
    int bmstd = 0;
    sz = width > 0 ? width : height;
    while (mode <= 8)
    {
        QString new_name;
        int n;
        switch (mode)
        {
        case 1:
            /* The best variant both for cpu usage and quality would be prescaled png of a needed size */
            mode++;
            if (width != IMAGE_W_H_UNSET && height != IMAGE_W_H_UNSET)
            {
                new_name = QString("%s_%d_%d.png").arg(name).arg(width, height);
            }
            break;
        case 2:
            mode++;
            /* Try to load image by the exact name given by user. For example, if she wants to
              scale some prescaled png variant to a new size given as function params, or have
              default png image to be displayed unscaled. */
            new_name = path;
            break;
        case 3:
            mode++;
            /* Next, try uncompressed and compressed svgs as they should give best quality but
               rendering might take more cpu resources when the image is displayed for the first time */
            new_name = QString("%s.svg").arg(name);
            break;
        case 4:
            mode++;
            new_name = QString("%s.svgz").arg(name);
            break;
        case 5:
            mode++;
            i = 0;
            /* If we have no size specifiers, try the default png now */
            if (sz <= 0)
            {
                new_name = QString("%s.png").arg(name);
                break;
            }
            /* Find best matching size from standard row */
            for (bmstd = 0; bmstd < numstdsizes; bmstd++)
                if (stdsizes[bmstd] > sz)
                    break;
            i = 1;
        /* Fall through */
        case 6:
            /* Select best matching image from standard row */
            if (sz > 0)
            {
                /* If size were specified, start with bmstd and then try standard sizes in row
                 * bmstd, bmstd+1, bmstd+2, .. numstdsizes-1, bmstd-1, bmstd-2, .., 0 */
                n = bmstd + i;
                if ((bmstd + i) >= numstdsizes)
                    n = numstdsizes - i - 1;

                if (++i == numstdsizes)
                    mode++;
            }
            else
            {
                /* If no size were specified, start with the smallest standard size and then try following ones */
                n = i++;
                if (i == numstdsizes)
                    mode += 2;
            }
            if (n < 0 || n >= numstdsizes)
                break;
            new_name = QString("%s_%d_%d.png").arg(name).arg(stdsizes[n], stdsizes[n]);
            break;

        case 7:
            /* Scaling the default prescaled png of unknown size to the needed size will give random quality loss */
            mode++;
            new_name = QString("%s.png").arg(name);
            break;
        case 8:
            /* xpm format is used as a last resort, because its not widely supported and we are moving to svg and png formats */
            mode++;
            new_name = QString("%s.xpm").arg(name);
            break;
        }
        if (new_name.isEmpty())
            continue;

        image->width = width;
        image->height = height;
        // dbg(lvl_debug, "Trying to load image '%s' for '%s' at %dx%d", new_name, path, width, height);

        image->hot = dpi_scale_point(&image->hot);
        if (image->width != IMAGE_W_H_UNSET)
            image->width = dpi_scale(image->width);
        if (image->height != IMAGE_W_H_UNSET)
            image->height = dpi_scale(image->height);
        image->priv = m_graphicsInterface.image_new(&image->meth, new_name, &image->width, &image->height, &image->hot, rotate);
        image->hot = dpi_unscale_point(&image->hot);
        if (image->width != IMAGE_W_H_UNSET)
            image->width = dpi_unscale(image->width);
        if (image->height != IMAGE_W_H_UNSET)
            image->height = dpi_unscale(image->height);

        if (image->priv)
        {
            // dbg(lvl_info, "Using image '%s' for '%s' at %dx%d", new_name, path, width, height);
            break;
        }
    }
}

/**
 * @brief Create a new image from file path, optionally scaled to w and h pixels and rotated.
 *
 * @param gra the graphics instance
 * @param path path of the image to load
 * @param w width to rescale to, or IMAGE_W_H_UNSET for original width
 * @param h height to rescale to, or IMAGE_W_H_UNSET for original height
 * @param rotate angle to rotate the image, in 90 degree steps (not supported by all plugins).
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
struct graphics_image *Graphics::image_new_scaled_rotated(QString &path, int w, int h, int rotate)
{
    struct graphics_image *image;
    char *hash_key = g_strdup_printf("%s*%d*%d*%d", path.toLocal8Bit().data(), w, h, rotate);
    struct file_wordexp *we;
    int i;
    char **paths;
    if (g_hash_table_lookup_extended(m_image_cache_hash, hash_key, NULL, (void **)&image))
    {
        g_free(hash_key);
        dbg(lvl_debug, "Found cached image%sfor '%s'", image ? " " : " miss ", path.toLocal8Bit().data());
        return image;
    }

    image = g_new0(struct graphics_image, 1);
    image->height = h;
    image->width = w;

    we = file_wordexp_new(path.toLocal8Bit().data());
    paths = file_wordexp_get_array(we);

    for (i = 0; i < file_wordexp_get_count(we) && !image->priv; i++)
    {
        char *ext;
        char *s, *name;
        char *pathi = paths[i];
        int len = strlen(pathi);
        int i, k;
        int newwidth = IMAGE_W_H_UNSET, newheight = IMAGE_W_H_UNSET;

        ext = g_utf8_strrchr(pathi, -1, '.');
        i = pathi - ext + len;

        /* Dont allow too long or too short file name extensions*/
        if (ext && ((i > 5) || (i < 1)))
            ext = NULL;

        /* Search for _w_h name part, begin from char before extension if it exists */
        if (ext)
            s = ext - 1;
        else
            s = pathi + len;

        k = 1;
        while (s > pathi && g_ascii_isdigit(*s))
        {
            if (newheight < 0)
                newheight = 0;
            newheight += (*s - '0') * k;
            k *= 10;
            s--;
        }

        if (k > 1 && s > pathi && *s == '_')
        {
            k = 1;
            s--;
            while (s > pathi && g_ascii_isdigit(*s))
            {
                if (newwidth < 0)
                    newwidth = 0;
                newwidth += (*s - '0') * k;
                ;
                k *= 10;
                s--;
            }
        }

        if (k == 1 || s <= pathi || *s != '_')
        {
            newwidth = IMAGE_W_H_UNSET;
            newheight = IMAGE_W_H_UNSET;
            if (ext)
                s = ext;
            else
                s = pathi + len;
        }

        /* If exact h and w values were given as function parameters, they take precedence over values guessed from the image name */
        if (w != IMAGE_W_H_UNSET)
            newwidth = w;
        if (h != IMAGE_W_H_UNSET)
            newheight = h;

        name = g_strndup(pathi, s - pathi);
        QString pathi_ = pathi;
        image_new_helper(image, pathi_, name, newwidth, newheight, rotate);
        g_free(name);
    }

    file_wordexp_destroy(we);

    if (!image->priv)
    {
        qDebug() << "No image for " << path;
        g_free(image);
        image = NULL;
    }

    g_hash_table_insert(m_image_cache_hash, hash_key, (gpointer)image);

    return image;
}

/**
 * Create a new image from file path
 * @param gra the graphics instance
 * @param path path of the image to load
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
struct graphics_image *Graphics::image_new(QString &path)
{
    return image_new_scaled_rotated(path, IMAGE_W_H_UNSET, IMAGE_W_H_UNSET, 0);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::image_free(struct graphics_image *img)
{
    /* Image is cached inside m_image_cache_hash. So it would be freed only when graphics is destroyed => Do nothing here. */
}

/**
 * @brief Start or finish a set of drawing operations.
 *
 * draw_mode(draw_mode_begin) must be invoked before performing any drawing
 * operations; this allows the graphics driver to perform any necessary setup.
 * draw_mode(draw_mode_end) must be invoked to finish a set of drawing operations;
 * this will typically clean up drawing resources and display the drawing result.
 * @param this_ graphics object that is being drawn to
 * @param mode specify beginning or end of drawing
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_mode(enum draw_mode_num mode, bool call_callback)
{
    if (call_callback)
    {
        enum attr_type callback_type = mode == draw_mode_end ? attr_postdraw : attr_predraw;
        callback_list_call_attr_0(m_callbacks, callback_type);
    }
    m_graphicsInterface.draw_mode(mode);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_lines(GraphicsContext *gc, QVector<LayoutCoord *> &p)
{
    struct point *p_scaled;
    int a;
    if (p.size() < ALLOCA_COORD_LIMIT)
        p_scaled = (point *)g_alloca(sizeof(struct point) * p.size());
    else
        p_scaled = (point *)g_malloc(sizeof(struct point) * p.size());

    for (a = 0; a < p.size(); a++)
        p_scaled[a] = dpi_scale_point(p[a]);
    m_graphicsInterface.draw_lines(&gc->get_context_interface(), p_scaled, p.size());
    if (p.size() >= ALLOCA_COORD_LIMIT)
        g_free(p_scaled);
}

/**
 * @brief Draw a circle
 * @param this_ The graphics instance on which to draw
 * @param gc The graphics context
 * @param p The coordinates of the center of the circle
 * @param r The radius of the circle
 *
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_circle(GraphicsContext *gc, LayoutCoord *p, int r)
{
    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface.draw_circle(&gc->get_context_interface(), &p_scaled, dpi_scale(r));
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_rectangle(GraphicsContext *gc, LayoutCoord *p, int w, int h)
{
    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface.draw_rectangle(&gc->get_context_interface(), &p_scaled, dpi_scale(w), dpi_scale(h));
}

/**
 * @brief Draw a plain polygon on the display
 *
 * @param gra The graphics instance on which to draw
 * @param gc The graphics context
 * @param[in] pin An array of points forming the polygon
 * @param count_in The number of elements inside @p pin
 */
void Graphics::draw_polygon(GraphicsContext *gc, QVector<LayoutCoord *> &pin)
{
    struct point *pin_scaled;
    int a;
    if (pin.size() < ALLOCA_COORD_LIMIT)
        pin_scaled = (point *)g_alloca(sizeof(struct point) * pin.size());
    else
        pin_scaled = (point *)g_malloc(sizeof(struct point) * pin.size());

    for (a = 0; a < pin.size(); a++)
        pin_scaled[a] = dpi_scale_point(pin[a]);
    m_graphicsInterface.draw_polygon(&gc->get_context_interface(), pin_scaled, pin.size());
    if (pin.size() >= ALLOCA_COORD_LIMIT)
        g_free(pin_scaled);
}

/**
 * @brief Draw a plain polygon with holes on the display
 *
 * @param gra The graphics instance on which to draw
 * @param gc The graphics context
 * @param[in] pin An array of points forming the polygon
 * @param count_in The number of elements inside @p pin
 * @param hole_count The number of hole polygons to cut out
 * @param pcount array of [hole_count] integers giving the number of
 *        points per hole
 * @param holes array of point arrays for the hole polygons
 */
void Graphics::draw_polygon_with_holes(GraphicsContext *gc, QVector<LayoutCoord *> &pin, DisplayitemPolyHoles &holes)
{

    struct point *pin_scaled;
    struct point **holes_scaled;
    int *hole_coord_count;
    if (pin.size() < ALLOCA_COORD_LIMIT)
    {
        pin_scaled = (point *)g_alloca(sizeof(struct point) * pin.size());
    }
    else
    {
        pin_scaled = (point *)g_malloc(sizeof(struct point) * pin.size());
    }
    if (holes.size() < ALLOCA_COORD_LIMIT)
    {
        holes_scaled = (point **)g_alloca(sizeof(struct point *) * holes.size());
        hole_coord_count = (int *)g_alloca(sizeof(int) * holes.size());
    }
    else
    {
        holes_scaled = (point **)g_malloc(sizeof(struct point *) * holes.size());
        hole_coord_count = (int *)g_malloc(sizeof(int) * holes.size());
    }
    /* scale the outline */
    for (int a = 0; a < pin.size(); a++)
    {
        pin_scaled[a] = dpi_scale_point(pin[a]);
    }
    /*scale the holes */
    for (int b = 0; b < holes.size(); b++)
    {
        QVector<LayoutCoord *> hole = holes[b];
        holes_scaled[b] = (point *)g_malloc(sizeof(*(holes_scaled[b])) * hole.size());
        for (int i = 0; i < hole.size(); i++)
        {
            holes_scaled[b][i] = dpi_scale_point(hole[i]);
        }
    }
    m_graphicsInterface.draw_polygon_with_holes(&gc->get_context_interface(), pin_scaled, pin.size(), holes.size(), hole_coord_count, holes_scaled);
    /* free the hole arrays */
    for (int b = 0; b < holes.size(); b++)
    {
        g_free(holes_scaled[b]);
    }
    if (pin.size() >= ALLOCA_COORD_LIMIT)
    {
        g_free(pin_scaled);
    }
    if (holes.size() >= ALLOCA_COORD_LIMIT)
    {
        g_free(holes_scaled);
        g_free(hole_coord_count);
    }
}

void Graphics::draw_rectangle_rounded(GraphicsContext *gc, struct point *plu, int w, int h,
                                      int r, int fill)
{
    // TODO: maybe fix, but not used
    // QVector<LayoutCoord *> p;
    // struct point pi0 = {plu->x + r, plu->y + r};
    // struct point pi1 = {plu->x + w - r, plu->y + r};
    // struct point pi2 = {plu->x + w - r, plu->y + h - r};
    // struct point pi3 = {plu->x + r, plu->y + h - r};
    // int i = 0;
    // for (int x = 0; i < r * 4 + 32; i++)
    // {
    //     p.append(new LayoutCoord());
    // }

    // circle_to_points(&pi2, r * 2, 0, -1, 258, p, i, 1);
    // circle_to_points(&pi1, r * 2, 0, 255, 258, p, i, 1);
    // circle_to_points(&pi0, r * 2, 0, 511, 258, p, i, 1);
    // circle_to_points(&pi3, r * 2, 0, 767, 258, p, i, 1);
    // p[i] = p[0];
    // i++;
    // QVector<LayoutCoord *> res = QVector<LayoutCoord *>(p.begin(), p.begin() + i);
    // if (fill)
    //     draw_polygon(gc, res);
    // else
    //     draw_lines(gc, res);

    // qDeleteAll(p);
    // qDeleteAll(res);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_text(GraphicsContext *gc1, GraphicsContext *gc2,
                         struct graphics_font *font, QString &text, LayoutCoord *p, int dx, int dy)
{
    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface.draw_text(&gc1->get_context_interface(), gc2 ? &gc2->get_context_interface() : nullptr, font->priv, text, &p_scaled, dx, dy);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::get_text_bbox(struct graphics_font *font, QString &text, int dx, int dy,
                             struct point *ret, int estimate)
{
    m_graphicsInterface.get_text_bbox(font->priv, text, dx, dy, ret, estimate);
    ret[0] = dpi_unscale_point(&(ret[0]));
    ret[1] = dpi_unscale_point(&(ret[1]));
    ret[2] = dpi_unscale_point(&(ret[2]));
    ret[3] = dpi_unscale_point(&(ret[3]));
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::overlay_disable(int disable)
{
    m_disabled = disable;
    m_graphicsInterface.overlay_disable(disable);
}

int Graphics::is_disabled()
{
    return m_disabled || (m_parent.has_value() && m_parent.value()->is_disabled());
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_image(GraphicsContext *gc, struct point *p, struct graphics_image *img)
{
    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface.draw_image(&gc->get_context_interface(), &p_scaled, img->priv);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_image_warp(GraphicsContext *gc, LayoutCoord *p, struct graphics_image *img)
{
    struct point p_scaled = dpi_scale_point(p);
    m_graphicsInterface.draw_image_warp(&gc->get_context_interface(), &p_scaled, 1, img->priv);
}

// ##############################################################################################################
// # Description:
// # Comment:
// # Authors: Martin Schaller (04/2008)
// ##############################################################################################################
int Graphics::draw_drag(struct point *p)
{
    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface.draw_drag(&p_scaled);
    return 1;
}

void Graphics::background_gc(GraphicsContext *gc)
{
    m_graphicsInterface.background_gc(&gc->get_context_interface());
}

void Graphics::set_layout(Layout *layout)
{
    if (layout)
    {
        m_gcBackground.set_background(layout->getColor());
        m_gcBackground.set_foreground(layout->getColor());
        m_default_font = layout->getFont();
    }
    background_gc(&m_gcBackground);
}

void Graphics::draw_background()
{
    LayoutCoord lu(&m_r.lu);
    draw_rectangle(&m_gcBackground, &lu, m_r.rl.x - m_r.lu.x, m_r.rl.y - m_r.lu.y);
}

void Graphics::set_z_order(int z_order)
{
    m_current_z_order = z_order;
}

/**
 * @brief Shows the native on-screen keyboard or other input method
 *
 * This method is a wrapper around the respective method of the graphics plugin.
 *
 * The caller should populate the {@code kbd} argument with appropriate {@code mode} and {@code lang}
 * members so the graphics plugin can determine the best matching layout.
 *
 * If an input method is shown, the graphics plugin should try to select the configuration which best
 * matches the specified {@code mode}. For example, if {@code mode} specifies a numeric layout, the
 * graphics plugin should select a numeric keyboard layout (if available), or the equivalent for another
 * input method (such as setting stroke recognition to identify strokes as numbers). Likewise, when an
 * alphanumeric-uppercase mode is requested, it should switch to uppercase input.
 *
 * Implementations should, however, consider that Navit's internal keyboard allows the user to switch
 * modes at will (the only exception being degree mode) and thus must not "lock" the user into a limited
 * layout with no means to switch to a general-purpose one. For example, house number entry in an
 * address search dialog may default to numeric mode, but since some house numbers may contain
 * non-numeric characters, a pure numeric keyboard is suitable only if the user has the option to switch
 * to an alphanumeric layout.
 *
 * When multiple alphanumeric layouts are available, the graphics plugin should use the {@code lang}
 * argument to determine the best layout.
 *
 * When selecting an input method, preference should always be given to the default or last selected
 * input method and configuration if it matches the requested {@code mode} and {@code lang}.
 *
 * If the native input method is going to obstruct parts of Navit's UI, the graphics plugin should set
 * {@code kbd->w} and {@code kbd->h} to the height and width to the appropriate value in pixels. A value
 * of -1 indicates that the input method fills the entire available width or height of the space
 * available to Navit. On windowed platforms, where the on-screen input method and Navit's window may be
 * moved relative to each other as needed and can be displayed alongside each other, the graphics plugin
 * should report 0 for both dimensions.
 *
 * @param this_ The graphics instance
 * @param kbd The keyboard instance
 *
 * @return 1 if the native keyboard is going to be displayed, 0 if not, -1 if the method is not
 * supported by the plugin
 */
int Graphics::show_native_keyboard(struct graphics_keyboard *kbd)
{
    int ret = m_graphicsInterface.show_native_keyboard(kbd);
    dbg(lvl_debug, "return %d", ret);
    return ret;
}

/**
 * @brief Hides the native on-screen keyboard or other input method
 *
 * This method is a wrapper around the respective method of the graphics plugin.
 *
 * A call to this function indicates that Navit no longer needs the input method and is about to reclaim
 * any screen real estate it may have previously reserved for the input method.
 *
 * On platforms that don't support overlapping windows this means that the on-screen input method should
 * be hidden, as it may otherwise obstruct parts of Navit's UI.
 *
 * On windowed platforms, where on-screen input methods can be displayed alongside Navit or moved around
 * as needed, the graphics driver should instead notify the on-screen method that it is no longer
 * expecting user input, allowing the input method to take the appropriate action.
 *
 * The graphics plugin must free any data it has stored in {@code kbd->gra_priv} and reset the pointer
 * to {@code NULL} to indicate it has done so.
 *
 * The caller may free {@code kbd} after this function returns.
 *
 * @param this The graphics instance
 * @param kbd The keyboard instance
 *
 * @return True if the call was successfully passed to the plugin, false if the method is not supported
 * by the plugin
 */
int Graphics::hide_native_keyboard(struct graphics_keyboard *kbd)
{
    m_graphicsInterface.hide_native_keyboard(kbd);
    return 1;
}

#include "attr.h"
#include <stdio.h>

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::label_line(GraphicsContext *fg, GraphicsContext *bg, struct graphics_font *font, QVector<LayoutCoord *> points, QString &label)
{
    int i, x, y, tl, tlm, th, thm, tlsq, l;
    float lsq;
    double dx, dy;
    LayoutCoord p_t;
    struct point pb[5];

    get_text_bbox(font, label, 0x10000, 0x00, pb, 1);
    tl = (pb[2].x - pb[0].x);
    th = (pb[0].y - pb[1].y);

    tlm = tl * 32;
    thm = th * 36;
    tlsq = tlm * tlm;
    for (i = 0; i < points.size() - 1; i++)
    {
        dx = points[i + 1]->getX() - points[i]->getX();
        dx *= 32;
        dy = points[i + 1]->getY() - points[i]->getY();
        dy *= 32;
        lsq = dx * dx + dy * dy;
        if (lsq > tlsq)
        {
            l = (int)sqrtf(lsq);
            x = points[i]->getX();
            y = points[i]->getY();
            if (dx < 0)
            {
                dx = -dx;
                dy = -dy;
                x = points[i + 1]->getX();
                y = points[i + 1]->getY();
            }
            x += (l - tlm) * dx / l / 64;
            y += (l - tlm) * dy / l / 64;
            x -= dy * thm / l / 64;
            y += dx * thm / l / 64;
            p_t.set(x, y);
            if (x < m_r.rl.x && x + tl > m_r.lu.x && y + tl > m_r.lu.y && y - tl < m_r.rl.y)
                draw_text(fg, bg, font, label, &p_t, dx * 0x10000 / l, dy * 0x10000 / l);
        }
    }
}

void Graphics::display_draw_arrow(LayoutCoord *p, navit_float dx, navit_float dy, navit_float width, struct display_context *dc, int filled)
{
    /* half the width in every direction */
    width /= 2;
    LayoutCoord start(-dx * width + dy * width, -dy * width - dx * width);
    LayoutCoord end(-dx * width - dy * width, -dy * width + dx * width);

    QVector<LayoutCoord *> pnt = {&start, p, &end};

    if (filled)
    {
        /* close the loop */
        pnt.append(pnt[0]);
        draw_polygon(dc->gc, pnt);
    }
    else
    {
        draw_lines(dc->gc, pnt);
    }
    qDeleteAll(pnt);
}

/**
 * @brief draw arrows along a multi polygon line
 *
 * This function draws arrows along a multi polygon line, and scales the
 * arrows according to current view settings by interpolating sizes at
 * given arrow position,
 *
 * @param gra current graphics instance handle
 * @param dc current drawing context
 * @param pnt array of points for this polyline
 * @param count number of points in pnt
 * @param width arrray of integers giving the expexted line width at the corresponding point
 * @param filled. True to draw filled arrows, false to draw only line arrows.
 */
void Graphics::display_draw_arrows(struct display_context *dc, QVector<LayoutCoord *> &pnt, QVector<int> &widths, int filled)
{
    navit_float dx, dy, dw, l;
    int i;
    LayoutCoord p;
    int x, y;
    int w;
    for (i = 0; i < pnt.size() - 1; i++)
    {
        /* get the X and Y size */
        dx = pnt[i + 1]->getX() - pnt[i]->getX();
        dy = pnt[i + 1]->getY() - pnt[i]->getY();
        dw = widths[i + 1] - widths[i];
        /* calculate the length of the way segment */
        l = navit_sqrt(dx * dx + dy * dy);
        if (l)
        {
            /* length is not zero */
            /* calculate the vector per length */
            dx = dx / l;
            dy = dy / l;
            dw = dw / l;
            /* different behaviour for oneway arrows than for routing graph ones */
            if (filled)
            {
                if (l > (2 * widths[i]))
                {
                    /* print arrow at middle point */
                    x += pnt[i]->getX() + dx * (l / 2);
                    y += pnt[i]->getY() + dy * (l / 2);
                    p.set(x, y);
                    w = widths[i];
                    w += dw * (l / 2);
                    display_draw_arrow(&p, dx, dy, w, dc, filled);
                }
                /* if line is quite long, print arrows at 1/4 and 3/4 length */
                if (l > (20 * widths[i]))
                {
                    /* at 1/4 the line length */
                    x += pnt[i]->getX() + dx * (l / 4);
                    y += pnt[i]->getY() + dy * (l / 4);
                    p.set(x, y);
                    w = widths[i];
                    w += dw * (l / 4);
                    display_draw_arrow(&p, dx, dy, w, dc, filled);
                    /* at 3/4 the arrow length */
                    x += pnt[i + 1]->getX() - dx * (l / 4);
                    y += pnt[i + 1]->getY() - dy * (l / 4);
                    p.set(x, y);
                    w = widths[i + 1];
                    w -= dw * (l / 4);
                    display_draw_arrow(&p, dx, dy, w, dc, filled);
                }
            }
            else
            {
                /*FIXME: what if line length was smaller than 15?*/
                /* print arrow 15 units from start */
                x += pnt[i]->getX() + dx * 15;
                y += pnt[i]->getY() + dy * 15;
                p.set(x, y);
                display_draw_arrow(&p, dx, dy, 20, dc, filled);
                /* print arrow 15 units before end */
                x += pnt[i + 1]->getX() - dx * 15;
                y += pnt[i + 1]->getY() - dy * 15;
                p.set(x, y);
                display_draw_arrow(&p, dx, dy, 20, dc, filled);
            }
        }
    }
}

void Graphics::display_draw_spike(LayoutCoord *p, navit_float dx, navit_float dy, navit_float width, struct display_context *dc)
{
    QVector<LayoutCoord *> pnt;
    navit_float l = navit_sqrt(dx * dx + dy * dy);
    pnt.append(new LayoutCoord(p));
    pnt.append(new LayoutCoord((-dy / l) * width, (dx / l) * width));
    draw_lines(dc->gc, pnt);
    qDeleteAll(pnt);
}

/**
 * @brief draw spikes along a multi polygon line
 *
 * This function draws spikes along a multi polygon line, and scales the
 * spikes according to current view settings by interpolating sizes at
 * given spike position,
 *
 * @param gra current graphics instance handle
 * @param dc current drawing context
 * @param pnt array of points for this polyline
 * @param count number of points in pnt
 * @param width array of integers giving the expected line width at the corresponding point
 * @param distance giving the distance between spikes
 */
void Graphics::display_draw_spikes(struct display_context *dc, QVector<LayoutCoord *> &pnt, QVector<int> &widths, int distance)
{
    navit_float dx, dy, dw, l;
    int i;
    LayoutCoord p;
    int x, y;
    int w;
    for (i = 0; i < pnt.size() - 1; i++)
    {
        /* get the X and Y size */
        dx = pnt[i + 1]->getX() - pnt[i]->getX();
        dy = pnt[i + 1]->getY() - pnt[i]->getY();
        dw = widths[i + 1] - widths[i];
        /* calculate the length of the way segment */
        l = navit_sqrt(dx * dx + dy * dy);
        if (l != 0)
        {
            /* length is not zero */
            if (l > widths[i])
            {
                /* length is bigger than the length of one spike */
                int a;
                int spike_count = l / distance;
                /* calculate the vector per spike */
                dx = dx / spike_count;
                dy = dy / spike_count;
                dw = dw / spike_count;
                for (a = 0; a < spike_count; a++)
                {
                    x += pnt[i]->getX() + dx * a;
                    y += pnt[i]->getY() + dy * a;
                    p.set(x, y);
                    w = widths[i];
                    w += dw * a;
                    display_draw_spike(&p, dx, dy, w, dc);
                }
            }
        }
    }
}

static int intersection(LayoutCoord *a1, int adx, int ady, LayoutCoord *b1, int bdx, int bdy, LayoutCoord *res)
{
    int n, a, b;
    n = bdy * adx - bdx * ady;
    a = bdx * (a1->getY() - b1->getY()) - bdy * (a1->getX() - b1->getX());
    b = adx * (a1->getY() - b1->getY()) - ady * (a1->getX() - b1->getX());
    dbg(lvl_debug, "a %d b %d n %d", a, b, n);
    if (n < 0)
    {
        n = -n;
        a = -a;
        b = -b;
    }
    if (n == 0)
        return 0;
    int x = a1->getX() + a * adx / n;
    int y = a1->getY() + a * ady / n;
    res->set(x, y);
    return 1;
}

struct circle
{
    short x, y, fowler;
} circle64[] = {
    {0, 128, 0},
    {13, 127, 13},
    {25, 126, 25},
    {37, 122, 38},
    {49, 118, 53},
    {60, 113, 67},
    {71, 106, 85},
    {81, 99, 104},
    {91, 91, 128},
    {99, 81, 152},
    {106, 71, 171},
    {113, 60, 189},
    {118, 49, 203},
    {122, 37, 218},
    {126, 25, 231},
    {127, 13, 243},
    {128, 0, 256},
    {127, -13, 269},
    {126, -25, 281},
    {122, -37, 294},
    {118, -49, 309},
    {113, -60, 323},
    {106, -71, 341},
    {99, -81, 360},
    {91, -91, 384},
    {81, -99, 408},
    {71, -106, 427},
    {60, -113, 445},
    {49, -118, 459},
    {37, -122, 474},
    {25, -126, 487},
    {13, -127, 499},
    {0, -128, 512},
    {-13, -127, 525},
    {-25, -126, 537},
    {-37, -122, 550},
    {-49, -118, 565},
    {-60, -113, 579},
    {-71, -106, 597},
    {-81, -99, 616},
    {-91, -91, 640},
    {-99, -81, 664},
    {-106, -71, 683},
    {-113, -60, 701},
    {-118, -49, 715},
    {-122, -37, 730},
    {-126, -25, 743},
    {-127, -13, 755},
    {-128, 0, 768},
    {-127, 13, 781},
    {-126, 25, 793},
    {-122, 37, 806},
    {-118, 49, 821},
    {-113, 60, 835},
    {-106, 71, 853},
    {-99, 81, 872},
    {-91, 91, 896},
    {-81, 99, 920},
    {-71, 106, 939},
    {-60, 113, 957},
    {-49, 118, 971},
    {-37, 122, 986},
    {-25, 126, 999},
    {-13, 127, 1011},
};

/**
 * @brief Create a set of points on a circle or on a circular arc
 *
 * @param center Center point of the circle
 * @param diameter Diameter of the circle
 * @param scale Unused
 * @param start Position of the first point on the circle (in 1/1024th of the circle), -1 being the bottom of the circle, 511 being the top of the circle
 * @param len Length of the arc on the circle, relative to start (in 1/1024th of the circle), 514 is half a circle, 1028 is a full circle (or 1027 if first and last points are connected with a line)
 * @param[out] res Returned an array of points that will form the resulting circle
 * @param[out] pos Index of the last point filled inside array @p res
 * @param dir Direction of the circle (valid values are 1 (counter-clockwise) or -1 (clockwise), other values may lead to unknown result)
 */
// static void circle_to_points(const struct point *center, int diameter, int scale, int start, int len, QVector<LayoutCoord *> &res,
//                              int *pos, int dir)
// {
//     struct circle *c;
//     int count = 64;
//     int end = start + len;
//     int i, step;
//     c = circle64;
//     if (diameter > 128)
//         step = 1;
//     else if (diameter > 64)
//         step = 2;
//     else if (diameter > 16)
//         step = 4;
//     else if (diameter > 4)
//         step = 8;
//     else
//         step = 16;
//     if (len > 0)
//     {
//         while (start < 0)
//         {
//             start += 1024;
//             end += 1024;
//         }
//         while (end > 0)
//         {
//             i = 0;
//             while (i < count && c[i].fowler <= start)
//                 i += step;
//             while (i < count && c[i].fowler < end)
//             {
//                 if (1 < *pos || 0 < dir)
//                 {
//                     res[*pos]->set(center->x + ((c[i].x * diameter + 128) >> 8), center->y + ((c[i].y * diameter + 128) >> 8));
//                     (*pos) += dir;
//                 }
//                 i += step;
//             }
//             end -= 1024;
//             start -= 1024;
//         }
//     }
//     else
//     {
//         while (start > 1024)
//         {
//             start -= 1024;
//             end -= 1024;
//         }
//         while (end < 1024)
//         {
//             i = count - 1;
//             while (i >= 0 && c[i].fowler >= start)
//                 i -= step;
//             while (i >= 0 && c[i].fowler > end)
//             {
//                 if (1 < *pos || 0 < dir)
//                 {
//                     res[*pos]->set(center->x + ((c[i].x * diameter + 128) >> 8), center->y + ((c[i].y * diameter + 128) >> 8));
//                     (*pos) += dir;
//                 }
//                 i -= step;
//             }
//             start += 1024;
//             end += 1024;
//         }
//     }
// }

static int fowler(int dy, int dx)
{
    int adx, ady; /* Absolute Values of Dx and Dy */
    int code;     /* Angular Region Classification Code */

    adx = (dx < 0) ? -dx : dx; /* Compute the absolute values. */
    ady = (dy < 0) ? -dy : dy;

    code = (adx < ady) ? 1 : 0;
    if (dx < 0)
        code += 2;
    if (dy < 0)
        code += 4;

    switch (code)
    {
    case 0:
        return (dx == 0) ? 0 : 128 * ady / adx; /* [  0, 45] */
    case 1:
        return (256 - (128 * adx / ady)); /* ( 45, 90] */
    case 3:
        return (256 + (128 * adx / ady)); /* ( 90,135) */
    case 2:
        return (512 - (128 * ady / adx)); /* [135,180] */
    case 6:
        return (512 + (128 * ady / adx)); /* (180,225] */
    case 7:
        return (768 - (128 * adx / ady)); /* (225,270) */
    case 5:
        return (768 + (128 * adx / ady)); /* [270,315) */
    case 4:
        return (1024 - (128 * ady / adx)); /* [315,360) */
    }
    return 0;
}

struct draw_polyline_shape
{
    int wi;
    int step;
    int fow;
    int dx, dy;
    int dxw, dyw;
    int l, lscale;
};
struct draw_polyline_context
{
    int prec;
    int positive_pos, negative_pos;
    QVector<LayoutCoord *> res;
    struct draw_polyline_shape shape;
    struct draw_polyline_shape prev_shape;
};

static void draw_shape_update(struct draw_polyline_shape *shape)
{
    shape->dxw = -(shape->dx * shape->wi * shape->lscale) / shape->l;
    shape->dyw = (shape->dy * shape->wi * shape->lscale) / shape->l;
}

static void draw_shape(struct draw_polyline_context *ctx, LayoutCoord *pnt, LayoutCoord *next_pnt, int wi)
{
    int dxs, dys, lscales;
    int lscale = 16;
    int l;
    struct draw_polyline_shape *shape = &ctx->shape;
    struct draw_polyline_shape *prev = &ctx->prev_shape;

    *prev = *shape;
    if (prev->wi != wi && prev->l)
    {
        prev->wi = wi;
        draw_shape_update(prev);
    }
    shape->wi = wi;
    shape->dx = (next_pnt->getX() - pnt->getX());
    shape->dy = (next_pnt->getY() - pnt->getY());
    if (wi > 16)
        shape->step = 4;
    else if (wi > 8)
        shape->step = 8;
    else
        shape->step = 16;
    dxs = shape->dx * shape->dx;
    dys = shape->dy * shape->dy;
    lscales = lscale * lscale;
    if (dxs + dys > lscales)
        l = uint_sqrt(dxs + dys) * lscale;
    else
        l = uint_sqrt((dxs + dys) * lscales);

    shape->fow = fowler(-shape->dy, shape->dx);
    dbg(lvl_debug, "fow=%d", shape->fow);
    if (!l)
        l = 1;
    if (wi * lscale > 10000)
        lscale = 10000 / wi;
    dbg_assert(wi * lscale <= 10000);
    shape->l = l;
    shape->lscale = lscale;
    shape->wi = wi;
    draw_shape_update(shape);
}

static LayoutCoord *draw_point(struct draw_polyline_shape *shape, LayoutCoord *src, LayoutCoord *dst, int pos)
{
    int x, y;
    if (pos)
    {
        x = (src->getX() * 2 - shape->dyw) / 2;
        y = (src->getY() * 2 - shape->dxw) / 2;
    }
    else
    {
        x = (src->getX() * 2 + shape->dyw) / 2;
        y = (src->getY() * 2 + shape->dxw) / 2;
    }
    dst->set(x, y);
    return dst;
}

static void draw_begin(struct draw_polyline_context *ctx, LayoutCoord *p)
{
    struct draw_polyline_shape *shape = &ctx->shape;
    int i, x, y;
    for (i = 0; i <= 32; i += shape->step)
    {
        x = (p->getX() * 256 + (shape->dyw * circle64[i].y) + (shape->dxw * circle64[i].x)) / 256;
        y = (p->getY() * 256 + (shape->dxw * circle64[i].y) - (shape->dyw * circle64[i].x)) / 256;
        ctx->res.append(new LayoutCoord(x, y));
    }
}

static int draw_middle(struct draw_polyline_context *ctx, LayoutCoord *p)
{
    int delta = ctx->prev_shape.fow - ctx->shape.fow;
    if (delta > 512)
        delta -= 1024;
    if (delta < -512)
        delta += 1024;
    if (delta < 16 && delta > -16)
    {
        ctx->res.prepend(draw_point(&ctx->shape, p, new LayoutCoord(), 0));
        ctx->res.append(draw_point(&ctx->shape, p, new LayoutCoord(), 1));
        return 1;
    }
    dbg(lvl_debug, "delta %d", delta);
    if (delta > 0)
    {
        LayoutCoord pos, poso;
        draw_point(&ctx->shape, p, &pos, 1);
        draw_point(&ctx->prev_shape, p, &poso, 1);
        if (delta >= 256)
            return 0;
        if (intersection(&pos, ctx->shape.dx, ctx->shape.dy, &poso, ctx->prev_shape.dx, ctx->prev_shape.dy,
                         ctx->res.last()))
        {
            delete ctx->res.takeLast();
            ctx->res.prepend(draw_point(&ctx->prev_shape, p, new LayoutCoord(), 0));
            ctx->res.prepend(draw_point(&ctx->shape, p, new LayoutCoord(), 0));
            return 1;
        }
    }
    else
    {
        LayoutCoord neg, nego;
        draw_point(&ctx->shape, p, &neg, 0);
        draw_point(&ctx->prev_shape, p, &nego, 0);
        if (delta <= -256)
            return 0;
        if (intersection(&neg, ctx->shape.dx, ctx->shape.dy, &nego, ctx->prev_shape.dx, ctx->prev_shape.dy,
                         ctx->res.first()))
        {
            delete ctx->res.takeFirst();
            ctx->res.append(draw_point(&ctx->prev_shape, p, new LayoutCoord(), 1));
            ctx->res.append(draw_point(&ctx->shape, p, new LayoutCoord(), 1));
            return 1;
        }
    }
    return 0;
}

static void draw_end(struct draw_polyline_context *ctx, LayoutCoord *p)
{
    int i, x, y;
    struct draw_polyline_shape *shape = &ctx->prev_shape;
    for (i = 0; i <= 32; i += shape->step)
    {
        x = (p->getX() * 256 + (shape->dyw * circle64[i].y) - (shape->dxw * circle64[i].x)) / 256;
        y = (p->getY() * 256 + (shape->dxw * circle64[i].y) + (shape->dyw * circle64[i].x)) / 256;
        ctx->res.prepend(new LayoutCoord(x, y));
    }
}

static void draw_init_ctx(struct draw_polyline_context *ctx, int maxpoints)
{
    ctx->prec = 1;
    ctx->positive_pos = maxpoints / 2;     // 100
    ctx->negative_pos = maxpoints / 2 - 1; // 99
}

void Graphics::draw_polyline_as_polygon(GraphicsContext *gc, QVector<LayoutCoord *> &pnt, QVector<int> &widths)
{
    int maxpoints = 200;
    struct draw_polyline_context ctx;
    int i = 0;
    int max_circle_points = 20;
    int count = pnt.size();
    if (pnt.size() < 2)
        return;
    ctx.shape.l = 0;
    ctx.shape.wi = 0;
    i = 0;
    draw_init_ctx(&ctx, maxpoints);
    draw_shape(&ctx, pnt[0], pnt[1], widths[0]);
    draw_begin(&ctx, pnt[0]);
    for (i = 1; i < count - 1; i++)
    {
        draw_shape(&ctx, pnt[i], pnt[i + 1], widths[i]);
        if (ctx.res.size() >= maxpoints - max_circle_points || !draw_middle(&ctx, pnt[i]))
        {
            draw_end(&ctx, pnt[i]);
            ctx.res.prepend(ctx.res.last());
            draw_polygon(gc, ctx.res);
            draw_init_ctx(&ctx, maxpoints);
            draw_begin(&ctx, pnt[i]);
        }
    }
    draw_shape(&ctx, pnt[count - 2], pnt.last(), widths.last());
    ctx.prev_shape = ctx.shape;
    draw_end(&ctx, pnt.last());
    ctx.res.prepend(ctx.res.last());
    draw_polygon(gc, ctx.res);

    qDeleteAll(ctx.res);
}

struct wpoint
{
    int x, y, w;
};

enum relative_pos
{
    INSIDE = 0,
    LEFT_OF = 1,
    RIGHT_OF = 2,
    ABOVE = 4,
    BELOW = 8
};

static int relative_pos(struct wpoint *p, struct point_rect *r)
{
    int relative_pos = INSIDE;
    if (p->x < r->lu.x)
        relative_pos = LEFT_OF;
    else if (p->x > r->rl.x)
        relative_pos = RIGHT_OF;
    if (p->y < r->lu.y)
        relative_pos |= ABOVE;
    else if (p->y > r->rl.y)
        relative_pos |= BELOW;
    return relative_pos;
}

static void clip_line_endoint_to_rect_edge(struct wpoint *p, int rel_pos, int dx, int dy, int dw, struct point_rect *clip_rect)
{
    // We must cast to float to avoid integer
    // overflow (i.e. undefined behaviour) at high
    // zoom levels.
    if (rel_pos & LEFT_OF)
    {
        p->y += (((float)clip_rect->lu.x) - p->x) * dy / dx;
        p->w += (((float)clip_rect->lu.x) - p->x) * dw / dx;
        p->x = clip_rect->lu.x;
    }
    else if (rel_pos & RIGHT_OF)
    {
        p->y += (((float)clip_rect->rl.x) - p->x) * dy / dx;
        p->w += (((float)clip_rect->rl.x) - p->x) * dw / dx;
        p->x = clip_rect->rl.x;
    }
    else if (rel_pos & ABOVE)
    {
        p->x += (((float)clip_rect->lu.y) - p->y) * dx / dy;
        p->w += (((float)clip_rect->lu.y) - p->y) * dw / dy;
        p->y = clip_rect->lu.y;
    }
    else if (rel_pos & BELOW)
    {
        p->x += (((float)clip_rect->rl.y) - p->y) * dx / dy;
        p->w += (((float)clip_rect->rl.y) - p->y) * dw / dy;
        p->y = clip_rect->rl.y;
    }
}

enum clip_result
{
    CLIPRES_INVISIBLE = 0,
    CLIPRES_VISIBLE = 1,
    CLIPRES_START_CLIPPED = 2,
    CLIPRES_END_CLIPPED = 4,
};

static int clip_line(struct wpoint *p1, struct wpoint *p2, struct point_rect *clip_rect)
{
    int rel_pos1, rel_pos2;
    int ret = CLIPRES_VISIBLE;
    int dx, dy, dw;
    rel_pos1 = relative_pos(p1, clip_rect);
    if (rel_pos1 != INSIDE)
        ret |= CLIPRES_START_CLIPPED;
    rel_pos2 = relative_pos(p2, clip_rect);
    if (rel_pos2 != INSIDE)
        ret |= CLIPRES_END_CLIPPED;
    dx = p2->x - p1->x;
    dy = p2->y - p1->y;
    dw = p2->w - p1->w;
    while ((rel_pos1 != INSIDE) || (rel_pos2 != INSIDE))
    {
        if (rel_pos1 & rel_pos2)
            return CLIPRES_INVISIBLE;
        clip_line_endoint_to_rect_edge(p1, rel_pos1, dx, dy, dw, clip_rect);
        rel_pos1 = relative_pos(p1, clip_rect);
        if (rel_pos1 & rel_pos2)
            return CLIPRES_INVISIBLE;
        clip_line_endoint_to_rect_edge(p2, rel_pos2, dx, dy, dw, clip_rect);
        rel_pos2 = relative_pos(p2, clip_rect);
    }
    return ret;
}

/**
 * @brief Draw polyline on the display
 *
 * Polylines are a serie of lines connected to each other.
 *
 * @param gra The graphics instance on which to draw
 * @param gc The graphics context
 * @param[in] pin An array of points forming the polygon
 * @param count_in The number of elements inside @p pin
 * @param[in] width An array of width matching the line starting from the corresponding @p pa (if all equal, all lines will have the same width)
 * @param poly A boolean indicating whether the polyline should be closed to form a polygon (only the contour of this polygon will be drawn)
 */
void Graphics::draw_polyline_clipped(GraphicsContext *gc, QVector<LayoutCoord *> &pa, QVector<int> &widths, int poly)
{
    QVector<LayoutCoord *> points_to_draw;
    QVector<int> _widths;
    struct wpoint segment_start, segment_end;

    int clip_result;
    int r_width, r_height;
    struct point_rect r = m_r;

    r_width = r.rl.x - r.lu.x;
    r_height = r.rl.y - r.lu.y;

    // Expand clipping rect by 1/3 so wide, slanted lines do not
    // partially end before screen border.
    // Ideally we would expand by the line width here, but in 3D
    // mode the width is variable and needs clipping itself, so that
    // would get complicated. Anyway, 1/3 of screen size should be
    // enough...
    r.lu.x -= r_width / 3;
    r.lu.y -= r_height / 3;
    r.rl.x += r_width / 3;
    r.rl.y += r_height / 3;
    // Iterate over line segments, push them into points_to_draw
    // until we reach a completely invisible segment...
    for (int i = 0; i < pa.size(); i++)
    {
        if (i)
        {
            segment_start.x = pa[i - 1]->getX();
            segment_start.y = pa[i - 1]->getX();
            segment_start.w = widths[(i - 1)];
            segment_end.x = pa[i]->getX();
            segment_end.y = pa[i]->getY();
            segment_end.w = widths[i];
            dbg(lvl_debug, "Segment: [%d, %d] - [%d, %d]...", segment_start.x, segment_start.y, segment_end.x, segment_end.y);
            clip_result = clip_line(&segment_start, &segment_end, &r);
            if (clip_result != CLIPRES_INVISIBLE)
            {
                dbg(lvl_debug, "....clipped to [%d, %d] - [%d, %d]", segment_start.x, segment_start.y, segment_end.x, segment_end.y);
                if ((i == 1) || (clip_result & CLIPRES_START_CLIPPED))
                {
                    points_to_draw.append(new LayoutCoord(segment_start.x, segment_start.y));
                    _widths.append(segment_start.w);
                }
                points_to_draw.append(new LayoutCoord(segment_end.x, segment_end.y));
                _widths.append(segment_end.w);
            }
            if ((i == pa.size() - 1) || (clip_result & CLIPRES_END_CLIPPED))
            {
                // ... then draw the resulting polyline
                if (points_to_draw.size() > 1)
                {
                    if (poly)
                    {
                        draw_polyline_as_polygon(gc, points_to_draw, _widths);
                    }
                    else
                    {
                        draw_lines(gc, points_to_draw);
                    }
                    qDeleteAll(points_to_draw);
                    points_to_draw.clear();
                    _widths.clear();
                }
            }
        }
    }

    qDeleteAll(points_to_draw);
}

static int is_inside(LayoutCoord *p, struct point_rect *r, int edge)
{
    switch (edge)
    {
    case 0:
        return p->getX() >= r->lu.x;
    case 1:
        return p->getX() <= r->rl.x;
    case 2:
        return p->getY() >= r->lu.y;
    case 3:
        return p->getY() <= r->rl.y;
    default:
        return 0;
    }
}

static void poly_intersection(LayoutCoord *p1, LayoutCoord *p2, struct point_rect *r, int edge, LayoutCoord *ret)
{
    int dx = p2->getX() - p1->getX();
    int dy = p2->getY() - p1->getY();
    int x;
    int y;
    switch (edge)
    {
    case 0:
        y = p1->getY() + ((float)r->lu.x - p1->getX()) * dy / dx;
        x = r->lu.x;
        break;
    case 1:
        y = p1->getY() + ((float)r->rl.x - p1->getX()) * dy / dx;
        x = r->rl.x;
        break;
    case 2:
        x = p1->getX() + ((float)r->lu.y - p1->getY()) * dx / dy;
        y = r->lu.y;
        break;
    case 3:
        x = p1->getX() + ((float)r->rl.y - p1->getY()) * dx / dy;
        y = r->rl.y;
        break;
    }
    ret->set(x, y);
}

/**
 * @brief clip a polygon inside a rectangle
 *
 * This function clippes a given polygon inside a rectangle. It writes the result into provided buffer.
 *
 * @param[in] r rectangle to clip into
 * @param[in] pin point array of input polygon
 * @param[in] count_in number of points in pin
 * @param[out] out preallocated buffer of at least count_in *8 +1 points size
 * @param[out] count_out size of out number of points, number of points used in out at return
 */
void Graphics::clip_polygon(struct point_rect *r, QVector<LayoutCoord *> &in, QVector<LayoutCoord *> &out)
{
    /* get a temp buffer to store points after one direction clipping.
     * since we are clipping 4 directions, result is always in out at the end*/
    int edge;
    int count;
    int count_out;

    QVector<LayoutCoord *> _temp;
    QVector<LayoutCoord *> *temp;
    QVector<LayoutCoord *> *pout;
    QVector<LayoutCoord *> *pin;

    /* sanity check */
    if (r == NULL)
    {
        return;
    }

    /* prepare buffers. We have two buffers that we flip over.
     * 1. the output buffer
     * 2. temp
     */
    temp = &_temp;
    /* use temp as first buffer. So we get the final result in out*/
    pout = temp;
    /* start with input polygon */
    pin = &in;
    /* start with number of points of source polygon*/
    count = in.size();

    /* clip all four directions of a rectangle */
    for (edge = 0; edge < 4; edge++)
    {
        int i;
        /* p is first element in current buffer */
        LayoutCoord *p = pin->first();
        /* s is lasst element in current buffer */
        LayoutCoord *s = pin->last();
        /* nothing written yet */
        count_out = 0;

        /* iterate all points in current buffer */
        for (i = 0; i < count; i++)
        {
            if (is_inside(p, r, edge))
            {
                if (!is_inside(s, r, edge))
                {
                    LayoutCoord *pi = new LayoutCoord();
                    /* current segment crosses border from outside to inside. Add crossing point with border first */
                    poly_intersection(s, p, r, edge, pi);
                    pout->append(pi);
                }
                /* add point if inside */
                pout->append(p);
            }
            else
            {
                if (is_inside(s, r, edge))
                {
                    LayoutCoord *pi = new LayoutCoord();
                    /*current segment crosses border from inside to outside. Add crossing point with border */
                    poly_intersection(p, s, r, edge, pi);
                    pout->append(pi);
                }
                /* skip point if outside */
            }
            /* move one coordinate forward */
            s = p;
            p++;
        }
        /* use result of last clipping for next */
        count = count_out;

        /* switch buffer */
        if (pout == temp)
        {
            pout = &out;
            pin = temp;
        }
        else
        {
            pin = &out;
            pout = temp;
        }
    }
}

/**
 * @brief Draw a plain polygon on the display
 *
 * @param gra The graphics instance on which to draw
 * @param gc The graphics context
 * @param[in] pin An array of points forming the polygon
 * @param count_in The number of elements inside @p pin
 */
void Graphics::draw_polygon_clipped(GraphicsContext *gc, QVector<LayoutCoord *> &pin)
{
    struct point_rect r = m_r;
    QVector<LayoutCoord *> clipped;

    clip_polygon(&r, pin, clipped);
    draw_polygon(gc, clipped);

    qDeleteAll(clipped);
}

/**
 * @brief Draw a plain polygon with holes on the display
 *
 * @param gra The graphics instance on which to draw
 * @param gc The graphics context
 * @param[in] pin An array of points forming the polygon
 * @param count_in The number of elements inside @p pin
 * @param hole_count The number of hole polygons to cut out
 * @param pcount array of [hole_count] integers giving the number of
 *        points per hole
 * @param holes array of point arrays for the hole polygons
 */
void Graphics::draw_polygon_with_holes_clipped(GraphicsContext *gc, QVector<LayoutCoord *> &pin, DisplayitemPolyHoles &holes)
{
    QVector<LayoutCoord *> clipped;
    QVector<QVector<LayoutCoord *>> found_holes;

    /* clip outer polygon */
    clip_polygon(&m_r, pin, clipped);
    /* clip the holes */
    for (QVector<LayoutCoord *> hole : holes)
    {
        QVector<LayoutCoord *> buffer;
        clip_polygon(&m_r, hole, buffer);
        if (buffer.size() > 0)
        {
            found_holes.append(buffer);
        }
    }
    /* call drawing function */
    draw_polygon_with_holes(gc, clipped, found_holes);

    /* if we had to allocate buffer, free it */

    qDeleteAll(clipped);
}

void Graphics::display_context_free(struct display_context *dc)
{
    // if (dc->gc)
    // delete dc->gc;
    // if (dc->gc_background)
    // delete dc->gc_background;
    if (dc->img)
        image_free(dc->img);
    dc->gc = NULL;
    dc->gc_background = NULL;
    dc->img = NULL;
}

struct graphics_font *Graphics::get_font(int size)
{
    if (size > 64)
        size = 64;
    if (size >= m_font_len)
    {
        m_font = g_renew(struct graphics_font *, m_font, size + 1);
        while (m_font_len <= size)
            m_font[m_font_len++] = NULL;
    }
    if (!m_font[size])
        m_font[size] = font_new(size * m_font_size, 0);
    return m_font[size];
}

void Graphics::draw_text_std(int text_size, QString &text, LayoutCoord *p)
{
    struct graphics_font *font = get_font(text_size);
    struct point bbox[4];
    int i;

    get_text_bbox(font, text, 0x10000, 0, bbox, 0);
    for (i = 0; i < 4; i++)
    {
        bbox[i].x += p->getX();
        bbox[i].y += p->getY();
    }
    LayoutCoord c(&bbox[1]);
    draw_rectangle(&m_gcForeground, &c, bbox[2].x - bbox[0].x, bbox[0].y - bbox[1].y + 5);
    draw_text(&m_gcMiddground, &m_gcForeground, font, text, p, 0x10000, 0);
}

QString Graphics::icon_path(QString icon)
{
    static char *navit_sharedir;
    QString ret("%s/icons/%s");
    struct file_wordexp *wordexp = NULL;
    // dbg(lvl_debug, "enter %s", icon);
    if (icon.contains("$"))
    {
        wordexp = file_wordexp_new(icon.toLocal8Bit().data());
        if (file_wordexp_get_count(wordexp))
            icon = file_wordexp_get_array(wordexp)[0];
    }
    if (icon.contains("/"))
        ret = icon;
    else
    {
#ifdef HAVE_API_ANDROID
        ret = g_strdup_printf("res/drawable/%s", icon);
#else
        if (!navit_sharedir)
            navit_sharedir = getenv("NAVIT_SHAREDIR");
        ret = ret.arg(navit_sharedir).arg(icon);
#endif
    }
    if (wordexp)
        file_wordexp_destroy(wordexp);
    return ret;
}

QString Graphics::texture_path(QString &texture)
{
    static char *navit_sharedir;
    QString ret;
    struct file_wordexp *wordexp = NULL;
    // dbg(lvl_debug, "enter %s", texture);
    if (texture.contains("$"))
    {
        wordexp = file_wordexp_new(texture.toLocal8Bit().data());
        if (file_wordexp_get_count(wordexp))
            texture = file_wordexp_get_array(wordexp)[0];
    }
    if (texture.contains("/"))
        ret = texture;
    else
    {
        if (!navit_sharedir)
            navit_sharedir = getenv("NAVIT_SHAREDIR");
        ret = QString("%s/textures/%s").arg(navit_sharedir, texture);
    }
    if (wordexp)
        file_wordexp_destroy(wordexp);
    return ret;
}

int Graphics::limit_count(QVector<LayoutCoord *> &coords, int count)
{
    int i;
    for (i = 1; i < count; i++)
    {
        if (coords[i]->getX() == coords[0]->getX() && coords[i]->getY() == coords[0]->getY())
            return i + 1;
    }
    return count;
}

/**
 * @brief Draw a multi-line text next to a specified point @p pref
 *
 * @param gra The graphics instance on which to draw
 * @param fg The graphics color to use to draw the text
 * @param bg The graphics background color to use to draw the text
 * @param font The font to use to draw the text
 * @param pref The position to draw the text (draw at the right and vertically aligned relatively to this point)
 * @param label The text to draw (may contain '\n' for multiline text, if so lines will be stacked vertically)
 * @param line_spacing The delta between each line (set its value at to least the font text size, to be readable)
 */
void Graphics::multiline_label_draw(GraphicsContext *fg, GraphicsContext *bg, struct graphics_font *font, struct point pref, QString &label, int line_spacing)
{
    QStringList label_lines;
    int max_lines = 10;

    for (QString nl : label.split("\n"))
    {
        for (QString nl2 : nl.split("\0"))
        {
            label_lines.append(nl2);
        }
    }
    if (label_lines.size() > max_lines)
    { /* Does label_nblines overflows the number of entries in array label_lines? */
        dbg(lvl_warning, "Too many lines (%d) in label, truncating to %d", label_lines.size(), max_lines);
        label_lines = QStringList(label_lines.begin(), label_lines.begin() + max_lines);
    }
    /* Horizontally, we position the label next to the specified point (on the right handside) */
    /* Vertically, we center the text with respect to specified point */
    LayoutCoord p(pref.x + 1, pref.y - (label_lines.size() * line_spacing) / 2);

    /* Parse all stored lines, and display them */
    for (QString line : label_lines)
    {
        draw_text(fg, bg, font, line, &p, 0x10000, 0);
        p.set(p.getX(), p.getY() + line_spacing);
    }
}

/**
 * @brief coordnate transfor hole coordinates
 *
 * This function transform a whole set of polygon holes. It therefore allocates memory
 * attached to a displayitem_poly_holes structure and call transform
 *
 * @param trans transformation to be used
 * @param pro projection to be used
 * @param in filled holes structure to transform
 * @param out structure to place result in. Remember to deallocate!
 * @param mindist minimal distance between points
 */
void Graphics::displayitem_transform_holes(struct transformation *trans, enum projection pro, DisplayitemPolyHoles &in, DisplayitemPolyHoles &out, int mindist)
{
    for (QVector<LayoutCoord *> hole : in)
    {
        QVector<LayoutCoord *> transformedHole;
        transform_point_buf(trans, pro, hole, transformedHole, mindist, 0, NULL);

        out.append(transformedHole);
    }
}

/**
 * @brief free hole structure allocated by displayitem_transform_holes
 *
 * @param holes structure to deallocate
 */
void Graphics::displayitem_free_holes(DisplayitemPolyHoles &holes)
{
    for (QVector<LayoutCoord *> hole : holes)
    {
        qDeleteAll(hole);
        hole.clear();
    }
    holes.clear();
}

void Graphics::displayitem_draw_polygon(struct display_context *dc, QVector<LayoutCoord *> &pa, DisplayitemPolyHoles &holes)
{

    LayoutPolygon *contextPolygon = static_cast<LayoutPolygon *>(dc->element);
    /* Set texture if any, and supported by graphics */
    if (!contextPolygon->getSrc().isEmpty())
    {
        struct graphics_image *texture;
        QString path = texture_path(contextPolygon->getSrc());
        texture = image_new_scaled_rotated(path, contextPolygon->getW(), contextPolygon->getH(), contextPolygon->getRotation());
        if (texture != NULL)
            dc->gc->set_texture(texture);
    }
    if (holes.size() > 0)
        draw_polygon_with_holes_clipped(dc->gc, pa, holes);
    else
        draw_polygon_clipped(dc->gc, pa);
}

void Graphics::displayitem_draw_polyline(struct display_context *dc, LayoutPolyline *element, QVector<LayoutCoord *> &pa, QVector<int> &widths)
{
    int i;
    dc->gc->set_linewidth(1);
    if (element->getWidth() > 0 && element->getDash().size() > 0)
        dc->gc->set_dashes(element->getWidth(), element->getOffset(), element->getDash());
    for (i = 0; i < pa.size(); i++)
    {
        if (widths[i] < 2)
            widths[i] = 2;
    }
    draw_polyline_clipped(dc->gc, pa, widths, element->getWidth() > 1);
}

void Graphics::displayitem_draw_circle(struct displayitem *di, struct display_context *dc, LayoutCircle *element, LayoutCoord *pa)
{
    if (pa)
    {
        if (element->getWidth() > 1)
            dc->gc->set_linewidth(element->getWidth());
        draw_circle(dc->gc, pa, element->getRadius());
        if (element->getTextSize())
        {
            struct graphics_font *font = get_font(element->getTextSize());
            GraphicsContext *gc_background = dc->gc_background;
            if (!gc_background && element->getBackgroundColor().isValid())
            {
                gc_background = new GraphicsContext(m_contextInterface, this);
                gc_background->set_foreground(element->getBackgroundColor());
                dc->gc_background = gc_background;
            }
            if (font)
            {
                struct point p;
                /* Set p to the center of the circle */
                p.x = pa->getX() + (element->getRadius() / 2);
                p.y = pa->getY() + (element->getRadius() / 2);
                multiline_label_draw(dc->gc, gc_background, font, p, di->label, element->getTextSize() + 1);
            }
            else
                dbg(lvl_error, "Failed to get font with size %d", element->getTextSize());
        }
    }
}

void Graphics::displayitem_draw_text(struct displayitem *di, struct display_context *dc, LayoutText *element, QVector<LayoutCoord *> &coords, DisplayitemPolyHoles &holes)
{
    if (coords.size() > 0)
    {
        struct graphics_font *font = get_font(element->getTextSize());
        GraphicsContext *gc_background = dc->gc_background;
        if (!gc_background && element->getBackgroundColor().isValid())
        {
            gc_background = new GraphicsContext(m_contextInterface, this);
            gc_background->set_foreground(element->getBackgroundColor());
            dc->gc_background = gc_background;
        }
        if (font)
        {
            label_line(dc->gc, gc_background, font, coords, di->label);

            for (QVector<LayoutCoord *> hole : holes)
            {
                label_line(dc->gc, gc_background, font, hole, di->label);
            }
        }
        else
            dbg(lvl_error, "Failed to get font with size %d", element->getTextSize());
    }
}

void Graphics::displayitem_draw_icon(struct displayitem *di, struct display_context *dc, LayoutIcon *element, LayoutCoord *pa, Layout *layout)
{
    if (pa)
    {
        struct graphics_image *img = dc->img;
        if (!img || item_is_custom_poi(di->item))
        {
            int icon_width = element->getW();
            int icon_height = element->getH();
            QString path;
            /* get the standard icon size out of the layout if unset */
            if (layout)
            {
                if (icon_height == -1)
                    icon_height = layout->getIconH();
                if (icon_width == -1)
                    icon_width = layout->getIconW();
            }
            if (item_is_custom_poi(di->item))
            {
                QString icon;
                QString src;

                if (img)
                    image_free(img);
                src = element->getSrc();
                if (src.isEmpty())
                    src = QString("%s");

                // Second element should be the src
                // TODO: add struct for label...
                assert(di->label.size() > 0);
                icon = src.arg(di->label[1]);
                path = icon_path(icon);
            }
            else
                path = icon_path(element->getSrc());
            img = image_new_scaled_rotated(path, icon_width, icon_height, element->getRotation());
            if (img)
                dc->img = img;
            else
                dbg(lvl_debug, "failed to load icon '%s'", path.toLocal8Bit().data());
        }
        if (img)
        {
            struct point p;
            if (element->getX() != -1 || element->getY() != -1)
            {
                p.x = pa->getX() - element->getX();
                p.y = pa->getY() - element->getY();
            }
            else
            {
                p.x = pa->getX() - img->hot.x;
                p.y = pa->getY() - img->hot.y;
            }
            draw_image(&m_gcBackground, &p, img);
        }
    }
}

void Graphics::displayitem_draw_image(struct displayitem *di, struct display_context *dc, LayoutCoord *pa)
{
    // dbg(lvl_debug, "image: '%s'", di->label);
    struct graphics_image *img = dc->img;
    // The first one should be the image path???
    img = image_new_scaled_rotated(di->label, IMAGE_W_H_UNSET, IMAGE_W_H_UNSET, 0);
    if (img)
        draw_image_warp(&m_gcBackground, pa, img);
}

/**
 * @brief Draw a displayitem element
 *
 * This function will invoke the appropriate draw primitive depending on the type of the element to draw
 *
 * @brief di The displayitem to draw
 * @brief l current layout for getting defaults and underground alpha
 * @brief dc The display_context to use to draw items
 */
void Graphics::displayitem_draw(struct displayitem *di, Layout *layout, struct display_context *dc)
{
    QVector<int> width;

    LayoutItemGraphElement *element = dc->element;
    int draw_underground = 0;

    while (di)
    {
        // int count = di->count;
        int mindist = dc->mindist;
        DisplayitemPolyHoles t_holes;
        QVector<LayoutCoord *> t_coords;

        di->z_order = ++(m_current_z_order);

        /* Skip elements that are to be drawn on oneway streets only
         * if street is not oneway or roundabout */
        if ((element->getOneway()) && ((!(di->flags & AF_ONEWAY)) || (di->flags & AF_ROUNDABOUT)))
        {
            di = di->next;
            continue;
        }

        if (!dc->gc)
        {
            GraphicsContext *gc = new GraphicsContext(m_contextInterface, this);
            dc->gc = gc;
            dc->gc->set_foreground(element->getColor());
        }

        /* If the element id flagged AF_UNDERGROUND, we apply predefined transparenc to it if
         * it's not the text. */
        if ((di->flags & AF_UNDERGROUND) && (dc->element->getType() != LayoutElementType::LayoutElementText))
        {
            if (!draw_underground)
            {
                QColor fg_color = element->getColor();
                fg_color.setAlpha(layout ? layout->getUndergroundAlpha() : UNDERGROUND_ALPHA_);
                dc->gc->set_foreground(fg_color);
                draw_underground = 1;
            }
        }
        else
        {
            if (draw_underground)
            {
                dc->gc->set_foreground(element->getColor());
                draw_underground = 0;
            }
        }
        // if (
        //     item_type_is_area(dc->type) &&
        //     (dc->element->getType() == LayoutElementType::LayoutElementPolyline ||
        //      dc->element->getType() == LayoutElementType::LayoutElementText))
        // limit = 0;

        displayitem_transform_holes(dc->trans, dc->pro, di->holes, t_holes, mindist);

        // if (limit)
        // count = limit_count(di->coords, count);

        // di->coords = QVector<LayoutCoord *>(di->coords.begin(), di->coords.begin() + count);

        if (dc->type == type_poly_water_tiled)
            mindist = 0;

        switch (element->getType())
        {
        case LayoutElementType::LayoutElementPolygon:
        {
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, 0, NULL);
            displayitem_draw_polygon(dc, t_coords, t_holes);
            break;
        }
        case LayoutElementType::LayoutElementPolyline:
        {
            LayoutPolyline *polyline = static_cast<LayoutPolyline *>(element);
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, polyline->getWidth(),
                                &width);
            displayitem_draw_polyline(dc, static_cast<LayoutPolyline *>(element), t_coords, width);
            break;
        }
        case LayoutElementType::LayoutElementCircle:
        {
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, 0);
            displayitem_draw_circle(di, dc, static_cast<LayoutCircle *>(element), t_coords.first());
            break;
        }
        case LayoutElementType::LayoutElementText:
        {
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, 0);
            displayitem_draw_text(di, dc, static_cast<LayoutText *>(element), t_coords, t_holes);
            break;
        }
        case LayoutElementType::LayoutElementIcon:
        {
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, 0);
            displayitem_draw_icon(di, dc, static_cast<LayoutIcon *>(element), t_coords.first(), layout);
            break;
        }
        case LayoutElementType::LayoutElementImage:
        {
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, 0);
            displayitem_draw_image(di, dc, t_coords.first());
            break;
        }
        case LayoutElementType::LayoutElementArrows:
        {
            LayoutArrows *arrows = static_cast<LayoutArrows *>(element);
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, arrows->getWidth(), &width);
            display_draw_arrows(dc, t_coords, width, arrows->getOneway());
            break;
        }
        case LayoutElementType::LayoutElementSpikes:
        {
            LayoutSpikes *spikes = static_cast<LayoutSpikes *>(element);
            transform_point_buf(dc->trans, dc->pro, di->coords, t_coords, mindist, spikes->getWidth(), &width);
            display_draw_spikes(dc, t_coords, width, spikes->getDistance());
            break;
        }
        case LayoutElementType::LayoutElementPoint:
            qWarning() << "Can't draw point";
        }
        /* free space allocated for holes */
        displayitem_free_holes(t_holes);
        qDeleteAll(t_coords);

        di = di->next;
    }
}

void Graphics::draw_itemgra(LayoutItemGraph *itemGraph, struct transformation *t, char *label)
{
    struct display_context dc;
    int max_coord = 32;
    int count = 0;
    char *buffer;
    struct displayitem *di;
    if (max_coord < ALLOCA_COORD_LIMIT)
    {
        buffer = (char *)g_alloca(sizeof(struct displayitem) + max_coord * sizeof(struct coord));
    }
    else
    {
        buffer = (char *)g_malloc(sizeof(struct displayitem) + max_coord * sizeof(struct coord));
    }
    di = (struct displayitem *)buffer;

    di->item.type = type_none;
    di->item.id_hi = 0;
    di->item.id_lo = 0;
    di->item.map = NULL;
    di->z_order = 0;
    di->label = QString(label);
    di->holes.clear();
    di->coords.clear();
    dc.gra = this;
    dc.gc = NULL;
    dc.gc_background = NULL;
    dc.img = NULL;
    dc.pro = projection_screen;
    dc.mindist = 0;
    dc.trans = t;
    dc.type = type_none;
    dc.maxlen = max_coord;
    for (LayoutItemGraphElement *element : itemGraph->getElements())
    {
        QVector<LayoutCoord *> coords = element->getCoords();
        if (coords.size() > 0)
        {
            if (coords.size() > max_coord)
            {
                qWarning() << "maximum number of coords reached: " << coords.size() << ">" << max_coord;
                count = max_coord;
            }
            else
                count = coords.size();
            di->coords = QVector<LayoutCoord *>(coords.begin(), coords.begin() + count);
        }
        else
        {
            LayoutCoord coord;
            di->coords.append(&coord);
        }
        dc.element = element;
        di->next = NULL;
        displayitem_draw(di, NULL, &dc);
        display_context_free(&dc);
    }
    if (max_coord >= ALLOCA_COORD_LIMIT)
    {
        g_free(buffer);
    }
}

/**
 * Get the map item which given displayitem is based on.
 * NOTE: returned structure doesn't contain any attributes or coordinates. type, map, idhi and idlow seem to be the only useable members.
 * @param di pointer to displayitem structure
 * @returns Pointer to struct item
 * @author Martin Schaller (04/2008)
 */
struct item *Graphics::displayitem_get_item(struct displayitem *di)
{
    return &di->item;
}

/**
 * Get the number of this item as it was last displayed on the screen, dependent of current layout. Items with lower numbers
 * are shaded by items with higher ones when they overlap. Zero means item was not displayed at all. If the item is displayed twice, its topmost
 * occurence is used.
 * @param di pointer to displayitem structure
 * @returns z-order of current item.
 */
int Graphics::displayitem_get_z_order(struct displayitem *di)
{
    return di->z_order;
}

int Graphics::displayitem_get_coord_count(struct displayitem *di)
{
    return di->coords.size();
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
QString Graphics::displayitem_get_label(struct displayitem *di)
{
    return di->label;
}

int Graphics::displayitem_get_displayed(struct displayitem *di)
{
    return 1;
}

/**
 * @brief get display resolution in DPI
 * This method returns the native display density in DPI
 * @param gra graphics handle
 * @returns dpi value. May be fraction therefore double.
 */
navit_float Graphics::get_dpi()
{
    return m_graphicsInterface.get_dpi();
}

int Graphics::get_dpi_factor()
{
    return m_dpi_factor;
}
#pragma region GraphicsContext

/**
 * Create a new graphics context.
 * @param gra associated graphics object for the new context
 * @returns new graphics context
 * @author Martin Schaller (04/2008)
 */
GraphicsContext::GraphicsContext(NavitGraphicsContextInterface &contextInterface, Graphics *graphics) : m_contextInterface(contextInterface), m_graphics(graphics)
{
}
GraphicsContext::~GraphicsContext()
{
}

/**
 * Set foreground color.
 * @param gc graphics context to set color for
 * @param c color to set
 * @author Martin Schaller (04/2008)
 */
void GraphicsContext::set_foreground(const QColor &c)
{
    m_contextInterface.set_foreground(c);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void GraphicsContext::set_background(const QColor &c)
{
    m_contextInterface.set_background(c);
}

/**
 * Set textured background to current graphics context.
 * @param gc Graphics context handle
 * @param img Allocated image
 * @returns void
 * @author metalstrolch (04/2020)
 */
void GraphicsContext::set_texture(struct graphics_image *img)
{
    m_contextInterface.set_texture(img);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void GraphicsContext::set_linewidth(int width)
{
    m_contextInterface.set_linewidth(m_graphics->dpi_scale(width));
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void GraphicsContext::set_dashes(int width, int offset, QVector<int> &dashes)
{
    int a;
    unsigned char *scaled_dashes = (unsigned char *)g_alloca(sizeof(unsigned char) * dashes.size());
    for (a = 0; a < dashes.size(); a++)
    {
        scaled_dashes[a] = m_graphics->dpi_scale(dashes[a]);
    }
    m_contextInterface.set_dashes(m_graphics->dpi_scale(width), m_graphics->dpi_scale(offset), scaled_dashes, dashes.size());
}

NavitGraphicsContextInterface &GraphicsContext::get_context_interface()
{
    return m_contextInterface;
}
#pragma endregion
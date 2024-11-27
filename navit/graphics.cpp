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
#include "config.h"
#include "debug.h"
#include "string.h"
#include "draw_info.h"
#include "point.h"
#include "graphics.h"
#include "projection.h"
#include "item.h"
#include "xmlconfig.h"
#include "map.h"
#include "coord.h"
#include "transform.h"
#include "plugin.h"
#include "profile.h"
#include "mapset.h"
#include "layout.h"
#include "route.h"
#include "util.h"
#include "callback.h"
#include "file.h"
#include "event.h"
#include "navit_wrapper.h"

/**
 * @brief maximum amount of coordinates to allocate on stack using g_alloca
 */
#define ALLOCA_COORD_LIMIT 16384

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

static void circle_to_points(const struct point *center, int diameter, int scale, int start, int len, struct point *res,
                             int *pos, int dir);

int Graphics::dpi_scale(int p)
{
    int result;
    result = p * m_dpi_factor;
    return result;
}
struct point Graphics::dpi_scale_point(struct point *p)
{
    struct point result = {-1, -1};
    result.x = dpi_scale(p->x);
    result.y = dpi_scale(p->y);
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
        m_gamma = attr->u.num;
        break;
    case attr_brightness:
        m_brightness = attr->u.num;
        break;
    case attr_contrast:
        m_contrast = attr->u.num;
        break;
    case attr_font_size:
        m_font_size = attr->u.num;
        return 1;
    default:
        return 0;
    }
    m_colormgmt = (m_gamma != 65536 || m_brightness != 0 || m_contrast != 65536);
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
    /* FIXME if m_graphicsInterface->doesn't have a setter, we don't even try the generic attrs - is that what we want? */
    dbg(lvl_debug, "enter");
    ret = m_graphicsInterface->set_attr(attr);
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

std::unique_ptr<GraphicsContext> make_graphic_context(Graphics &graphics)
{
    return std::unique_ptr<GraphicsContext>(nullptr);
}

std::unique_ptr<NavitGraphicsInterface> make_graphics()
{
    return std::unique_ptr<NavitGraphicsInterface>(nullptr);
}

Graphics::Graphics(Graphics *parent, attr **attrs) : m_graphicsInterface(make_graphics()),
                                                                                                m_gcBackground(make_graphic_context(*this)),
                                                                                                m_gcMiddground(make_graphic_context(*this)),
                                                                                                m_gcForeground(make_graphic_context(*this))
{
    struct attr *type_attr, cbl_attr, *real_dpi_attr, *virtual_dpi_attr;
    struct graphics_priv *(*graphicstype_new)(NavitHandle nav, struct graphics_methods *meth, struct attr **attrs,
                                              struct callback_list *cbl);

    if (!(type_attr = attr_search(attrs, attr_type)))
    {
        dbg(lvl_error, "Graphics plugin type is not set.");
        return;
    }

    graphicstype_new = plugin_get_category(plugin_category_graphics, type_attr->u.str);
    if (!graphicstype_new)
    {
        dbg(lvl_error, "Failed to load graphics plugin %s.", type_attr->u.str);
        return;
    }

    m_attrs = attr_list_dup(attrs);
    /* start with no scaling */
    m_dpi_factor = 1;
    m_callbacks = callback_list_new();
    cbl_attr.type = attr_callback_list;
    cbl_attr.u.callback_list = m_callbacks;
    callback_list_add_patch_function(m_callbacks, Graphics::static_dpi_patch, (void *)this_);
    m_attrs = attr_generic_add_attr(m_attrs, &cbl_attr);
    m_priv = (*graphicstype_new)(parent->u.navit, &m_graphicsInterface, m_attrs, m_callbacks);
    m_brightness = 0;
    m_contrast = 65536;
    m_gamma = 65536;
    m_font_size = 20;
    m_image_cache_hash = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    /*get dpi */
    virtual_dpi_attr = attr_search(attrs, attr_virtual_dpi);
    real_dpi_attr = attr_search(attrs, attr_real_dpi);
    if (virtual_dpi_attr != NULL)
    {
        navit_float virtual_dpi, real_dpi = 0;
        virtual_dpi = virtual_dpi_attr->u.num;
        if (real_dpi_attr != NULL)
            real_dpi = real_dpi_attr->u.num;
        else
            real_dpi = get_dpi();
        if ((real_dpi != 0) && (virtual_dpi != 0))
        {
            m_dpi_factor = round(real_dpi / virtual_dpi);
            if (m_dpi_factor < 1)
                m_dpi_factor = 1;
            dbg(lvl_error, "Using virtual dpi %f, real dpi %f factor %d", virtual_dpi, real_dpi, m_dpi_factor);
        }
    }
    if (m_dpi_factor != 1)
        callback_list_call_attr_2(m_callbacks, attr_resize, GINT_TO_POINTER(navit_get_width(parent->u.navit)),
                                  GINT_TO_POINTER(navit_get_height(parent->u.navit)));
    while (*attrs)
    {
        set_attr_do(*attrs);
        attrs++;
    }
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
struct graphics *Graphics::overlay_new(Graphics *parent, struct point *p, int w, int h, int wraparound)
{
    struct point_rect pr;
    struct point p_scaled;
    int w_scaled, h_scaled;

    m_dpi_factor = parent->get_dpi_factor();
    p_scaled = parent->dpi_scale_point(p);
    w_scaled = parent->dpi_scale(w);
    h_scaled = parent->dpi_scale(h);
    m_priv = parent->meth.overlay_new(parent->priv, &m_graphicsInterface, &p_scaled, w_scaled, h_scaled, wraparound);
    m_image_cache_hash = m_parent->getImageCacheHash();
    m_parent = parent;
    pr.lu.x = 0;
    pr.lu.y = 0;
    pr.rl.x = w;
    pr.rl.y = h;
    m_font_size = 20;
    set_rect(&pr);
    if (!m_priv)
    {
        g_free(this_);
        this_ = NULL;
    }
    return this_;
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
    return attr_generic_get_attr(m_attrs, NULL, type, attr, iter);
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
    m_graphicsInterface->overlay_resize(&p_scaled, w_scaled, h_scaled, wraparound);
}

void Graphics::gc_init()
{
    struct color background = {COLOR_BACKGROUND_};
    struct color black = {COLOR_BLACK_};
    struct color white = {COLOR_WHITE_};

    m_gcBackground.set_background(&background);
    m_gcBackground.set_foreground(&background);
    m_gcMiddground.set_background(&black);
    m_gcMiddground.set_foreground(&white);
    m_gcForeground.set_background(&white);
    m_gcForeground.set_foreground(&black);
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
    background_gc(m_gcBackground);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void *Graphics::get_data(const char *type)
{
    return (m_graphicsInterface->get_data(type));
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

    return named_font_new(m_default_font, size, flags);
}

struct graphics_font *Graphics::named_font_new(char *font, int size, int flags)
{
    struct graphics_font *this_;

    this_ = g_new0(struct graphics_font, 1);
    this_->priv = m_graphicsInterface->font_new(&this_->meth, font, dpi_scale(size), flags);
    return this_;
}

void Graphics::font_destroy(struct graphics_font *gra_font)
{
    if (!gra_font)
        return;
    gra_font->meth.font_destroy(gra_font->priv);
    g_free(gra_font);
}

/**
 * Destroy graphics
 * Called when navit exits
 * @param gra The graphics instance
 * @returns nothing
 * @author David Tegze (02/2011)
 */
void Graphics::free()
{
    /* If it's not an overlay, free the image cache. */
    if (!m_parent)
    {
        struct graphics_image *img;
        GList *ll, *l;

        /* We can't specify context (pointer to struct graphics) for g_hash_table_new to have it passed to free function
           so we have to free img->priv manually, the rest would be freed by g_hash_table_destroy. GHashTableIter isn't used because it
           broke n800 build at r5107.
        */
        for (ll = l = g_hash_to_list(m_image_cache_hash); l; l = g_list_next(l))
        {
            img = (graphics_image *)l->data;
            if (img)
                m_graphicsInterface->image_free(img->priv);
        }
        g_list_free(ll);
        g_hash_table_destroy(m_image_cache_hash);
    }

    attr_list_free(m_attrs);
    // m_gcBackground->destroy();
    // m_gcMiddground->destroy();
    // m_gcForeground->destroy();
    g_free(m_default_font);
    font_destroy_all();
    g_free(m_font);
    // m_graphicsInterface->destroy();
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
        m_font[i]->meth.font_destroy(m_font[i]->priv);
        g_free(m_font[i]);
        m_font[i] = NULL;
    }
}

void Graphics::convert_color(struct color *in, struct color *out)
{
    *out = *in;
    if (m_colormgmt == 0)
    {
        return;
    }
    if (m_brightness)
    {
        out->r += m_brightness;
        out->g += m_brightness;
        out->b += m_brightness;
    }
    if (m_contrast != 65536)
    {
        out->r = out->r * m_contrast / 65536;
        out->g = out->g * m_contrast / 65536;
        out->b = out->b * m_contrast / 65536;
    }
    if (out->r < 0)
        out->r = 0;
    if (out->r > 65535)
        out->r = 65535;
    if (out->g < 0)
        out->g = 0;
    if (out->g > 65535)
        out->g = 65535;
    if (out->b < 0)
        out->b = 0;
    if (out->b > 65535)
        out->b = 65535;
    if (m_gamma != 65536)
    {
        out->r = pow(out->r / 65535.0, m_gamma / 65536.0) * 65535.0;
        out->g = pow(out->g / 65535.0, m_gamma / 65536.0) * 65535.0;
        out->b = pow(out->b / 65535.0, m_gamma / 65536.0) * 65535.0;
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
struct graphics_image *Graphics::image_new_scaled(char *path, int w, int h)
{
    return image_new_scaled_rotated(path, w, h, 0);
}

void Graphics::image_new_helper(graphics_image *image, char *path, char *name, int width, int height, int rotate)
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
        char *new_name = NULL;
        int n;
        switch (mode)
        {
        case 1:
            /* The best variant both for cpu usage and quality would be prescaled png of a needed size */
            mode++;
            if (width != IMAGE_W_H_UNSET && height != IMAGE_W_H_UNSET)
            {
                new_name = g_strdup_printf("%s_%d_%d.png", name, width, height);
            }
            break;
        case 2:
            mode++;
            /* Try to load image by the exact name given by user. For example, if she wants to
              scale some prescaled png variant to a new size given as function params, or have
              default png image to be displayed unscaled. */
            new_name = g_strdup(path);
            break;
        case 3:
            mode++;
            /* Next, try uncompressed and compressed svgs as they should give best quality but
               rendering might take more cpu resources when the image is displayed for the first time */
            new_name = g_strdup_printf("%s.svg", name);
            break;
        case 4:
            mode++;
            new_name = g_strdup_printf("%s.svgz", name);
            break;
        case 5:
            mode++;
            i = 0;
            /* If we have no size specifiers, try the default png now */
            if (sz <= 0)
            {
                new_name = g_strdup_printf("%s.png", name);
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
            new_name = g_strdup_printf("%s_%d_%d.png", name, stdsizes[n], stdsizes[n]);
            break;

        case 7:
            /* Scaling the default prescaled png of unknown size to the needed size will give random quality loss */
            mode++;
            new_name = g_strdup_printf("%s.png", name);
            break;
        case 8:
            /* xpm format is used as a last resort, because its not widely supported and we are moving to svg and png formats */
            mode++;
            new_name = g_strdup_printf("%s.xpm", name);
            break;
        }
        if (!new_name)
            continue;

        image->width = width;
        image->height = height;
        dbg(lvl_debug, "Trying to load image '%s' for '%s' at %dx%d", new_name, path, width, height);

        image->hot = dpi_scale_point(&image->hot);
        if (image->width != IMAGE_W_H_UNSET)
            image->width = dpi_scale(image->width);
        if (image->height != IMAGE_W_H_UNSET)
            image->height = dpi_scale(image->height);
        image->priv = m_graphicsInterface->image_new(&image->meth, new_name, &image->width, &image->height, &image->hot, rotate);
        image->hot = dpi_unscale_point(&image->hot);
        if (image->width != IMAGE_W_H_UNSET)
            image->width = dpi_unscale(image->width);
        if (image->height != IMAGE_W_H_UNSET)
            image->height = dpi_unscale(image->height);

        if (image->priv)
        {
            dbg(lvl_info, "Using image '%s' for '%s' at %dx%d", new_name, path, width, height);
            g_free(new_name);
            break;
        }
        g_free(new_name);
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
struct graphics_image *Graphics::image_new_scaled_rotated(char *path, int w, int h, int rotate)
{
    struct graphics_image *image;
    char *hash_key = g_strdup_printf("%s*%d*%d*%d", path, w, h, rotate);
    struct file_wordexp *we;
    int i;
    char **paths;
    if (g_hash_table_lookup_extended(m_image_cache_hash, hash_key, NULL, (void **)&image))
    {
        g_free(hash_key);
        dbg(lvl_debug, "Found cached image%sfor '%s'", image ? " " : " miss ", path);
        return image;
    }

    image = g_new0(struct graphics_image, 1);
    image->height = h;
    image->width = w;

    we = file_wordexp_new(path);
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
        image_new_helper(image, pathi, name, newwidth, newheight, rotate);
        g_free(name);
    }

    file_wordexp_destroy(we);

    if (!image->priv)
    {
        dbg(lvl_error, "No image for '%s'", path);
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
struct graphics_image *Graphics::image_new(char *path)
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
    m_graphicsInterface->draw_mode(mode);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_lines(GraphicsContext *gc, struct point *p, int count)
{
    struct point *p_scaled;
    int a;
    if (count < ALLOCA_COORD_LIMIT)
        p_scaled = (point *)g_alloca(sizeof(struct point) * count);
    else
        p_scaled = (point *)g_malloc(sizeof(struct point) * count);

    for (a = 0; a < count; a++)
        p_scaled[a] = dpi_scale_point(&(p[a]));
    m_graphicsInterface->draw_lines(gc, p_scaled, count);
    if (count >= ALLOCA_COORD_LIMIT)
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
void Graphics::draw_circle(GraphicsContext *gc, struct point *p, int r)
{
    struct point *pnt;
    int i = 0;
    if ((r * 4 + 64) < ALLOCA_COORD_LIMIT)
        pnt = (point *)g_alloca(sizeof(struct point) * (r * 4 + 64));
    else
        pnt = (point *)g_malloc(sizeof(struct point) * (r * 4 + 64));

    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface->draw_circle(gc, &p_scaled, dpi_scale(this_, r));

    if ((r * 4 + 64) >= ALLOCA_COORD_LIMIT)
        g_free(pnt);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_rectangle(GraphicsContext *gc, struct point *p, int w, int h)
{
    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface->draw_rectangle(gc, &p_scaled, dpi_scale(w), dpi_scale(h));
}

/**
 * @brief Draw a plain polygon on the display
 *
 * @param gra The graphics instance on which to draw
 * @param gc The graphics context
 * @param[in] pin An array of points forming the polygon
 * @param count_in The number of elements inside @p pin
 */
void Graphics::draw_polygon(GraphicsContext *gc, struct point *pin, int count_in)
{
    struct point *pin_scaled;
    int a;
    if (count_in < ALLOCA_COORD_LIMIT)
        pin_scaled = (point *)g_alloca(sizeof(struct point) * count_in);
    else
        pin_scaled = (point *)g_malloc(sizeof(struct point) * count_in);

    for (a = 0; a < count_in; a++)
        pin_scaled[a] = dpi_scale_point(&(pin[a]));
    m_graphicsInterface->draw_polygon(gc, pin_scaled, count_in);
    if (count_in >= ALLOCA_COORD_LIMIT)
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
void Graphics::draw_polygon_with_holes(GraphicsContext *gc, struct point *pin, int count_in, int hole_count, int *ccount, struct point **holes)
{

    struct point *pin_scaled;
    struct point **holes_scaled;
    int a;
    int b;
    if (count_in < ALLOCA_COORD_LIMIT)
    {
        pin_scaled = (point *)g_alloca(sizeof(struct point) * count_in);
    }
    else
    {
        pin_scaled = (point *)g_malloc(sizeof(struct point) * count_in);
    }
    if (hole_count < ALLOCA_COORD_LIMIT)
    {
        holes_scaled = (point **)g_alloca(sizeof(struct point *) * hole_count);
    }
    else
    {
        holes_scaled = (point **)g_malloc(sizeof(struct point *) * hole_count);
    }
    /* scale the outline */
    for (a = 0; a < count_in; a++)
        pin_scaled[a] = dpi_scale_point(&(pin[a]));
    /*scale the holes */
    for (b = 0; b < hole_count; b++)
    {
        holes_scaled[b] = (point *)g_malloc(sizeof(*(holes_scaled[b])) * ccount[b]);
        for (a = 0; a < ccount[b]; a++)
            holes_scaled[b][a] = dpi_scale_point(&(holes[b][a]));
    }
    m_graphicsInterface->draw_polygon_with_holes(gc, pin_scaled, count_in, hole_count, ccount, holes_scaled);
    /* free the hole arrays */
    for (b = 0; b < hole_count; b++)
        g_free(holes_scaled[b]);
    if (count_in >= ALLOCA_COORD_LIMIT)
        g_free(pin_scaled);
    if (hole_count >= ALLOCA_COORD_LIMIT)
        g_free(holes_scaled);
}

void Graphics::draw_rectangle_rounded(GraphicsContext *gc, struct point *plu, int w, int h,
                                      int r, int fill)
{
    struct point *p;
    struct point pi0 = {plu->x + r, plu->y + r};
    struct point pi1 = {plu->x + w - r, plu->y + r};
    struct point pi2 = {plu->x + w - r, plu->y + h - r};
    struct point pi3 = {plu->x + r, plu->y + h - r};
    int i = 0;
    if ((r * 4 + 32) < ALLOCA_COORD_LIMIT)
        p = (point *)g_alloca(sizeof(struct point) * (r * 4 + 32));
    else
        p = (point *)g_malloc(sizeof(struct point) * (r * 4 + 32));

    circle_to_points(&pi2, r * 2, 0, -1, 258, p, &i, 1);
    circle_to_points(&pi1, r * 2, 0, 255, 258, p, &i, 1);
    circle_to_points(&pi0, r * 2, 0, 511, 258, p, &i, 1);
    circle_to_points(&pi3, r * 2, 0, 767, 258, p, &i, 1);
    p[i] = p[0];
    i++;
    if (fill)
        draw_polygon(gc, p, i);
    else
        draw_lines(gc, p, i);
    if ((r * 4 + 32) >= ALLOCA_COORD_LIMIT)
        g_free(p);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_text(GraphicsContext *gc1, GraphicsContext *gc2,
                         struct graphics_font *font, char *text, struct point *p, int dx, int dy)
{
    struct point p_scaled;
    p_scaled = dpi_scale_point(p);
    m_graphicsInterface->draw_text(gc1, gc2 ? gc2 : NULL, font->priv, text, &p_scaled, dx, dy);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::get_text_bbox(struct graphics_font *font, char *text, int dx, int dy,
                             struct point *ret, int estimate)
{
    m_graphicsInterface->get_text_bbox(font->priv, text, dx, dy, ret, estimate);
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
    m_graphicsInterface->overlay_disable(disable);
}

int Graphics::is_disabled()
{
    return m_disabled || (m_parent && m_parent->is_disabled());
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
    m_graphicsInterface->draw_image(gc, &p_scaled, img->priv);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void Graphics::draw_image_warp(GraphicsContext *gc, struct point *p, int count, struct graphics_image *img)
{
    struct point *p_scaled;
    int a;
    if (count < ALLOCA_COORD_LIMIT)
        p_scaled = (point *)g_alloca(sizeof(struct point) * count);
    else
        p_scaled = (point *)g_malloc(sizeof(struct point) * count);

    for (a = 0; a < count; a++)
        p_scaled[a] = dpi_scale_point(&(p[a]));
    m_graphicsInterface->draw_image_warp(gc, p_scaled, count, img->priv);
    if (count >= ALLOCA_COORD_LIMIT)
        g_free(p_scaled);
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
    m_graphicsInterface->draw_drag(&p_scaled);
    return 1;
}

void Graphics::background_gc(GraphicsContext *gc)
{
    m_graphicsInterface->background_gc(gc);
}

void Graphics::set_layout(struct layout *l)
{
    if (l)
    {
        m_gcBackground->set_background(&l->color);
        m_gcBackground->set_foreground(&l->color);
        g_free(m_default_font);
        m_default_font = g_strdup(l->font);
    }
    background_gc(m_gcBackground);
}

void Graphics::draw_background()
{
    draw_rectangle(m_gcBackground, &m_r.lu, m_r.rl.x - m_r.lu.x, m_r.rl.y - m_r.lu.y);
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
    int ret = m_graphicsInterface->show_native_keyboard(kbd);
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
    m_graphicsInterface->hide_native_keyboard(kbd);
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
void Graphics::label_line(GraphicsContext *fg, GraphicsContext *bg, struct graphics_font *font, struct point *p, int count, char *label)
{
    int i, x, y, tl, tlm, th, thm, tlsq, l;
    float lsq;
    double dx, dy;
    struct point p_t;
    struct point pb[5];

    get_text_bbox(font, label, 0x10000, 0x00, pb, 1);
    tl = (pb[2].x - pb[0].x);
    th = (pb[0].y - pb[1].y);

    tlm = tl * 32;
    thm = th * 36;
    tlsq = tlm * tlm;
    for (i = 0; i < count - 1; i++)
    {
        dx = p[i + 1].x - p[i].x;
        dx *= 32;
        dy = p[i + 1].y - p[i].y;
        dy *= 32;
        lsq = dx * dx + dy * dy;
        if (lsq > tlsq)
        {
            l = (int)sqrtf(lsq);
            x = p[i].x;
            y = p[i].y;
            if (dx < 0)
            {
                dx = -dx;
                dy = -dy;
                x = p[i + 1].x;
                y = p[i + 1].y;
            }
            x += (l - tlm) * dx / l / 64;
            y += (l - tlm) * dy / l / 64;
            x -= dy * thm / l / 64;
            y += dx * thm / l / 64;
            p_t.x = x;
            p_t.y = y;
            if (x < m_r.rl.x && x + tl > m_r.lu.x && y + tl > m_r.lu.y && y - tl < m_r.rl.y)
                draw_text(fg, bg, font, label, &p_t, dx * 0x10000 / l, dy * 0x10000 / l);
        }
    }
}

void Graphics::display_draw_arrow(struct point *p, navit_float dx, navit_float dy, navit_float width, struct display_context *dc, int filled)
{
    struct point pnt[4];
    /* half the width in every direction */
    width /= 2;
    pnt[0] = pnt[1] = pnt[2] = *p;
    pnt[0].x += -dx * width + dy * width;
    pnt[0].y += -dy * width - dx * width;
    pnt[2].x += -dx * width - dy * width;
    pnt[2].y += -dy * width + dx * width;
    if (filled)
    {
        /* close the loop */
        pnt[3] = pnt[0];
        draw_polygon(dc->gc, pnt, 4);
    }
    else
    {
        draw_lines(dc->gc, pnt, 3);
    }
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
void Graphics::display_draw_arrows(struct display_context *dc, struct point *pnt, int count, int *width, int filled)
{
    navit_float dx, dy, dw, l;
    int i;
    struct point p;
    int w;
    for (i = 0; i < count - 1; i++)
    {
        /* get the X and Y size */
        dx = pnt[i + 1].x - pnt[i].x;
        dy = pnt[i + 1].y - pnt[i].y;
        dw = width[i + 1] - width[i];
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
                if (l > (2 * width[i]))
                {
                    /* print arrow at middle point */
                    p = pnt[i];
                    p.x += dx * (l / 2);
                    p.y += dy * (l / 2);
                    w = width[i];
                    w += dw * (l / 2);
                    display_draw_arrow(&p, dx, dy, w, dc, filled);
                }
                /* if line is quite long, print arrows at 1/4 and 3/4 length */
                if (l > (20 * width[i]))
                {
                    /* at 1/4 the line length */
                    p = pnt[i];
                    p.x += dx * (l / 4);
                    p.y += dy * (l / 4);
                    w = width[i];
                    w += dw * (l / 4);
                    display_draw_arrow(&p, dx, dy, w, dc, filled);
                    /* at 3/4 the arrow length */
                    p = pnt[i + 1];
                    p.x -= dx * (l / 4);
                    p.y -= dy * (l / 4);
                    w = width[i + 1];
                    w -= dw * (l / 4);
                    display_draw_arrow(&p, dx, dy, w, dc, filled);
                }
            }
            else
            {
                /*FIXME: what if line length was smaller than 15?*/
                /* print arrow 15 units from start */
                p = pnt[i];
                p.x += dx * 15;
                p.y += dy * 15;
                display_draw_arrow(&p, dx, dy, 20, dc, filled);
                /* print arrow 15 units before end */
                p = pnt[i + 1];
                p.x -= dx * 15;
                p.y -= dy * 15;
                display_draw_arrow(&p, dx, dy, 20, dc, filled);
            }
        }
    }
}

void Graphics::display_draw_spike(struct point *p, navit_float dx, navit_float dy, navit_float width, struct display_context *dc)
{
    struct point pnt[2];
    navit_float l = navit_sqrt(dx * dx + dy * dy);
    pnt[0] = pnt[1] = *p;
    pnt[1].x += (-dy / l) * width;
    pnt[1].y += (dx / l) * width;
    draw_lines(dc->gc, pnt, 2);
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
void Graphics::display_draw_spikes(struct display_context *dc, struct point *pnt, int count, int *width, int distance)
{
    navit_float dx, dy, dw, l;
    int i;
    struct point p;
    int w;
    for (i = 0; i < count - 1; i++)
    {
        /* get the X and Y size */
        dx = pnt[i + 1].x - pnt[i].x;
        dy = pnt[i + 1].y - pnt[i].y;
        dw = width[i + 1] - width[i];
        /* calculate the length of the way segment */
        l = navit_sqrt(dx * dx + dy * dy);
        if (l != 0)
        {
            /* length is not zero */
            if (l > width[i])
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
                    p = pnt[i];
                    p.x += dx * a;
                    p.y += dy * a;
                    w = width[i];
                    w += dw * a;
                    display_draw_spike(&p, dx, dy, w, dc);
                }
            }
        }
    }
}

static int intersection(struct point *a1, int adx, int ady, struct point *b1, int bdx, int bdy, struct point *res)
{
    int n, a, b;
    dbg(lvl_debug, "%d,%d - %d,%d x %d,%d-%d,%d", a1->x, a1->y, a1->x + adx, a1->y + ady, b1->x, b1->y, b1->x + bdx, b1->y + bdy);
    n = bdy * adx - bdx * ady;
    a = bdx * (a1->y - b1->y) - bdy * (a1->x - b1->x);
    b = adx * (a1->y - b1->y) - ady * (a1->x - b1->x);
    dbg(lvl_debug, "a %d b %d n %d", a, b, n);
    if (n < 0)
    {
        n = -n;
        a = -a;
        b = -b;
    }
    if (n == 0)
        return 0;
    res->x = a1->x + a * adx / n;
    res->y = a1->y + a * ady / n;
    dbg(lvl_debug, "%d,%d", res->x, res->y);
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
static void circle_to_points(const struct point *center, int diameter, int scale, int start, int len, struct point *res,
                             int *pos, int dir)
{
    struct circle *c;
    int count = 64;
    int end = start + len;
    int i, step;
    c = circle64;
    if (diameter > 128)
        step = 1;
    else if (diameter > 64)
        step = 2;
    else if (diameter > 16)
        step = 4;
    else if (diameter > 4)
        step = 8;
    else
        step = 16;
    if (len > 0)
    {
        while (start < 0)
        {
            start += 1024;
            end += 1024;
        }
        while (end > 0)
        {
            i = 0;
            while (i < count && c[i].fowler <= start)
                i += step;
            while (i < count && c[i].fowler < end)
            {
                if (1 < *pos || 0 < dir)
                {
                    res[*pos].x = center->x + ((c[i].x * diameter + 128) >> 8);
                    res[*pos].y = center->y + ((c[i].y * diameter + 128) >> 8);
                    (*pos) += dir;
                }
                i += step;
            }
            end -= 1024;
            start -= 1024;
        }
    }
    else
    {
        while (start > 1024)
        {
            start -= 1024;
            end -= 1024;
        }
        while (end < 1024)
        {
            i = count - 1;
            while (i >= 0 && c[i].fowler >= start)
                i -= step;
            while (i >= 0 && c[i].fowler > end)
            {
                if (1 < *pos || 0 < dir)
                {
                    res[*pos].x = center->x + ((c[i].x * diameter + 128) >> 8);
                    res[*pos].y = center->y + ((c[i].y * diameter + 128) >> 8);
                    (*pos) += dir;
                }
                i -= step;
            }
            start += 1024;
            end += 1024;
        }
    }
}

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
    int ppos, npos;
    struct point *res;
    struct draw_polyline_shape shape;
    struct draw_polyline_shape prev_shape;
};

static void draw_shape_update(struct draw_polyline_shape *shape)
{
    shape->dxw = -(shape->dx * shape->wi * shape->lscale) / shape->l;
    shape->dyw = (shape->dy * shape->wi * shape->lscale) / shape->l;
}

static void draw_shape(struct draw_polyline_context *ctx, struct point *pnt, int wi)
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
    shape->dx = (pnt[1].x - pnt[0].x);
    shape->dy = (pnt[1].y - pnt[0].y);
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

static void draw_point(struct draw_polyline_shape *shape, struct point *src, struct point *dst, int pos)
{
    if (pos)
    {
        dst->x = (src->x * 2 - shape->dyw) / 2;
        dst->y = (src->y * 2 - shape->dxw) / 2;
    }
    else
    {
        dst->x = (src->x * 2 + shape->dyw) / 2;
        dst->y = (src->y * 2 + shape->dxw) / 2;
    }
}

static void draw_begin(struct draw_polyline_context *ctx, struct point *p)
{
    struct draw_polyline_shape *shape = &ctx->shape;
    int i;
    for (i = 0; i <= 32; i += shape->step)
    {
        ctx->res[ctx->ppos].x = (p->x * 256 + (shape->dyw * circle64[i].y) + (shape->dxw * circle64[i].x)) / 256;
        ctx->res[ctx->ppos].y = (p->y * 256 + (shape->dxw * circle64[i].y) - (shape->dyw * circle64[i].x)) / 256;
        ctx->ppos++;
    }
}

static int draw_middle(struct draw_polyline_context *ctx, struct point *p)
{
    int delta = ctx->prev_shape.fow - ctx->shape.fow;
    if (delta > 512)
        delta -= 1024;
    if (delta < -512)
        delta += 1024;
    if (delta < 16 && delta > -16)
    {
        draw_point(&ctx->shape, p, &ctx->res[ctx->npos--], 0);
        draw_point(&ctx->shape, p, &ctx->res[ctx->ppos++], 1);
        return 1;
    }
    dbg(lvl_debug, "delta %d", delta);
    if (delta > 0)
    {
        struct point pos, poso;
        draw_point(&ctx->shape, p, &pos, 1);
        draw_point(&ctx->prev_shape, p, &poso, 1);
        if (delta >= 256)
            return 0;
        if (intersection(&pos, ctx->shape.dx, ctx->shape.dy, &poso, ctx->prev_shape.dx, ctx->prev_shape.dy,
                         &ctx->res[ctx->ppos]))
        {
            ctx->ppos++;
            draw_point(&ctx->prev_shape, p, &ctx->res[ctx->npos--], 0);
            draw_point(&ctx->shape, p, &ctx->res[ctx->npos--], 0);
            return 1;
        }
    }
    else
    {
        struct point neg, nego;
        draw_point(&ctx->shape, p, &neg, 0);
        draw_point(&ctx->prev_shape, p, &nego, 0);
        if (delta <= -256)
            return 0;
        if (intersection(&neg, ctx->shape.dx, ctx->shape.dy, &nego, ctx->prev_shape.dx, ctx->prev_shape.dy,
                         &ctx->res[ctx->npos]))
        {
            ctx->npos--;
            draw_point(&ctx->prev_shape, p, &ctx->res[ctx->ppos++], 1);
            draw_point(&ctx->shape, p, &ctx->res[ctx->ppos++], 1);
            return 1;
        }
    }
    return 0;
}

static void draw_end(struct draw_polyline_context *ctx, struct point *p)
{
    int i;
    struct draw_polyline_shape *shape = &ctx->prev_shape;
    for (i = 0; i <= 32; i += shape->step)
    {
        ctx->res[ctx->npos].x = (p->x * 256 + (shape->dyw * circle64[i].y) - (shape->dxw * circle64[i].x)) / 256;
        ctx->res[ctx->npos].y = (p->y * 256 + (shape->dxw * circle64[i].y) + (shape->dyw * circle64[i].x)) / 256;
        ctx->npos--;
    }
}

static void draw_init_ctx(struct draw_polyline_context *ctx, int maxpoints)
{
    ctx->prec = 1;
    ctx->ppos = maxpoints / 2;
    ctx->npos = maxpoints / 2 - 1;
}

void Graphics::draw_polyline_as_polygon(GraphicsContext *gc, struct point *pnt, int count, int *width)
{
    int maxpoints = 200;
    struct draw_polyline_context ctx;
    int i = 0;
    int max_circle_points = 20;
    if (count < 2)
        return;
    ctx.shape.l = 0;
    ctx.shape.wi = 0;
    if (maxpoints < ALLOCA_COORD_LIMIT)
        ctx.res = (point *)g_alloca(sizeof(struct point) * maxpoints);
    else
        ctx.res = (point *)g_malloc(sizeof(struct point) * maxpoints);
    i = 0;
    draw_init_ctx(&ctx, maxpoints);
    draw_shape(&ctx, pnt, *width++);
    draw_begin(&ctx, &pnt[0]);
    for (i = 1; i < count - 1; i++)
    {
        draw_shape(&ctx, pnt + i, *width++);
        if (ctx.npos < max_circle_points || ctx.ppos >= maxpoints - max_circle_points || !draw_middle(&ctx, &pnt[i]))
        {
            draw_end(&ctx, &pnt[i]);
            ctx.res[ctx.npos] = ctx.res[ctx.ppos - 1];
            draw_polygon(gc, ctx.res + ctx.npos, ctx.ppos - ctx.npos);
            draw_init_ctx(&ctx, maxpoints);
            draw_begin(&ctx, &pnt[i]);
        }
    }
    draw_shape(&ctx, &pnt[count - 2], *width++);
    ctx.prev_shape = ctx.shape;
    draw_end(&ctx, &pnt[count - 1]);
    ctx.res[ctx.npos] = ctx.res[ctx.ppos - 1];
    draw_polygon(gc, ctx.res + ctx.npos, ctx.ppos - ctx.npos);
    if (maxpoints >= ALLOCA_COORD_LIMIT)
        g_free(ctx.res);
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
void Graphics::draw_polyline_clipped(GraphicsContext *gc, struct point *pa, int count, int *width, int poly)
{
    struct point *points_to_draw;
    int *w;
    struct wpoint segment_start, segment_end;
    int i, points_to_draw_cnt = 0;
    int clip_result;
    int r_width, r_height;
    struct point_rect r = m_r;

    if (count < ALLOCA_COORD_LIMIT)
    {
        points_to_draw = (point *)g_alloca(sizeof(struct point) * (count + 1));
        w = (int *)g_alloca(sizeof(int) * (count + 1));
    }
    else
    {
        points_to_draw = (point *)g_malloc(sizeof(struct point) * (count + 1));
        w = (int *)g_malloc(sizeof(int) * (count + 1));
    }

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
    for (i = 0; i < count; i++)
    {
        if (i)
        {
            segment_start.x = pa[i - 1].x;
            segment_start.y = pa[i - 1].y;
            segment_start.w = width[(i - 1)];
            segment_end.x = pa[i].x;
            segment_end.y = pa[i].y;
            segment_end.w = width[i];
            dbg(lvl_debug, "Segment: [%d, %d] - [%d, %d]...", segment_start.x, segment_start.y, segment_end.x, segment_end.y);
            clip_result = clip_line(&segment_start, &segment_end, &r);
            if (clip_result != CLIPRES_INVISIBLE)
            {
                dbg(lvl_debug, "....clipped to [%d, %d] - [%d, %d]", segment_start.x, segment_start.y, segment_end.x, segment_end.y);
                if ((i == 1) || (clip_result & CLIPRES_START_CLIPPED))
                {
                    points_to_draw[points_to_draw_cnt].x = segment_start.x;
                    points_to_draw[points_to_draw_cnt].y = segment_start.y;
                    w[points_to_draw_cnt] = segment_start.w;
                    points_to_draw_cnt++;
                }
                points_to_draw[points_to_draw_cnt].x = segment_end.x;
                points_to_draw[points_to_draw_cnt].y = segment_end.y;
                w[points_to_draw_cnt] = segment_end.w;
                points_to_draw_cnt++;
            }
            if ((i == count - 1) || (clip_result & CLIPRES_END_CLIPPED))
            {
                // ... then draw the resulting polyline
                if (points_to_draw_cnt > 1)
                {
                    if (poly)
                    {
                        draw_polyline_as_polygon(gc, points_to_draw, points_to_draw_cnt, w);
                    }
                    else
                        draw_lines(gc, points_to_draw, points_to_draw_cnt);
                    points_to_draw_cnt = 0;
                }
            }
        }
    }
    if (count >= ALLOCA_COORD_LIMIT)
    {
        g_free(points_to_draw);
        g_free(w);
    }
}

static int is_inside(struct point *p, struct point_rect *r, int edge)
{
    switch (edge)
    {
    case 0:
        return p->x >= r->lu.x;
    case 1:
        return p->x <= r->rl.x;
    case 2:
        return p->y >= r->lu.y;
    case 3:
        return p->y <= r->rl.y;
    default:
        return 0;
    }
}

static void poly_intersection(struct point *p1, struct point *p2, struct point_rect *r, int edge, struct point *ret)
{
    int dx = p2->x - p1->x;
    int dy = p2->y - p1->y;
    switch (edge)
    {
    case 0:
        ret->y = p1->y + ((float)r->lu.x - p1->x) * dy / dx;
        ret->x = r->lu.x;
        break;
    case 1:
        ret->y = p1->y + ((float)r->rl.x - p1->x) * dy / dx;
        ret->x = r->rl.x;
        break;
    case 2:
        ret->x = p1->x + ((float)r->lu.y - p1->y) * dx / dy;
        ret->y = r->lu.y;
        break;
    case 3:
        ret->x = p1->x + ((float)r->rl.y - p1->y) * dx / dy;
        ret->y = r->rl.y;
        break;
    }
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
void Graphics::clip_polygon(struct point_rect *r, struct point *in, int count_in, struct point *out, int *count_out)
{
    /* get a temp buffer to store points after one direction clipping.
     * since we are clipping 4 directions, result is always in out at the end*/
    struct point *temp;
    struct point *pout;
    struct point *pin;
    int edge;
    int count;

    /* sanity check */
    if ((r == NULL) || (in == NULL) || (out == NULL) || (count_out == NULL) || (*count_out < count_in * 8 + 1))
    {
        return;
    }

    /* prepare buffers. We have two buffers that we flip over.
     * 1. the output buffer
     * 2. temp
     */
    if (count_in < ALLOCA_COORD_LIMIT)
    {
        temp = (point *)g_alloca(sizeof(struct point) * (count_in < ALLOCA_COORD_LIMIT ? count_in * 8 + 1 : 0));
    }
    else
    {
        /* too big. Allocate a buffer (slower) */
        temp = (point *)g_new(struct point, count_in * 8 + 1);
    }
    /* use temp as first buffer. So we get the final result in out*/
    pout = temp;
    /* start with input polygon */
    pin = in;
    /* start with number of points of source polygon*/
    count = count_in;

    /* clip all four directions of a rectangle */
    for (edge = 0; edge < 4; edge++)
    {
        int i;
        /* p is first element in current buffer */
        struct point *p = pin;
        /* s is lasst element in current buffer */
        struct point *s = pin + count - 1;
        /* nothing written yet */
        *count_out = 0;

        /* iterate all points in current buffer */
        for (i = 0; i < count; i++)
        {
            if (is_inside(p, r, edge))
            {
                if (!is_inside(s, r, edge))
                {
                    struct point pi;
                    /* current segment crosses border from outside to inside. Add crossing point with border first */
                    poly_intersection(s, p, r, edge, &pi);
                    pout[(*count_out)++] = pi;
                }
                /* add point if inside */
                pout[(*count_out)++] = *p;
            }
            else
            {
                if (is_inside(s, r, edge))
                {
                    struct point pi;
                    /*current segment crosses border from inside to outside. Add crossing point with border */
                    poly_intersection(p, s, r, edge, &pi);
                    pout[(*count_out)++] = pi;
                }
                /* skip point if outside */
            }
            /* move one coordinate forward */
            s = p;
            p++;
        }
        /* use result of last clipping for next */
        count = *count_out;

        /* switch buffer */
        if (pout == temp)
        {
            pout = out;
            pin = temp;
        }
        else
        {
            pin = out;
            pout = temp;
        }
    }

    /* have clipped poly in out. And number of points now in *count_out */

    /* if we had to allocate the buffer, we need to free it */
    if (count_in >= ALLOCA_COORD_LIMIT)
    {
        g_free(temp);
    }
    return;
}

/**
 * @brief Draw a plain polygon on the display
 *
 * @param gra The graphics instance on which to draw
 * @param gc The graphics context
 * @param[in] pin An array of points forming the polygon
 * @param count_in The number of elements inside @p pin
 */
void Graphics::draw_polygon_clipped(GraphicsContext *gc, struct point *pin, int count_in)
{
    struct point_rect r = m_r;
    struct point *clipped;
    int count_out = count_in * 8 + 1;

    /* prepare buffer */
    if (count_in < ALLOCA_COORD_LIMIT)
    {
        /* use on stack buffer */
        clipped = (point *)g_alloca(sizeof(struct point) * (count_in < ALLOCA_COORD_LIMIT ? count_in * 8 + 1 : 0));
    }
    else
    {
        /* too big. allocate buffer (slower) */
        clipped = (point *)g_new(struct point, count_in * 8 + 1);
    }

    clip_polygon(&r, pin, count_in, clipped, &count_out);
    draw_polygon(gc, clipped, count_out);

    /* if we had to allocate buffer, free it */
    if (count_in >= ALLOCA_COORD_LIMIT)
    {
        g_free(clipped);
    }
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
void Graphics::draw_polygon_with_holes_clipped(GraphicsContext *gc, struct point *pin, int count_in, int hole_count, int *ccount, struct point **holes)
{
    int i;
    struct point_rect r = m_r;
    struct point *clipped;
    int total_count_in;
    int count_out;
    int count_used;
    int found_hole_count;
    int *found_ccount;
    struct point **found_holes;
    int need_free;
    /* get total node count for polygon plus all holes */
    total_count_in = count_in;
    for (i = 0; i < hole_count; i++)
    {
        total_count_in += ccount[i];
    }
    count_out = total_count_in * 8 + 1 + hole_count;

    /* prepare buffer for outer and all holes!*/
    if (count_out < ALLOCA_COORD_LIMIT)
    {
        /* use on stack buffer */
        clipped = (point *)g_alloca(sizeof(struct point) * count_out);
        /* no need to free on stack buffer */
        need_free = 0;
    }
    else
    {
        /* too big. allocate buffer (slower) */
        clipped = (point *)g_new(struct point, count_out);
        /* remember to free this, as we change count_out soon */
        need_free = 1;
    }
    count_used = 0;

    /* prepare arrays for new holes */
    if (hole_count < ALLOCA_COORD_LIMIT)
    {
        found_ccount = (int *)g_alloca(sizeof(int) * hole_count);
        found_holes = (point **)g_alloca(sizeof(struct point *) * hole_count);
    }
    else
    {
        found_ccount = (int *)g_malloc(sizeof(int) * hole_count);
        found_holes = (point **)g_malloc(sizeof(struct point *) * hole_count);
    }
    found_hole_count = 0;

    /* clip outer polygon */
    clip_polygon(&r, pin, count_in, clipped, &count_out);
    count_used += count_out;
    /* clip the holes */
    for (i = 0; i < hole_count; i++)
    {
        struct point *buffer = clipped + count_used;
        int count = total_count_in * 8 + 1 + hole_count - count_used;
        clip_polygon(&r, holes[i], ccount[i], buffer, &count);
        count_used += count;
        if (count > 0)
        {
            /* only if there are points left after clipping */
            found_ccount[found_hole_count] = count;
            found_holes[found_hole_count] = buffer;
            found_hole_count++;
        }
    }
    /* call drawing function */
    draw_polygon_with_holes(gc, clipped, count_out, found_hole_count, found_ccount, found_holes);
    if (hole_count >= ALLOCA_COORD_LIMIT)
    {
        g_free(found_ccount);
        g_free(found_holes);
    }
    /* if we had to allocate buffer, free it */
    if (need_free)
    {
        g_free(clipped);
    }
}

void Graphics::display_context_free(struct display_context *dc)
{
    if (dc->gc)
        delete dc->gc;
    if (dc->gc_background)
        delete dc->gc_background;
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

void Graphics::draw_text_std(int text_size, char *text, struct point *p)
{
    struct graphics_font *font = get_font(text_size);
    struct point bbox[4];
    int i;

    get_text_bbox(font, text, 0x10000, 0, bbox, 0);
    for (i = 0; i < 4; i++)
    {
        bbox[i].x += p->x;
        bbox[i].y += p->y;
    }
    draw_rectangle(m_gcForeground, &bbox[1], bbox[2].x - bbox[0].x, bbox[0].y - bbox[1].y + 5);
    draw_text(m_gcMiddground, m_gcForeground, font, text, p, 0x10000, 0);
}

char *Graphics::icon_path(const char *icon)
{
    static char *navit_sharedir;
    char *ret = NULL;
    struct file_wordexp *wordexp = NULL;
    dbg(lvl_debug, "enter %s", icon);
    if (strchr(icon, '$'))
    {
        wordexp = file_wordexp_new(icon);
        if (file_wordexp_get_count(wordexp))
            icon = file_wordexp_get_array(wordexp)[0];
    }
    if (strchr(icon, '/'))
        ret = g_strdup(icon);
    else
    {
#ifdef HAVE_API_ANDROID
        ret = g_strdup_printf("res/drawable/%s", icon);
#else
        if (!navit_sharedir)
            navit_sharedir = getenv("NAVIT_SHAREDIR");
        ret = g_strdup_printf("%s/icons/%s", navit_sharedir, icon);
#endif
    }
    if (wordexp)
        file_wordexp_destroy(wordexp);
    return ret;
}

char *Graphics::texture_path(const char *texture)
{
    static char *navit_sharedir;
    char *ret = NULL;
    struct file_wordexp *wordexp = NULL;
    dbg(lvl_debug, "enter %s", texture);
    if (strchr(texture, '$'))
    {
        wordexp = file_wordexp_new(texture);
        if (file_wordexp_get_count(wordexp))
            texture = file_wordexp_get_array(wordexp)[0];
    }
    if (strchr(texture, '/'))
        ret = g_strdup(texture);
    else
    {
#ifdef HAVE_API_ANDROID
        // TODO: Fix path for textures on android. Leave the same as for icons for now
        //
        ret = g_strdup_printf("res/drawable/%s", texture);
#else
        if (!navit_sharedir)
            navit_sharedir = getenv("NAVIT_SHAREDIR");
        ret = g_strdup_printf("%s/textures/%s", navit_sharedir, texture);
#endif
    }
    if (wordexp)
        file_wordexp_destroy(wordexp);
    return ret;
}

int Graphics::limit_count(struct coord *c, int count)
{
    int i;
    for (i = 1; i < count; i++)
    {
        if (c[i].x == c[0].x && c[i].y == c[0].y)
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
void Graphics::multiline_label_draw(GraphicsContext *fg, GraphicsContext *bg, struct graphics_font *font, struct point pref, const char *label, int line_spacing)
{

    char *input_label = g_strdup(label);
    char *label_lines[10]; /* Max 10 lines of text */
    int label_nblines = 0;
    int label_linepos = 0;
    char *startline = input_label;
    char *endline = startline;
    while (endline && *endline != '\0')
    {
        while (*endline != '\0' && *endline != '\n')
        { /* Search for new line */
            endline = g_utf8_next_char(endline);
        }
        if (*endline == '\0')
            endline = NULL;  /* This means we reached the end of string */
        if (endline)         /* Test if we got a new line character ('\n') */
            *endline = '\0'; /* Terminate string at line ('\n') and print this line */
        label_lines[label_nblines++] = startline;
        if (endline == NULL) /* endline is NULL, this was the last line of the multi-line string */
            break;
        endline++;           /* No need for g_utf8_next_char() here, as we know '\n' is a single byte UTF-8 char */
        startline = endline; /* Start processing next line, by setting startline to its first character */
    }
    if (label_nblines > (sizeof(label_lines) / sizeof(char
                                                          *)))
    { /* Does label_nblines overflows the number of entries in array label_lines? */
        dbg(lvl_warning, "Too many lines (%d) in label \"%s\", truncating to %lu", label_nblines, label,
            sizeof(label_lines) / sizeof(char *));
        label_nblines = sizeof(label_lines) / sizeof(char *);
    }
    /* Horizontally, we position the label next to the specified point (on the right handside) */
    pref.x += 1;
    /* Vertically, we center the text with respect to specified point */
    pref.y -= (label_nblines * line_spacing) / 2;

    /* Parse all stored lines, and display them */
    for (label_linepos = 0; label_linepos < label_nblines; label_linepos++)
    {
        draw_text(fg, bg, font, label_lines[label_linepos],
                  &pref, 0x10000, 0);
        pref.y += line_spacing;
    }
    g_free(input_label);
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
void Graphics::displayitem_transform_holes(struct transformation *trans, enum projection pro, struct displayitem_poly_holes *in, struct displayitem_poly_holes *out, int mindist)
{
    if (out == NULL)
        return;
    out->count = 0;
    out->ccount = NULL;
    out->coords = NULL;
    if ((in != NULL) && (in->count > 0))
    {
        int a, transform_res;
        /* alloc space for hole conversion. To be freed with displayitem_free_holes later*/
        out->count = in->count;
        out->ccount = (int *)g_malloc0(sizeof(*(out->ccount)) * in->count);
        out->coords = (coord **)g_malloc0(sizeof(*(out->coords)) * in->count);
        for (a = 0; a < in->count; a++)
        {
            int buf_size = sizeof(*(out->coords[a])) * in->ccount[a];
            in->ccount[a] = limit_count(in->coords[a], in->ccount[a]);
            out->coords[a] = (coord *)g_malloc0(buf_size);
            transform_res = transform_point_buf(trans, pro, in->coords[a], (struct point *)(out->coords[a]), buf_size, in->ccount[a],
                                                mindist, 0, NULL);
            /* if we did not have enough buf space for transfrom_point_buf, we try again with double the buffer size,
               until we succeed. */
            while (transform_res == TRANSFORM_ERR_BUF_SPACE)
            {
                buf_size *= 2;
                out->coords[a] = (coord *)g_realloc(out->coords[a], buf_size);
                transform_res = transform_point_buf(trans, pro, in->coords[a], (struct point *)(out->coords[a]), buf_size,
                                                    in->ccount[a], mindist, 0, NULL);
            }
            out->ccount[a] = transform_res;
        }
    }
}

/**
 * @brief free hole structure allocated by displayitem_transform_holes
 *
 * @param holes structure to deallocate
 */
void Graphics::displayitem_free_holes(struct displayitem_poly_holes *holes)
{
    if (holes == NULL)
        return;
    if (holes->count > 0)
    {
        int a;
        for (a = 0; a < holes->count; a++)
        {
            g_free(holes->coords[a]);
        }
        g_free(holes->ccount);
        g_free(holes->coords);
    }
}

void Graphics::displayitem_draw_polygon(struct display_context *dc, struct point *pa, int count, struct displayitem_poly_holes *holes)
{

    /* Set texture if any, and supported by graphics */
    if (dc->e->u.polygon.src != NULL)
    {
        char *path;
        struct graphics_image *texture;
        path = texture_path(dc->e->u.polygon.src);
        texture = image_new_scaled_rotated(path, dc->e->u.polygon.width, dc->e->u.polygon.height,
                                           dc->e->u.polygon.rotation);
        g_free(path);
        if (texture != NULL)
            dc->gc->set_texture(texture);
    }
    if ((holes != NULL) && (holes->count > 0))
        draw_polygon_with_holes_clipped(dc->gc, pa, count, holes->count, holes->ccount,
                                        (struct point **)holes->coords);
    else
        draw_polygon_clipped(dc->gc, pa, count);
}

void Graphics::displayitem_draw_polyline(struct display_context *dc, struct element *e, struct point *pa, int count, int *width)
{
    int i;
    dc->gc->set_linewidth(1);
    if (e->u.polyline.width > 0 && e->u.polyline.dash_num > 0)
        dc->gc->set_dashes(e->u.polyline.width, e->u.polyline.offset, e->u.polyline.dash_table,
                           e->u.polyline.dash_num);
    for (i = 0; i < count; i++)
    {
        if (width[i] < 2)
            width[i] = 2;
    }
    draw_polyline_clipped(dc->gc, pa, count, width, e->u.polyline.width > 1);
}

void Graphics::displayitem_draw_circle(struct displayitem *di, struct display_context *dc, struct element *e, struct point *pa, int count)
{
    if (count)
    {
        if (e->u.circle.width > 1)
            dc->gc->set_linewidth(e->u.polyline.width);
        draw_circle(dc->gc, pa, e->u.circle.radius);
        if (di->label && e->text_size)
        {
            struct graphics_font *font = get_font(e->text_size);
            GraphicsContext *gc_background = dc->gc_background;
            if (!gc_background && e->u.circle.background_color.a)
            {
                gc_background = new GraphicsContext(*this);
                gc_background->set_foreground(&e->u.circle.background_color);
                dc->gc_background = gc_background;
            }
            if (font)
            {
                struct point p;
                /* Set p to the center of the circle */
                p.x = pa[0].x + (e->u.circle.radius / 2);
                p.y = pa[0].y + (e->u.circle.radius / 2);
                multiline_label_draw(dc->gc, gc_background, font, p, di->label, e->text_size + 1);
            }
            else
                dbg(lvl_error, "Failed to get font with size %d", e->text_size);
        }
    }
}

void Graphics::displayitem_draw_text(struct displayitem *di, struct display_context *dc, struct element *e, struct point *pa, int count, struct displayitem_poly_holes *holes)
{
    if (count && di->label)
    {
        struct graphics_font *font = get_font(e->text_size);
        GraphicsContext *gc_background = dc->gc_background;
        if (!gc_background && e->u.text.background_color.a)
        {
            gc_background = new GraphicsContext(*this);
            gc_background->set_foreground(&e->u.text.background_color);
            dc->gc_background = gc_background;
        }
        if (font)
        {
            int a;
            label_line(dc->gc, gc_background, font, pa, count, di->label);
            if (holes != NULL)
            {
                for (a = 0; a < holes->count; a++)
                    label_line(dc->gc, gc_background, font, (struct point *)holes->coords[a], holes->ccount[a], di->label);
            }
        }
        else
            dbg(lvl_error, "Failed to get font with size %d", e->text_size);
    }
}

void Graphics::displayitem_draw_icon(struct displayitem *di, struct display_context *dc, struct element *e, struct point *pa, int count, struct layout *l)
{
    if (count)
    {
        struct graphics_image *img = dc->img;
        if (!img || item_is_custom_poi(di->item))
        {
            int icon_width = e->u.icon.width;
            int icon_height = e->u.icon.height;
            char *path;
            /* get the standard icon size out of the layout if unset */
            if (l != NULL)
            {
                if (icon_height == -1)
                    icon_height = l->icon_h;
                if (icon_width == -1)
                    icon_width = l->icon_w;
            }
            if (item_is_custom_poi(di->item))
            {
                char *icon;
                char *src;
                if (img)
                    image_free(img);
                src = e->u.icon.src;
                if (!src || !src[0])
                    src = "%s";
                icon = g_strdup_printf(src, di->label + strlen(di->label) + 1);
                path = icon_path(icon);
                g_free(icon);
            }
            else
                path = icon_path(e->u.icon.src);
            img = image_new_scaled_rotated(path, icon_width, icon_height, e->u.icon.rotation);
            if (img)
                dc->img = img;
            else
                dbg(lvl_debug, "failed to load icon '%s'", path);
            g_free(path);
        }
        if (img)
        {
            struct point p;
            if (e->u.icon.x != -1 || e->u.icon.y != -1)
            {
                p.x = pa[0].x - e->u.icon.x;
                p.y = pa[0].y - e->u.icon.y;
            }
            else
            {
                p.x = pa[0].x - img->hot.x;
                p.y = pa[0].y - img->hot.y;
            }
            draw_image(m_gcBackground, &p, img);
        }
    }
}

void Graphics::displayitem_draw_image(struct displayitem *di, struct display_context *dc, struct point *pa, int count)
{
    dbg(lvl_debug, "image: '%s'", di->label);
    struct graphics_image *img = dc->img;
    img = image_new_scaled_rotated(di->label, IMAGE_W_H_UNSET, IMAGE_W_H_UNSET, 0);
    if (img)
        draw_image_warp(m_gcBackground, pa, count, img);
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
void Graphics::displayitem_draw(struct displayitem *di, struct layout *l, struct display_context *dc)
{
    int *width;
    int limit = 0;
    struct point *pa;
    struct element *e = dc->e;
    int draw_underground = 0;
    long pa_buf_size = sizeof(struct point) * dc->maxlen;

    if (dc->maxlen < ALLOCA_COORD_LIMIT)
    {
        width = (int *)g_alloca(sizeof(int) * dc->maxlen);
        pa = (point *)g_alloca(pa_buf_size);
    }
    else
    {
        width = (int *)g_malloc(sizeof(int) * dc->maxlen);
        pa = (point *)g_malloc(pa_buf_size);
    }

    while (di)
    {
        int count = di->count, mindist = dc->mindist;
        struct displayitem_poly_holes t_holes;
        t_holes.count = 0;

        di->z_order = ++(m_current_z_order);

        /* Skip elements that are to be drawn on oneway streets only
         * if street is not oneway or roundabout */
        if ((e->oneway) && ((!(di->flags & AF_ONEWAY)) || (di->flags & AF_ROUNDABOUT)))
        {
            di = di->next;
            continue;
        }

        if (!dc->gc)
        {
            GraphicsContext *gc = new GraphicsContext(*this);
            dc->gc = gc;
            dc->gc->set_foreground(&e->color);
        }

        /* If the element id flagged AF_UNDERGROUND, we apply predefined transparenc to it if
         * it's not the text. */
        if ((di->flags & AF_UNDERGROUND) && (dc->e->type != element::element_text))
        {
            if (!draw_underground)
            {
                struct color fg_color = e->color;
                fg_color.a = (l != NULL) ? l->underground_alpha : UNDERGROUND_ALPHA_;
                dc->gc->set_foreground(&fg_color);
                draw_underground = 1;
            }
        }
        else
        {
            if (draw_underground)
            {
                dc->gc->set_foreground(&e->color);
                draw_underground = 0;
            }
        }
        if (item_type_is_area(dc->type) && (dc->e->type == element::element_polyline || dc->e->type == element::element_text))
            limit = 0;

        displayitem_transform_holes(dc->trans, dc->pro, di->holes, &t_holes, mindist);

        if (limit)
            count = limit_count(di->c, count);
        if (dc->type == type_poly_water_tiled)
            mindist = 0;
        if (dc->e->type == element::element_polyline)
            count = transform_point_buf(dc->trans, dc->pro, di->c, pa, pa_buf_size, count, mindist, e->u.polyline.width,
                                        width);
        else if (dc->e->type == element::element_arrows)
            count = transform_point_buf(dc->trans, dc->pro, di->c, pa, pa_buf_size, count, mindist, e->u.arrows.width,
                                        width);
        else if (dc->e->type == element::element_spikes)
            count = transform_point_buf(dc->trans, dc->pro, di->c, pa, pa_buf_size, count, mindist, e->u.spikes.width,
                                        width);
        else
            count = transform_point_buf(dc->trans, dc->pro, di->c, pa, pa_buf_size, count, mindist, 0, NULL);
        switch (e->type)
        {
        case element::element_polygon:
            displayitem_draw_polygon(dc, pa, count, &t_holes);
            break;
        case element::element_polyline:
            displayitem_draw_polyline(dc, e, pa, count, width);
            break;
        case element::element_circle:
            displayitem_draw_circle(di, dc, e, pa, count);
            break;
        case element::element_text:
            displayitem_draw_text(di, dc, e, pa, count, &t_holes);
            break;
        case element::element_icon:
            displayitem_draw_icon(di, dc, e, pa, count, l);
            break;
        case element::element_image:
            displayitem_draw_image(di, dc, pa, count);
            break;
        case element::element_arrows:
            display_draw_arrows(dc, pa, count, width, e->oneway);
            break;
        case element::element_spikes:
            display_draw_spikes(dc, pa, count, width, e->u.spikes.distance);
            break;
        default:
            dbg(lvl_error, "Unhandled element type %d", e->type);
        }
        /* free space allocated for holes */
        displayitem_free_holes(&t_holes);

        di = di->next;
    }
    if (dc->maxlen >= ALLOCA_COORD_LIMIT)
    {
        g_free(width);
        g_free(pa);
    }
}

void Graphics::draw_itemgra(struct itemgra *itm, struct transformation *t, char *label)
{
    GList *es;
    struct display_context dc;
    int max_coord = 32;
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

    es = itm->elements;
    di->item.type = type_none;
    di->item.id_hi = 0;
    di->item.id_lo = 0;
    di->item.map = NULL;
    di->z_order = 0;
    di->label = label;
    di->holes = NULL;
    dc.gra = this;
    dc.gc = NULL;
    dc.gc_background = NULL;
    dc.img = NULL;
    dc.pro = projection_screen;
    dc.mindist = 0;
    dc.trans = t;
    dc.type = type_none;
    dc.maxlen = max_coord;
    while (es)
    {
        struct element *e = (struct element *)es->data;
        if (e->coord_count)
        {
            if (e->coord_count > max_coord)
            {
                dbg(lvl_error, "maximum number of coords reached: %d > %d", e->coord_count, max_coord);
                di->count = max_coord;
            }
            else
                di->count = e->coord_count;
            memcpy(di->c, e->coord, di->count * sizeof(struct coord));
        }
        else
        {
            di->c[0].x = 0;
            di->c[0].y = 0;
            di->count = 1;
        }
        dc.e = e;
        di->next = NULL;
        displayitem_draw(di, NULL, &dc);
        display_context_free(&dc);
        es = g_list_next(es);
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
    return di->count;
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
char *Graphics::displayitem_get_label(struct displayitem *di)
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
    return m_graphicsInterface->get_dpi();
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
GraphicsContext::GraphicsContext(NavitGraphicsContextInterface &contextInterface, Graphics &graphics) : m_contextInterface(contextInterface) m_graphics(graphics)
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
void GraphicsContext::set_foreground(struct color *c)
{
    struct color cn;
    m_graphics.convert_color(c, &cn);
    c = &cn;
    m_contextInterface.set_foreground(c);
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void GraphicsContext::set_background(struct color *c)
{
    struct color cn;
    m_graphics.convert_color(c, &cn);
    c = &cn;
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
    m_contextInterface.set_linewidth(m_graphics.dpi_scale(width));
}

/**
 * FIXME
 * @param <>
 * @returns <>
 * @author Martin Schaller (04/2008)
 */
void GraphicsContext::set_dashes(int width, int offset, unsigned char dash_list[], int n)
{
    int a;
    unsigned char *dash_list_scaled = (unsigned char *)g_alloca(sizeof(unsigned char) * n);
    for (a = 0; a < n; a++)
    {
        dash_list_scaled[a] = m_graphics.dpi_scale(dash_list[a]);
    }
    m_contextInterface.set_dashes(m_graphics.dpi_scale(width), m_graphics.dpi_scale(offset),
                                  dash_list_scaled, n);
}

#pragma endregion
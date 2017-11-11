/*****************************************************************************
 * renderer_discovery.c : Renderer Discovery functions
 *****************************************************************************
 * Copyright(C) 2016 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 *(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include <vlc_common.h>
#include <vlc_atomic.h>
#include <vlc_renderer_discovery.h>
#include <vlc_probe.h>
#include <vlc_modules.h>
#include <libvlc.h>

struct vlc_renderer_item_t
{
    char *psz_name;
    char *psz_type;
    char *psz_sout;
    char *psz_icon_uri;
    char *psz_demux_filter;
    int i_flags;
    atomic_uint refs;
};

static void
item_free(vlc_renderer_item_t *p_item)
{
    free(p_item->psz_name);
    free(p_item->psz_type);
    free(p_item->psz_sout);
    free(p_item->psz_icon_uri);
    free(p_item->psz_demux_filter);
    free(p_item);
}

vlc_renderer_item_t *
vlc_renderer_item_new(const char *psz_type, const char *psz_name,
                      const char *psz_uri, const char *psz_extra_sout,
                      const char *psz_demux_filter, const char *psz_icon_uri,
                      int i_flags)
{
    assert(psz_uri != NULL);
    vlc_renderer_item_t *p_item = NULL;
    vlc_url_t url;
    vlc_UrlParse(&url, psz_uri);

    if (url.psz_protocol == NULL || url.psz_host == NULL)
        goto error;

    p_item = calloc(1, sizeof(vlc_renderer_item_t));
    if (unlikely(p_item == NULL))
        goto error;

    if ((p_item->psz_type = strdup(psz_type)) == NULL)
        goto error;

    if (psz_name != NULL)
        p_item->psz_name = strdup(psz_name);
    else if (asprintf(&p_item->psz_name, "%s (%s)", url.psz_protocol,
                      url.psz_host) == -1)
        p_item->psz_name = NULL;
    if (p_item->psz_name == NULL)
        goto error;

    if (asprintf(&p_item->psz_sout, "%s{ip=%s,port=%d%s%s}",
                 url.psz_protocol, url.psz_host, url.i_port,
                 psz_extra_sout != NULL ? "," : "",
                 psz_extra_sout != NULL ? psz_extra_sout : "") == -1)
        goto error;

    if (psz_icon_uri && (p_item->psz_icon_uri = strdup(psz_icon_uri)) == NULL)
        goto error;

    if (psz_demux_filter && (p_item->psz_demux_filter = strdup(psz_demux_filter)) == NULL)
        goto error;

    p_item->i_flags = i_flags;
    atomic_init(&p_item->refs, 1);
    vlc_UrlClean(&url);
    return p_item;

error:
    vlc_UrlClean(&url);
    if (p_item != NULL)
        item_free(p_item);
    return NULL;
}

const char *
vlc_renderer_item_name(const vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_name;
}

const char *
vlc_renderer_item_type(const vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_type;
}

const char *
vlc_renderer_item_sout(const vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_sout;
}

const char *
vlc_renderer_item_icon_uri(const vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_icon_uri;
}

const char *
vlc_renderer_item_demux_filter(const vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    return p_item->psz_demux_filter;
}

int
vlc_renderer_item_flags(const vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    return p_item->i_flags;
}

vlc_renderer_item_t *
vlc_renderer_item_hold(vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    atomic_fetch_add(&p_item->refs, 1);
    return p_item;
}

void
vlc_renderer_item_release(vlc_renderer_item_t *p_item)
{
    assert(p_item != NULL);

    int refs = atomic_fetch_sub(&p_item->refs, 1);
    assert(refs != 0 );
    if( refs != 1 )
        return;
    item_free(p_item);
}

struct vlc_rd_probe
{
    char *psz_name;
    char *psz_longname;
};

int
vlc_rd_probe_add(vlc_probe_t *probe, const char *psz_name,
                 const char *psz_longname)
{
    struct vlc_rd_probe names = { strdup(psz_name), strdup(psz_longname) };

    if (unlikely(names.psz_name == NULL || names.psz_longname == NULL
                 || vlc_probe_add(probe, &names, sizeof(names))))
    {
        free(names.psz_name);
        free(names.psz_longname);
        return VLC_ENOMEM;
    }
    return VLC_PROBE_CONTINUE;
}

#undef vlc_rd_get_names
int
vlc_rd_get_names(vlc_object_t *p_obj, char ***pppsz_names,
                 char ***pppsz_longnames)
{
    size_t i_count;
    struct vlc_rd_probe *p_tab = vlc_probe(p_obj, "renderer probe", &i_count);

    if (i_count == 0)
    {
        free(p_tab);
        return VLC_EGENERIC;
    }

    char **ppsz_names = vlc_alloc(i_count + 1, sizeof(char *));
    char **ppsz_longnames = vlc_alloc(i_count + 1, sizeof(char *));

    if (unlikely(ppsz_names == NULL || ppsz_longnames == NULL))
    {
        free(ppsz_names);
        free(ppsz_longnames);
        free(p_tab);
        return VLC_EGENERIC;
    }

    for (size_t i = 0; i < i_count; i++)
    {
        ppsz_names[i] = p_tab[i].psz_name;
        ppsz_longnames[i] = p_tab[i].psz_longname;
    }
    ppsz_names[i_count] = ppsz_longnames[i_count] = NULL;
    free(p_tab);
    *pppsz_names = ppsz_names;
    *pppsz_longnames = ppsz_longnames;
    return VLC_SUCCESS;
}

void vlc_rd_release(vlc_renderer_discovery_t *p_rd)
{
    module_unneed(p_rd, p_rd->p_module);
    config_ChainDestroy(p_rd->p_cfg);
    free(p_rd->psz_name);
    vlc_object_release(p_rd);
}

vlc_renderer_discovery_t *
vlc_rd_new(vlc_object_t *p_obj, const char *psz_name,
           const struct vlc_renderer_discovery_owner *restrict owner)
{
    vlc_renderer_discovery_t *p_rd;

    p_rd = vlc_custom_create(p_obj, sizeof(*p_rd), "renderer discovery");
    if(!p_rd)
        return NULL;
    free(config_ChainCreate(&p_rd->psz_name, &p_rd->p_cfg, psz_name));

    p_rd->owner = *owner;
    p_rd->p_module = module_need(p_rd, "renderer_discovery",
                                 p_rd->psz_name, true);
    if (p_rd->p_module == NULL)
    {
        msg_Err(p_rd, "no suitable renderer discovery module for '%s'",
            psz_name);
        free(p_rd->psz_name);
        config_ChainDestroy(p_rd->p_cfg);
        vlc_object_release(p_rd);
        p_rd = NULL;
    }

    return p_rd;
}

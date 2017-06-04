/*******************************************************************************
 * xspf.c : XSPF playlist import functions
 *******************************************************************************
 * Copyright (C) 2006-2011 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Daniel Stränger <vlc at schmaller dot de>
 *          Yoann Peronneau <yoann@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 ******************************************************************************/
/**
 * \file modules/demux/playlist/xspf.c
 * \brief XSPF playlist import functions
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_access.h>

#include <vlc_xml.h>
#include <vlc_arrays.h>
#include <vlc_strings.h>
#include <vlc_url.h>
#include "playlist.h"

#define SIMPLE_INTERFACE  (input_item_t    *p_input,\
                           const char      *psz_name,\
                           char            *psz_value)
#define COMPLEX_INTERFACE (stream_t           *p_demux,\
                           input_item_node_t  *p_input_node,\
                           xml_reader_t       *p_xml_reader,\
                           const char         *psz_element)

/* prototypes */
static bool parse_playlist_node COMPLEX_INTERFACE;
static bool parse_tracklist_node COMPLEX_INTERFACE;
static bool parse_track_node COMPLEX_INTERFACE;
static bool parse_extension_node COMPLEX_INTERFACE;
static bool parse_extitem_node COMPLEX_INTERFACE;
static bool set_item_info SIMPLE_INTERFACE;
static bool set_option SIMPLE_INTERFACE;
static bool skip_element COMPLEX_INTERFACE;

/* datatypes */
typedef struct
{
    const char *name;
    union
    {
        bool (*smpl) SIMPLE_INTERFACE;
        bool (*cmplx) COMPLEX_INTERFACE;
    } pf_handler;
    bool cmplx;
} xml_elem_hnd_t;
struct demux_sys_t
{
    input_item_t **pp_tracklist;
    int i_tracklist_entries;
    int i_track_id;
    char * psz_base;
};

static int ReadDir(stream_t *, input_item_node_t *);

/**
 * \brief XSPF submodule initialization function
 */
int Import_xspf(vlc_object_t *p_this)
{
    stream_t *p_demux = (stream_t *)p_this;

    CHECK_FILE(p_demux);

    if( !stream_HasExtension( p_demux, ".xspf" )
     && !stream_IsMimeType( p_demux->p_source, "application/xspf+xml" ) )
        return VLC_EGENERIC;

    demux_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    msg_Dbg(p_demux, "using XSPF playlist reader");
    p_demux->p_sys = sys;
    p_demux->pf_readdir = ReadDir;
    p_demux->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

void Close_xspf(vlc_object_t *p_this)
{
    stream_t *p_demux = (stream_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;
    for (int i = 0; i < p_sys->i_tracklist_entries; i++)
        if (p_sys->pp_tracklist[i])
            input_item_Release(p_sys->pp_tracklist[i]);
    free(p_sys->pp_tracklist);
    free(p_sys->psz_base);
    free(p_sys);
}

/**
 * \brief demuxer function for XSPF parsing
 */
static int ReadDir(stream_t *p_demux, input_item_node_t *p_subitems)
{
    demux_sys_t *sys = p_demux->p_sys;
    int i_ret = -1;
    xml_reader_t *p_xml_reader = NULL;
    const char *name = NULL;

    sys->pp_tracklist = NULL;
    sys->i_tracklist_entries = 0;
    sys->i_track_id = -1;
    sys->psz_base = strdup(p_demux->psz_url);

    /* create new xml parser from stream */
    p_xml_reader = xml_ReaderCreate(p_demux, p_demux->p_source);
    if (!p_xml_reader)
        goto end;

    /* locating the root node */
    if (xml_ReaderNextNode(p_xml_reader, &name) != XML_READER_STARTELEM)
    {
        msg_Err(p_demux, "can't read xml stream");
        goto end;
    }

    /* checking root node name */
    if (strcmp(name, "playlist"))
    {
        msg_Err(p_demux, "invalid root node name <%s>", name);
        goto end;
    }

    i_ret = parse_playlist_node(p_demux, p_subitems,
                                 p_xml_reader, "playlist") ? 0 : -1;

    for (int i = 0 ; i < sys->i_tracklist_entries ; i++)
    {
        input_item_t *p_new_input = sys->pp_tracklist[i];
        if (p_new_input)
        {
            input_item_node_AppendItem(p_subitems, p_new_input);
        }
    }

end:
    if (p_xml_reader)
        xml_ReaderDelete(p_xml_reader);
    return i_ret; /* Needed for correct operation of go back */
}

static const xml_elem_hnd_t *get_handler(const xml_elem_hnd_t *tab, size_t n, const char *name)
{
    for (size_t i = 0; i < n; i++)
        if (!strcmp(name, tab[i].name))
            return &tab[i];
    return NULL;
}
#define get_handler(tab, name) get_handler(tab, ARRAY_SIZE(tab), name)

/**
 * \brief parse the root node of a XSPF playlist
 * \param p_demux demuxer instance
 * \param p_input_item current input item
 * \param p_xml_reader xml reader instance
 * \param psz_element name of element to parse
 */
static bool parse_playlist_node COMPLEX_INTERFACE
{
    demux_sys_t *sys = p_demux->p_sys;
    input_item_t *p_input_item = p_input_node->p_item;
    char *psz_value = NULL;
    bool b_version_found = false;
    int i_node;
    bool b_ret = false;
    const xml_elem_hnd_t *p_handler = NULL;

    static const xml_elem_hnd_t pl_elements[] =
        { {"title",        {.smpl = set_item_info}, false },
          {"creator",      {.smpl = set_item_info}, false },
          {"annotation",   {.smpl = set_item_info}, false },
          {"info",         {NULL}, false },
          {"location",     {NULL}, false },
          {"identifier",   {NULL}, false },
          {"image",        {.smpl = set_item_info}, false },
          {"date",         {NULL}, false },
          {"license",      {NULL}, false },
          {"attribution",  {.cmplx = skip_element}, true },
          {"link",         {NULL}, false },
          {"meta",         {NULL}, false },
          {"extension",    {.cmplx = parse_extension_node}, true },
          {"trackList",    {.cmplx = parse_tracklist_node}, true },
        };
/* read all playlist attributes */
    const char *name, *value;
    while ((name = xml_ReaderNextAttr(p_xml_reader, &value)) != NULL)
    {
        if (!strcmp(name, "version"))
        {
            b_version_found = true;
            if (strcmp(value, "0") && strcmp(value, "1"))
                msg_Warn(p_demux, "unsupported XSPF version %s", value);
        }
        else if (!strcmp(name, "xmlns") || !strcmp(name, "xmlns:vlc"))
            ;
        else if (!strcmp(name, "xml:base"))
        {
            free(sys->psz_base);
            sys->psz_base = strdup(value);
        }
        else
            msg_Warn(p_demux, "invalid <playlist> attribute: \"%s\"", name);
    }
    /* attribute version is mandatory !!! */
    if (!b_version_found)
        msg_Warn(p_demux, "<playlist> requires \"version\" attribute");

    psz_value = NULL;
    while ((i_node = xml_ReaderNextNode(p_xml_reader, &name)) > 0)
        switch (i_node)
    {
    case XML_READER_STARTELEM:
        FREENULL(psz_value);

        if (!*name)
        {
            msg_Err(p_demux, "invalid XML stream");
            goto end;
        }
        p_handler = get_handler(pl_elements, name);
        if (!p_handler)
        {
            msg_Err(p_demux, "unexpected element <%s>", name);
            goto end;
        }
        /* complex content is parsed in a separate function */
        if (p_handler->cmplx)
        {
            if (!p_handler->pf_handler.cmplx(p_demux, p_input_node,
                        p_xml_reader, p_handler->name))
                return false;
            p_handler = NULL;
        }
        break;

    case XML_READER_TEXT:
        FREENULL(psz_value);
        psz_value = strdup(name);
        if (unlikely(!psz_value))
            goto end;
        break;

    case XML_READER_ENDELEM:
        /* leave if the current parent node <playlist> is terminated */
        if (!strcmp(name, psz_element))
        {
            b_ret = true;
            goto end;
        }
        /* there MUST have been a start tag for that element name */
        if (!p_handler || !p_handler->name || strcmp(p_handler->name, name))
        {
            msg_Err(p_demux, "there's no open element left for <%s>", name);
            goto end;
        }

        if (p_handler->pf_handler.smpl)
            p_handler->pf_handler.smpl(p_input_item, p_handler->name, psz_value);
        FREENULL(psz_value);
        p_handler = NULL;
        break;
    }

end:
    free(psz_value);
    return b_ret;
}

/**
 * \brief parses the tracklist node which only may contain <track>s
 */
static bool parse_tracklist_node COMPLEX_INTERFACE
{
    VLC_UNUSED(psz_element);
    const char *name;
    unsigned i_ntracks = 0;
    int i_node;

    /* now parse the <track>s */
    while ((i_node = xml_ReaderNextNode(p_xml_reader, &name)) > 0)
    {
        if (i_node == XML_READER_STARTELEM)
        {
            if (strcmp(name, "track"))
            {
                msg_Err(p_demux, "unexpected child of <trackList>: <%s>",
                         name);
                return false;
            }

            /* parse the track data in a separate function */
            if (parse_track_node(p_demux, p_input_node, p_xml_reader, "track"))
                i_ntracks++;
        }
        else if (i_node == XML_READER_ENDELEM)
            break;
    }

    /* the <trackList> has to be terminated */
    if (i_node != XML_READER_ENDELEM)
    {
        msg_Err(p_demux, "there's a missing </trackList>");
        return false;
    }
    if (strcmp(name, "trackList"))
    {
        msg_Err(p_demux, "expected: </trackList>, found: </%s>", name);
        return false;
    }

    msg_Dbg(p_demux, "parsed %u tracks successfully", i_ntracks);
    return true;
}

/**
 * \brief parse one track element
 * \param COMPLEX_INTERFACE
 */
static bool parse_track_node COMPLEX_INTERFACE
{
    input_item_t *p_input_item = p_input_node->p_item;
    const char *name;
    char *psz_value = NULL;
    const xml_elem_hnd_t *p_handler = NULL;
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_node;

    static const xml_elem_hnd_t track_elements[] =
        { {"location",     {NULL}, false },
          {"identifier",   {NULL}, false },
          {"title",        {.smpl = set_item_info}, false },
          {"creator",      {.smpl = set_item_info}, false },
          {"annotation",   {.smpl = set_item_info}, false },
          {"info",         {.smpl = set_item_info}, false },
          {"image",        {.smpl = set_item_info}, false },
          {"album",        {.smpl = set_item_info}, false },
          {"trackNum",     {.smpl = set_item_info}, false },
          {"duration",     {.smpl = set_item_info}, false },
          {"link",         {NULL}, false },
          {"meta",         {NULL}, false },
          {"extension",    {.cmplx = parse_extension_node}, true },
        };

    input_item_t *p_new_input = input_item_New(NULL, NULL);
    if (!p_new_input)
        return false;
    input_item_node_t *p_new_node = input_item_node_Create(p_new_input);

    /* reset i_track_id */
    p_sys->i_track_id = -1;

    while ((i_node = xml_ReaderNextNode(p_xml_reader, &name)) > 0)
        switch (i_node)
    {
    case XML_READER_STARTELEM:
        FREENULL(psz_value);

        if (!*name)
        {
            msg_Err(p_demux, "invalid XML stream");
            goto end;
        }
        p_handler = get_handler(track_elements, name);
        if (!p_handler)
        {
            msg_Err(p_demux, "unexpected element <%s>", name);
            goto end;
        }
        /* complex content is parsed in a separate function */
        if (p_handler->cmplx)
        {
            if (!p_handler->pf_handler.cmplx(p_demux, p_new_node,
                                             p_xml_reader, p_handler->name)) {
                input_item_node_Delete(p_new_node);
                input_item_Release(p_new_input);
                return false;
            }

            p_handler = NULL;
        }
        break;

    case XML_READER_TEXT:
        free(psz_value);
        psz_value = strdup(name);
        if (unlikely(!psz_value))
            goto end;
        break;

    case XML_READER_ENDELEM:
        /* leave if the current parent node <track> is terminated */
        if (!strcmp(name, psz_element))
        {
            free(psz_value);

            /* Make sure we have a URI */
            char *psz_uri = input_item_GetURI(p_new_input);
            if (!psz_uri)
                input_item_SetURI(p_new_input, "vlc://nop");
            else
                free(psz_uri);

            if (p_sys->i_track_id < 0
             || (size_t)p_sys->i_track_id >= (SIZE_MAX / sizeof(p_new_input)))
            {
                input_item_node_AppendNode(p_input_node, p_new_node);
                input_item_Release(p_new_input);
                return true;
            }

            if (p_sys->i_track_id >= p_sys->i_tracklist_entries)
            {
                input_item_t **pp;
                pp = realloc(p_sys->pp_tracklist,
                    (p_sys->i_track_id + 1) * sizeof(*pp));
                if (!pp)
                {
                    input_item_Release(p_new_input);
                    input_item_node_Delete(p_new_node);
                    return false;
                }
                p_sys->pp_tracklist = pp;
                while (p_sys->i_track_id >= p_sys->i_tracklist_entries)
                    pp[p_sys->i_tracklist_entries++] = NULL;
            }
            else if (p_sys->pp_tracklist[p_sys->i_track_id] != NULL)
            {
                msg_Err(p_demux, "track ID %d collision", p_sys->i_track_id);
                input_item_Release(p_new_input);
                input_item_node_Delete(p_new_node);
                return false;
            }

            p_sys->pp_tracklist[ p_sys->i_track_id ] = p_new_input;
            input_item_node_Delete(p_new_node);
            return true;
        }
        /* there MUST have been a start tag for that element name */
        if (!p_handler || !p_handler->name || strcmp(p_handler->name, name))
        {
            msg_Err(p_demux, "there's no open element left for <%s>", name);
            goto end;
        }

        /* special case: location */
        if (!strcmp(p_handler->name, "location"))
        {
            if (psz_value == NULL)
                input_item_SetURI(p_new_input, "vlc://nop");
            else
            {
                char* psz_uri = ProcessMRL( psz_value, p_sys->psz_base );

                if( !psz_uri )
                {
                    msg_Warn( p_demux, "unable to process MRL: %s", psz_value );
                    goto end;
                }

                input_item_SetURI(p_new_input, psz_uri);
                free(psz_uri);
            }

            input_item_CopyOptions(p_new_input, p_input_item);
        }
        else
        {
            /* there MUST be an item */
            if (p_handler->pf_handler.smpl)
                p_handler->pf_handler.smpl(p_new_input, p_handler->name,
                                            psz_value);
        }
        FREENULL(psz_value);
        p_handler = NULL;
        break;
    }
    msg_Err(p_demux, "unexpected end of xml data");

end:

    input_item_Release(p_new_input);
    input_item_node_Delete(p_new_node);
    free(psz_value);
    return false;
}

/**
 * \brief handles the supported <track> sub-elements
 */
static bool set_item_info SIMPLE_INTERFACE
{
    /* exit if setting is impossible */
    if (!psz_name || !psz_value || !p_input)
        return false;

    vlc_xml_decode(psz_value);

    /* handle each info element in a separate "if" clause */
    if (!strcmp(psz_name, "title"))
        input_item_SetTitle(p_input, psz_value);
    else if (!strcmp(psz_name, "creator"))
        input_item_SetArtist(p_input, psz_value);
    else if (!strcmp(psz_name, "album"))
        input_item_SetAlbum(p_input, psz_value);
    else if (!strcmp(psz_name, "trackNum"))
        input_item_SetTrackNum(p_input, psz_value);
    else if (!strcmp(psz_name, "duration"))
        p_input->i_duration = atol(psz_value) * INT64_C(1000);
    else if (!strcmp(psz_name, "annotation"))
        input_item_SetDescription(p_input, psz_value);
    else if (!strcmp(psz_name, "info"))
        input_item_SetURL(p_input, psz_value);
    else if (!strcmp(psz_name, "image") && *psz_value)
        input_item_SetArtURL(p_input, psz_value);
    return true;
}

/**
 * \brief handles the <vlc:option> elements
 */
static bool set_option SIMPLE_INTERFACE
{
    /* exit if setting is impossible */
    if (!psz_name || !psz_value || !p_input)
        return false;

    vlc_xml_decode(psz_value);

    input_item_AddOption(p_input, psz_value, 0);

    return true;
}

/**
 * \brief parse the extension node of a XSPF playlist
 */
static bool parse_extension_node COMPLEX_INTERFACE
{
    demux_sys_t *sys = p_demux->p_sys;
    input_item_t *p_input_item = p_input_node->p_item;
    char *psz_value = NULL;
    char *psz_title = NULL;
    char *psz_application = NULL;
    int i_node;
    const xml_elem_hnd_t *p_handler = NULL;
    input_item_t *p_new_input = NULL;

    static const xml_elem_hnd_t pl_elements[] =
        { {"vlc:node",   {.cmplx = parse_extension_node}, true },
          {"vlc:item",   {.cmplx = parse_extitem_node}, true },
          {"vlc:id",     {NULL}, false },
          {"vlc:option", {.smpl = set_option}, false },
        };

    /* read all extension node attributes */
    const char *name, *value;
    while ((name = xml_ReaderNextAttr(p_xml_reader, &value)) != NULL)
    {
        if (!strcmp(name, "title"))
        {
            free(psz_title);
            psz_title = strdup(value);
            if (likely(psz_title != NULL))
                vlc_xml_decode(psz_title);
        }
        else if (!strcmp(name, "application"))
        {
            free(psz_application);
            psz_application = strdup(value);
        }
        else
            msg_Warn(p_demux, "invalid <%s> attribute:\"%s\"", psz_element,
                      name);
    }

    /* attribute title is mandatory except for <extension> */
    if (!strcmp(psz_element, "vlc:node"))
    {
        if (!psz_title)
        {
            msg_Warn(p_demux, "<vlc:node> requires \"title\" attribute");
            goto error;
        }
        p_new_input = input_item_NewDirectory("vlc://nop", psz_title,
                                              ITEM_NET_UNKNOWN);
        if (p_new_input)
        {
            p_input_node =
                input_item_node_AppendItem(p_input_node, p_new_input);
            p_input_item = p_new_input;
        }
    }
    else if (!strcmp(psz_element, "extension"))
    {
        if (!psz_application)
        {
            msg_Warn(p_demux, "<extension> requires \"application\" attribute");
            goto error;
        }
        /* Skip the extension if the application is not vlc
           This will skip all children of the current node */
        else if (strcmp(psz_application, "http://www.videolan.org/vlc/playlist/0"))
        {
            msg_Dbg(p_demux, "Skipping \"%s\" extension tag", psz_application);
            skip_element( NULL, NULL, p_xml_reader, NULL );
            goto success;
        }
    }

    /* parse the child elements */
    while ((i_node = xml_ReaderNextNode(p_xml_reader, &name)) > 0)
    {
        switch (i_node)
        {
            case XML_READER_STARTELEM:
                FREENULL(psz_value);

                if (!*name)
                {
                    msg_Err(p_demux, "invalid xml stream");
                    goto error;
                }
                p_handler = get_handler(pl_elements, name);
                if (!p_handler)
                {
                    msg_Err(p_demux, "unexpected element <%s>", name);
                    goto error;
                }
                /* complex content is parsed in a separate function */
                if (p_handler->cmplx)
                {
                    if (p_handler->pf_handler.cmplx(p_demux,
                                                     p_input_node,
                                                     p_xml_reader,
                                                     p_handler->name))
                    {
                        p_handler = NULL;
                    }
                    else
                        goto error;
                }
                break;

            case XML_READER_TEXT:
                FREENULL(psz_value);
                psz_value = strdup(name);
                if (unlikely(!psz_value))
                    goto error;
                break;

            case XML_READER_ENDELEM:
                /* leave if the current parent node is terminated */
                if (!strcmp(name, psz_element))
                    goto success;

                /* there MUST have been a start tag for that element name */
                if (!p_handler || !p_handler->name
                    || strcmp(p_handler->name, name))
                {
                    msg_Err(p_demux, "there's no open element left for <%s>",
                             name);
                    goto error;
                }

                /* special tag <vlc:id> */
                if (!strcmp(p_handler->name, "vlc:id") && psz_value )
                {
                    sys->i_track_id = atoi(psz_value);
                }
                else if (p_handler->pf_handler.smpl)
                {
                    p_handler->pf_handler.smpl(p_input_item, p_handler->name,
                                                psz_value);
                }
                FREENULL(psz_value);
                p_handler = NULL;
                break;
        }
    }

success: ;
    bool b_success = true;
out:
    if (p_new_input)
        input_item_Release(p_new_input);
    free(psz_application);
    free(psz_title);
    free(psz_value);
    return b_success;
error:
    b_success = false;
    goto out;
}

/**
 * \brief parse the extension item node of a XSPF playlist
 */
static bool parse_extitem_node COMPLEX_INTERFACE
{
    VLC_UNUSED(psz_element);
    demux_sys_t *sys = p_demux->p_sys;
    input_item_t *p_new_input = NULL;
    int i_tid = -1;

    /* read all extension item attributes */
    const char *name, *value;
    while ((name = xml_ReaderNextAttr(p_xml_reader, &value)) != NULL)
    {
        /* attribute: href */
        if (!strcmp(name, "tid"))
            i_tid = atoi(value);
        /* unknown attribute */
        else
            msg_Warn(p_demux, "invalid <vlc:item> attribute: \"%s\"", name);
    }

    /* attribute href is mandatory */
    if (i_tid < 0)
    {
        msg_Warn(p_demux, "<vlc:item> requires \"tid\" attribute");
        return false;
    }

    if (i_tid >= sys->i_tracklist_entries)
    {
        msg_Warn(p_demux, "invalid \"tid\" attribute");
        return false;
    }

    p_new_input = sys->pp_tracklist[ i_tid ];
    if (p_new_input)
    {
        input_item_node_AppendItem(p_input_node, p_new_input);
        input_item_Release(p_new_input);
        sys->pp_tracklist[i_tid] = NULL;
    }

    return true;
}

/**
 * \brief skips complex element content that we can't manage
 */
static bool skip_element COMPLEX_INTERFACE
{
    VLC_UNUSED(p_demux); VLC_UNUSED(p_input_node);
    VLC_UNUSED(psz_element);

    for (unsigned lvl = 1; lvl;)
        switch (xml_ReaderNextNode(p_xml_reader, NULL))
        {
            case XML_READER_STARTELEM:
            {
                if( !xml_ReaderIsEmptyElement( p_xml_reader ) )
                    ++lvl;
                break;
            }
            case XML_READER_ENDELEM: lvl--; break;
            case 0: case -1: return false;
        }

    return true;
}

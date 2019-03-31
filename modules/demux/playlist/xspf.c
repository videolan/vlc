/*******************************************************************************
 * xspf.c : XSPF playlist import functions
 *******************************************************************************
 * Copyright (C) 2006-2017 VLC authors and VideoLAN
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

#include <limits.h>

#define SIMPLE_INTERFACE  (input_item_t    *p_input,\
                           const char      *psz_name,\
                           char            *psz_value,\
                           void            *opaque)
#define COMPLEX_INTERFACE (stream_t           *p_stream,\
                           input_item_node_t  *p_input_node,\
                           xml_reader_t       *p_xml_reader,\
                           const char         *psz_element,\
                           bool                b_empty_node)

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

typedef struct
{
    input_item_t **pp_tracklist;
    int i_tracklist_entries;
    int i_track_id;
    char * psz_base;
} xspf_sys_t;

static int ReadDir(stream_t *, input_item_node_t *);

/**
 * \brief XSPF submodule initialization function
 */
int Import_xspf(vlc_object_t *p_this)
{
    stream_t *p_stream = (stream_t *)p_this;

    CHECK_FILE(p_stream);

    if( !stream_HasExtension( p_stream, ".xspf" )
     && !stream_IsMimeType( p_stream->s, "application/xspf+xml" ) )
        return VLC_EGENERIC;

    xspf_sys_t *sys = calloc(1, sizeof (*sys));
    if (unlikely(sys == NULL))
        return VLC_ENOMEM;

    msg_Dbg(p_stream, "using XSPF playlist reader");
    p_stream->p_sys = sys;
    p_stream->pf_readdir = ReadDir;
    p_stream->pf_control = access_vaDirectoryControlHelper;

    return VLC_SUCCESS;
}

void Close_xspf(vlc_object_t *p_this)
{
    stream_t *p_stream = (stream_t *)p_this;
    xspf_sys_t *p_sys = p_stream->p_sys;
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
static int ReadDir(stream_t *p_stream, input_item_node_t *p_subitems)
{
    xspf_sys_t *sys = p_stream->p_sys;
    int i_ret = -1;
    xml_reader_t *p_xml_reader = NULL;
    const char *name = NULL;

    sys->pp_tracklist = NULL;
    sys->i_tracklist_entries = 0;
    sys->i_track_id = -1;
    sys->psz_base = strdup(p_stream->psz_url);

    /* create new xml parser from stream */
    p_xml_reader = xml_ReaderCreate(p_stream, p_stream->s);
    if (!p_xml_reader)
        goto end;

    /* locating the root node */
    if (xml_ReaderNextNode(p_xml_reader, &name) != XML_READER_STARTELEM)
    {
        msg_Err(p_stream, "can't read xml stream");
        goto end;
    }

    /* checking root node name */
    if (strcmp(name, "playlist"))
    {
        msg_Err(p_stream, "invalid root node name <%s>", name);
        goto end;
    }

    if(xml_ReaderIsEmptyElement(p_xml_reader))
        goto end;

    i_ret = parse_playlist_node(p_stream, p_subitems,
                                 p_xml_reader, "playlist", false ) ? 0 : -1;

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

static const char *get_node_attribute(xml_reader_t *p_xml_reader, const char *psz_name)
{
    const char *name, *value;
    while ((name = xml_ReaderNextAttr(p_xml_reader, &value)) != NULL)
    {
        if (!strcmp(name, psz_name))
            return value;
    }
    return NULL;
}

/**
 * \brief generic node parsing of a XSPF playlist
 * \param p_stream stream instance
 * \param input_item_node_t current input node
 * \param p_input_item current input item
 * \param p_xml_reader xml reader instance
 * \param psz_root_node current node name to parse
 * \param pl_elements xml_elem_hnd_t handlers array
 * \param i_pl_elements xml_elem_hnd_t array count
 */
static bool parse_node(stream_t *p_stream,
                       input_item_node_t *p_input_node, input_item_t *p_input_item,
                       xml_reader_t *p_xml_reader, const char *psz_root_node,
                       const xml_elem_hnd_t *pl_elements, size_t i_pl_elements)
{
    bool b_ret = false;

    char *psz_value = NULL;
    const char *name;
    int i_node;
    const xml_elem_hnd_t *p_handler = NULL;

    while ((i_node = xml_ReaderNextNode(p_xml_reader, &name)) > XML_READER_NONE)
    {
        const bool b_empty = xml_ReaderIsEmptyElement(p_xml_reader);

        switch (i_node)
        {
            case XML_READER_STARTELEM:
                FREENULL(psz_value);
                if (!*name)
                {
                    msg_Err(p_stream, "invalid XML stream");
                    goto end;
                }

                p_handler = get_handler(pl_elements, i_pl_elements, name);
                if (!p_handler)
                {
                    msg_Warn(p_stream, "skipping unexpected element <%s>", name);
                    if(!skip_element(NULL, NULL, p_xml_reader, name, b_empty))
                        return false;
                }
                else
                {
                    /* complex content is parsed in a separate function */
                    if (p_handler->cmplx)
                    {
                        if (!p_handler->pf_handler.cmplx(p_stream, p_input_node,
                                                         p_xml_reader, p_handler->name,
                                                         b_empty))
                            return false;
                        /* Complex reader does read the named end element */
                        p_handler = NULL;
                    }
                }
                break;

            case XML_READER_TEXT:
                free(psz_value);
                if(!p_handler)
                {
                    psz_value = NULL;
                }
                else
                {
                    psz_value = strdup(name);
                    if (unlikely(!psz_value))
                        goto end;
                }
                break;

            case XML_READER_ENDELEM:
                /* leave if the current parent node is terminated */
                if (!strcmp(name, psz_root_node))
                {
                    b_ret = true;
                    goto end;
                }

                if(p_handler)
                {
                    /* there MUST have been a start tag for that element name */
                    if (strcmp(p_handler->name, name))
                    {
                        msg_Err(p_stream, "there's no open element left for <%s>", name);
                        goto end;
                    }

                    if (p_handler->pf_handler.smpl)
                        p_handler->pf_handler.smpl(p_input_item, p_handler->name,
                                                   psz_value, p_stream->p_sys);

                    free(psz_value);
                    psz_value = NULL;
                    p_handler = NULL;
                }
                break;
        }
    }

end:
    free(psz_value);

    return b_ret;
}

/**
 * \brief parse the root node of a XSPF playlist
 * \param p_stream stream instance
 * \param p_input_item current input item
 * \param p_xml_reader xml reader instance
 * \param psz_element name of element to parse
 */
static bool parse_playlist_node COMPLEX_INTERFACE
{
    xspf_sys_t *sys = p_stream->p_sys;

    if(b_empty_node)
        return false;

    bool b_version_found = false;
    /* read all playlist attributes */
    const char *psz_name, *psz_value;
    while ((psz_name = xml_ReaderNextAttr(p_xml_reader, &psz_value)) != NULL)
    {
        if (!strcmp(psz_name, "version"))
        {
            b_version_found = true;
            if (strcmp(psz_value, "0") && strcmp(psz_value, "1"))
                msg_Warn(p_stream, "unsupported XSPF version %s", psz_value);
        }
        else if (!strcmp(psz_name, "xmlns") || !strcmp(psz_name, "xmlns:vlc"))
            ;
        else if (!strcmp(psz_name, "xml:base"))
        {
            free(sys->psz_base);
            sys->psz_base = strdup(psz_value);
        }
        else
            msg_Warn(p_stream, "invalid <playlist> attribute: \"%s\"", psz_name);
    }
    /* attribute version is mandatory !!! */
    if (!b_version_found)
        msg_Warn(p_stream, "<playlist> requires \"version\" attribute");

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

    return parse_node(p_stream, p_input_node, p_input_node->p_item,
                      p_xml_reader, psz_element,
                      pl_elements, ARRAY_SIZE(pl_elements));
}

/**
 * \brief parses the tracklist node which only may contain <track>s
 */
static bool parse_tracklist_node COMPLEX_INTERFACE
{
    VLC_UNUSED(psz_element);

    if(b_empty_node)
        return true;

    /* parse the child elements */
    static const xml_elem_hnd_t pl_elements[] =
        { {"track",   {.cmplx = parse_track_node}, true },
        };

    return parse_node(p_stream, p_input_node, p_input_node->p_item,
                      p_xml_reader, psz_element,
                      pl_elements, ARRAY_SIZE(pl_elements));
}

/**
 * \brief handles the <location> elements
 */
static bool parse_location SIMPLE_INTERFACE
{
    VLC_UNUSED(psz_name);
    xspf_sys_t *p_sys = (xspf_sys_t *) opaque;
    char* psz_uri = ProcessMRL( psz_value, p_sys->psz_base );
    if(psz_uri)
    {
        input_item_SetURI(p_input, psz_uri);
        free(psz_uri);
    }
    return psz_uri != NULL;
}

/**
 * \brief parse one track element
 * \param COMPLEX_INTERFACE
 */
static bool parse_track_node COMPLEX_INTERFACE
{
    xspf_sys_t *p_sys = p_stream->p_sys;

    if(b_empty_node)
        return true;

    input_item_t *p_new_input = input_item_New(NULL, NULL);
    if (!p_new_input)
        return false;

    /* increfs p_new_input */
    input_item_node_t *p_new_node = input_item_node_Create(p_new_input);
    if(!p_new_node)
    {
        input_item_Release(p_new_input);
        return false;
    }

    /* reset i_track_id */
    p_sys->i_track_id = -1;

    static const xml_elem_hnd_t track_elements[] =
        { {"location",     {.smpl = parse_location}, false },
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

    bool b_ret = parse_node(p_stream, p_new_node, p_new_input,
                            p_xml_reader, psz_element,
                            track_elements, ARRAY_SIZE(track_elements));
    if(b_ret)
    {
        /* Make sure we have a URI */
        char *psz_uri = input_item_GetURI(p_new_input);
        if (!psz_uri)
            input_item_SetURI(p_new_input, INPUT_ITEM_URI_NOP);
        else
            free(psz_uri);

        if (p_sys->i_track_id < 0 ||
            p_sys->i_track_id == INT_MAX ||
            (size_t)p_sys->i_track_id >= (SIZE_MAX / sizeof(p_new_input)))
        {
            input_item_node_AppendNode(p_input_node, p_new_node);
            p_new_node = NULL;
        }
        else
        {
            /* Extend array as needed */
            if (p_sys->i_track_id >= p_sys->i_tracklist_entries)
            {
                input_item_t **pp;
                pp = realloc(p_sys->pp_tracklist,
                             (p_sys->i_track_id + 1) * sizeof(*pp));
                if (pp)
                {
                    p_sys->pp_tracklist = pp;
                    while (p_sys->i_track_id >= p_sys->i_tracklist_entries)
                        pp[p_sys->i_tracklist_entries++] = NULL;
                }
            }

            if (p_sys->i_track_id < p_sys->i_tracklist_entries)
            {
                input_item_t **pp_insert = &p_sys->pp_tracklist[p_sys->i_track_id];

                if (*pp_insert != NULL)
                {
                    msg_Warn(p_stream, "track ID %d collision", p_sys->i_track_id);
                    input_item_node_AppendItem(p_input_node, p_new_input);
                }
                else
                {
                    *pp_insert = p_new_input;
                    p_new_input = NULL;
                }
            }
            else b_ret = false;
        }
    }

    if(p_new_node)
        input_item_node_Delete(p_new_node); /* decrefs p_new_input */
    if(p_new_input)
        input_item_Release(p_new_input);

    return b_ret;
}

/**
 * \brief handles the supported <track> sub-elements
 */
static bool set_item_info SIMPLE_INTERFACE
{
    xspf_sys_t *p_sys = (xspf_sys_t *) opaque;

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
        p_input->i_duration = VLC_TICK_FROM_MS(atol(psz_value));
    else if (!strcmp(psz_name, "annotation"))
        input_item_SetDescription(p_input, psz_value);
    else if (!strcmp(psz_name, "info"))
    {
        char *psz_mrl = ProcessMRL( psz_value, p_sys->psz_base );
        if( psz_mrl )
            input_item_SetURL(p_input, psz_mrl);
        free( psz_mrl );
    }
    else if (!strcmp(psz_name, "image") && *psz_value)
    {
        char *psz_mrl = ProcessMRL( psz_value, p_sys->psz_base );
        if( psz_mrl )
            input_item_SetArtURL(p_input, psz_mrl);
        free( psz_mrl );
    }
    return true;
}

/**
 * \brief handles the <vlc:option> elements
 */
static bool set_option SIMPLE_INTERFACE
{
    VLC_UNUSED(opaque);
    /* exit if setting is impossible */
    if (!psz_name || !psz_value || !p_input)
        return false;

    vlc_xml_decode(psz_value);

    input_item_AddOption(p_input, psz_value, 0);

    return true;
}

/**
 * \brief handles the <vlc:id> elements
 */
static bool parse_vlcid SIMPLE_INTERFACE
{
    VLC_UNUSED(p_input); VLC_UNUSED(psz_name);
    xspf_sys_t *sys = (xspf_sys_t *) opaque;
    if(psz_value)
        sys->i_track_id = atoi(psz_value);
    return true;
}

/**
 * \brief parse the vlc:node of a XSPF playlist
 */
static bool parse_vlcnode_node COMPLEX_INTERFACE
{
    input_item_t *p_input_item = p_input_node->p_item;
    char *psz_title = NULL;

    if(b_empty_node)
        return true;

    /* read all extension node attributes */
    const char *psz_attr = get_node_attribute(p_xml_reader, "title");
    if(psz_attr)
    {
        psz_title = strdup(psz_attr);
        if (likely(psz_title != NULL))
            vlc_xml_decode(psz_title);
    }

    /* attribute title is mandatory */
    if (!psz_title)
    {
        msg_Warn(p_stream, "<vlc:node> requires \"title\" attribute");
        return false;
    }
    input_item_t *p_new_input =
        input_item_NewDirectory(INPUT_ITEM_URI_NOP, psz_title, ITEM_NET_UNKNOWN);
    free(psz_title);
    if (p_new_input)
    {
        p_input_node =
                input_item_node_AppendItem(p_input_node, p_new_input);
        p_input_item = p_new_input;
    }

    /* parse the child elements */
    static const xml_elem_hnd_t pl_elements[] =
        { {"vlc:node",   {.cmplx = parse_vlcnode_node}, true },
          {"vlc:item",   {.cmplx = parse_extitem_node}, true },
          {"vlc:id",     {.smpl = parse_vlcid}, false },
          {"vlc:option", {.smpl = set_option}, false },
        };

    bool b_ret = parse_node(p_stream, p_input_node, p_input_item,
                            p_xml_reader, psz_element,
                            pl_elements, ARRAY_SIZE(pl_elements));

    if (p_new_input)
        input_item_Release(p_new_input);

    return b_ret;
}

/**
 * \brief parse the extension node of a XSPF playlist
 */
static bool parse_extension_node COMPLEX_INTERFACE
{
    if(b_empty_node)
        return false;

    /* read all extension node attributes */
    const char *psz_application = get_node_attribute(p_xml_reader, "application");
    if (!psz_application)
    {
        msg_Warn(p_stream, "<extension> requires \"application\" attribute");
        return false;
    }

    /* Skip the extension if the application is not vlc
           This will skip all children of the current node */
    if (strcmp(psz_application, "http://www.videolan.org/vlc/playlist/0"))
    {
        msg_Dbg(p_stream, "Skipping \"%s\" extension tag", psz_application);
        return skip_element( NULL, NULL, p_xml_reader, psz_element, b_empty_node );
    }

    /* parse the child elements */
    static const xml_elem_hnd_t pl_elements[] =
        { {"vlc:node",   {.cmplx = parse_vlcnode_node}, true },
          {"vlc:id",     {.smpl = parse_vlcid}, false },
          {"vlc:option", {.smpl = set_option}, false },
        };

    return parse_node(p_stream, p_input_node, p_input_node->p_item,
                      p_xml_reader, psz_element,
                      pl_elements, ARRAY_SIZE(pl_elements));
}

/**
 * \brief parse the extension item node of a XSPF playlist
 */

static bool parse_extitem_node COMPLEX_INTERFACE
{
    VLC_UNUSED(psz_element);
    xspf_sys_t *sys = p_stream->p_sys;
    input_item_t *p_new_input = NULL;
    int i_tid = -1;

    if(!b_empty_node)
        return false;

    /* read all extension node attributes */
    const char *psz_tid = get_node_attribute(p_xml_reader, "tid");
    if(psz_tid)
        i_tid = atoi(psz_tid);

    /* attribute href is mandatory */
    if (!psz_tid || i_tid < 0)
    {
        msg_Warn(p_stream, "<vlc:item> requires valid \"tid\" attribute");
        return false;
    }

    if (i_tid >= sys->i_tracklist_entries ||
        !(p_new_input = sys->pp_tracklist[ i_tid ]) )
    {
        msg_Warn(p_stream, "non existing \"tid\" %d referenced", i_tid);
        return true;
    }

    input_item_node_AppendItem(p_input_node, p_new_input);
    input_item_Release(p_new_input);
    sys->pp_tracklist[i_tid] = NULL;

    return true;
}

/**
 * \brief skips complex element content that we can't manage
 */
static bool skip_element COMPLEX_INTERFACE
{
    VLC_UNUSED(p_stream); VLC_UNUSED(p_input_node);

    if(b_empty_node)
        return true;

    /* Const reference changes if we read again */
    char *psz_end = psz_element ? strdup(psz_element) : NULL;
    const char *name;
    unsigned lvl = 1;
    bool b_ret = true;
    while(lvl > 0 && b_ret)
    {
        switch (xml_ReaderNextNode(p_xml_reader, &name))
        {
            case XML_READER_STARTELEM:
                if( !xml_ReaderIsEmptyElement( p_xml_reader ) )
                    ++lvl;
                break;
            case XML_READER_ENDELEM:
                lvl--;
                break;
            case XML_READER_NONE:
            case XML_READER_ERROR:
                b_ret = false;
                break;
            default:
                break;
        }
    }

    if(b_ret) /* Ensure we end on same node type */
        b_ret &= (!name || !psz_end || !strcmp(psz_end, name));

    free(psz_end);

    return b_ret;
}

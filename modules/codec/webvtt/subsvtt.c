/*****************************************************************************
 * subsvtt.c: Decoder for WEBVTT as ISO1446-30 payload
 *****************************************************************************
 * Copyright (C) 2017 VideoLabs, VLC authors and VideoLAN
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
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_charset.h>
#include <vlc_subpicture.h>
#include <vlc_codec.h>
#include <vlc_stream.h>
#include <vlc_memstream.h>
#include <assert.h>

#include "../substext.h"
#include "../../demux/mp4/minibox.h"
#include "webvtt.h"


#ifdef HAVE_CSS
#  include "css_parser.h"
#  include "css_style.h"
#endif

#include <ctype.h>

//#define SUBSVTT_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct webvtt_region_t webvtt_region_t;
typedef struct webvtt_dom_node_t webvtt_dom_node_t;
typedef struct webvtt_dom_cue_t webvtt_dom_cue_t;

#define WEBVTT_REGION_LINES_COUNT          18
#define WEBVTT_DEFAULT_LINE_HEIGHT_VH    5.33
#define WEBVTT_LINE_TO_HEIGHT_RATIO      1.06
#define WEBVTT_MAX_DEPTH                 20 /* recursion prevention for now */

enum webvtt_align_e
{
    WEBVTT_ALIGN_AUTO,
    WEBVTT_ALIGN_LEFT,
    WEBVTT_ALIGN_CENTER,
    WEBVTT_ALIGN_RIGHT,
    WEBVTT_ALIGN_START,
    WEBVTT_ALIGN_END,
};

typedef struct
{
    float value;
    bool b_auto;
} webvtt_auto_value_t;

typedef struct
{
    char *psz_region;
    enum webvtt_align_e vertical;
    bool b_snap_to_lines;
    webvtt_auto_value_t line;
    enum webvtt_align_e linealign;
    float position;
    enum webvtt_align_e positionalign;
    webvtt_auto_value_t size;
    enum webvtt_align_e align;
} webvtt_cue_settings_t;

enum webvtt_node_type_e
{
    NODE_TAG,
    NODE_TEXT,
    NODE_CUE,
    NODE_REGION,
};

#define WEBVTT_NODE_BASE_MEMBERS \
    enum webvtt_node_type_e type;\
    webvtt_dom_node_t *p_parent;\
    webvtt_dom_node_t *p_next;

struct webvtt_region_t
{
    WEBVTT_NODE_BASE_MEMBERS
    char *psz_id;
    float f_width;
    unsigned i_lines_max_scroll;
    float anchor_x;
    float anchor_y;
    float viewport_anchor_x;
    float viewport_anchor_y;
    bool b_scroll_up;
    text_style_t *p_cssstyle;
    webvtt_dom_node_t *p_child;
};

struct webvtt_dom_cue_t
{
    WEBVTT_NODE_BASE_MEMBERS
    char *psz_id;
    vlc_tick_t i_start;
    vlc_tick_t i_stop;
    webvtt_cue_settings_t settings;
    unsigned i_lines;
    text_style_t *p_cssstyle;
    webvtt_dom_node_t *p_child;
};

typedef struct
{
    WEBVTT_NODE_BASE_MEMBERS
    char *psz_text;
} webvtt_dom_text_t;

typedef struct
{
    WEBVTT_NODE_BASE_MEMBERS
    vlc_tick_t i_start;
    char *psz_tag;
    char *psz_attrs;
    text_style_t *p_cssstyle;
    webvtt_dom_node_t *p_child;
} webvtt_dom_tag_t;

struct webvtt_dom_node_t
{
    WEBVTT_NODE_BASE_MEMBERS
};

typedef struct
{
    webvtt_dom_tag_t *p_root;
#ifdef HAVE_CSS
    /* CSS */
    vlc_css_rule_t *p_css_rules;
#endif
} decoder_sys_t;

#define ATOM_iden VLC_FOURCC('i', 'd', 'e', 'n')
#define ATOM_payl VLC_FOURCC('p', 'a', 'y', 'l')
#define ATOM_sttg VLC_FOURCC('s', 't', 't', 'g')
#define ATOM_vttc VLC_FOURCC('v', 't', 't', 'c')
#define ATOM_vtte VLC_FOURCC('v', 't', 't', 'e')
#define ATOM_vttx VLC_FOURCC('v', 't', 't', 'x')

/*****************************************************************************
 *
 *****************************************************************************/

static bool parse_percent( const char *psz, float *value )
{
    char *psz_end;
    float d = us_strtof( psz, &psz_end );
    if( d >= 0.0 && d <= 100.0 && *psz_end == '%' )
        *value = d / 100.0;
    return psz_end != psz;
}

static bool parse_percent_tuple( const char *psz, float *x, float *y )
{
    char *psz_end;
    float a = us_strtof( psz, &psz_end );
    if( psz_end != psz &&
        a >= 0.0 && a <= 100.0 && psz_end && *psz_end == '%' )
    {
        psz = strchr( psz_end, ',' );
        if( psz )
        {
            float b = us_strtof( ++psz, &psz_end );
            if( psz_end != psz &&
                b >= 0.0 && b <= 100.0 && psz_end && *psz_end == '%' )
            {
                *x = a / 100.0;
                *y = b / 100.0;
                return true;
            }
        }
    }
    return false;
}

typedef struct
{
    float x,y,w,h;
} webvtt_rect_t;

static void webvtt_get_cueboxrect( const webvtt_cue_settings_t *p_settings,
                                   webvtt_rect_t *p_rect )
{
    float extent;
    float indent_anchor_position;
    enum webvtt_align_e alignment_on_indent_anchor;

    /* Position of top or left depending on writing direction */
    float line_offset;
    if( !p_settings->line.b_auto ) /* numerical */
    {
        if( p_settings->b_snap_to_lines ) /* line # */
            line_offset = p_settings->line.value /
                          (WEBVTT_REGION_LINES_COUNT * WEBVTT_LINE_TO_HEIGHT_RATIO);
        else
            line_offset = p_settings->line.value;
    }
    else line_offset = 1.0;

    if( p_settings->position < 0 )
    {
        if( p_settings->align == WEBVTT_ALIGN_LEFT )
            indent_anchor_position = 0;
        else if( p_settings->align == WEBVTT_ALIGN_RIGHT )
            indent_anchor_position = 1.0;
        else
            indent_anchor_position = 0.5; /* center */
    }
    else indent_anchor_position = p_settings->position;

    if( p_settings->positionalign == WEBVTT_ALIGN_AUTO )
    {
        /* text align */
        if( p_settings->align == WEBVTT_ALIGN_LEFT ||
            p_settings->align == WEBVTT_ALIGN_RIGHT )
            alignment_on_indent_anchor = p_settings->align;
        else
            alignment_on_indent_anchor = WEBVTT_ALIGN_CENTER;
    }
    else alignment_on_indent_anchor = p_settings->positionalign;

    if( !p_settings->size.b_auto )
        extent = p_settings->size.value;
    else
        extent = 0.0;

    /* apply */

    /* we need 100% or size for inner_align to work on writing direction */
    if( p_settings->vertical == WEBVTT_ALIGN_AUTO ) /* Horizontal text */
    {
        p_rect->y = line_offset > 0 ? line_offset : 1.0 + line_offset;
        p_rect->w = (extent) ? extent : 1.0;
        if( indent_anchor_position > 0 &&
            (alignment_on_indent_anchor == WEBVTT_ALIGN_LEFT ||
             alignment_on_indent_anchor == WEBVTT_ALIGN_START) )
        {
            p_rect->x  = indent_anchor_position;
            p_rect->w -= p_rect->x;
        }
    }
    else /* Vertical text */
    {
        if( p_settings->vertical == WEBVTT_ALIGN_LEFT )
            p_rect->x = line_offset > 0 ? 1.0 - line_offset : -line_offset;
        else
            p_rect->x = line_offset > 0 ? line_offset : 1.0 + line_offset;
        p_rect->y = (extent) ? extent : 1.0;

        if( indent_anchor_position > 0 &&
            alignment_on_indent_anchor == WEBVTT_ALIGN_START )
        {
            p_rect->y  = indent_anchor_position;
            p_rect->h -= p_rect->y;
        }
    }
}

static void webvtt_cue_settings_ParseTuple( webvtt_cue_settings_t *p_settings,
                                            const char *psz_key, const char *psz_value )
{
    if( !strcmp( psz_key, "vertical" ) )
    {
        if( !strcmp( psz_value, "rl" ) )
            p_settings->vertical = WEBVTT_ALIGN_RIGHT;
        else if( !strcmp( psz_value, "lr" ) )
            p_settings->vertical = WEBVTT_ALIGN_LEFT;
        else
            p_settings->vertical = WEBVTT_ALIGN_AUTO;
    }
    else if( !strcmp( psz_key, "line" ) )
    {
        p_settings->line.b_auto = false;
        if( strchr( psz_value, '%' ) )
        {
            parse_percent( psz_value, &p_settings->line.value );
            p_settings->b_snap_to_lines = false;
        }
        else
            p_settings->line.value = us_strtof( psz_value, NULL );
        /* else auto */

        const char *psz_align = strchr( psz_value, ',' );
        if( psz_align++ )
        {
            if( !strcmp( psz_align, "center" ) )
                p_settings->linealign = WEBVTT_ALIGN_CENTER;
            else if( !strcmp( psz_align, "end" ) )
                p_settings->linealign = WEBVTT_ALIGN_END;
            else
                p_settings->linealign = WEBVTT_ALIGN_START;
        }
    }
    else if( !strcmp( psz_key, "position" ) )
    {
        parse_percent( psz_value, &p_settings->position );
        const char *psz_align = strchr( psz_value, ',' );
        if( psz_align++ )
        {
            if( !strcmp( psz_align, "line-left" ) )
                p_settings->positionalign = WEBVTT_ALIGN_LEFT;
            else if( !strcmp( psz_align, "line-right" ) )
                p_settings->positionalign = WEBVTT_ALIGN_RIGHT;
            else if( !strcmp( psz_align, "center" ) )
                p_settings->positionalign = WEBVTT_ALIGN_CENTER;
            else
                p_settings->positionalign = WEBVTT_ALIGN_AUTO;
        }
    }
    else if( !strcmp( psz_key, "size" ) )
    {
        parse_percent( psz_value, &p_settings->size.value );
        p_settings->size.b_auto = false;
    }
    else if( !strcmp( psz_key, "region" ) )
    {
        free( p_settings->psz_region );
        p_settings->psz_region = strdup( psz_value );
    }
    else if( !strcmp( psz_key, "align" ) )
    {
        if( !strcmp( psz_value, "start" ) )
            p_settings->align = WEBVTT_ALIGN_START;
        else  if( !strcmp( psz_value, "end" ) )
            p_settings->align = WEBVTT_ALIGN_END;
        else  if( !strcmp( psz_value, "left" ) )
            p_settings->align = WEBVTT_ALIGN_LEFT;
        else  if( !strcmp( psz_value, "right" ) )
            p_settings->align = WEBVTT_ALIGN_RIGHT;
        else
            p_settings->align = WEBVTT_ALIGN_CENTER;
    }
}

static void webvtt_cue_settings_Parse( webvtt_cue_settings_t *p_settings,
                                       char *p_str )
{
    char *p_save;
    char *psz_tuple;
    do
    {
        psz_tuple = strtok_r( p_str, " ", &p_save );
        p_str = NULL;
        if( psz_tuple )
        {
            const char *psz_split = strchr( psz_tuple, ':' );
            if( psz_split && psz_split[1] != 0 && psz_split != psz_tuple )
            {
                char *psz_key = strndup( psz_tuple, psz_split - psz_tuple );
                if( psz_key )
                {
                    webvtt_cue_settings_ParseTuple( p_settings, psz_key, psz_split + 1 );
                    free( psz_key );
                }
            }
        }
    } while( psz_tuple );
}

static void webvtt_cue_settings_Clean( webvtt_cue_settings_t *p_settings )
{
    free( p_settings->psz_region );
}

static void webvtt_cue_settings_Init( webvtt_cue_settings_t *p_settings )
{
    p_settings->psz_region = NULL;
    p_settings->vertical = WEBVTT_ALIGN_AUTO;
    p_settings->b_snap_to_lines = true;
    p_settings->line.b_auto = true;
    p_settings->line.value = 1.0;
    p_settings->linealign = WEBVTT_ALIGN_START;
    p_settings->position = -1;
    p_settings->positionalign = WEBVTT_ALIGN_AUTO;
    p_settings->size.value = 1.0; /* 100% */
    p_settings->size.b_auto = true;
    p_settings->align = WEBVTT_ALIGN_CENTER;
}

/*****************************************************************************
 *
 *****************************************************************************/
#ifdef SUBSVTT_DEBUG
static void webvtt_domnode_Debug( webvtt_dom_node_t *p_node, int i_depth )
{
    for( ; p_node ; p_node = p_node->p_next )
    {
        for( int i=0; i<i_depth; i++) printf(" ");
        if( p_node->type == NODE_TEXT )
        {
            printf("TEXT %s\n", ((webvtt_dom_text_t *)p_node)->psz_text );
        }
        else if( p_node->type == NODE_TAG )
        {
            webvtt_dom_tag_t *p_tag = (webvtt_dom_tag_t *)p_node;
            printf("TAG%s (%s)\n", p_tag->psz_tag, p_tag->psz_attrs );
            webvtt_domnode_Debug( p_tag->p_child, i_depth + 1 );
        }
        else if( p_node->type == NODE_CUE )
        {
            webvtt_dom_cue_t *p_cue = (webvtt_dom_cue_t *)p_node;
            printf("CUE %s\n", p_cue->psz_id );
            webvtt_domnode_Debug( p_cue->p_child, i_depth + 1 );
        }
        else if( p_node->type == NODE_REGION )
        {
            webvtt_region_t *p_region = (webvtt_region_t *)p_node;
            printf("REGION %s\n", p_region->psz_id );
            webvtt_domnode_Debug( p_region->p_child, i_depth + 1 );
        }
    }
}
#define webvtt_domnode_Debug(a,b) webvtt_domnode_Debug((webvtt_dom_node_t *)a,b)
#endif

static void webvtt_domnode_ChainDelete( webvtt_dom_node_t *p_node );
static void webvtt_dom_cue_Delete( webvtt_dom_cue_t *p_cue );
static void webvtt_region_Delete( webvtt_region_t *p_region );

static void webvtt_dom_text_Delete( webvtt_dom_text_t *p_node )
{
    free( p_node->psz_text );
    free( p_node );
}

static void webvtt_dom_tag_Delete( webvtt_dom_tag_t *p_node )
{
    text_style_Delete( p_node->p_cssstyle );
    free( p_node->psz_attrs );
    free( p_node->psz_tag );
    webvtt_domnode_ChainDelete( p_node->p_child );
    free( p_node );
}

static void webvtt_domnode_AppendLast( webvtt_dom_node_t **pp_append,
                                       webvtt_dom_node_t *p_node )
{
    while( *pp_append )
        pp_append = &((*pp_append)->p_next);
    *pp_append = p_node;
}

#define webvtt_domnode_AppendLast( a, b ) \
    webvtt_domnode_AppendLast( (webvtt_dom_node_t **) a, (webvtt_dom_node_t *) b )

static void webvtt_domnode_ChainDelete( webvtt_dom_node_t *p_node )
{
    while( p_node )
    {
        webvtt_dom_node_t *p_next = p_node->p_next;

        if( p_node->type == NODE_TAG )
            webvtt_dom_tag_Delete( (webvtt_dom_tag_t *) p_node );
        else if( p_node->type == NODE_TEXT )
            webvtt_dom_text_Delete( (webvtt_dom_text_t *) p_node );
        else if( p_node->type == NODE_CUE )
            webvtt_dom_cue_Delete( (webvtt_dom_cue_t *) p_node );
        else if( p_node->type == NODE_REGION )
            webvtt_region_Delete( (webvtt_region_t *) p_node );

        p_node = p_next;
    }
}

static webvtt_dom_text_t * webvtt_dom_text_New( webvtt_dom_node_t *p_parent )
{
    webvtt_dom_text_t *p_node = calloc( 1, sizeof(*p_node) );
    if( p_node )
    {
        p_node->type = NODE_TEXT;
        p_node->p_parent = p_parent;
    }
    return p_node;
}

static webvtt_dom_tag_t * webvtt_dom_tag_New( webvtt_dom_node_t *p_parent )
{
    webvtt_dom_tag_t *p_node = calloc( 1, sizeof(*p_node) );
    if( p_node )
    {
        p_node->i_start = -1;
        p_node->type = NODE_TAG;
        p_node->p_parent = p_parent;
    }
    return p_node;
}

static webvtt_dom_node_t * webvtt_domnode_getParentByTag( webvtt_dom_node_t *p_parent,
                                                         const char *psz_tag )
{
    for( ; p_parent ; p_parent = p_parent->p_parent )
    {
        if( p_parent->type == NODE_TAG )
        {
            webvtt_dom_tag_t *p_node = (webvtt_dom_tag_t *) p_parent;
            if( p_node->psz_tag && psz_tag && !strcmp( p_node->psz_tag, psz_tag ) )
                break;
        }
    }
    return p_parent;
}

static webvtt_dom_node_t * webvtt_domnode_getFirstChild( webvtt_dom_node_t *p_node )
{
    webvtt_dom_node_t *p_child = NULL;
    switch( p_node->type )
    {
        case NODE_CUE:
            p_child  = ((webvtt_dom_cue_t *)p_node)->p_child;
            break;
        case NODE_REGION:
            p_child  = ((webvtt_region_t *)p_node)->p_child;
            break;
        case NODE_TAG:
            p_child  = ((webvtt_dom_tag_t *)p_node)->p_child;
            break;
        default:
            break;
    }
    return p_child;
}
#define webvtt_domnode_getFirstChild(a) webvtt_domnode_getFirstChild((webvtt_dom_node_t *)a)

#ifdef HAVE_CSS
static vlc_tick_t webvtt_domnode_GetPlaybackTime( const webvtt_dom_node_t *p_node, bool b_end )
{
    for( ; p_node; p_node = p_node->p_parent )
    {
        if( p_node->type == NODE_TAG )
        {
            vlc_tick_t i_start = ((const webvtt_dom_tag_t *) p_node)->i_start;
            if( i_start > -1 && !b_end )
                return i_start;
        }
        else if( p_node->type == NODE_CUE )
        {
            break;
        }
    }
    if( p_node )
        return b_end ? ((const webvtt_dom_cue_t *) p_node)->i_stop:
                       ((const webvtt_dom_cue_t *) p_node)->i_start;
    return VLC_TICK_INVALID;
}

static bool webvtt_domnode_Match_Class( const webvtt_dom_node_t *p_node, const char *psz )
{
    const size_t i_len = strlen( psz );
    if( p_node->type == NODE_TAG )
    {
        const webvtt_dom_tag_t *p_tagnode = (webvtt_dom_tag_t *) p_node;
        for( const char *p = p_tagnode->psz_attrs; p && psz; p++ )
        {
            p = strstr( p, psz );
            if( !p )
                return false;
            if( p > p_tagnode->psz_attrs && p[-1] == '.' && !isalnum(p[i_len]) )
                return true;
        }
    }
    return false;
}

static bool webvtt_domnode_Match_Id( const webvtt_dom_node_t *p_node, const char *psz_id )
{
    if( !psz_id )
        return false;
    if( *psz_id == '#' )
        psz_id++;
    if( p_node->type == NODE_REGION )
        return ((webvtt_region_t *)p_node)->psz_id &&
                !strcmp( ((webvtt_region_t *)p_node)->psz_id, psz_id );
    else if( p_node->type == NODE_CUE )
        return ((webvtt_dom_cue_t *)p_node)->psz_id &&
                !strcmp( ((webvtt_dom_cue_t *)p_node)->psz_id, psz_id );
    return false;
}

static bool webvtt_domnode_Match_Tag( const webvtt_dom_node_t *p_node, const char *psz_tag )
{
    if( p_node->type == NODE_TAG && psz_tag )
    {
        /* special case, not allowed to match anywhere but root */
        if( !strcmp(psz_tag, "video") && p_node->p_parent )
            return false;
        return ((webvtt_dom_tag_t *)p_node)->psz_tag &&
                !strcmp( ((webvtt_dom_tag_t *)p_node)->psz_tag, psz_tag );
    }
    else return false;
}

static bool webvtt_domnode_Match_PseudoClass( const webvtt_dom_node_t *p_node, const char *psz,
                                              vlc_tick_t i_playbacktime )
{
    if( !strcmp(psz, "past") || !strcmp(psz, "future") )
    {
        vlc_tick_t i_start = webvtt_domnode_GetPlaybackTime( p_node, false );
        return ( *psz == 'p' ) ? i_start < i_playbacktime : i_start > i_playbacktime;
    }
    return false;
}

static bool webvtt_domnode_Match_PseudoElement( const webvtt_dom_node_t *p_node, const char *psz )
{
    if( !strcmp(psz, "cue") )
        return p_node->type == NODE_CUE;
    else if( !strcmp(psz, "cue-region") )
        return p_node->type == NODE_REGION;
    return false;
}

static bool MatchAttribute( const char *psz_attr, const char *psz_lookup, enum vlc_css_match_e match )
{
    switch( match )
    {
        case MATCH_EQUALS:
            return !strcmp( psz_attr, psz_lookup );
        case MATCH_INCLUDES:
        {
            const char *p = strstr( psz_attr, psz_lookup );
            if( p && ( p == psz_attr || isspace(p[-1]) ) )
            {
                const char *end = p + strlen( psz_lookup );
                return (*end == 0 || isspace(*end));
            }
            break;
        }
        case MATCH_DASHMATCH:
        {
            size_t i_len = strlen(psz_lookup);
            if( !strncmp( psz_attr, psz_lookup, i_len ) )
            {
                const char *end = psz_attr + i_len;
                return (*end == 0 || !isalnum(*end) );
            }
            break;
        }
        case MATCH_BEGINSWITH:
            return !strncmp( psz_attr, psz_lookup, strlen(psz_lookup) );
        case MATCH_ENDSWITH:
        {
            const char *p = strstr( psz_attr, psz_lookup );
            return (p && *p && p[1] == 0);
        }
        case MATCH_CONTAINS:
            return !!strstr( psz_attr, psz_lookup );
        default:
            break;
    }
    return false;
}

static bool webvtt_domnode_Match_Attribute( const webvtt_dom_node_t *p_node,
                                            const char *psz, const vlc_css_selector_t *p_matchsel )
{
    if( p_node->type == NODE_TAG && p_matchsel )
    {
        const webvtt_dom_tag_t *p_tagnode = (webvtt_dom_tag_t *) p_node;

        if( ( !strcmp( p_tagnode->psz_tag, "v" ) && !strcmp( psz, "voice" ) ) || /* v = only voice */
            ( !strcmp( p_tagnode->psz_tag, "lang" ) && !strcmp( psz, "lang" ) ) )
        {
            const char *psz_start = NULL;
            /* skip classes decl */
            for( const char *p = p_tagnode->psz_attrs; *p; p++ )
            {
                if( isspace(*p) )
                {
                    psz_start = p + 1;
                }
                else if( psz_start != NULL )
                {
                    break;
                }
            }

            if( psz_start == NULL || *psz_start == 0 )
                psz_start = p_tagnode->psz_attrs;

            if( !p_matchsel ) /* attribute check only */
                return strlen( psz_start ) > 0;

            return MatchAttribute( psz_start, p_matchsel->psz_name, p_matchsel->match );
        }
    }
    return false;
}

static bool webvtt_domnode_MatchType( const webvtt_dom_node_t *p_node,
                                      const vlc_css_selector_t *p_sel, vlc_tick_t i_playbacktime )
{
    switch( p_sel->type )
    {
        case SELECTOR_SIMPLE:
            return webvtt_domnode_Match_Tag( p_node, p_sel->psz_name );
        case SELECTOR_PSEUDOCLASS:
            return webvtt_domnode_Match_PseudoClass( p_node, p_sel->psz_name,
                                                     i_playbacktime );
        case SELECTOR_PSEUDOELEMENT:
            return webvtt_domnode_Match_PseudoElement( p_node, p_sel->psz_name );
        case SPECIFIER_ID:
            return webvtt_domnode_Match_Id( p_node, p_sel->psz_name );
        case SPECIFIER_CLASS:
            return webvtt_domnode_Match_Class( p_node, p_sel->psz_name );
        case SPECIFIER_ATTRIB:
            return webvtt_domnode_Match_Attribute( p_node, p_sel->psz_name, p_sel->p_matchsel );
    }
    return false;
}
#endif

static text_style_t ** get_ppCSSStyle( webvtt_dom_node_t *p_node )
{
    switch( p_node->type )
    {
        case NODE_CUE:
            return &((webvtt_dom_cue_t *)p_node)->p_cssstyle;
        case NODE_REGION:
            return &((webvtt_region_t *)p_node)->p_cssstyle;
        case NODE_TAG:
            return &((webvtt_dom_tag_t *)p_node)->p_cssstyle;
        default:
            return NULL;
    }
}

static text_style_t * webvtt_domnode_getCSSStyle( webvtt_dom_node_t *p_node )
{
    text_style_t **pp_style = get_ppCSSStyle( p_node );
    if( pp_style )
        return *pp_style;
    return NULL;
}
#define webvtt_domnode_getCSSStyle(a) webvtt_domnode_getCSSStyle((webvtt_dom_node_t *)a)

static bool webvtt_domnode_supportsCSSStyle( webvtt_dom_node_t *p_node )
{
    return get_ppCSSStyle( p_node ) != NULL;
}

static void webvtt_domnode_setCSSStyle( webvtt_dom_node_t *p_node, text_style_t *p_style )
{
    text_style_t **pp_style = get_ppCSSStyle( p_node );
    if( !pp_style )
    {
        assert( pp_style );
        if( p_style )
            text_style_Delete( p_style );
        return;
    }
    if( *pp_style )
        text_style_Delete( *pp_style );
    *pp_style = p_style;
}

#ifdef HAVE_CSS
static void webvtt_domnode_SelectNodesInTree( const webvtt_dom_node_t *p_tree,
                                              const vlc_css_selector_t *p_sel, int i_max_depth,
                                              vlc_tick_t i_playbacktime, vlc_array_t *p_results );

static void webvtt_domnode_SelectChildNodesInTree( const webvtt_dom_node_t *p_tree,
                                                   const vlc_css_selector_t *p_sel, int i_max_depth,
                                                   vlc_tick_t i_playbacktime, vlc_array_t *p_results )
{
    const webvtt_dom_node_t *p_child = webvtt_domnode_getFirstChild( p_tree );
    if( i_max_depth > 0 )
    {
        for( ; p_child; p_child = p_child->p_next )
            webvtt_domnode_SelectNodesInTree( p_child, p_sel, i_max_depth - 1,
                                              i_playbacktime, p_results );
    }
}

static void webvtt_domnode_SelectNodesBySpeficier( const webvtt_dom_node_t *p_node,
                                                   const vlc_css_selector_t *p_spec,
                                                   vlc_tick_t i_playbacktime, vlc_array_t *p_results )
{
    if( p_spec == NULL )
        return;

    switch( p_spec->combinator )
    {
        case RELATION_DESCENDENT:
            webvtt_domnode_SelectChildNodesInTree( p_node, p_spec, WEBVTT_MAX_DEPTH,
                                                   i_playbacktime, p_results );
            break;
        case RELATION_DIRECTADJACENT:
            for( const webvtt_dom_node_t *p_adj = p_node->p_next; p_adj; p_adj = p_adj->p_next )
                webvtt_domnode_SelectChildNodesInTree( p_adj, p_spec, 1,
                                                       i_playbacktime, p_results );
            break;
        case RELATION_INDIRECTADJACENT:
            for( const webvtt_dom_node_t *p_adj = webvtt_domnode_getFirstChild( p_node->p_parent );
                                          p_adj && p_adj != p_node; p_adj = p_adj->p_next )
                webvtt_domnode_SelectChildNodesInTree( p_adj, p_spec, 1,
                                                       i_playbacktime, p_results );
            break;
        case RELATION_CHILD:
            webvtt_domnode_SelectChildNodesInTree( p_node, p_spec, 1,
                                                   i_playbacktime, p_results );
            break;
        case RELATION_SELF:
            webvtt_domnode_SelectNodesInTree( p_node, p_spec, WEBVTT_MAX_DEPTH,
                                              i_playbacktime, p_results );
    }
}

static void webvtt_domnode_SelectNodesInTree( const webvtt_dom_node_t *p_node,
                                              const vlc_css_selector_t *p_sel, int i_max_depth,
                                              vlc_tick_t i_playbacktime, vlc_array_t *p_results )
{
    if( p_node == NULL )
        return;

    if( webvtt_domnode_MatchType( p_node, p_sel, i_playbacktime ) )
    {
        if( p_sel->specifiers.p_first == NULL )
        {
            /* End of matching, this node is part of results */
            (void) vlc_array_append( p_results, (void *) p_node );
        }
        else webvtt_domnode_SelectNodesBySpeficier( p_node, p_sel->specifiers.p_first,
                                                    i_playbacktime, p_results );
    }

    /* lookup other subnodes */
    webvtt_domnode_SelectChildNodesInTree( p_node, p_sel, i_max_depth - 1,
                                           i_playbacktime, p_results );
}

static void webvtt_domnode_SelectRuleNodes( const webvtt_dom_node_t *p_root, const vlc_css_rule_t *p_rule,
                                            vlc_tick_t i_playbacktime, vlc_array_t *p_results )
{
    if(!p_root || p_root->type != NODE_TAG)
        return;
    const webvtt_dom_node_t *p_cues = ((const webvtt_dom_tag_t *)p_root)->p_child;
    for( const vlc_css_selector_t *p_sel = p_rule->p_selectors; p_sel; p_sel = p_sel->p_next )
    {
        vlc_array_t tempresults;
        vlc_array_init( &tempresults );
        for( const webvtt_dom_node_t *p_node = p_cues; p_node; p_node = p_node->p_next )
        {
            webvtt_domnode_SelectNodesInTree( p_node, p_sel, WEBVTT_MAX_DEPTH,
                                              i_playbacktime, &tempresults );
        }
        for( size_t i=0; i<vlc_array_count(&tempresults); i++ )
            (void) vlc_array_append( p_results, vlc_array_item_at_index( &tempresults, i ) );
        vlc_array_clear( &tempresults );
    }
}
#endif

static inline bool IsEndTag( const char *psz )
{
    return psz[1] == '/';
}

/* returns first opening and last chars of next tag, only when valid */
static const char * FindNextTag( const char *psz, const char **ppsz_taglast )
{
    psz = strchr( psz, '<' );
    if( psz )
    {
        *ppsz_taglast = strchr( psz + 1, '>' );
        if( *ppsz_taglast )
        {
            const size_t tagsize = *ppsz_taglast - psz + 1;
            if( tagsize <= 3 )
            {
                if( tagsize < 2 || IsEndTag(psz) )
                    *ppsz_taglast = psz = NULL;
            }
        } else psz = NULL;
    }
    return psz;
}

/* Points to first char of tag name and sets *ppsz_attrs to attributes */
static const char *SplitTag( const char *psz_tag, size_t *pi_tag, const char **ppsz_attrs )
{
    psz_tag += IsEndTag( psz_tag ) ? 2 : 1;
    const char *p = psz_tag;
    *pi_tag = 0;
    if( isalpha( *p ) )
    {
        while( isalnum( *p ) )
        {
            p++;
            (*pi_tag)++;
        }
        while( isspace( *p ) )
            p++;
    }
    *ppsz_attrs = p;
    return psz_tag;
}

/*****************************************************************************
 *
 *****************************************************************************/
static webvtt_dom_cue_t * webvtt_dom_cue_New( vlc_tick_t i_start, vlc_tick_t i_end )
{
    webvtt_dom_cue_t *p_cue = calloc( 1, sizeof(*p_cue) );
    if( p_cue )
    {
        p_cue->type = NODE_CUE;
        p_cue->psz_id = NULL;
        p_cue->i_start = i_start;
        p_cue->i_stop = i_end;
        p_cue->p_child = NULL;
        p_cue->i_lines = 0;
        p_cue->p_cssstyle = NULL;
        webvtt_cue_settings_Init( &p_cue->settings );
    }
    return p_cue;
}

static void webvtt_dom_cue_ClearText( webvtt_dom_cue_t *p_cue )
{
    webvtt_domnode_ChainDelete( p_cue->p_child );
    p_cue->p_child = NULL;
    p_cue->i_lines = 0;
}

static void webvtt_dom_cue_Delete( webvtt_dom_cue_t *p_cue )
{
    text_style_Delete( p_cue->p_cssstyle );
    webvtt_dom_cue_ClearText( p_cue );
    webvtt_cue_settings_Clean( &p_cue->settings );
    free( p_cue->psz_id );
    free( p_cue );
}

/* reduces by one line */
static unsigned webvtt_dom_cue_Reduced( webvtt_dom_cue_t *p_cue )
{
    if( p_cue->i_lines < 1 )
        return 0;

    for( webvtt_dom_node_t *p_node = p_cue->p_child;
                           p_node; p_node = p_node->p_next )
    {
        if( p_node->type != NODE_TEXT )
            continue;
        webvtt_dom_text_t *p_textnode = (webvtt_dom_text_t *) p_node;
        const char *nl = strchr( p_textnode->psz_text, '\n' );
        if( nl )
        {
            size_t i_len = strlen( p_textnode->psz_text );
            size_t i_remain = i_len - (nl - p_textnode->psz_text);
            char *psz_new = strndup( nl + 1, i_remain );
            free( p_textnode->psz_text );
            p_textnode->psz_text = psz_new;
            return --p_cue->i_lines;
        }
        else
        {
            free( p_textnode->psz_text );
            p_textnode->psz_text = NULL;
            /* FIXME: probably can do a local nodes cleanup */
        }
    }

    return p_cue->i_lines;
}

/*****************************************************************************
 *
 *****************************************************************************/

static void webvtt_region_ParseTuple( webvtt_region_t *p_region,
                                      const char *psz_key, const char *psz_value )
{
    if( !strcmp( psz_key, "id" ) )
    {
        free( p_region->psz_id );
        p_region->psz_id = strdup( psz_value );
    }
    else if( !strcmp( psz_key, "width" ) )
    {
        parse_percent( psz_value, &p_region->f_width );
    }
    else if( !strcmp( psz_key, "regionanchor" ) )
    {
        parse_percent_tuple( psz_value, &p_region->anchor_x,
                                        &p_region->anchor_y );
    }
    else if( !strcmp( psz_key, "viewportanchor" ) )
    {
        parse_percent_tuple( psz_value, &p_region->viewport_anchor_x,
                                        &p_region->viewport_anchor_y );
    }
    else if( !strcmp( psz_key, "lines" ) )
    {
        int i = atoi( psz_value );
        if( i > 0 )
            p_region->i_lines_max_scroll = __MIN(i, WEBVTT_REGION_LINES_COUNT);
    }
    else if( !strcmp( psz_key, "scroll" ) )
    {
        p_region->b_scroll_up = !strcmp( psz_value, "up" );
    }
}

static void webvtt_region_Parse( webvtt_region_t *p_region, char *psz_line )
{
    char *p_save;
    char *psz_tuple;
    char *p_str = psz_line;
    do
    {
        psz_tuple = strtok_r( p_str, " ", &p_save );
        p_str = NULL;
        if( psz_tuple )
        {
            const char *psz_split = strchr( psz_tuple, ':' );
            if( psz_split && psz_split[1] != 0 && psz_split != psz_tuple )
            {
                char *psz_key = strndup( psz_tuple, psz_split - psz_tuple );
                if( psz_key )
                {
                    webvtt_region_ParseTuple( p_region, psz_key, psz_split + 1 );
                    free( psz_key );
                }
            }
        }
    } while( psz_tuple );
}

static unsigned webvtt_region_CountLines( const webvtt_region_t *p_region )
{
    unsigned i_lines = 0;
    for( const webvtt_dom_node_t *p_node = p_region->p_child;
                                  p_node; p_node = p_node->p_next )
    {
        assert( p_node->type == NODE_CUE );
        if( p_node->type != NODE_CUE )
            continue;
        i_lines += ((const webvtt_dom_cue_t *)p_node)->i_lines;
    }
    return i_lines;
}

static void webvtt_region_ClearCues( webvtt_region_t *p_region )
{
    webvtt_domnode_ChainDelete( p_region->p_child );
    p_region->p_child = NULL;
}

static void ClearCuesByTime( webvtt_dom_node_t **pp_next, vlc_tick_t i_time )
{
    while( *pp_next )
    {
        webvtt_dom_node_t *p_node = *pp_next;
        if( p_node )
        {
            if( p_node->type == NODE_CUE )
            {
                webvtt_dom_cue_t *p_cue = (webvtt_dom_cue_t *)p_node;
                if( p_cue->i_stop <= i_time )
                {
                    *pp_next = p_node->p_next;
                    p_node->p_next = NULL;
                    webvtt_dom_cue_Delete( p_cue );
                    continue;
                }
            }
            else if( p_node->type == NODE_REGION )
            {
                webvtt_region_t *p_region = (webvtt_region_t *) p_node;
                ClearCuesByTime( &p_region->p_child, i_time );
            }
            pp_next = &p_node->p_next;
        }
    }
}

/* Remove top most line/cue for bottom insert */
static void webvtt_region_Reduce( webvtt_region_t *p_region )
{
    if( p_region->p_child )
    {
        assert( p_region->p_child->type == NODE_CUE );
        if( p_region->p_child->type != NODE_CUE )
            return;
        webvtt_dom_cue_t *p_cue = (webvtt_dom_cue_t *)p_region->p_child;
        if( p_cue->i_lines == 1 ||
            webvtt_dom_cue_Reduced( p_cue ) < 1 )
        {
            p_region->p_child = p_cue->p_next;
            p_cue->p_next = NULL;
            webvtt_dom_cue_Delete( p_cue );
        }
    }
}

static void webvtt_region_AddCue( webvtt_region_t *p_region,
                                  webvtt_dom_cue_t *p_cue )
{
    webvtt_dom_node_t **pp_add = &p_region->p_child;
    while( *pp_add )
        pp_add = &((*pp_add)->p_next);
    *pp_add = (webvtt_dom_node_t *)p_cue;
    p_cue->p_parent = (webvtt_dom_node_t *)p_region;

    for( ;; )
    {
        unsigned i_lines = webvtt_region_CountLines( p_region );
        if( i_lines > 0 &&
            ( i_lines > WEBVTT_REGION_LINES_COUNT ||
             (p_region->b_scroll_up && i_lines > p_region->i_lines_max_scroll)) )
        {
            webvtt_region_Reduce( p_region ); /* scrolls up */
            assert( webvtt_region_CountLines( p_region ) < i_lines );
        }
        else break;
    }
}

static void webvtt_region_Delete( webvtt_region_t *p_region )
{
    text_style_Delete( p_region->p_cssstyle );
    webvtt_region_ClearCues( p_region );
    free( p_region->psz_id );
    free( p_region );
}

static webvtt_region_t * webvtt_region_New( void )
{
    webvtt_region_t *p_region = malloc(sizeof(*p_region));
    if( p_region )
    {
        p_region->type = NODE_REGION;
        p_region->psz_id = NULL;
        p_region->p_next = NULL;
        p_region->f_width = 1.0; /* 100% */
        p_region->anchor_x = 0;
        p_region->anchor_y = 1.0; /* 100% */
        p_region->i_lines_max_scroll = 3;
        p_region->viewport_anchor_x = 0;
        p_region->viewport_anchor_y = 1.0; /* 100% */
        p_region->b_scroll_up = false;
        p_region->p_cssstyle = NULL;
        p_region->p_child = NULL;
    }
    return p_region;
}

static webvtt_region_t * webvtt_region_GetByID( decoder_sys_t *p_sys,
                                                const char *psz_id )
{
    if( !psz_id )
        return NULL;
    for( webvtt_dom_node_t *p_node = p_sys->p_root->p_child;
                            p_node; p_node = p_node->p_next )
    {
        if( p_node->type == NODE_REGION )
        {
            webvtt_region_t *p_region = (webvtt_region_t *) p_node;
            if( p_region->psz_id && !strcmp( psz_id, p_region->psz_id ) )
                return p_region;
        }
    }
    return NULL;
}

/*****************************************************************************
 *
 *****************************************************************************/
static unsigned CountNewLines( const char *psz )
{
    unsigned i = 0;
    while( psz && *psz )
        psz = strchr( psz + 1, '\n' );
    return i;
}

static webvtt_dom_node_t * CreateDomNodes( const char *psz_text, unsigned *pi_lines )
{
    webvtt_dom_node_t *p_head = NULL;
    webvtt_dom_node_t **pp_append = &p_head;
    webvtt_dom_node_t *p_parent = p_head;
    *pi_lines = 0;

    while( *psz_text )
    {
        const char *psz_taglast;
        const char *psz_tag = FindNextTag( psz_text, &psz_taglast );
        if( psz_tag )
        {
            if( psz_tag - psz_text > 0 )
            {
                webvtt_dom_text_t *p_node = webvtt_dom_text_New( p_parent );
                if( p_node )
                {
                    p_node->psz_text = strndup( psz_text, psz_tag - psz_text );
                    *pi_lines += ((*pi_lines == 0) ? 1 : 0) + CountNewLines( p_node->psz_text );
                    *pp_append = (webvtt_dom_node_t *) p_node;
                    pp_append = &p_node->p_next;
                }
            }

            if( ! IsEndTag( psz_tag ) )
            {
                webvtt_dom_tag_t *p_node = webvtt_dom_tag_New( p_parent );
                if( p_node )
                {
                    const char *psz_attrs = NULL;
                    size_t i_name;
                    const char *psz_name = SplitTag( psz_tag, &i_name, &psz_attrs );
                    p_node->psz_tag = strndup( psz_name, i_name );
                    if( psz_attrs != psz_taglast )
                        p_node->psz_attrs = strndup( psz_attrs, psz_taglast - psz_attrs );
                    /* <hh:mm::ss:fff> time tags */
                    if( p_node->psz_attrs && isdigit(p_node->psz_attrs[0]) )
                        (void) webvtt_scan_time( p_node->psz_attrs, &p_node->i_start );
                    *pp_append = (webvtt_dom_node_t *) p_node;
                    p_parent = (webvtt_dom_node_t *) p_node;
                    pp_append = &p_node->p_child;
                }
            }
            else
            {
                if( p_parent )
                {
                    const char *psz_attrs = NULL;
                    size_t i_name;
                    const char *psz_name = SplitTag( psz_tag, &i_name, &psz_attrs );
                    char *psz_tagname = strndup( psz_name, i_name );

                    /* Close at matched parent node level due to unclosed tags
                     * like <b><v stuff>foo</b> */
                    p_parent = webvtt_domnode_getParentByTag( p_parent, psz_tagname );
                    if( p_parent ) /* continue as parent next */
                    {
                        pp_append = &p_parent->p_next;
                        p_parent = p_parent->p_parent;
                    }
                    else /* back as top node */
                        pp_append = &p_head->p_next;
                    while( *pp_append )
                        pp_append = &((*pp_append)->p_next);

                    free( psz_tagname );
                }
                else break; /* End tag for non open tag */
            }
            psz_text = psz_taglast + 1;
        }
        else /* Special case: end */
        {
            webvtt_dom_text_t *p_node = webvtt_dom_text_New( p_parent );
            if( p_node )
            {
                p_node->psz_text = strdup( psz_text );
                *pi_lines += ((*pi_lines == 0) ? 1 : 0) + CountNewLines( p_node->psz_text );
                *pp_append = (webvtt_dom_node_t *) p_node;
            }
            break;
        }
    }

    return p_head;
}

static void ProcessCue( decoder_t *p_dec, const char *psz, webvtt_dom_cue_t *p_cue )
{
    VLC_UNUSED(p_dec);

    if( p_cue->p_child )
        return;
    p_cue->p_child = CreateDomNodes( psz, &p_cue->i_lines );
    for( webvtt_dom_node_t *p_child = p_cue->p_child; p_child; p_child = p_child->p_next )
        p_child->p_parent = (webvtt_dom_node_t *)p_cue;
#ifdef SUBSVTT_DEBUG
    webvtt_domnode_Debug( (webvtt_dom_node_t *) p_cue, 0 );
#endif
}

static text_style_t * ComputeStyle( decoder_t *p_dec, const webvtt_dom_node_t *p_leaf )
{
    VLC_UNUSED(p_dec);
    text_style_t *p_style = NULL;
    text_style_t *p_dfltstyle = NULL;
    vlc_tick_t i_tagtime = -1;

    for( const webvtt_dom_node_t *p_node = p_leaf ; p_node; p_node = p_node->p_parent )
    {
        bool b_nooverride = false;
        if( p_node->type == NODE_CUE )
        {
            const webvtt_dom_cue_t *p_cue = (const webvtt_dom_cue_t *)p_node;
            if( p_cue )
            {
                if( i_tagtime > -1 ) /* don't override timed stylings */
                    b_nooverride = true;
            }
        }
        else if( p_node->type == NODE_TAG )
        {
            const webvtt_dom_tag_t *p_tagnode = (const webvtt_dom_tag_t *)p_node;

            if( p_tagnode->i_start > -1 )
            {
                /* Ignore other timed stylings */
                if( i_tagtime == -1 )
                    i_tagtime = p_tagnode->i_start;
                else
                    continue;
            }

            if ( p_tagnode->psz_tag )
            {
                if ( !strcmp( p_tagnode->psz_tag, "b" ) )
                {
                    if( p_style || (p_style = text_style_Create( STYLE_NO_DEFAULTS )) )
                    {
                        p_style->i_style_flags |= STYLE_BOLD;
                        p_style->i_features |= STYLE_HAS_FLAGS;
                    }
                }
                else if ( !strcmp( p_tagnode->psz_tag, "i" ) )
                {
                    if( p_style || (p_style = text_style_Create( STYLE_NO_DEFAULTS )) )
                    {
                        p_style->i_style_flags |= STYLE_ITALIC;
                        p_style->i_features |= STYLE_HAS_FLAGS;
                    }
                }
                else if ( !strcmp( p_tagnode->psz_tag, "u" ) )
                {
                    if( p_style || (p_style = text_style_Create( STYLE_NO_DEFAULTS )) )
                    {
                        p_style->i_style_flags |= STYLE_UNDERLINE;
                        p_style->i_features |= STYLE_HAS_FLAGS;
                    }
                }
                else if ( !strcmp( p_tagnode->psz_tag, "v" ) && p_tagnode->psz_attrs )
                {
#ifdef HAVE_CSS
                    decoder_sys_t *p_sys = p_dec->p_sys;
                    if( p_sys->p_css_rules == NULL ) /* Only auto style when no CSS sheet */
#endif
                    {
                        if( p_style || (p_style = text_style_Create( STYLE_NO_DEFAULTS )) )
                        {
                            unsigned a = 0;
                            for( char *p = p_tagnode->psz_attrs; *p; p++ )
                                a = (a << 3) ^ *p;
                            p_style->i_font_color = (0x7F7F7F | a) & 0xFFFFFF;
                            p_style->i_features |= STYLE_HAS_FONT_COLOR;
                        }
                    }
                }
                else if( !strcmp( p_tagnode->psz_tag, "c" ) && p_tagnode->psz_attrs )
                {
                    static const struct
                    {
                        const char *psz;
                        uint32_t i_color;
                    } CEAcolors[] = {
                        { "white",  0xFFFFFF },
                        { "lime",   0x00FF00 },
                        { "cyan",   0x00FFFF },
                        { "red",    0xFF0000 },
                        { "yellow", 0xFFFF00 },
                        { "magenta",0xFF00FF },
                        { "blue",   0x0000FF },
                        { "black",  0x000000 },
                    };
                    char *saveptr = NULL;
                    char *psz_tok = strtok_r( p_tagnode->psz_attrs, ".", &saveptr );
                    for( ; psz_tok; psz_tok = strtok_r( NULL, ".", &saveptr ) )
                    {
                        bool bg = !strncmp( psz_tok, "bg_", 3 );
                        const char *psz_class = (bg) ? psz_tok + 3 : psz_tok;
                        for( size_t i=0; i<ARRAY_SIZE(CEAcolors); i++ )
                        {
                            if( strcmp( psz_class, CEAcolors[i].psz ) )
                                continue;
                            if( p_dfltstyle ||
                               (p_dfltstyle = text_style_Create( STYLE_NO_DEFAULTS )) )
                            {
                                if( bg )
                                {
                                    p_dfltstyle->i_background_color = CEAcolors[i].i_color;
                                    p_dfltstyle->i_background_alpha = STYLE_ALPHA_OPAQUE;
                                    p_dfltstyle->i_features |= STYLE_HAS_BACKGROUND_COLOR |
                                                               STYLE_HAS_BACKGROUND_ALPHA;
                                    p_dfltstyle->i_style_flags |= STYLE_BACKGROUND;
                                    p_dfltstyle->i_features |= STYLE_HAS_FLAGS;
                                }
                                else
                                {
                                    p_dfltstyle->i_font_color = CEAcolors[i].i_color;
                                    p_dfltstyle->i_features |= STYLE_HAS_FONT_COLOR;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        const text_style_t *p_nodestyle = webvtt_domnode_getCSSStyle( p_node );
        if( p_nodestyle )
        {
            if( p_style )
                text_style_Merge( p_style, p_nodestyle, false );
            else if( !b_nooverride )
                p_style = text_style_Duplicate( p_nodestyle );
        }

        /* Default classes */
        if( p_dfltstyle )
        {
            if( p_style )
            {
                text_style_Merge( p_style, p_dfltstyle, false );
                text_style_Delete( p_dfltstyle );
            }
            else p_style = p_dfltstyle;
            p_dfltstyle = NULL;
        }
    }

    return p_style;
}

static int GetCueTextAlignment( const webvtt_dom_cue_t *p_cue )
{
    switch( p_cue->settings.align )
    {
        case WEBVTT_ALIGN_LEFT:
            return SUBPICTURE_ALIGN_LEFT;
        case WEBVTT_ALIGN_RIGHT:
            return SUBPICTURE_ALIGN_RIGHT;
        case WEBVTT_ALIGN_START: /* vertical provides rl or rl base direction */
            return (p_cue->settings.vertical == WEBVTT_ALIGN_RIGHT) ?
                     SUBPICTURE_ALIGN_RIGHT : SUBPICTURE_ALIGN_LEFT;
        case WEBVTT_ALIGN_END:
            return (p_cue->settings.vertical == WEBVTT_ALIGN_RIGHT) ?
                     SUBPICTURE_ALIGN_LEFT : SUBPICTURE_ALIGN_RIGHT;
        default:
            return 0;
    }
}

struct render_variables_s
{
    const webvtt_region_t *p_region;
    float i_left_offset;
    float i_left;
    float i_top_offset;
    float i_top;
};

static text_segment_t *ConvertRubyNodeToSegment( const webvtt_dom_node_t *p_node )
{
    text_segment_ruby_t *p_ruby = NULL;
    text_segment_ruby_t **pp_rt_append = &p_ruby;

    const char *psz_base = NULL;

    for( ; p_node ; p_node = p_node->p_next )
    {
        if( p_node->type == NODE_TEXT )
        {
            const webvtt_dom_text_t *p_textnode = (const webvtt_dom_text_t *) p_node;
            psz_base = p_textnode->psz_text;
        }
        else if( p_node->type == NODE_TAG )
        {
            const webvtt_dom_tag_t *p_tag = (const webvtt_dom_tag_t *)p_node;
            if( !strcmp(p_tag->psz_tag, "rt") && p_tag->p_child &&
                p_tag->p_child->type == NODE_TEXT )
            {
                const webvtt_dom_text_t *p_rttext = (const webvtt_dom_text_t *)p_tag->p_child;
                *pp_rt_append = text_segment_ruby_New( psz_base, p_rttext->psz_text );
                if( *pp_rt_append )
                    pp_rt_append = &(*pp_rt_append)->p_next;
            }
            psz_base = NULL;
        }
    }

    return ( p_ruby ) ? text_segment_FromRuby( p_ruby ) : NULL;
}

static text_segment_t *ConvertNodesToSegments( decoder_t *p_dec,
                                               struct render_variables_s *p_vars,
                                               const webvtt_dom_cue_t *p_cue,
                                               const webvtt_dom_node_t *p_node )
{
    text_segment_t *p_head = NULL;
    text_segment_t **pp_append = &p_head;
    for( ; p_node ; p_node = p_node->p_next )
    {
        while( *pp_append )
            pp_append = &((*pp_append)->p_next);

        if( p_node->type == NODE_TEXT )
        {
            const webvtt_dom_text_t *p_textnode = (const webvtt_dom_text_t *) p_node;
            if( p_textnode->psz_text == NULL )
                continue;

            *pp_append = text_segment_New( p_textnode->psz_text );
            if( *pp_append )
            {
                if( (*pp_append)->psz_text )
                    vlc_xml_decode( (*pp_append)->psz_text );
                (*pp_append)->style = ComputeStyle( p_dec, p_node );
            }
        }
        else if( p_node->type == NODE_TAG )
        {
            const webvtt_dom_tag_t *p_tag = (const webvtt_dom_tag_t *)p_node;
            if( strcmp(p_tag->psz_tag, "ruby") )
                *pp_append = ConvertNodesToSegments( p_dec, p_vars, p_cue,
                                                     p_tag->p_child );
            else
                *pp_append = ConvertRubyNodeToSegment( p_tag->p_child );
        }
    }
    return p_head;
}

static text_segment_t *ConvertCueToSegments( decoder_t *p_dec,
                                             struct render_variables_s *p_vars,
                                             const webvtt_dom_cue_t *p_cue )
{
    return ConvertNodesToSegments( p_dec, p_vars, p_cue, p_cue->p_child );
}

static void ChainCueSegments( const webvtt_dom_cue_t *p_cue, text_segment_t *p_new,
                              text_segment_t **pp_append )
{
    if( p_new )
    {
        bool b_newline = *pp_append;

        while( *pp_append )
            pp_append = &((*pp_append)->p_next);

        if( b_newline ) /* auto newlines */
        {
            *pp_append = text_segment_New( "\n" );
            if( *pp_append )
                pp_append = &((*pp_append)->p_next);
        }

        if( p_cue->settings.vertical == WEBVTT_ALIGN_LEFT ) /* LTR */
        {
            *pp_append = text_segment_New( "\u2067" );
            if( *pp_append )
                pp_append = &((*pp_append)->p_next);
        }

        *pp_append = p_new;

        if( p_cue->settings.vertical == WEBVTT_ALIGN_LEFT )
        {
            *pp_append = text_segment_New( "\u2069" );
            if( *pp_append )
                pp_append = &((*pp_append)->p_next);
        }
    }
}

static text_segment_t * ConvertCuesToSegments( decoder_t *p_dec, vlc_tick_t i_start, vlc_tick_t i_stop,
                                               struct render_variables_s *p_vars,
                                               const webvtt_dom_cue_t *p_cue )
{
    text_segment_t *p_segments = NULL;
    text_segment_t **pp_append = &p_segments;
    VLC_UNUSED(i_stop);

    for( ; p_cue; p_cue = (const webvtt_dom_cue_t *) p_cue->p_next )
    {
        if( p_cue->type != NODE_CUE )
            continue;

        if( p_cue->i_start > i_start || p_cue->i_stop <= i_start )
            continue;

        text_segment_t *p_new = ConvertCueToSegments( p_dec, p_vars, p_cue );
        ChainCueSegments( p_cue, p_new, pp_append );
    }
    return p_segments;
}

static void GetTimedTags( const webvtt_dom_node_t *p_node,
                           vlc_tick_t i_start, vlc_tick_t i_stop, vlc_array_t *p_times )
{
    for( ; p_node; p_node = p_node->p_next )
    {
        switch( p_node->type )
        {
            case NODE_TAG:
            {
                const webvtt_dom_tag_t *p_tag = (const webvtt_dom_tag_t *) p_node;
                if( p_tag->i_start > -1 && p_tag->i_start >= i_start && p_tag->i_start < i_stop )
                    (void) vlc_array_append( p_times, (void *) p_tag );
                GetTimedTags( p_tag->p_child, i_start, i_stop, p_times );
            } break;
            case NODE_REGION:
            case NODE_CUE:
                GetTimedTags( webvtt_domnode_getFirstChild( p_node ),
                              i_start, i_stop, p_times );
                break;
            default:
                break;
        }
    }
}

static void CreateSpuOrNewUpdaterRegion( decoder_t *p_dec,
                                         subpicture_t **pp_spu,
                                         substext_updater_region_t **pp_updtregion )
{
    if( *pp_spu == NULL )
    {
        *pp_spu = decoder_NewSubpictureText( p_dec );
        if( *pp_spu )
        {
            subtext_updater_sys_t *p_spusys = (*pp_spu)->updater.p_sys;
            *pp_updtregion = &p_spusys->region;
        }
    }
    else
    {
        substext_updater_region_t *p_new =
                                SubpictureUpdaterSysRegionNew( );
        if( p_new )
        {
            SubpictureUpdaterSysRegionAdd( *pp_updtregion, p_new );
            *pp_updtregion = p_new;
        }
    }
}

static void ClearCSSStyles( webvtt_dom_node_t *p_node )
{
    if( webvtt_domnode_supportsCSSStyle( p_node ) )
        webvtt_domnode_setCSSStyle( p_node, NULL );
    webvtt_dom_node_t *p_child = webvtt_domnode_getFirstChild( p_node );
    for ( ; p_child ; p_child = p_child->p_next )
        ClearCSSStyles( p_child );
}

#ifdef HAVE_CSS
static void ApplyCSSRules( decoder_t *p_dec, const vlc_css_rule_t *p_rule,
                           vlc_tick_t i_playbacktime )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    for ( ;  p_rule ; p_rule = p_rule->p_next )
    {
        vlc_array_t results;
        vlc_array_init( &results );

        webvtt_domnode_SelectRuleNodes( (webvtt_dom_node_t *) p_sys->p_root,
                                        p_rule, i_playbacktime, &results );

        for( const vlc_css_declaration_t *p_decl = p_rule->p_declarations;
                                          p_decl; p_decl = p_decl->p_next )
        {
            for( size_t i=0; i<vlc_array_count(&results); i++ )
            {
                webvtt_dom_node_t *p_node = vlc_array_item_at_index( &results, i );
                if( !webvtt_domnode_supportsCSSStyle( p_node ) )
                    continue;

                text_style_t *p_style = webvtt_domnode_getCSSStyle( p_node );
                if( !p_style )
                {
                    p_style = text_style_Create( STYLE_NO_DEFAULTS );
                    webvtt_domnode_setCSSStyle( p_node, p_style );
                }

                if( !p_style )
                    continue;

                webvtt_FillStyleFromCssDeclaration( p_decl, p_style );
            }
        }
        vlc_array_clear( &results );
    }
}
#endif

static void RenderRegions( decoder_t *p_dec, vlc_tick_t i_start, vlc_tick_t i_stop )
{
    subpicture_t *p_spu = NULL;
    substext_updater_region_t *p_updtregion = NULL;
    decoder_sys_t *p_sys = p_dec->p_sys;

#ifdef HAVE_CSS
    ApplyCSSRules( p_dec, p_sys->p_css_rules, i_start );
#endif

    const webvtt_dom_cue_t *p_rlcue = NULL;
    for( const webvtt_dom_node_t *p_node = p_sys->p_root->p_child;
                                  p_node; p_node = p_node->p_next )
    {
        if( p_node->type == NODE_REGION )
        {
            const webvtt_region_t *p_vttregion = (const webvtt_region_t *) p_node;
            /* Variables */
            struct render_variables_s v;
            v.p_region = p_vttregion;
            v.i_left_offset = p_vttregion->anchor_x * p_vttregion->f_width;
            v.i_left = p_vttregion->viewport_anchor_x - v.i_left_offset;
            v.i_top_offset = p_vttregion->anchor_y * p_vttregion->i_lines_max_scroll *
                             WEBVTT_DEFAULT_LINE_HEIGHT_VH / 100.0;
            v.i_top = p_vttregion->viewport_anchor_y - v.i_top_offset;
            /* !Variables */

            text_segment_t *p_segments =
                    ConvertCuesToSegments( p_dec, i_start, i_stop, &v,
                                          (const webvtt_dom_cue_t *)p_vttregion->p_child );
            if( !p_segments )
                continue;

            CreateSpuOrNewUpdaterRegion( p_dec, &p_spu, &p_updtregion );
            if( !p_spu || !p_updtregion )
            {
                text_segment_ChainDelete( p_segments );
                continue;
            }

            p_updtregion->align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
            p_updtregion->inner_align = GetCueTextAlignment( (const webvtt_dom_cue_t *)p_vttregion->p_child );
            p_updtregion->origin.x = v.i_left;
            p_updtregion->origin.y = v.i_top;
            p_updtregion->extent.x = p_vttregion->f_width;

            p_updtregion->flags = UPDT_REGION_ORIGIN_X_IS_RATIO|UPDT_REGION_ORIGIN_Y_IS_RATIO
                                | UPDT_REGION_EXTENT_X_IS_RATIO;
            p_updtregion->p_segments = p_segments;
        }
        else if ( p_node->type == NODE_CUE )
        {
            if( p_rlcue == NULL )
                p_rlcue = ( const webvtt_dom_cue_t * ) p_node;
        }
    }

    /* regionless cues */
    if ( p_rlcue )
    {
        /* Variables */
        struct render_variables_s v;
        v.p_region = NULL;
        v.i_left_offset = 0.0;
        v.i_left = 0.0;
        v.i_top_offset = 0.0;
        v.i_top = 0.0;
        /* !Variables */

        for( const webvtt_dom_cue_t *p_cue = p_rlcue; p_cue;
             p_cue = (const webvtt_dom_cue_t *) p_cue->p_next )
        {
            if( p_cue->type != NODE_CUE )
                continue;

            if( p_cue->i_start > i_start || p_cue->i_stop <= i_start )
                continue;

            text_segment_t *p_segments = ConvertCueToSegments( p_dec, &v, p_cue );
            if( !p_segments )
                continue;

            CreateSpuOrNewUpdaterRegion( p_dec, &p_spu, &p_updtregion );
            if( !p_updtregion )
            {
                text_segment_ChainDelete( p_segments );
                continue;
            }

            if( p_cue->settings.line.b_auto )
            {
                p_updtregion->align = SUBPICTURE_ALIGN_BOTTOM;
            }
            else
            {
                webvtt_rect_t rect = { 0,0,0,0 };
                webvtt_get_cueboxrect( &p_cue->settings, &rect );
                p_updtregion->align = SUBPICTURE_ALIGN_TOP|SUBPICTURE_ALIGN_LEFT;
                p_updtregion->origin.x = rect.x;
                p_updtregion->origin.y = rect.y;
                p_updtregion->extent.x = rect.w;
                p_updtregion->extent.y = rect.h;
                p_updtregion->flags |= (UPDT_REGION_ORIGIN_X_IS_RATIO|UPDT_REGION_ORIGIN_Y_IS_RATIO|
                                        UPDT_REGION_EXTENT_X_IS_RATIO|UPDT_REGION_EXTENT_Y_IS_RATIO);
            }

            p_updtregion->inner_align = GetCueTextAlignment( p_cue );
            p_updtregion->p_segments = p_segments;
        }
    }

    if( p_spu )
    {
        p_spu->i_start = i_start;
        p_spu->i_stop = i_stop;
        p_spu->b_ephemer  = true; /* !important */
        p_spu->b_absolute = false; /* can't be absolute as snap to lines can overlap ! */

        subtext_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;
        p_spu_sys->p_default_style->f_font_relsize = WEBVTT_DEFAULT_LINE_HEIGHT_VH /
                                                     WEBVTT_LINE_TO_HEIGHT_RATIO;
        decoder_QueueSub( p_dec, p_spu );
    }
}

static int timedtagsArrayCmp( const void *a, const void *b )
{
    const webvtt_dom_tag_t *ta = *((const webvtt_dom_tag_t **) a);
    const webvtt_dom_tag_t *tb = *((const webvtt_dom_tag_t **) b);
    const int64_t result = ta->i_start - tb->i_start;
    return result == 0 ? 0 : result > 0 ? 1 : -1;
}

static void Render( decoder_t *p_dec, vlc_tick_t i_start, vlc_tick_t i_stop )
{
    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_array_t timedtags;
    vlc_array_init( &timedtags );

    GetTimedTags( p_sys->p_root->p_child, i_start, i_stop, &timedtags );
    if( timedtags.i_count )
        qsort( timedtags.pp_elems, timedtags.i_count, sizeof(*timedtags.pp_elems), timedtagsArrayCmp );

    vlc_tick_t i_substart = i_start;
    for( size_t i=0; i<timedtags.i_count; i++ )
    {
         const webvtt_dom_tag_t *p_tag =
                 (const webvtt_dom_tag_t *) vlc_array_item_at_index( &timedtags, i );
         if( p_tag->i_start != i_substart ) /* might be duplicates */
         {
             if( i > 0 )
                 ClearCSSStyles( (webvtt_dom_node_t *)p_sys->p_root );
             RenderRegions( p_dec, i_substart, p_tag->i_start );
             i_substart = p_tag->i_start;
         }
    }
    if( i_substart != i_stop )
    {
        if( i_substart != i_start )
            ClearCSSStyles( (webvtt_dom_node_t *)p_sys->p_root );
        RenderRegions( p_dec, i_substart, i_stop );
    }

    vlc_array_clear( &timedtags );
}

static int ProcessISOBMFF( decoder_t *p_dec,
                           const uint8_t *p_buffer, size_t i_buffer,
                           vlc_tick_t i_start, vlc_tick_t i_stop )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    mp4_box_iterator_t it;
    mp4_box_iterator_Init( &it, p_buffer, i_buffer );
    while( mp4_box_iterator_Next( &it ) )
    {
        if( it.i_type == ATOM_vttc || it.i_type == ATOM_vttx )
        {
            webvtt_dom_cue_t *p_cue = webvtt_dom_cue_New( i_start, i_stop );
            if( !p_cue )
                continue;

            mp4_box_iterator_t vtcc;
            mp4_box_iterator_Init( &vtcc, it.p_payload, it.i_payload );
            while( mp4_box_iterator_Next( &vtcc ) )
            {
                char *psz = NULL;
                switch( vtcc.i_type )
                {
                    case ATOM_iden:
                        free( p_cue->psz_id );
                        p_cue->psz_id = strndup( (char *) vtcc.p_payload, vtcc.i_payload );
                        break;
                    case ATOM_sttg:
                    {
                        psz = strndup( (char *) vtcc.p_payload, vtcc.i_payload );
                        if( psz )
                            webvtt_cue_settings_Parse( &p_cue->settings, psz );
                    } break;
                    case ATOM_payl:
                    {
                        psz = strndup( (char *) vtcc.p_payload, vtcc.i_payload );
                        if( psz )
                            ProcessCue( p_dec, psz, p_cue );
                    } break;
                }
                free( psz );
            }

            webvtt_region_t *p_region = webvtt_region_GetByID( p_sys,
                                                               p_cue->settings.psz_region );
            if( p_region )
            {
                webvtt_region_AddCue( p_region, p_cue );
                assert( p_region->p_child );
            }
            else
            {
                webvtt_domnode_AppendLast( &p_sys->p_root->p_child, p_cue );
                p_cue->p_parent = (webvtt_dom_node_t *) p_sys->p_root;
            }
        }
    }
    return 0;
}

struct parser_ctx
{
    webvtt_region_t *p_region;
#ifdef HAVE_CSS
    struct vlc_memstream css;
    bool b_css_memstream_opened;
#endif
    decoder_t *p_dec;
};

static void ParserHeaderHandler( void *priv, enum webvtt_header_line_e s,
                                 bool b_new, const char *psz_line )
{
    struct parser_ctx *ctx = (struct parser_ctx *)priv;
    decoder_t *p_dec = ctx->p_dec;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( b_new || !psz_line /* commit */ )
    {
        if( ctx->p_region )
        {
            if( ctx->p_region->psz_id )
            {
                webvtt_domnode_AppendLast( &p_sys->p_root->p_child, ctx->p_region );
                ctx->p_region->p_parent = (webvtt_dom_node_t *) p_sys->p_root;
                msg_Dbg( p_dec, "added new region %s", ctx->p_region->psz_id );
            }
            /* incomplete region decl (no id at least) */
            else webvtt_region_Delete( ctx->p_region );
            ctx->p_region = NULL;
        }
#ifdef HAVE_CSS
        else if( ctx->b_css_memstream_opened )
        {
            if( vlc_memstream_close( &ctx->css ) == VLC_SUCCESS )
            {
                vlc_css_parser_t p;
                vlc_css_parser_Init(&p);
                vlc_css_parser_ParseBytes( &p,
                                          (const uint8_t *) ctx->css.ptr,
                                           ctx->css.length );
#  ifdef CSS_PARSER_DEBUG
                vlc_css_parser_Debug( &p );
#  endif
                vlc_css_rule_t **pp_append = &p_sys->p_css_rules;
                while( *pp_append )
                    pp_append = &((*pp_append)->p_next);
                *pp_append = p.rules.p_first;
                p.rules.p_first = NULL;

                vlc_css_parser_Clean(&p);
                free( ctx->css.ptr );
            }
        }
#endif

        if( !psz_line )
            return;

        if( b_new )
        {
            if( s == WEBVTT_HEADER_REGION )
                ctx->p_region = webvtt_region_New();
#ifdef HAVE_CSS
            else if( s == WEBVTT_HEADER_STYLE )
                ctx->b_css_memstream_opened = !vlc_memstream_open( &ctx->css );
#endif
            return;
        }
    }

    if( s == WEBVTT_HEADER_REGION && ctx->p_region )
        webvtt_region_Parse( ctx->p_region, (char*) psz_line );
#ifdef HAVE_CSS
    else if( s == WEBVTT_HEADER_STYLE && ctx->b_css_memstream_opened )
    {
        vlc_memstream_puts( &ctx->css, psz_line );
        vlc_memstream_putc( &ctx->css, '\n' );
    }
#endif
}

static void LoadExtradata( decoder_t *p_dec )
{
    stream_t *p_stream = vlc_stream_MemoryNew( p_dec,
                                               p_dec->fmt_in.p_extra,
                                               p_dec->fmt_in.i_extra,
                                               true );
    if( !p_stream )
        return;

   struct parser_ctx ctx;
#ifdef HAVE_CSS
   ctx.b_css_memstream_opened = false;
#endif
   ctx.p_region = NULL;
   ctx.p_dec = p_dec;
   webvtt_text_parser_t *p_parser =
           webvtt_text_parser_New( &ctx, NULL, NULL, ParserHeaderHandler );
   if( p_parser )
   {
        char *psz_line;
        while( (psz_line = vlc_stream_ReadLine( p_stream )) )
            webvtt_text_parser_Feed( p_parser, psz_line );
        webvtt_text_parser_Delete( p_parser );
        /* commit using null */
        ParserHeaderHandler( &ctx, 0, false, NULL );
   }

    vlc_stream_Delete( p_stream );
}

/****************************************************************************
 * Flush:
 ****************************************************************************/
static void Flush( decoder_t *p_dec )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    ClearCuesByTime( &p_sys->p_root->p_child, INT64_MAX );
}

/****************************************************************************
 * DecodeBlock: decoder data entry point
 ****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    decoder_sys_t *p_sys = p_dec->p_sys;

    vlc_tick_t i_start = p_block->i_pts - VLC_TICK_0;
    vlc_tick_t i_stop = i_start + p_block->i_length;

    if( p_block->i_flags & BLOCK_FLAG_DISCONTINUITY )
        Flush( p_dec );
    else
        ClearCuesByTime( &p_sys->p_root->p_child, i_start );

    ProcessISOBMFF( p_dec, p_block->p_buffer, p_block->i_buffer,
                    i_start, i_stop );

    Render( p_dec, i_start, i_stop );

    block_Release( p_block );
    return VLCDEC_SUCCESS;
}

/*****************************************************************************
 * webvtt_CloseDecoder: clean up the decoder
 *****************************************************************************/
void webvtt_CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    webvtt_domnode_ChainDelete( (webvtt_dom_node_t *) p_sys->p_root );

#ifdef HAVE_CSS
    vlc_css_rules_Delete( p_sys->p_css_rules );
#endif

    free( p_sys );
}

/*****************************************************************************
 * webvtt_OpenDecoder: probe the decoder and return score
 *****************************************************************************/
int webvtt_OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_WEBVTT )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_sys->p_root = webvtt_dom_tag_New( NULL );
    if( !p_sys->p_root )
    {
        free( p_sys );
        return VLC_ENOMEM;
    }
    p_sys->p_root->psz_tag = strdup( "video" );

    p_dec->pf_decode = DecodeBlock;
    p_dec->pf_flush  = Flush;

    if( p_dec->fmt_in.i_extra )
        LoadExtradata( p_dec );

    return VLC_SUCCESS;
}

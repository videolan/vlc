/*****************************************************************************
 * substtml.c : TTML subtitles decoder
 *****************************************************************************
 * Copyright (C) 2015-2017 VLC authors and VideoLAN
 *
 * Authors: Hugo Beauzée-Luyssen <hugo@beauzee.fr>
 *          Sushma Reddy <sushma.reddy@research.iiit.ac.in>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_codec.h>
#include <vlc_xml.h>
#include <vlc_stream.h>
#include <vlc_text_style.h>
#include <vlc_charset.h>

#include <ctype.h>
#include <assert.h>

#include "substext.h"
#include "ttml.h"

//#define TTML_DEBUG

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    text_style_t*   font_style;
    int             i_text_align;
    int             i_direction;
    bool            b_direction_set;
    bool            b_preserve_space;
}  ttml_style_t;

typedef struct
{
    vlc_dictionary_t regions;
    tt_node_t *      p_rootnode; /* for now. FIXME: split header */
} ttml_context_t;

typedef struct
{
    subpicture_updater_sys_region_t updt;
    text_segment_t **pp_last_segment;
} ttml_region_t;

struct decoder_sys_t
{
    int                     i_align;
};

enum
{
    UNICODE_BIDI_LTR = 0,
    UNICODE_BIDI_RTL = 1,
    UNICODE_BIDI_EMBEDDED = 2,
    UNICODE_BIDI_OVERRIDE = 4,
};

static tt_node_t *ParseTTML( decoder_t *, const uint8_t *, size_t );

static void ttml_style_Delete( ttml_style_t* p_ttml_style )
{
    text_style_Delete( p_ttml_style->font_style );
    free( p_ttml_style );
}

static ttml_style_t * ttml_style_New( )
{
    ttml_style_t *p_ttml_style = calloc( 1, sizeof( ttml_style_t ) );
    if( unlikely( !p_ttml_style ) )
        return NULL;

    p_ttml_style->font_style = text_style_Create( STYLE_NO_DEFAULTS );
    if( unlikely( !p_ttml_style->font_style ) )
    {
        free( p_ttml_style );
        return NULL;
    }
    return p_ttml_style;
}

static void ttml_region_Delete( ttml_region_t *p_region )
{
    SubpictureUpdaterSysRegionClean( &p_region->updt );
    free( p_region );
}

static ttml_region_t *ttml_region_New( )
{
    ttml_region_t *p_ttml_region = calloc( 1, sizeof( ttml_region_t ) );
    if( unlikely( !p_ttml_region ) )
        return NULL;

    SubpictureUpdaterSysRegionInit( &p_ttml_region->updt );
    p_ttml_region->pp_last_segment = &p_ttml_region->updt.p_segments;
    /* Align to bottom by default. !Warn: center align is obtained with NO flags */
    p_ttml_region->updt.align = SUBPICTURE_ALIGN_BOTTOM;

    return p_ttml_region;
}

static tt_node_t * FindNode( tt_node_t *p_node, const char *psz_nodename,
                             size_t i_maxdepth, const char *psz_id )
{
    if( !tt_node_NameCompare( p_node->psz_node_name, psz_nodename ) )
    {
        if( psz_id != NULL )
        {
            char *psz = vlc_dictionary_value_for_key( &p_node->attr_dict, "xml:id" );
            if( psz && !strcmp( psz, psz_id ) )
                return p_node;
        }
        else return p_node;
    }

    if( i_maxdepth == 0 )
        return NULL;

    for( tt_basenode_t *p_child = p_node->p_child;
                        p_child; p_child = p_child->p_next )
    {
        if( p_child->i_type == TT_NODE_TYPE_TEXT )
            continue;

        p_node = FindNode( (tt_node_t *) p_child, psz_nodename, i_maxdepth - 1, psz_id );
        if( p_node )
            return p_node;
    }

    return NULL;
}

static void FillTextStyle( const char *psz_attr, const char *psz_val,
                           text_style_t *p_text_style )
{
    if( !strcasecmp ( "tts:fontFamily", psz_attr ) )
    {
        free( p_text_style->psz_fontname );
        p_text_style->psz_fontname = strdup( psz_val );
    }
    else if( !strcasecmp( "tts:opacity", psz_attr ) )
    {
        p_text_style->i_background_alpha = atoi( psz_val );
        p_text_style->i_font_alpha = atoi( psz_val );
        p_text_style->i_features |= STYLE_HAS_BACKGROUND_ALPHA | STYLE_HAS_FONT_ALPHA;
    }
    else if( !strcasecmp( "tts:fontSize", psz_attr ) )
    {
        char* psz_end = NULL;
        float size = us_strtof( psz_val, &psz_end );
        if( *psz_end == '%' )
            p_text_style->f_font_relsize = size;
        else
            p_text_style->i_font_size = (int)( size + 0.5 );
    }
    else if( !strcasecmp( "tts:color", psz_attr ) )
    {
        unsigned int i_color = vlc_html_color( psz_val, NULL );
        p_text_style->i_font_color = (i_color & 0xffffff);
        p_text_style->i_font_alpha = (i_color & 0xFF000000) >> 24;
        p_text_style->i_features |= STYLE_HAS_FONT_COLOR | STYLE_HAS_FONT_ALPHA;
    }
    else if( !strcasecmp( "tts:backgroundColor", psz_attr ) )
    {
        unsigned int i_color = vlc_html_color( psz_val, NULL );
        p_text_style->i_background_color = i_color & 0xFFFFFF;
        p_text_style->i_background_alpha = (i_color & 0xFF000000) >> 24;
        p_text_style->i_features |= STYLE_HAS_BACKGROUND_COLOR
                                                  | STYLE_HAS_BACKGROUND_ALPHA;
        p_text_style->i_style_flags |= STYLE_BACKGROUND;
    }
    else if( !strcasecmp( "tts:fontStyle", psz_attr ) )
    {
        if( !strcasecmp ( "italic", psz_val ) || !strcasecmp ( "oblique", psz_val ) )
            p_text_style->i_style_flags |= STYLE_ITALIC;
        else
            p_text_style->i_style_flags &= ~STYLE_ITALIC;
        p_text_style->i_features |= STYLE_HAS_FLAGS;
    }
    else if( !strcasecmp ( "tts:fontWeight", psz_attr ) )
    {
        if( !strcasecmp ( "bold", psz_val ) )
            p_text_style->i_style_flags |= STYLE_BOLD;
        else
            p_text_style->i_style_flags &= ~STYLE_BOLD;
        p_text_style->i_features |= STYLE_HAS_FLAGS;
    }
    else if( !strcasecmp ( "tts:textDecoration", psz_attr ) )
    {
        if( !strcasecmp ( "underline", psz_val ) )
            p_text_style->i_style_flags |= STYLE_UNDERLINE;
        else if( !strcasecmp ( "noUnderline", psz_val ) )
            p_text_style->i_style_flags &= ~STYLE_UNDERLINE;
        if( !strcasecmp ( "lineThrough", psz_val ) )
            p_text_style->i_style_flags |= STYLE_STRIKEOUT;
        else if( !strcasecmp ( "noLineThrough", psz_val ) )
            p_text_style->i_style_flags &= ~STYLE_STRIKEOUT;
        p_text_style->i_features |= STYLE_HAS_FLAGS;
    }
    else if( !strcasecmp( "tts:textOutline", psz_attr ) )
    {
        char *value = strdup( psz_val );
        char* psz_saveptr = NULL;
        char* token = (value) ? strtok_r( value, " ", &psz_saveptr ) : NULL;
        // <color>? <length> <length>?
        if( token != NULL )
        {
            bool b_ok = false;
            unsigned int color = vlc_html_color( token, &b_ok );
            if( b_ok )
            {
                p_text_style->i_outline_color = color & 0xFFFFFF;
                p_text_style->i_outline_alpha = (color & 0xFF000000) >> 24;
                token = strtok_r( NULL, " ", &psz_saveptr );
                if( token != NULL )
                {
                    char* psz_end = NULL;
                    int i_outline_width = strtol( token, &psz_end, 10 );
                    if( psz_end != token )
                    {
                        // Assume unit is pixel, and ignore border radius
                        p_text_style->i_outline_width = i_outline_width;
                    }
                }
            }
        }
        free( value );
    }
}

static void FillRegionStyle( const char *psz_attr, const char *psz_val,
                             ttml_region_t *p_region )
{
    if( !strcasecmp( "tts:displayAlign", psz_attr ) )
    {
        if( !strcasecmp ( "top", psz_val ) )
            p_region->updt.align = SUBPICTURE_ALIGN_TOP;
        else if( !strcasecmp ( "center", psz_val ) )
            p_region->updt.align = 0;
        else
            p_region->updt.align = SUBPICTURE_ALIGN_BOTTOM;
    }
    else if( !strcasecmp ( "tts:origin", psz_attr ) )
    {
        const char *psz_token = psz_val;
        while( isspace( *psz_token ) )
            psz_token++;

        const char *psz_separator = strchr( psz_token, ' ' );
        if( psz_separator == NULL )
            return;
        const char *psz_percent_sign = strchr( psz_token, '%' );

        p_region->updt.origin.x = atoi( psz_token );
        if( psz_percent_sign != NULL && psz_percent_sign < psz_separator )
            p_region->updt.flags |= UPDT_REGION_EXTENT_X_IS_PERCENTILE;

        while( isspace( *psz_separator ) )
            psz_separator++;
        psz_token = psz_separator;
        psz_percent_sign = strchr( psz_token, '%' );

        p_region->updt.origin.y = atoi( psz_token );
        if( psz_percent_sign != NULL )
            p_region->updt.flags |= UPDT_REGION_EXTENT_Y_IS_PERCENTILE;
    }
}

static void FillTTMLStyle( const char *psz_attr, const char *psz_val,
                           ttml_style_t *p_ttml_style )
{
    if( !strcasecmp( "tts:textAlign", psz_attr ) )
    {
        if( !strcasecmp ( "left", psz_val ) )
            p_ttml_style->i_text_align = SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp ( "right", psz_val ) )
            p_ttml_style->i_text_align = SUBPICTURE_ALIGN_RIGHT;
        else if( !strcasecmp ( "center", psz_val ) )
            p_ttml_style->i_text_align = 0;
        else if( !strcasecmp ( "start", psz_val ) ) /* FIXME: should be BIDI based */
            p_ttml_style->i_text_align = SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp ( "end", psz_val ) )  /* FIXME: should be BIDI based */
            p_ttml_style->i_text_align = SUBPICTURE_ALIGN_RIGHT;
    }
    else if( !strcasecmp( "tts:direction", psz_attr ) )
    {
        if( !strcasecmp( "rtl", psz_val ) )
        {
            p_ttml_style->i_direction |= UNICODE_BIDI_RTL;
            p_ttml_style->b_direction_set = true;
        }
        else if( !strcasecmp( "ltr", psz_val ) )
        {
            p_ttml_style->i_direction |= UNICODE_BIDI_LTR;
            p_ttml_style->b_direction_set = true;
        }
    }
    else if( !strcasecmp( "tts:unicodeBidi", psz_attr ) )
    {
            if( !strcasecmp( "bidiOverride", psz_val ) )
                p_ttml_style->i_direction |= UNICODE_BIDI_OVERRIDE & ~UNICODE_BIDI_EMBEDDED;
            else if( !strcasecmp( "embed", psz_val ) )
                p_ttml_style->i_direction |= UNICODE_BIDI_EMBEDDED & ~UNICODE_BIDI_OVERRIDE;
    }
    else if( !strcasecmp( "tts:writingMode", psz_attr ) )
    {
        if( !strcasecmp( "rl", psz_val ) || !strcasecmp( "rltb", psz_val ) )
        {
            p_ttml_style->i_direction = UNICODE_BIDI_RTL | UNICODE_BIDI_OVERRIDE;
            //p_ttml_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;
            p_ttml_style->b_direction_set = true;
        }
        else if( !strcasecmp( "lr", psz_val ) || !strcasecmp( "lrtb", psz_val ) )
        {
            p_ttml_style->i_direction = UNICODE_BIDI_LTR | UNICODE_BIDI_OVERRIDE;
            //p_ttml_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT;
            p_ttml_style->b_direction_set = true;
        }
    }
    else if( !strcasecmp( "xml:space", psz_attr ) )
    {
        p_ttml_style->b_preserve_space = !strcmp( "preserve", psz_val );
    }
    else FillTextStyle( psz_attr, psz_val, p_ttml_style->font_style );
}

static void DictionnaryMerge( const vlc_dictionary_t *p_src, vlc_dictionary_t *p_dst )
{
    for( int i = 0; i < p_src->i_size; ++i )
    {
        for ( const vlc_dictionary_entry_t* p_entry = p_src->p_entries[i];
                                            p_entry != NULL; p_entry = p_entry->p_next )
        {
            if( !strncmp( "tts:", p_entry->psz_key, 4 ) &&
                !vlc_dictionary_has_key( p_dst, p_entry->psz_key ) )
                vlc_dictionary_insert( p_dst, p_entry->psz_key, p_entry->p_value );
        }
    }
}

static void DictMergeWithStyleID( ttml_context_t *p_ctx, const char *psz_id,
                                  vlc_dictionary_t *p_dst )
{
    assert(p_ctx->p_rootnode);
    if( psz_id && p_ctx->p_rootnode )
    {
        /* Lookup referenced style ID */
        const tt_node_t *p_node = FindNode( p_ctx->p_rootnode,
                                            "style", -1, psz_id );
        if( p_node )
            DictionnaryMerge( &p_node->attr_dict, p_dst );
    }
}

static void DictMergeWithRegionID( ttml_context_t *p_ctx, const char *psz_id,
                                   vlc_dictionary_t *p_dst )
{
    assert(p_ctx->p_rootnode);
    if( psz_id && p_ctx->p_rootnode )
    {
        const tt_node_t *p_regionnode = FindNode( p_ctx->p_rootnode,
                                                 "region", -1, psz_id );
        if( !p_regionnode )
            return;

        /* First fill with style elements */
        for( const tt_basenode_t *p_child = p_regionnode->p_child;
                                  p_child; p_child = p_child->p_next )
        {
            if( unlikely( p_child->i_type == TT_NODE_TYPE_TEXT ) )
                continue;

            const tt_node_t *p_node = (const tt_node_t *) p_child;
            if( !tt_node_NameCompare( p_node->psz_node_name, "style" ) )
            {
                DictionnaryMerge( &p_node->attr_dict, p_dst );
            }
        }

        /* Merge region attributes */
        DictionnaryMerge( &p_regionnode->attr_dict, p_dst );
    }
}

static ttml_style_t * InheritTTMLStyles( ttml_context_t *p_ctx, tt_node_t *p_node )
{
    assert( p_node );
    ttml_style_t *p_ttml_style = NULL;
    vlc_dictionary_t merged;
    vlc_dictionary_init( &merged, 0 );

    /* Merge dics backwards without overwriting */
    for( ; p_node; p_node = p_node->p_parent )
    {
        const char *psz_regionid = (const char *)
                vlc_dictionary_value_for_key( &p_node->attr_dict, "region" );
        if( psz_regionid )
            DictMergeWithRegionID( p_ctx, psz_regionid, &merged );

        const char *psz_styleid = (const char *)
                vlc_dictionary_value_for_key( &p_node->attr_dict, "style" );
        if( psz_styleid )
            DictMergeWithStyleID( p_ctx, psz_styleid, &merged );

        DictionnaryMerge( &p_node->attr_dict, &merged );
    }

    if( merged.i_size && merged.p_entries[0] && (p_ttml_style = ttml_style_New()) )
    {
        for( int i = 0; i < merged.i_size; ++i )
        {
            for ( vlc_dictionary_entry_t* p_entry = merged.p_entries[i];
                  p_entry != NULL; p_entry = p_entry->p_next )
            {
                FillTTMLStyle( p_entry->psz_key, p_entry->p_value, p_ttml_style );
            }
        }
    }

    vlc_dictionary_clear( &merged, NULL, NULL );

    return p_ttml_style;
}

static int ParseTTMLChunk( xml_reader_t *p_reader, tt_node_t **pp_rootnode )
{
    const char* psz_node_name;

    do
    {
        int i_type = xml_ReaderNextNode( p_reader, &psz_node_name );

        if( i_type <= XML_READER_NONE )
            break;

        switch(i_type)
        {
            default:
                break;

            case XML_READER_STARTELEM:
                if( tt_node_NameCompare( psz_node_name, "tt" ) ||
                    *pp_rootnode != NULL )
                    return VLC_EGENERIC;

                *pp_rootnode = tt_node_New( p_reader, NULL, psz_node_name );
                if( !*pp_rootnode ||
                    tt_nodes_Read( p_reader, *pp_rootnode ) != VLC_SUCCESS )
                    return VLC_EGENERIC;
                break;

            case XML_READER_ENDELEM:
                if( !*pp_rootnode ||
                    tt_node_NameCompare( psz_node_name, (*pp_rootnode)->psz_node_name ) )
                    return VLC_EGENERIC;
                break;
        }

    } while( 1 );

    if( *pp_rootnode == NULL )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

static void BIDIConvert( text_segment_t *p_segment, int i_direction )
{
    /*
    * For bidirectionnal support, we use different enum
    * to recognize different cases, en then we add the
    * corresponding unicode character to the text of
    * the text_segment.
    */
    static const struct
    {
        const char* psz_uni_start;
        const char* psz_uni_end;
    } p_bidi[] = {
        { "\u2066", "\u2069" },
        { "\u2067", "\u2069" },
        { "\u202A", "\u202C" },
        { "\u202B", "\u202C" },
        { "\u202D", "\u202C" },
        { "\u202E", "\u202C" },
    };

    if( unlikely((size_t)i_direction >= ARRAY_SIZE(p_bidi)) )
        return;

    char *psz_text = NULL;
    if( asprintf( &psz_text, "%s%s%s", p_bidi[i_direction].psz_uni_start,
                  p_segment->psz_text, p_bidi[i_direction].psz_uni_end ) < 0 )
    {
        free( p_segment->psz_text );
        p_segment->psz_text = psz_text;
    }
}

static void StripSpacing( text_segment_t *p_segment )
{
    /* Newlines must be replaced */
    char *p = p_segment->psz_text;
    while( (p = strchr( p, '\n' )) )
        *p = ' ';
}

static ttml_region_t *GetTTMLRegion( ttml_context_t *p_ctx, const char *psz_region_id )
{
    ttml_region_t *p_region = ( ttml_region_t * )
            vlc_dictionary_value_for_key( &p_ctx->regions, psz_region_id ? psz_region_id : "" );
    if( p_region == NULL )
    {
        if( psz_region_id && strcmp( psz_region_id, "" ) ) /* not default region */
        {
            /* Create if missing and exists as node */
            const tt_node_t *p_node = FindNode( p_ctx->p_rootnode, "region", -1, psz_region_id );
            if( p_node && (p_region = ttml_region_New()) )
            {
                /* Fill from its own attributes */
                for( int i = 0; i < p_node->attr_dict.i_size; ++i )
                {
                    for ( vlc_dictionary_entry_t* p_entry = p_node->attr_dict.p_entries[i];
                          p_entry != NULL; p_entry = p_entry->p_next )
                    {
                        FillRegionStyle( p_entry->psz_key, p_entry->p_value, p_region );
                    }
                }
            }
            vlc_dictionary_insert( &p_ctx->regions, psz_region_id, p_region );
        }
        else if( (p_region = ttml_region_New()) ) /* create default */
        {
            vlc_dictionary_insert( &p_ctx->regions, "", p_region );
        }
    }
    return p_region;
}

static void AppendLineBreakToRegion( ttml_region_t *p_region )
{
    text_segment_t *p_segment = text_segment_New( "\n" );
    if( p_segment )
    {
        *p_region->pp_last_segment = p_segment;
        p_region->pp_last_segment = &p_segment->p_next;
    }
}

static void AppendTextToRegion( ttml_context_t *p_ctx, const tt_textnode_t *p_ttnode,
                                ttml_region_t *p_region )
{
    text_segment_t *p_segment;

    if( p_region == NULL )
        return;

    p_segment = text_segment_New( p_ttnode->psz_text );
    if( p_segment )
    {
        ttml_style_t *s = InheritTTMLStyles( p_ctx, p_ttnode->p_parent );
        if( s )
        {
            p_segment->style = s->font_style;
            s->font_style = NULL;

            if( !s->b_preserve_space )
                StripSpacing( p_segment );
            if( s->b_direction_set )
                BIDIConvert( p_segment, s->i_direction );

            ttml_style_Delete( s );
        }
    }

    *p_region->pp_last_segment = p_segment;
    p_region->pp_last_segment = &p_segment->p_next;
}

static void ConvertNodesToRegionContent( ttml_context_t *p_ctx, const tt_node_t *p_node,
                                         ttml_region_t *p_region, int64_t i_playbacktime )
{
    if( i_playbacktime != -1 &&
       !tt_timings_Contains( &p_node->timings, i_playbacktime ) )
        return;

    const char *psz_regionid = (const char *)
        vlc_dictionary_value_for_key( &p_node->attr_dict, "region" );

    /* Region isn't set or is changing */
    if( psz_regionid || p_region == NULL )
        p_region = GetTTMLRegion( p_ctx, psz_regionid );

    /* awkward paragraph handling */
    if( !tt_node_NameCompare( p_node->psz_node_name, "p" ) &&
        p_region->updt.p_segments )
    {
        AppendLineBreakToRegion( p_region );
    }

    for( const tt_basenode_t *p_child = p_node->p_child;
                              p_child; p_child = p_child->p_next )
    {
        if( p_child->i_type == TT_NODE_TYPE_TEXT )
        {
            AppendTextToRegion( p_ctx, (const tt_textnode_t *) p_child, p_region );
        }
        else if( !tt_node_NameCompare( ((const tt_node_t *)p_child)->psz_node_name, "br" ) )
        {
            AppendLineBreakToRegion( p_region );
        }
        else
        {
            ConvertNodesToRegionContent( p_ctx, (const tt_node_t *) p_child,
                                         p_region, i_playbacktime );
        }
    }
}

static tt_node_t *ParseTTML( decoder_t *p_dec, const uint8_t *p_buffer, size_t i_buffer )
{
    stream_t*       p_sub;
    xml_reader_t*   p_xml_reader;

    p_sub = vlc_stream_MemoryNew( p_dec, (uint8_t*) p_buffer, i_buffer, true );
    if( unlikely( p_sub == NULL ) )
        return NULL;

    p_xml_reader = xml_ReaderCreate( p_dec, p_sub );
    if( unlikely( p_xml_reader == NULL ) )
    {
        vlc_stream_Delete( p_sub );
        return NULL;
    }

    tt_node_t *p_rootnode = NULL;
    if( ParseTTMLChunk( p_xml_reader, &p_rootnode ) != VLC_SUCCESS )
    {
        if( p_rootnode )
            tt_node_RecursiveDelete( p_rootnode );
        p_rootnode = NULL;
    }

    xml_ReaderDelete( p_xml_reader );
    vlc_stream_Delete( p_sub );

    return p_rootnode;
}

static ttml_region_t *GenerateRegions( tt_node_t *p_rootnode, int64_t i_playbacktime )
{
    ttml_region_t*  p_regions = NULL;
    ttml_region_t** pp_region_last = &p_regions;

    if( !tt_node_NameCompare( p_rootnode->psz_node_name, "tt" ) )
    {
        const tt_node_t *p_bodynode = FindNode( p_rootnode, "body", 1, NULL );
        if( p_bodynode )
        {
            ttml_context_t context;
            context.p_rootnode = p_rootnode;
            vlc_dictionary_init( &context.regions, 1 );
            ConvertNodesToRegionContent( &context, p_bodynode, NULL, i_playbacktime );

            for( int i = 0; i < context.regions.i_size; ++i )
            {
                for ( const vlc_dictionary_entry_t* p_entry = context.regions.p_entries[i];
                                                    p_entry != NULL; p_entry = p_entry->p_next )
                {
                    *pp_region_last = (ttml_region_t *) p_entry->p_value;
                    pp_region_last = (ttml_region_t **) &(*pp_region_last)->updt.p_next;
                }
            }

            vlc_dictionary_clear( &context.regions, NULL, NULL );
        }
    }
    else if ( !tt_node_NameCompare( p_rootnode->psz_node_name, "div" ) ||
              !tt_node_NameCompare( p_rootnode->psz_node_name, "p" ) )
    {
        /* TODO */
    }

    return p_regions;
}

static int ParseBlock( decoder_t *p_dec, const block_t *p_block )
{
    int64_t *p_timings_array = NULL;
    size_t   i_timings_count = 0;

    /* We Only support absolute timings */
    tt_timings_t temporal_extent;
    temporal_extent.i_type = TT_TIMINGS_PARALLEL;
    temporal_extent.i_begin = 0;
    temporal_extent.i_end = -1;
    temporal_extent.i_dur = -1;

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        return VLCDEC_SUCCESS;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts <= VLC_TS_INVALID )
    {
        msg_Warn( p_dec, "subtitle without a date" );
        return VLCDEC_SUCCESS;
    }

    tt_node_t *p_rootnode = ParseTTML( p_dec, p_block->p_buffer, p_block->i_buffer );
    if( !p_rootnode )
        return VLCDEC_SUCCESS;

    tt_timings_Resolve( (tt_basenode_t *) p_rootnode, &temporal_extent,
                        &p_timings_array, &i_timings_count );

#ifdef TTML_DEBUG
    for( size_t i=0; i<i_timings_count; i++ )
        printf("%ld ", p_timings_array[i]);
    printf("\n");
#endif

    for( size_t i=0; i+1 < i_timings_count; i++ )
    {
        /* We Only support absolute timings (2) */
        if( p_timings_array[i] + VLC_TS_0 < p_block->i_dts )
            continue;

        if( p_timings_array[i] + VLC_TS_0 > p_block->i_dts + p_block->i_length )
            break;

        subpicture_t *p_spu = NULL;
        ttml_region_t *p_regions = GenerateRegions( p_rootnode, p_timings_array[i] );
        if( p_regions && ( p_spu = decoder_NewSubpictureText( p_dec ) ) )
        {
            p_spu->i_start    = VLC_TS_0 + p_timings_array[i];
            p_spu->i_stop     = VLC_TS_0 + p_timings_array[i+1] - 1;
            p_spu->b_ephemer  = true;
            p_spu->b_absolute = false;

            subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;
            subpicture_updater_sys_region_t *p_updtregion = NULL;

            /* Create region update info from each ttml region */
            for( ttml_region_t *p_region = p_regions;
                 p_region; p_region = (ttml_region_t *) p_region->updt.p_next )
            {
                if( p_updtregion == NULL )
                {
                    p_updtregion = &p_spu_sys->region;
                }
                else
                {
                    p_updtregion = SubpictureUpdaterSysRegionNew();
                    if( p_updtregion == NULL )
                        break;
                    SubpictureUpdaterSysRegionAdd( &p_spu_sys->region, p_updtregion );
                }

                /* broken legacy align var (can't handle center...) */
                if( p_dec->p_sys->i_align & SUBPICTURE_ALIGN_MASK )
                {
                    p_spu_sys->region.align = p_dec->p_sys->i_align & (SUBPICTURE_ALIGN_BOTTOM|SUBPICTURE_ALIGN_TOP);
                    p_spu_sys->region.inner_align = p_dec->p_sys->i_align & (SUBPICTURE_ALIGN_LEFT|SUBPICTURE_ALIGN_RIGHT);
                }

                /* copy and take ownership of pointeds */
                *p_updtregion = p_region->updt;
                p_updtregion->p_next = NULL;
                p_region->updt.p_region_style = NULL;
                p_region->updt.p_segments = NULL;
            }

        }

        /* cleanup */
        while( p_regions )
        {
            ttml_region_t *p_nextregion = (ttml_region_t *) p_regions->updt.p_next;
            ttml_region_Delete( p_regions );
            p_regions = p_nextregion;
        }

        if( p_spu )
            decoder_QueueSub( p_dec, p_spu );
    }

    tt_node_RecursiveDelete( p_rootnode );

    free( p_timings_array );

    return VLCDEC_SUCCESS;
}



/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    int ret = ParseBlock( p_dec, p_block );
    block_Release( p_block );
    return ret;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_CODEC_TTML )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    p_dec->pf_decode = DecodeBlock;
    p_dec->fmt_out.i_cat = SPU_ES;
    p_sys->i_align = var_InheritInteger( p_dec, "ttml-align" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    free( p_sys );
}

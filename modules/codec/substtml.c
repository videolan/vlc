/*****************************************************************************
 * substtml.c : TTML subtitles decoder
 *****************************************************************************
 * Copyright (C) 2015 VLC authors and VideoLAN
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
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_codec.h>
#include <vlc_xml.h>
#include <vlc_stream.h>
#include <vlc_text_style.h>

#include "substext.h"

#include <ctype.h>

#define ALIGN_TEXT N_("Subtitle justification")
#define ALIGN_LONGTEXT N_("Set the justification of subtitles")

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static text_segment_t *ParseTTMLSubtitles( decoder_t *, subpicture_updater_sys_t *, char * );

vlc_module_begin ()
    set_capability( "decoder", 10 )
    set_shortname( N_("TTML decoder"))
    set_description( N_("TTML subtitles decoder") )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    add_integer( "ttml-align", 0, ALIGN_TEXT, ALIGN_LONGTEXT, false )
vlc_module_end ();

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

typedef struct
{
    char*           psz_styleid;
    text_style_t*   font_style;
    int             i_align;
    int             i_margin_h;
    int             i_margin_v;
    int             i_margin_percent_h;
    int             i_margin_percent_v;
}  ttml_style_t;

struct decoder_sys_t
{
    int                     i_align;
    ttml_style_t**          pp_styles;
    size_t                  i_styles;
};

static ttml_style_t *FindTextStyle( decoder_t *p_dec, const char *psz_style )
{
    decoder_sys_t  *p_sys = p_dec->p_sys;    

    for( size_t i = 0; i < p_sys->i_styles; i++ )
    {
        if( !strcmp( p_sys->pp_styles[i]->psz_styleid, psz_style ) )
        {
            return p_sys->pp_styles[i];
        }
    }
    return NULL;
}

typedef struct style_stack style_stack_t;
struct  style_stack
{
    ttml_style_t* p_style;
    style_stack_t* p_next;
};

static bool PushStyle( style_stack_t **pp_stack, ttml_style_t* p_style )
{
    style_stack_t* p_entry = malloc( sizeof( *p_entry) );
    if ( unlikely( p_entry == NULL ) )
        return false;
    p_entry->p_style = p_style;
    p_entry->p_next = *pp_stack;
    *pp_stack = p_entry;
    return true;
}

static void PopStyle( style_stack_t** pp_stack )
{
    if ( *pp_stack == NULL )
        return;
    style_stack_t* p_next = (*pp_stack)->p_next;
    free( *pp_stack );
    *pp_stack = p_next;
}

static void ClearStack( style_stack_t* p_stack )
{
    while ( p_stack != NULL )
    {
        style_stack_t* p_next = p_stack->p_next;
        free( p_stack );
        p_stack = p_next;
    }
}

static text_style_t* CurrentStyle( style_stack_t* p_stack )
{
    if ( p_stack == NULL )
        return text_style_Create( STYLE_NO_DEFAULTS );
    return text_style_Duplicate( p_stack->p_style->font_style );
}

static void ParseTTMLStyle( decoder_t *p_dec, xml_reader_t* p_reader )
{
    decoder_sys_t* p_sys = p_dec->p_sys;
    ttml_style_t *p_ttml_style = NULL;
    ttml_style_t *p_base_style = NULL;

    p_ttml_style = calloc( 1, sizeof(ttml_style_t) );

    if ( unlikely( !p_ttml_style ) )
        return ;
    p_ttml_style->font_style = text_style_Create( STYLE_NO_DEFAULTS );
    if( unlikely( !p_ttml_style->font_style ) )
    {
        free( p_ttml_style );
        return ;
    }

    const char *attr, *val;

    while( (attr = xml_ReaderNextAttr( p_reader, &val ) ) )
    {
        if ( !strcasecmp( attr, "style" ) )
        {
            for( size_t i = 0; i < p_sys->i_styles; i++ )
            {
                if( !strcasecmp( p_sys->pp_styles[i]->psz_styleid, val ) )
                {
                    p_base_style = p_sys->pp_styles[i];
                    break;
                }
            }
        }
        else if ( !strcasecmp( "xml:id", attr ) )
        {
            free( p_ttml_style->psz_styleid );
            p_ttml_style->psz_styleid = strdup( val );
        }
        else if ( !strcasecmp ( "tts:fontFamily", attr ) )
        {
            free( p_ttml_style->font_style->psz_fontname );
            p_ttml_style->font_style->psz_fontname = strdup( val );
        }
        else if ( !strcasecmp( "tts:fontSize", attr ) )
        {
            p_ttml_style->font_style->i_font_size  = atoi( val );
        }
        else if ( !strcasecmp( "tts:color", attr ) )
        {
            unsigned int i_color = vlc_html_color( val, NULL );
            p_ttml_style->font_style->i_font_color = (i_color & 0xffffff);
            p_ttml_style->font_style->i_font_alpha = (i_color & 0xFF000000) >> 24;
            p_ttml_style->font_style->i_features |= STYLE_HAS_FONT_COLOR | STYLE_HAS_FONT_ALPHA;
        }
        else if ( !strcasecmp( "tts:backgroundColor", attr ) )
        {
            unsigned int i_color = vlc_html_color( val, NULL );
            p_ttml_style->font_style->i_background_color = i_color & 0xFFFFFF;
            p_ttml_style->font_style->i_background_alpha = (i_color & 0xFF000000) >> 24;
            p_ttml_style->font_style->i_features |= STYLE_HAS_BACKGROUND_COLOR
                                                      | STYLE_HAS_BACKGROUND_ALPHA;
            p_ttml_style->font_style->i_style_flags |= STYLE_BACKGROUND;
        }
        else if ( !strcasecmp( "tts:textAlign", attr ) )
        {
            if ( !strcasecmp ( "left", val ) )
                p_ttml_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT;
            else if ( !strcasecmp ( "right", val ) )
                p_ttml_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;
            else if ( !strcasecmp ( "center", val ) )
                p_ttml_style->i_align = SUBPICTURE_ALIGN_BOTTOM;
            else if ( !strcasecmp ( "start", val ) )
                p_ttml_style->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;
            else if ( !strcasecmp ( "end", val ) )
                p_ttml_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;
        }
        else if ( !strcasecmp( "tts:fontStyle", attr ) )
        {
            if ( !strcasecmp ( "italic", val ) || !strcasecmp ( "oblique", val ) )
                p_ttml_style->font_style->i_style_flags |= STYLE_ITALIC;
            else
                p_ttml_style->font_style->i_style_flags &= ~STYLE_ITALIC;
            p_ttml_style->font_style->i_features |= STYLE_HAS_FLAGS;
        }
        else if ( !strcasecmp ( "tts:fontWeight", attr ) )
        {
            if ( !strcasecmp ( "bold", val ) )
                p_ttml_style->font_style->i_style_flags |= STYLE_BOLD;
            else
                p_ttml_style->font_style->i_style_flags &= ~STYLE_BOLD;
            p_ttml_style->font_style->i_features |= STYLE_HAS_FLAGS;
        }
        else if ( !strcasecmp ( "tts:textDecoration", attr ) )
        {
            if ( !strcasecmp ( "underline", val ) )
                p_ttml_style->font_style->i_style_flags |= STYLE_UNDERLINE;
            else if ( !strcasecmp ( "noUnderline", val ) )
                p_ttml_style->font_style->i_style_flags &= ~STYLE_UNDERLINE;
            if ( !strcasecmp ( "lineThrough", val ) )
                p_ttml_style->font_style->i_style_flags |= STYLE_STRIKEOUT;
            else if ( !strcasecmp ( "noLineThrough", val ) )
                p_ttml_style->font_style->i_style_flags &= ~STYLE_STRIKEOUT;
            p_ttml_style->font_style->i_features |= STYLE_HAS_FLAGS;
        }
        else if ( !strcasecmp ( "tts:origin", attr ) )
        {
            const char *psz_token = val;
            while ( isspace( *psz_token ) )
                psz_token++;

            const char *psz_separator = strchr( psz_token, ' ' );
            if ( psz_separator == NULL )
            {
                msg_Warn( p_dec, "Invalid origin attribute: \"%s\"", val );
                continue;
            }
            const char *psz_percent_sign = strchr( psz_token, '%' );

            if( psz_percent_sign != NULL && psz_percent_sign < psz_separator )
            {
                p_ttml_style->i_margin_h = 0;
                p_ttml_style->i_margin_percent_h = atoi( psz_token );
            }
            else
            {
                p_ttml_style->i_margin_h = atoi( psz_token );
                p_ttml_style->i_margin_percent_h = 0;
            }
            while ( isspace( *psz_separator ) )
                psz_separator++;
            psz_token = psz_separator;
            psz_percent_sign = strchr( psz_token, '%' );
            if( psz_percent_sign != NULL )
            {
                p_ttml_style->i_margin_v = 0;
                p_ttml_style->i_margin_percent_v = atoi( val );
            }
            else
            {
                p_ttml_style->i_margin_v = atoi( val );
                p_ttml_style->i_margin_percent_v = 0;
            }
        }
        else if ( !strcasecmp( "tts:textOutline", attr ) )
        {
            char *value = strdup( val );
            char* psz_saveptr = NULL;
            char* token = strtok_r( value, " ", &psz_saveptr );
            // <color>? <length> <length>?
            bool b_ok = false;
            unsigned int color = vlc_html_color( token, &b_ok );
            if ( b_ok )
            {
                p_ttml_style->font_style->i_outline_color = color & 0xFFFFFF;
                p_ttml_style->font_style->i_outline_alpha = (color & 0xFF000000) >> 24;
                token = strtok_r( NULL, " ", &psz_saveptr );
            }
            char* psz_end = NULL;
            int i_outline_width = strtol( token, &psz_end, 10 );
            if ( psz_end != token )
            {
                // Assume unit is pixel, and ignore border radius
                p_ttml_style->font_style->i_outline_width = i_outline_width;
            }
            free( value );
        }
    }
    if ( p_base_style != NULL )
    {
        text_style_Merge( p_ttml_style->font_style, p_base_style->font_style, false );
    }
    if ( p_ttml_style->psz_styleid == NULL )
    {
        free( p_ttml_style->font_style->psz_fontname );
        free( p_ttml_style );
        return ;
    }

    TAB_APPEND( p_sys->i_styles, p_sys->pp_styles, p_ttml_style );
    return ;
}

static void ParseTTMLStyles( decoder_t* p_dec )
{
    stream_t* p_stream = vlc_stream_MemoryNew( p_dec, (uint8_t*)p_dec->fmt_in.p_extra, p_dec->fmt_in.i_extra, true );
    if( unlikely( p_stream == NULL ) )
        return ;

    xml_reader_t* p_reader = xml_ReaderCreate( p_dec, p_stream );
    if( unlikely( p_reader == NULL ) )
    {
        vlc_stream_Delete( p_stream );
        return ;
    }
    const char* psz_name;
    int i_type = xml_ReaderNextNode( p_reader, &psz_name );

    if ( i_type == XML_READER_STARTELEM && ( !strcasecmp( psz_name, "head" ) || !strcasecmp( psz_name, "tt:head" ) ) )
    {
        do
        {
            i_type = xml_ReaderNextNode( p_reader, &psz_name );
            if ( i_type == XML_READER_STARTELEM && ( !strcasecmp( "styling", psz_name ) ||
                                                     !strcasecmp( "tt:styling", psz_name ) ) )
            {
                i_type = xml_ReaderNextNode( p_reader, &psz_name );
                while ( i_type != XML_READER_ENDELEM || ( strcasecmp( psz_name, "styling" ) && strcasecmp( psz_name, "tt:styling" ) ) )
                {
                    ParseTTMLStyle( p_dec, p_reader );
                    i_type = xml_ReaderNextNode( p_reader, &psz_name );
                }
            }
        } while ( i_type != XML_READER_ENDELEM || ( strcasecmp( psz_name, "head" ) && strcasecmp( psz_name, "tt:head" ) ) );
    }
    xml_ReaderDelete( p_reader );
    vlc_stream_Delete( p_stream );
}

static text_segment_t *ParseTTMLSubtitles( decoder_t *p_dec, subpicture_updater_sys_t *p_update_sys, char *psz_subtitle )
{
    stream_t*       p_sub = NULL;
    xml_reader_t*   p_xml_reader = NULL;
    text_segment_t* p_first_segment = NULL;
    text_segment_t* p_current_segment = NULL;
    style_stack_t*  p_style_stack = NULL;

    p_sub = vlc_stream_MemoryNew( p_dec, (uint8_t*)psz_subtitle, strlen( psz_subtitle ), true );
    if( unlikely( p_sub == NULL ) )
        return NULL;

    p_xml_reader = xml_ReaderCreate( p_dec, p_sub );
    if( unlikely( p_xml_reader == NULL ) )
    {
        vlc_stream_Delete( p_sub );
        return NULL;
    }

    const char *node;
    int i_type;

    i_type = xml_ReaderNextNode( p_xml_reader, &node );
    while ( i_type != XML_READER_NONE && i_type > 0 )
    {
        if( i_type == XML_READER_STARTELEM && ( !strcasecmp( node, "p" ) || !strcasecmp( node, "tt:p" ) ) )
        {
            text_segment_t* p_segment = text_segment_New( NULL );
            if ( unlikely( p_segment == NULL ) )
                goto fail;
            const char* psz_attr_name;
            const char* psz_attr_value;
            while ( ( psz_attr_name = xml_ReaderNextAttr( p_xml_reader, &psz_attr_value ) ) != NULL )
            {
                if ( !strcasecmp( psz_attr_name, "style" ) )
                {
                    ttml_style_t* p_style = FindTextStyle( p_dec, psz_attr_value );
                    if ( p_style == NULL )
                    {
                        msg_Warn( p_dec, "Style \"%s\" not found", psz_attr_value );
                        break;
                    }
                    if( p_style->i_margin_h )
                        p_update_sys->x = p_style->i_margin_h;
                    else
                        p_update_sys->x = p_style->i_margin_percent_h;

                    if( p_style->i_margin_v )
                        p_update_sys->y = p_style->i_margin_v;
                    else
                        p_update_sys->y = p_style->i_margin_percent_v;
                    p_update_sys->align = p_style->i_align;

                    if ( PushStyle( &p_style_stack, p_style ) == false )
                    {
                        text_segment_Delete( p_segment );
                        goto fail;
                    }
                    p_segment->style = CurrentStyle( p_style_stack );
                    break;
                }
            }
            if ( p_segment->style == NULL )
                p_segment->style = text_style_Create( STYLE_NO_DEFAULTS );
            if ( p_first_segment == NULL )
            {
                p_first_segment = p_segment;
                p_current_segment = p_segment;
            }
            else
            {
                p_current_segment->p_next = p_segment;
                p_current_segment = p_segment;
            }
        }
        else if ( i_type == XML_READER_ENDELEM && ( !strcasecmp( node, "p" ) || !strcasecmp( node, "tt:p" ) ) )
        {
            PopStyle( &p_style_stack );
            p_current_segment = NULL;
        }
        else if ( i_type == XML_READER_STARTELEM && !strcasecmp( node, "br" ) )
        {
            if ( p_current_segment != NULL && p_current_segment->psz_text != NULL )
            {
                char* psz_text = NULL;
                if ( asprintf( &psz_text, "%s\n", p_current_segment->psz_text ) != -1 )
                {
                    free( p_current_segment->psz_text );
                    p_current_segment->psz_text = psz_text;
                }
            }
        }
        else if ( i_type == XML_READER_TEXT )
        {
            if ( p_current_segment == NULL )
            {
                p_current_segment = text_segment_New( node );
                p_current_segment->style = CurrentStyle( p_style_stack );
                if ( p_first_segment == NULL )
                    p_first_segment = p_current_segment;
                else
                    p_first_segment->p_next = p_current_segment;
            }
            else if ( p_current_segment->psz_text == NULL )
            {
                p_current_segment->psz_text = strdup( node );
                vlc_xml_decode( p_current_segment->psz_text );
            }
            else
            {
                size_t i_previous_len = strlen( p_current_segment->psz_text );
                char* psz_text = NULL;
                if ( asprintf( &psz_text, "%s%s", p_current_segment->psz_text, node ) != -1 )
                {
                    free( p_current_segment->psz_text );
                    p_current_segment->psz_text = psz_text;
                    // Don't process text multiple time, just check for the appended section
                    vlc_xml_decode( p_current_segment->psz_text + i_previous_len );
                }
            }
        }
        i_type = xml_ReaderNextNode( p_xml_reader, &node );
    }

    ClearStack( p_style_stack );
    xml_ReaderDelete( p_xml_reader );
    vlc_stream_Delete( p_sub );

    return p_first_segment;

fail:
    text_segment_ChainDelete( p_first_segment );
    ClearStack( p_style_stack );
    xml_ReaderDelete( p_xml_reader );
    vlc_stream_Delete( p_sub );
    return NULL;
}

static subpicture_t *ParseText( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = NULL;
    char *psz_subtitle = NULL;

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        return NULL;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts <= VLC_TS_INVALID )
    {
        msg_Warn( p_dec, "subtitle without a date" );
        return NULL;
    }

    /* Check validity of packet data */
    /* An "empty" line containing only \0 can be used to force
       and ephemer picture from the screen */

    if( p_block->i_buffer < 1 )
    {
        msg_Warn( p_dec, "no subtitle data" );
        return NULL;
    }

    psz_subtitle = malloc( p_block->i_buffer );
    if ( unlikely( psz_subtitle == NULL ) )
        return NULL;
    memcpy( psz_subtitle, p_block->p_buffer, p_block->i_buffer );

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpictureText( p_dec );
    if( !p_spu )
    {
        free( psz_subtitle );
        return NULL;
    }
    p_spu->i_start    = p_block->i_pts;
    p_spu->i_stop     = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer  = (p_block->i_length == 0);
    p_spu->b_absolute = false;

    subpicture_updater_sys_t *p_spu_sys = p_spu->updater.p_sys;

    p_spu_sys->align = SUBPICTURE_ALIGN_BOTTOM | p_sys->i_align;
    p_spu_sys->p_segments = ParseTTMLSubtitles( p_dec, p_spu_sys, psz_subtitle );
    free( psz_subtitle );

    return p_spu;
}



/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    if( !pp_block || *pp_block == NULL )
        return NULL;

    block_t* p_block = *pp_block;
    subpicture_t *p_spu = ParseText( p_dec, p_block );

    block_Release( p_block );
    *pp_block = NULL;

    return p_spu;
}

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if ( p_dec->fmt_in.i_codec != VLC_CODEC_TTML )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    p_dec->p_sys = p_sys = calloc( 1, sizeof( *p_sys ) );
    if( unlikely( p_sys == NULL ) )
        return VLC_ENOMEM;

    if ( p_dec->fmt_in.p_extra != NULL && p_dec->fmt_in.i_extra > 0 )
        ParseTTMLStyles( p_dec );

    p_dec->pf_decode_sub = DecodeBlock;
    p_dec->fmt_out.i_cat = SPU_ES;
    p_sys->i_align = var_InheritInteger( p_dec, "ttml-align" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    for ( size_t i = 0; i < p_sys->i_styles; ++i )
    {
        free( p_sys->pp_styles[i]->psz_styleid );
        text_style_Delete( p_sys->pp_styles[i]->font_style );
        free( p_sys->pp_styles[i] );
    }
    TAB_CLEAN( p_sys->i_styles, p_sys->pp_styles );

    free( p_sys );
}

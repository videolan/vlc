/*****************************************************************************
 * subsusf.c : USF subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Bernie Purcell <bitmap@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "subsdec.h"
#include <vlc_plugin.h>
#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

static subpicture_t *DecodeBlock   ( decoder_t *, block_t ** );
static char         *CreatePlainText( char * );
static int           ParseImageAttachments( decoder_t *p_dec );

static subpicture_t        *ParseText     ( decoder_t *, block_t * );
static void                 ParseUSFHeader( decoder_t * );
static subpicture_region_t *ParseUSFString( decoder_t *, char *, subpicture_t * );
static subpicture_region_t *LoadEmbeddedImage( decoder_t *p_dec, subpicture_t *p_spu, const char *psz_filename, int i_transparent_color );

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/

vlc_module_begin ()
    set_capability( "decoder", 40 )
    set_shortname( N_("USFSubs"))
    set_description( N_("USF subtitles decoder") )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    /* We inherit subsdec-align and subsdec-formatted from subsdec.c */
vlc_module_end ()

/*****************************************************************************
 * OpenDecoder: probe the decoder and return score
 *****************************************************************************
 * Tries to launch a decoder and return score so that the interface is able
 * to chose.
 *****************************************************************************/
static int OpenDecoder( vlc_object_t *p_this )
{
    decoder_t     *p_dec = (decoder_t*)p_this;
    decoder_sys_t *p_sys;

    if( p_dec->fmt_in.i_codec != VLC_FOURCC('u','s','f',' ') )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = calloc(1, sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    p_dec->pf_decode_sub = DecodeBlock;
    p_dec->fmt_out.i_cat = SPU_ES;
    p_dec->fmt_out.i_codec = 0;

    /* Unused fields of p_sys - not needed for USF decoding */
    p_sys->b_ass = false;
    p_sys->iconv_handle = (vlc_iconv_t)-1;
    p_sys->b_autodetect_utf8 = false;

    /* init of p_sys */
    p_sys->i_align = 0;
    p_sys->i_original_height = 0;
    p_sys->i_original_width = 0;
    TAB_INIT( p_sys->i_ssa_styles, p_sys->pp_ssa_styles );
    TAB_INIT( p_sys->i_images, p_sys->pp_images );

    /* USF subtitles are mandated to be UTF-8, so don't need vlc_iconv */

    p_sys->i_align = var_CreateGetInteger( p_dec, "subsdec-align" );

    ParseImageAttachments( p_dec );

    if( var_CreateGetBool( p_dec, "subsdec-formatted" ) )
    {
        if( p_dec->fmt_in.i_extra > 0 )
            ParseUSFHeader( p_dec );
    }

    return VLC_SUCCESS;
}

/****************************************************************************
 * DecodeBlock: the whole thing
 ****************************************************************************
 * This function must be fed with complete subtitles units.
 ****************************************************************************/
static subpicture_t *DecodeBlock( decoder_t *p_dec, block_t **pp_block )
{
    subpicture_t *p_spu;
    block_t *p_block;

    if( !pp_block || *pp_block == NULL )
        return NULL;

    p_block = *pp_block;

    p_spu = ParseText( p_dec, p_block );

    block_Release( p_block );
    *pp_block = NULL;

    return p_spu;
}

/*****************************************************************************
 * CloseDecoder: clean up the decoder
 *****************************************************************************/
static void CloseDecoder( vlc_object_t *p_this )
{
    decoder_t *p_dec = (decoder_t *)p_this;
    decoder_sys_t *p_sys = p_dec->p_sys;

    if( p_sys->pp_ssa_styles )
    {
        int i;
        for( i = 0; i < p_sys->i_ssa_styles; i++ )
        {
            if( !p_sys->pp_ssa_styles[i] )
                continue;

            free( p_sys->pp_ssa_styles[i]->psz_stylename );
            free( p_sys->pp_ssa_styles[i]->font_style.psz_fontname );
            free( p_sys->pp_ssa_styles[i] );
        }
        TAB_CLEAN( p_sys->i_ssa_styles, p_sys->pp_ssa_styles );
    }
    if( p_sys->pp_images )
    {
        int i;
        for( i = 0; i < p_sys->i_images; i++ )
        {
            if( !p_sys->pp_images[i] )
                continue;

            if( p_sys->pp_images[i]->p_pic )
                picture_Release( p_sys->pp_images[i]->p_pic );
            free( p_sys->pp_images[i]->psz_filename );

            free( p_sys->pp_images[i] );
        }
        TAB_CLEAN( p_sys->i_images, p_sys->pp_images );
    }

    free( p_sys );
}

/*****************************************************************************
 * ParseText: parse an text subtitle packet and send it to the video output
 *****************************************************************************/
static subpicture_t *ParseText( decoder_t *p_dec, block_t *p_block )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    subpicture_t *p_spu = NULL;
    char *psz_subtitle = NULL;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts == 0 )
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

    /* Should be resiliant against bad subtitles */
    psz_subtitle = strndup( (const char *)p_block->p_buffer,
                            p_block->i_buffer );
    if( psz_subtitle == NULL )
        return NULL;

    /* USF Subtitles are mandated to be UTF-8 -- make sure it is */
    if (EnsureUTF8( psz_subtitle ) == NULL)
    {
        msg_Err( p_dec, "USF subtitles must be in UTF-8 format.\n"
                 "This stream contains USF subtitles which aren't." );
    }

    /* Create the subpicture unit */
    p_spu = decoder_NewSubpicture( p_dec );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        free( psz_subtitle );
        return NULL;
    }

    /* Decode USF strings */
    p_spu->p_region = ParseUSFString( p_dec, psz_subtitle, p_spu );

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer = (p_block->i_length == 0);
    p_spu->b_absolute = false;
    p_spu->i_original_picture_width = p_sys->i_original_width;
    p_spu->i_original_picture_height = p_sys->i_original_height;

    free( psz_subtitle );

    return p_spu;
}

static char *GrabAttributeValue( const char *psz_attribute,
                                 const char *psz_tag_start )
{
    if( psz_attribute && psz_tag_start )
    {
        char *psz_tag_end = strchr( psz_tag_start, '>' );
        char *psz_found   = strcasestr( psz_tag_start, psz_attribute );

        if( psz_found )
        {
            psz_found += strlen( psz_attribute );

            if(( *(psz_found++) == '=' ) &&
               ( *(psz_found++) == '\"' ))
            {
                if( psz_found < psz_tag_end )
                {
                    int   i_len = strcspn( psz_found, "\"" );
                    return strndup( psz_found, i_len );
                }
            }
        }
    }
    return NULL;
}

static ssa_style_t *ParseStyle( decoder_sys_t *p_sys, char *psz_subtitle )
{
    ssa_style_t *p_style   = NULL;
    char        *psz_style = GrabAttributeValue( "style", psz_subtitle );

    if( psz_style )
    {
        int i;

        for( i = 0; i < p_sys->i_ssa_styles; i++ )
        {
            if( !strcmp( p_sys->pp_ssa_styles[i]->psz_stylename, psz_style ) )
                p_style = p_sys->pp_ssa_styles[i];
        }
        free( psz_style );
    }
    return p_style;
}

static int ParsePositionAttributeList( char *psz_subtitle, int *i_align,
                                       int *i_x, int *i_y )
{
    int   i_mask = 0;

    char *psz_align    = GrabAttributeValue( "alignment", psz_subtitle );
    char *psz_margin_x = GrabAttributeValue( "horizontal-margin", psz_subtitle );
    char *psz_margin_y = GrabAttributeValue( "vertical-margin", psz_subtitle );
    /* -- UNSUPPORTED
    char *psz_relative = GrabAttributeValue( "relative-to", psz_subtitle );
    char *psz_rotate_x = GrabAttributeValue( "rotate-x", psz_subtitle );
    char *psz_rotate_y = GrabAttributeValue( "rotate-y", psz_subtitle );
    char *psz_rotate_z = GrabAttributeValue( "rotate-z", psz_subtitle );
    */

    *i_align = SUBPICTURE_ALIGN_BOTTOM;
    *i_x = 0;
    *i_y = 0;

    if( psz_align )
    {
        if( !strcasecmp( "TopLeft", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp( "TopCenter", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_TOP;
        else if( !strcasecmp( "TopRight", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_RIGHT;
        else if( !strcasecmp( "MiddleLeft", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp( "MiddleCenter", psz_align ) )
            *i_align = 0;
        else if( !strcasecmp( "MiddleRight", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_RIGHT;
        else if( !strcasecmp( "BottomLeft", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT;
        else if( !strcasecmp( "BottomCenter", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_BOTTOM;
        else if( !strcasecmp( "BottomRight", psz_align ) )
            *i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;

        i_mask |= ATTRIBUTE_ALIGNMENT;
        free( psz_align );
    }
    if( psz_margin_x )
    {
        *i_x = atoi( psz_margin_x );
        if( strchr( psz_margin_x, '%' ) )
            i_mask |= ATTRIBUTE_X_PERCENT;
        else
            i_mask |= ATTRIBUTE_X;

        free( psz_margin_x );
    }
    if( psz_margin_y )
    {
        *i_y = atoi( psz_margin_y );
        if( strchr( psz_margin_y, '%' ) )
            i_mask |= ATTRIBUTE_Y_PERCENT;
        else
            i_mask |= ATTRIBUTE_Y;

        free( psz_margin_y );
    }
    return i_mask;
}

static void SetupPositions( subpicture_region_t *p_region, char *psz_subtitle )
{
    int           i_mask = 0;
    int           i_align;
    int           i_x, i_y;

    i_mask = ParsePositionAttributeList( psz_subtitle, &i_align, &i_x, &i_y );

    if( i_mask & ATTRIBUTE_ALIGNMENT )
        p_region->i_align = i_align;

    /* TODO: Setup % based offsets properly, without adversely affecting
     *       everything else in vlc. Will address with separate patch, to
     *       prevent this one being any more complicated.
     */
    if( i_mask & ATTRIBUTE_X )
        p_region->i_x = i_x;
    else if( i_mask & ATTRIBUTE_X_PERCENT )
        p_region->i_x = 0;

    if( i_mask & ATTRIBUTE_Y )
        p_region->i_y = i_y;
    else if( i_mask & ATTRIBUTE_Y_PERCENT )
        p_region->i_y = 0;
}

static subpicture_region_t *CreateTextRegion( decoder_t *p_dec,
                                              subpicture_t *p_spu,
                                              char *psz_subtitle,
                                              int i_len,
                                              int i_sys_align )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    subpicture_region_t  *p_text_region;
    video_format_t        fmt;

    /* Create a new subpicture region */
    memset( &fmt, 0, sizeof(video_format_t) );
    fmt.i_chroma = VLC_FOURCC('T','E','X','T');
    fmt.i_aspect = 0;
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_text_region = subpicture_region_New( &fmt );

    if( p_text_region != NULL )
    {
        ssa_style_t  *p_style = NULL;

        p_text_region->psz_text = NULL;
        p_text_region->psz_html = strndup( psz_subtitle, i_len );
        if( ! p_text_region->psz_html )
        {
            subpicture_region_Delete( p_text_region );
            return NULL;
        }

        p_style = ParseStyle( p_sys, p_text_region->psz_html );
        if( !p_style )
        {
            int i;

            for( i = 0; i < p_sys->i_ssa_styles; i++ )
            {
                if( !strcasecmp( p_sys->pp_ssa_styles[i]->psz_stylename, "Default" ) )
                    p_style = p_sys->pp_ssa_styles[i];
            }
        }

        if( p_style )
        {
            msg_Dbg( p_dec, "style is: %s", p_style->psz_stylename );

            p_text_region->p_style = &p_style->font_style;
            p_text_region->i_align = p_style->i_align;

            /* TODO: Setup % based offsets properly, without adversely affecting
             *       everything else in vlc. Will address with separate patch,
             *       to prevent this one being any more complicated.

                     * p_style->i_margin_percent_h;
                     * p_style->i_margin_percent_v;
             */
            p_text_region->i_x         = p_style->i_margin_h;
            p_text_region->i_y         = p_style->i_margin_v;

        }
        else
        {
            p_text_region->i_align = SUBPICTURE_ALIGN_BOTTOM | i_sys_align;
            p_text_region->i_x = i_sys_align ? 20 : 0;
            p_text_region->i_y = 10;
        }
        /* Look for position arguments which may override the style-based
         * defaults.
         */
        SetupPositions( p_text_region, psz_subtitle );

        p_text_region->p_next = NULL;
    }
    return p_text_region;
}

static int ParseImageAttachments( decoder_t *p_dec )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    input_attachment_t  **pp_attachments;
    int                   i_attachments_cnt;
    int                   k = 0;

    if( VLC_SUCCESS != decoder_GetInputAttachments( p_dec, &pp_attachments, &i_attachments_cnt ))
        return VLC_EGENERIC;

    for( k = 0; k < i_attachments_cnt; k++ )
    {
        input_attachment_t *p_attach = pp_attachments[k];

        vlc_fourcc_t type = image_Mime2Fourcc( p_attach->psz_mime );

        if( ( type != 0 ) &&
            ( p_attach->i_data > 0 ) &&
            ( p_attach->p_data != NULL ) )
        {
            picture_t         *p_pic = NULL;
            image_handler_t   *p_image;

            p_image = image_HandlerCreate( p_dec );
            if( p_image != NULL )
            {
                block_t   *p_block;

                p_block = block_New( p_image->p_parent, p_attach->i_data );

                if( p_block != NULL )
                {
                    video_format_t     fmt_in;
                    video_format_t     fmt_out;

                    memcpy( p_block->p_buffer, p_attach->p_data, p_attach->i_data );

                    memset( &fmt_in,  0, sizeof( video_format_t));
                    memset( &fmt_out, 0, sizeof( video_format_t));

                    fmt_in.i_chroma  = type;
                    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');

                    /* Find a suitable decoder module */
                    if( module_exists( "sdl_image" ) )
                    {
                        /* ffmpeg thinks it can handle bmp properly but it can't (at least
                         * not all of them), so use sdl_image if it is available */

                        vlc_value_t val;

                        var_Create( p_dec, "codec", VLC_VAR_MODULE | VLC_VAR_DOINHERIT );
                        val.psz_string = (char*) "sdl_image";
                        var_Set( p_dec, "codec", val );
                    }

                    p_pic = image_Read( p_image, p_block, &fmt_in, &fmt_out );
                    var_Destroy( p_dec, "codec" );
                }

                image_HandlerDelete( p_image );
            }
            if( p_pic )
            {
                image_attach_t *p_picture = malloc( sizeof(image_attach_t) );

                if( p_picture )
                {
                    p_picture->psz_filename = strdup( p_attach->psz_name );
                    p_picture->p_pic = p_pic;

                    TAB_APPEND( p_sys->i_images, p_sys->pp_images, p_picture );
                }
            }
        }
        vlc_input_attachment_Delete( pp_attachments[ k ] );
    }
    free( pp_attachments );

    return VLC_SUCCESS;
}

static void ParseUSFHeaderTags( decoder_t *p_dec, xml_reader_t *p_xml_reader )
{
    decoder_sys_t *p_sys = p_dec->p_sys;
    char *psz_node;
    ssa_style_t *p_style = NULL;
    int i_style_level = 0;
    int i_metadata_level = 0;

    while ( xml_ReaderRead( p_xml_reader ) == 1 )
    {
        switch ( xml_ReaderNodeType( p_xml_reader ) )
        {
            case XML_READER_TEXT:
            case XML_READER_NONE:
                break;
            case XML_READER_ENDELEM:
                psz_node = xml_ReaderName( p_xml_reader );

                if( !psz_node )
                    break;
                switch (i_style_level)
                {
                    case 0:
                        if( !strcasecmp( "metadata", psz_node ) && (i_metadata_level == 1) )
                        {
                            i_metadata_level--;
                        }
                        break;
                    case 1:
                        if( !strcasecmp( "styles", psz_node ) )
                        {
                            i_style_level--;
                        }
                        break;
                    case 2:
                        if( !strcasecmp( "style", psz_node ) )
                        {
                            TAB_APPEND( p_sys->i_ssa_styles, p_sys->pp_ssa_styles, p_style );

                            p_style = NULL;
                            i_style_level--;
                        }
                        break;
                }

                free( psz_node );
                break;
            case XML_READER_STARTELEM:
                psz_node = xml_ReaderName( p_xml_reader );

                if( !psz_node )
                    break;

                if( !strcasecmp( "metadata", psz_node ) && (i_style_level == 0) )
                {
                    i_metadata_level++;
                }
                else if( !strcasecmp( "resolution", psz_node ) &&
                         ( i_metadata_level == 1) )
                {
                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "x", psz_name ) )
                                p_sys->i_original_width = atoi( psz_value );
                            else if( !strcasecmp( "y", psz_name ) )
                                p_sys->i_original_height = atoi( psz_value );
                        }
                        free( psz_name );
                        free( psz_value );
                    }
                }
                else if( !strcasecmp( "styles", psz_node ) && (i_style_level == 0) )
                {
                    i_style_level++;
                }
                else if( !strcasecmp( "style", psz_node ) && (i_style_level == 1) )
                {
                    i_style_level++;

                    p_style = calloc( 1, sizeof(ssa_style_t) );
                    if( ! p_style )
                    {
                        free( psz_node );
                        return;
                    }
                    /* All styles are supposed to default to Default, and then
                     * one or more settings are over-ridden.
                     * At the moment this only effects styles defined AFTER
                     * Default in the XML
                     */
                    int i;
                    for( i = 0; i < p_sys->i_ssa_styles; i++ )
                    {
                        if( !strcasecmp( p_sys->pp_ssa_styles[i]->psz_stylename, "Default" ) )
                        {
                            ssa_style_t *p_default_style = p_sys->pp_ssa_styles[i];

                            memcpy( p_style, p_default_style, sizeof( ssa_style_t ) );
                            p_style->font_style.psz_fontname = strdup( p_style->font_style.psz_fontname );
                            p_style->psz_stylename = NULL;
                        }
                    }

                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "name", psz_name ) )
                                p_style->psz_stylename = strdup( psz_value);
                        }
                        free( psz_name );
                        free( psz_value );
                    }
                }
                else if( !strcasecmp( "fontstyle", psz_node ) && (i_style_level == 2) )
                {
                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "face", psz_name ) )
                            {
                                free( p_style->font_style.psz_fontname );
                                p_style->font_style.psz_fontname = strdup( psz_value );
                            }
                            else if( !strcasecmp( "size", psz_name ) )
                            {
                                if( ( *psz_value == '+' ) || ( *psz_value == '-' ) )
                                {
                                    int i_value = atoi( psz_value );

                                    if( ( i_value >= -5 ) && ( i_value <= 5 ) )
                                        p_style->font_style.i_font_size  +=
                                            ( i_value * p_style->font_style.i_font_size ) / 10;
                                    else if( i_value < -5 )
                                        p_style->font_style.i_font_size  = - i_value;
                                    else if( i_value > 5 )
                                        p_style->font_style.i_font_size  = i_value;
                                }
                                else
                                    p_style->font_style.i_font_size  = atoi( psz_value );
                            }
                            else if( !strcasecmp( "italic", psz_name ) )
                            {
                                if( !strcasecmp( "yes", psz_value ))
                                    p_style->font_style.i_style_flags |= STYLE_ITALIC;
                                else
                                    p_style->font_style.i_style_flags &= ~STYLE_ITALIC;
                            }
                            else if( !strcasecmp( "weight", psz_name ) )
                            {
                                if( !strcasecmp( "bold", psz_value ))
                                    p_style->font_style.i_style_flags |= STYLE_BOLD;
                                else
                                    p_style->font_style.i_style_flags &= ~STYLE_BOLD;
                            }
                            else if( !strcasecmp( "underline", psz_name ) )
                            {
                                if( !strcasecmp( "yes", psz_value ))
                                    p_style->font_style.i_style_flags |= STYLE_UNDERLINE;
                                else
                                    p_style->font_style.i_style_flags &= ~STYLE_UNDERLINE;
                            }
                            else if( !strcasecmp( "color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_font_color = (col & 0x00ffffff);
                                    p_style->font_style.i_font_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "outline-color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_outline_color = (col & 0x00ffffff);
                                    p_style->font_style.i_outline_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "outline-level", psz_name ) )
                            {
                                p_style->font_style.i_outline_width = atoi( psz_value );
                            }
                            else if( !strcasecmp( "shadow-color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_shadow_color = (col & 0x00ffffff);
                                    p_style->font_style.i_shadow_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "shadow-level", psz_name ) )
                            {
                                p_style->font_style.i_shadow_width = atoi( psz_value );
                            }
                            else if( !strcasecmp( "back-color", psz_name ) )
                            {
                                if( *psz_value == '#' )
                                {
                                    unsigned long col = strtol(psz_value+1, NULL, 16);
                                    p_style->font_style.i_karaoke_background_color = (col & 0x00ffffff);
                                    p_style->font_style.i_karaoke_background_alpha = (col >> 24) & 0xff;
                                }
                            }
                            else if( !strcasecmp( "spacing", psz_name ) )
                            {
                                p_style->font_style.i_spacing = atoi( psz_value );
                            }
                        }
                        free( psz_name );
                        free( psz_value );
                    }
                }
                else if( !strcasecmp( "position", psz_node ) && (i_style_level == 2) )
                {
                    while ( xml_ReaderNextAttr( p_xml_reader ) == VLC_SUCCESS )
                    {
                        char *psz_name = xml_ReaderName ( p_xml_reader );
                        char *psz_value = xml_ReaderValue ( p_xml_reader );

                        if( psz_name && psz_value )
                        {
                            if( !strcasecmp( "alignment", psz_name ) )
                            {
                                if( !strcasecmp( "TopLeft", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;
                                else if( !strcasecmp( "TopCenter", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_TOP;
                                else if( !strcasecmp( "TopRight", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_RIGHT;
                                else if( !strcasecmp( "MiddleLeft", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_LEFT;
                                else if( !strcasecmp( "MiddleCenter", psz_value ) )
                                    p_style->i_align = 0;
                                else if( !strcasecmp( "MiddleRight", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_RIGHT;
                                else if( !strcasecmp( "BottomLeft", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT;
                                else if( !strcasecmp( "BottomCenter", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_BOTTOM;
                                else if( !strcasecmp( "BottomRight", psz_value ) )
                                    p_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;
                            }
                            else if( !strcasecmp( "horizontal-margin", psz_name ) )
                            {
                                if( strchr( psz_value, '%' ) )
                                {
                                    p_style->i_margin_h = 0;
                                    p_style->i_margin_percent_h = atoi( psz_value );
                                }
                                else
                                {
                                    p_style->i_margin_h = atoi( psz_value );
                                    p_style->i_margin_percent_h = 0;
                                }
                            }
                            else if( !strcasecmp( "vertical-margin", psz_name ) )
                            {
                                if( strchr( psz_value, '%' ) )
                                {
                                    p_style->i_margin_v = 0;
                                    p_style->i_margin_percent_v = atoi( psz_value );
                                }
                                else
                                {
                                    p_style->i_margin_v = atoi( psz_value );
                                    p_style->i_margin_percent_v = 0;
                                }
                            }
                        }
                        free( psz_name );
                        free( psz_value );
                    }
                }

                free( psz_node );
                break;
        }
    }
    free( p_style );
}



static subpicture_region_t *ParseUSFString( decoder_t *p_dec,
                                            char *psz_subtitle,
                                            subpicture_t *p_spu_in )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    subpicture_t         *p_spu = p_spu_in;
    subpicture_region_t  *p_region_first = NULL;
    subpicture_region_t  *p_region_upto  = p_region_first;

    while( *psz_subtitle )
    {
        if( *psz_subtitle == '<' )
        {
            char *psz_end = NULL;

            if(( !strncasecmp( psz_subtitle, "<text ", 6 )) ||
               ( !strncasecmp( psz_subtitle, "<text>", 6 )))
            {
                psz_end = strcasestr( psz_subtitle, "</text>" );

                if( psz_end )
                {
                    subpicture_region_t  *p_text_region;

                    psz_end += strcspn( psz_end, ">" ) + 1;

                    p_text_region = CreateTextRegion( p_dec,
                                                      p_spu,
                                                      psz_subtitle,
                                                      psz_end - psz_subtitle,
                                                      p_sys->i_align );

                    if( p_text_region )
                    {
                        p_text_region->psz_text = CreatePlainText( p_text_region->psz_html );

                        if( ! var_CreateGetBool( p_dec, "subsdec-formatted" ) )
                        {
                            free( p_text_region->psz_html );
                            p_text_region->psz_html = NULL;
                        }
                    }

                    if( !p_region_first )
                    {
                        p_region_first = p_region_upto = p_text_region;
                    }
                    else if( p_text_region )
                    {
                        p_region_upto->p_next = p_text_region;
                        p_region_upto = p_region_upto->p_next;
                    }
                }
            }
            else if(( !strncasecmp( psz_subtitle, "<karaoke ", 9 )) ||
                    ( !strncasecmp( psz_subtitle, "<karaoke>", 9 )))
            {
                psz_end = strcasestr( psz_subtitle, "</karaoke>" );

                if( psz_end )
                {
                    subpicture_region_t  *p_text_region;

                    psz_end += strcspn( psz_end, ">" ) + 1;

                    p_text_region = CreateTextRegion( p_dec,
                                                      p_spu,
                                                      psz_subtitle,
                                                      psz_end - psz_subtitle,
                                                      p_sys->i_align );

                    if( p_text_region )
                    {
                        if( ! var_CreateGetBool( p_dec, "subsdec-formatted" ) )
                        {
                            free( p_text_region->psz_html );
                            p_text_region->psz_html = NULL;
                        }
                    }
                    if( !p_region_first )
                    {
                        p_region_first = p_region_upto = p_text_region;
                    }
                    else if( p_text_region )
                    {
                        p_region_upto->p_next = p_text_region;
                        p_region_upto = p_region_upto->p_next;
                    }
                }
            }
            else if(( !strncasecmp( psz_subtitle, "<image ", 7 )) ||
                    ( !strncasecmp( psz_subtitle, "<image>", 7 )))
            {
                subpicture_region_t *p_image_region = NULL;

                char *psz_end = strcasestr( psz_subtitle, "</image>" );
                char *psz_content = strchr( psz_subtitle, '>' );
                int   i_transparent = -1;

                /* If a colorkey parameter is specified, then we have to map
                 * that index in the picture through as transparent (it is
                 * required by the USF spec but is also recommended that if the
                 * creator really wants a transparent colour that they use a
                 * type like PNG that properly supports it; this goes doubly
                 * for VLC because the pictures are stored internally in YUV
                 * and the resulting colour-matching may not produce the
                 * desired results.)
                 */
                char *psz_tmp = GrabAttributeValue( "colorkey", psz_subtitle );
                if( psz_tmp )
                {
                    if( *psz_tmp == '#' )
                        i_transparent = strtol( psz_tmp + 1, NULL, 16 ) & 0x00ffffff;
                    free( psz_tmp );
                }
                if( psz_content && ( psz_content < psz_end ) )
                {
                    char *psz_filename = strndup( &psz_content[1], psz_end - &psz_content[1] );
                    if( psz_filename )
                    {
                        p_image_region = LoadEmbeddedImage( p_dec, p_spu,
                                            psz_filename, i_transparent );
                        free( psz_filename );
                    }
                }

                if( psz_end ) psz_end += strcspn( psz_end, ">" ) + 1;

                if( p_image_region )
                {
                    SetupPositions( p_image_region, psz_subtitle );

                    p_image_region->p_next   = NULL;
                    p_image_region->psz_text = NULL;
                    p_image_region->psz_html = NULL;

                }
                if( !p_region_first )
                {
                    p_region_first = p_region_upto = p_image_region;
                }
                else if( p_image_region )
                {
                    p_region_upto->p_next = p_image_region;
                    p_region_upto = p_region_upto->p_next;
                }
            }
            if( psz_end )
                psz_subtitle = psz_end - 1;

            psz_subtitle += strcspn( psz_subtitle, ">" );
        }

        psz_subtitle++;
    }

    return p_region_first;
}

/*****************************************************************************
 * ParseUSFHeader: Retrieve global formatting information etc
 *****************************************************************************/
static void ParseUSFHeader( decoder_t *p_dec )
{
    stream_t      *p_sub = NULL;
    xml_t         *p_xml = NULL;
    xml_reader_t  *p_xml_reader = NULL;

    p_sub = stream_MemoryNew( VLC_OBJECT(p_dec),
                              p_dec->fmt_in.p_extra,
                              p_dec->fmt_in.i_extra,
                              true );
    if( !p_sub )
        return;

    p_xml = xml_Create( p_dec );
    if( p_xml )
    {
        p_xml_reader = xml_ReaderCreate( p_xml, p_sub );
        if( p_xml_reader )
        {
            /* Look for Root Node */
            if( xml_ReaderRead( p_xml_reader ) == 1 )
            {
                char *psz_node = xml_ReaderName( p_xml_reader );

                if( !strcasecmp( "usfsubtitles", psz_node ) )
                    ParseUSFHeaderTags( p_dec, p_xml_reader );

                free( psz_node );
            }

            xml_ReaderDelete( p_xml, p_xml_reader );
        }
        xml_Delete( p_xml );
    }
    stream_Delete( p_sub );
}

/* Function now handles tags which has attribute values, and tries
 * to deal with &' commands too. It no longer modifies the string
 * in place, so that the original text can be reused
 */
static char *StripTags( char *psz_subtitle )
{
    char *psz_text_start;
    char *psz_text;

    psz_text = psz_text_start = malloc( strlen( psz_subtitle ) + 1 );
    if( !psz_text_start )
        return NULL;

    while( *psz_subtitle )
    {
        /* Mask out any pre-existing LFs in the subtitle */
        if( *psz_subtitle == '\n' )
            *psz_subtitle = ' ';

        if( *psz_subtitle == '<' )
        {
            if( strncasecmp( psz_subtitle, "<br/>", 5 ) == 0 )
                *psz_text++ = '\n';

            psz_subtitle += strcspn( psz_subtitle, ">" );
        }
        else if( *psz_subtitle == '&' )
        {
            if( !strncasecmp( psz_subtitle, "&lt;", 4 ))
            {
                *psz_text++ = '<';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else if( !strncasecmp( psz_subtitle, "&gt;", 4 ))
            {
                *psz_text++ = '>';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else if( !strncasecmp( psz_subtitle, "&amp;", 5 ))
            {
                *psz_text++ = '&';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else if( !strncasecmp( psz_subtitle, "&quot;", 6 ))
            {
                *psz_text++ = '\"';
                psz_subtitle += strcspn( psz_subtitle, ";" );
            }
            else
            {
                /* Assume it is just a normal ampersand */
                *psz_text++ = '&';
            }
        }
        else
        {
            *psz_text++ = *psz_subtitle;
        }

        psz_subtitle++;
    }
    *psz_text = '\0';
    psz_text_start = realloc( psz_text_start, strlen( psz_text_start ) + 1 );

    return psz_text_start;
}

/* Turn a HTML subtitle, turn into a plain-text version,
 *  complete with sensible whitespace compaction
 */

static char *CreatePlainText( char *psz_subtitle )
{
    char *psz_text = StripTags( psz_subtitle );
    char *s;

    if( !psz_text )
        return NULL;

    s = strpbrk( psz_text, "\t\r\n " );
    while( s )
    {
        int   k;
        char  spc = ' ';
        int   i_whitespace = strspn( s, "\t\r\n " );

        /* Favour '\n' over other whitespaces - if one of these
         * occurs in the whitespace use a '\n' as our value,
         * otherwise just use a ' '
         */
        for( k = 0; k < i_whitespace; k++ )
            if( s[k] == '\n' ) spc = '\n';

        if( i_whitespace > 1 )
        {
            memmove( &s[1],
                     &s[i_whitespace],
                     strlen( s ) - i_whitespace + 1 );
        }
        *s++ = spc;

        s = strpbrk( s, "\t\r\n " );
    }
    return psz_text;
}

/****************************************************************************
 * download and resize image located at psz_url
 ***************************************************************************/
static subpicture_region_t *LoadEmbeddedImage( decoder_t *p_dec,
                                               subpicture_t *p_spu,
                                               const char *psz_filename,
                                               int i_transparent_color )
{
    decoder_sys_t         *p_sys = p_dec->p_sys;
    subpicture_region_t   *p_region;
    video_format_t         fmt_out;
    int                    k;
    picture_t             *p_pic = NULL;

    for( k = 0; k < p_sys->i_images; k++ )
    {
        if( p_sys->pp_images &&
            !strcmp( p_sys->pp_images[k]->psz_filename, psz_filename ) )
        {
            p_pic = p_sys->pp_images[k]->p_pic;
            break;
        }
    }

    if( !p_pic )
    {
        msg_Err( p_dec, "Unable to read image %s", psz_filename );
        return NULL;
    }

    /* Display the feed's image */
    memset( &fmt_out, 0, sizeof( video_format_t));

    fmt_out.i_chroma = VLC_FOURCC('Y','U','V','A');
    fmt_out.i_aspect = VOUT_ASPECT_FACTOR;
    fmt_out.i_sar_num = fmt_out.i_sar_den = 1;
    fmt_out.i_width =
        fmt_out.i_visible_width = p_pic->format.i_visible_width;
    fmt_out.i_height =
        fmt_out.i_visible_height = p_pic->format.i_visible_height;

    p_region = subpicture_region_New( &fmt_out );
    if( !p_region )
    {
        msg_Err( p_dec, "cannot allocate SPU region" );
        return NULL;
    }
    assert( p_pic->format.i_chroma == VLC_FOURCC('Y','U','V','A') );
    /* FIXME the copy is probably not needed anymore */
    picture_CopyPixels( p_region->p_picture, p_pic );

    /* This isn't the best way to do this - if you really want transparency, then
     * you're much better off using an image type that supports it like PNG. The
     * spec requires this support though.
     */
    if( i_transparent_color > 0 )
    {
        int i_r = ( i_transparent_color >> 16 ) & 0xff;
        int i_g = ( i_transparent_color >>  8 ) & 0xff;
        int i_b = ( i_transparent_color       ) & 0xff;

        /* FIXME it cannot work as the yuv conversion code will probably NOT match
         * this one  */
        int i_y = ( ( (  66 * i_r + 129 * i_g +  25 * i_b + 128 ) >> 8 ) + 16 );
        int i_u =   ( ( -38 * i_r -  74 * i_g + 112 * i_b + 128 ) >> 8 ) + 128 ;
        int i_v =   ( ( 112 * i_r -  94 * i_g -  18 * i_b + 128 ) >> 8 ) + 128 ;

        assert( p_region->fmt.i_chroma == VLC_FOURCC('Y','U','V','A') );
        for( unsigned int y = 0; y < p_region->fmt.i_height; y++ )
        {
            for( unsigned int x = 0; x < p_region->fmt.i_width; x++ )
            {
                if( p_region->p_picture->Y_PIXELS[y*p_region->p_picture->Y_PITCH + x] != i_y ||
                    p_region->p_picture->U_PIXELS[y*p_region->p_picture->U_PITCH + x] != i_u ||
                    p_region->p_picture->V_PIXELS[y*p_region->p_picture->V_PITCH + x] != i_v )
                    continue;
                p_region->p_picture->A_PIXELS[y*p_region->p_picture->A_PITCH + x] = 0;

            }
        }
    }
    return p_region;
}

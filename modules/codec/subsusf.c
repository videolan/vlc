/*****************************************************************************
 * subsusf.c : USF subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 VLC authors and VideoLAN
 *
 * Authors: Bernie Purcell <bitmap@videolan.org>
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
#include <assert.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_modules.h>
#include <vlc_codec.h>
#include <vlc_input.h>
#include <vlc_charset.h>
#include <vlc_image.h>
#include <vlc_xml.h>
#include <vlc_stream.h>

/*****************************************************************************
 * Module descriptor.
 *****************************************************************************/
static int  OpenDecoder   ( vlc_object_t * );
static void CloseDecoder  ( vlc_object_t * );

#define FORMAT_TEXT N_("Formatted Subtitles")
#define FORMAT_LONGTEXT N_("Some subtitle formats allow for text formatting. " \
 "VLC partly implements this, but you can choose to disable all formatting.")

vlc_module_begin ()
    set_capability( "spu decoder", 40 )
    set_shortname( N_("USFSubs"))
    set_description( N_("USF subtitles decoder") )
    set_callbacks( OpenDecoder, CloseDecoder )
    set_category( CAT_INPUT )
    set_subcategory( SUBCAT_INPUT_SCODEC )
    add_bool( "subsdec-formatted", true, FORMAT_TEXT, FORMAT_LONGTEXT,
                 false )
vlc_module_end ()


/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
enum
{
    ATTRIBUTE_ALIGNMENT = (1 << 0),
    ATTRIBUTE_X         = (1 << 1),
    ATTRIBUTE_X_PERCENT = (1 << 2),
    ATTRIBUTE_Y         = (1 << 3),
    ATTRIBUTE_Y_PERCENT = (1 << 4),
};

typedef struct
{
    char       *psz_filename;
    picture_t  *p_pic;
} image_attach_t;

typedef struct
{
    char *          psz_stylename; /* The name of the style, no comma's allowed */
    text_style_t *  p_style;
    int             i_align;
    int             i_margin_h;
    int             i_margin_v;
    int             i_margin_percent_h;
    int             i_margin_percent_v;
}  ssa_style_t;

typedef struct
{
    int                 i_original_height;
    int                 i_original_width;
    int                 i_align;          /* Subtitles alignment on the vout */

    ssa_style_t         **pp_ssa_styles;
    int                 i_ssa_styles;

    image_attach_t      **pp_images;
    int                 i_images;
} decoder_sys_t;

static int           DecodeBlock   ( decoder_t *, block_t * );
static char         *CreatePlainText( char * );
static char         *StripTags( char *psz_subtitle );
static int           ParseImageAttachments( decoder_t *p_dec );

static subpicture_t        *ParseText     ( decoder_t *, block_t * );
static void                 ParseUSFHeader( decoder_t * );
static subpicture_region_t *ParseUSFString( decoder_t *, char * );
static subpicture_region_t *LoadEmbeddedImage( decoder_t *p_dec, const char *psz_filename, int i_transparent_color );

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

    if( p_dec->fmt_in.i_codec != VLC_CODEC_USF )
        return VLC_EGENERIC;

    /* Allocate the memory needed to store the decoder's structure */
    if( ( p_dec->p_sys = p_sys = calloc(1, sizeof(decoder_sys_t)) ) == NULL )
        return VLC_ENOMEM;

    p_dec->pf_decode = DecodeBlock;
    p_dec->fmt_out.i_codec = 0;

    /* init of p_sys */
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
static int DecodeBlock( decoder_t *p_dec, block_t *p_block )
{
    subpicture_t *p_spu;

    if( p_block == NULL ) /* No Drain */
        return VLCDEC_SUCCESS;

    p_spu = ParseText( p_dec, p_block );

    block_Release( p_block );
    if( p_spu != NULL )
        decoder_QueueSub( p_dec, p_spu );
    return VLCDEC_SUCCESS;
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
        for( int i = 0; i < p_sys->i_ssa_styles; i++ )
        {
            if( !p_sys->pp_ssa_styles[i] )
                continue;

            free( p_sys->pp_ssa_styles[i]->psz_stylename );
            text_style_Delete( p_sys->pp_ssa_styles[i]->p_style );
            free( p_sys->pp_ssa_styles[i] );
        }
        TAB_CLEAN( p_sys->i_ssa_styles, p_sys->pp_ssa_styles );
    }
    if( p_sys->pp_images )
    {
        for( int i = 0; i < p_sys->i_images; i++ )
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

    if( p_block->i_flags & BLOCK_FLAG_CORRUPTED )
        return NULL;

    /* We cannot display a subpicture with no date */
    if( p_block->i_pts == VLC_TICK_INVALID )
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
    p_spu = decoder_NewSubpicture( p_dec, NULL );
    if( !p_spu )
    {
        msg_Warn( p_dec, "can't get spu buffer" );
        free( psz_subtitle );
        return NULL;
    }

    /* Decode USF strings */
    p_spu->p_region = ParseUSFString( p_dec, psz_subtitle );

    p_spu->i_start = p_block->i_pts;
    p_spu->i_stop = p_block->i_pts + p_block->i_length;
    p_spu->b_ephemer = (p_block->i_length == VLC_TICK_INVALID);
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
    ssa_style_t *p_ssa_style = NULL;
    char        *psz_style = GrabAttributeValue( "style", psz_subtitle );

    if( psz_style )
    {
        for( int i = 0; i < p_sys->i_ssa_styles; i++ )
        {
            if( !strcmp( p_sys->pp_ssa_styles[i]->psz_stylename, psz_style ) )
                p_ssa_style = p_sys->pp_ssa_styles[i];
        }
        free( psz_style );
    }
    return p_ssa_style;
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
                                              char *psz_subtitle,
                                              int i_sys_align )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    subpicture_region_t  *p_text_region;
    video_format_t        fmt;

    /* Create a new subpicture region */
    video_format_Init( &fmt, VLC_CODEC_TEXT );
    fmt.i_width = fmt.i_height = 0;
    fmt.i_x_offset = fmt.i_y_offset = 0;
    p_text_region = subpicture_region_New( &fmt );
    video_format_Clean( &fmt );

    if( p_text_region != NULL )
    {
        ssa_style_t  *p_ssa_style = NULL;

        p_ssa_style = ParseStyle( p_sys, psz_subtitle );
        if( !p_ssa_style )
        {
            for( int i = 0; i < p_sys->i_ssa_styles; i++ )
            {
                if( !strcasecmp( p_sys->pp_ssa_styles[i]->psz_stylename, "Default" ) )
                    p_ssa_style = p_sys->pp_ssa_styles[i];
            }
        }

        /* Set default or user align/magin.
         * Style overriden if no user value. */
        p_text_region->i_x = i_sys_align > 0 ? 20 : 0;
        p_text_region->i_y = 10;
        p_text_region->i_align = SUBPICTURE_ALIGN_BOTTOM |
                                 ((i_sys_align > 0) ? i_sys_align : 0);

        if( p_ssa_style )
        {
            msg_Dbg( p_dec, "style is: %s", p_ssa_style->psz_stylename );

            /* TODO: Setup % based offsets properly, without adversely affecting
             *       everything else in vlc. Will address with separate patch,
             *       to prevent this one being any more complicated.

                     * p_ssa_style->i_margin_percent_h;
                     * p_ssa_style->i_margin_percent_v;
             */
            if( i_sys_align == -1 )
            {
                p_text_region->i_align     = p_ssa_style->i_align;
                p_text_region->i_x         = p_ssa_style->i_margin_h;
                p_text_region->i_y         = p_ssa_style->i_margin_v;
            }
            p_text_region->p_text = text_segment_NewInheritStyle( p_ssa_style->p_style );
        }
        else
        {
            p_text_region->p_text = text_segment_New( NULL );
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

    if( VLC_SUCCESS != decoder_GetInputAttachments( p_dec, &pp_attachments, &i_attachments_cnt ))
        return VLC_EGENERIC;

    for( int k = 0; k < i_attachments_cnt; k++ )
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

                p_block = block_Alloc( p_attach->i_data );

                if( p_block != NULL )
                {
                    es_format_t        es_in;
                    video_format_t     fmt_out;

                    memcpy( p_block->p_buffer, p_attach->p_data, p_attach->i_data );

                    es_format_Init( &es_in, VIDEO_ES, type );
                    es_in.video.i_chroma = type;
                    video_format_Init( &fmt_out, VLC_CODEC_YUVA );

                    /* Find a suitable decoder module */
                    if( module_exists( "sdl_image" ) )
                    {
                        /* ffmpeg thinks it can handle bmp properly but it can't (at least
                         * not all of them), so use sdl_image if it is available */

                        var_Create( p_dec, "codec", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
                        var_SetString( p_dec, "codec", "sdl_image" );
                    }

                    p_pic = image_Read( p_image, p_block, &es_in, &fmt_out );
                    var_Destroy( p_dec, "codec" );
                    es_format_Clean( &es_in );
                    video_format_Clean( &fmt_out );
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
    const char *node;
    ssa_style_t *p_ssa_style = NULL;
    int i_style_level = 0;
    int i_metadata_level = 0;
    int type;

    while( (type = xml_ReaderNextNode( p_xml_reader, &node )) > 0 )
    {
        switch( type )
        {
            case XML_READER_ENDELEM:
                switch (i_style_level)
                {
                    case 0:
                        if( !strcasecmp( "metadata", node ) && (i_metadata_level == 1) )
                            i_metadata_level--;
                        break;
                    case 1:
                        if( !strcasecmp( "styles", node ) )
                            i_style_level--;
                        break;
                    case 2:
                        if( !strcasecmp( "style", node ) )
                        {
                            TAB_APPEND( p_sys->i_ssa_styles, p_sys->pp_ssa_styles, p_ssa_style );

                            p_ssa_style = NULL;
                            i_style_level--;
                        }
                        break;
                }
                break;

            case XML_READER_STARTELEM:
                if( !strcasecmp( "metadata", node ) && (i_style_level == 0) )
                    i_metadata_level++;
                else if( !strcasecmp( "resolution", node ) &&
                         ( i_metadata_level == 1) )
                {
                    const char *attr, *val;
                    while( (attr = xml_ReaderNextAttr( p_xml_reader, &val )) )
                    {
                        if( !strcasecmp( "x", attr ) )
                            p_sys->i_original_width = atoi( val );
                        else if( !strcasecmp( "y", attr ) )
                            p_sys->i_original_height = atoi( val );
                    }
                }
                else if( !strcasecmp( "styles", node ) && (i_style_level == 0) )
                {
                    i_style_level++;
                }
                else if( !strcasecmp( "style", node ) && (i_style_level == 1) )
                {
                    i_style_level++;

                    p_ssa_style = calloc( 1, sizeof(ssa_style_t) );
                    if( unlikely(!p_ssa_style) )
                        return;
                    p_ssa_style->p_style = text_style_Create( STYLE_NO_DEFAULTS );
                    if( unlikely(!p_ssa_style->p_style) )
                    {
                        free(p_ssa_style);
                        return;
                    }
                    /* All styles are supposed to default to Default, and then
                     * one or more settings are over-ridden.
                     * At the moment this only effects styles defined AFTER
                     * Default in the XML
                     */
                    for( int i = 0; i < p_sys->i_ssa_styles; i++ )
                    {
                        if( !strcasecmp( p_sys->pp_ssa_styles[i]->psz_stylename, "Default" ) )
                        {
                            ssa_style_t *p_default_style = p_sys->pp_ssa_styles[i];
                            text_style_t *p_orig_text_style = p_ssa_style->p_style;

                            memcpy( p_ssa_style, p_default_style, sizeof( ssa_style_t ) );

                            // reset data-members that are not to be overwritten
                            p_ssa_style->p_style = p_orig_text_style;
                            p_ssa_style->psz_stylename = NULL;

                            //FIXME: Make font_style a pointer. Actually we double copy some data here,
                            //   we use text_style_Copy to avoid copying psz_fontname, though .
                            text_style_Copy( p_ssa_style->p_style, p_default_style->p_style );
                        }
                    }

                    const char *attr, *val;
                    while( (attr = xml_ReaderNextAttr( p_xml_reader, &val )) )
                    {
                        if( !strcasecmp( "name", attr ) )
                        {
                            free( p_ssa_style->psz_stylename );
                            p_ssa_style->psz_stylename = strdup( val );
                        }
                    }
                }
                else if( !strcasecmp( "fontstyle", node ) && (i_style_level == 2) )
                {
                    const char *attr, *val;
                    while( (attr = xml_ReaderNextAttr( p_xml_reader, &val )) )
                    {
                        if( !strcasecmp( "face", attr ) )
                        {
                            free( p_ssa_style->p_style->psz_fontname );
                            p_ssa_style->p_style->psz_fontname = strdup( val );
                        }
                        else if( !strcasecmp( "size", attr ) )
                        {
                            if( ( *val == '+' ) || ( *val == '-' ) )
                            {
                                int i_value = atoi( val );

                                if( ( i_value >= -5 ) && ( i_value <= 5 ) )
                                    p_ssa_style->p_style->i_font_size  +=
                                       ( i_value * p_ssa_style->p_style->i_font_size ) / 10;
                                else if( i_value < -5 )
                                    p_ssa_style->p_style->i_font_size  = - i_value;
                                else if( i_value > 5 )
                                    p_ssa_style->p_style->i_font_size  = i_value;
                            }
                            else
                                p_ssa_style->p_style->i_font_size  = atoi( val );
                        }
                        else if( !strcasecmp( "italic", attr ) )
                        {
                            if( !strcasecmp( "yes", val ))
                                p_ssa_style->p_style->i_style_flags |= STYLE_ITALIC;
                            else
                                p_ssa_style->p_style->i_style_flags &= ~STYLE_ITALIC;
                            p_ssa_style->p_style->i_features |= STYLE_HAS_FLAGS;
                        }
                        else if( !strcasecmp( "weight", attr ) )
                        {
                            if( !strcasecmp( "bold", val ))
                                p_ssa_style->p_style->i_style_flags |= STYLE_BOLD;
                            else
                                p_ssa_style->p_style->i_style_flags &= ~STYLE_BOLD;
                            p_ssa_style->p_style->i_features |= STYLE_HAS_FLAGS;
                        }
                        else if( !strcasecmp( "underline", attr ) )
                        {
                            if( !strcasecmp( "yes", val ))
                                p_ssa_style->p_style->i_style_flags |= STYLE_UNDERLINE;
                            else
                                p_ssa_style->p_style->i_style_flags &= ~STYLE_UNDERLINE;
                            p_ssa_style->p_style->i_features |= STYLE_HAS_FLAGS;
                        }
                        else if( !strcasecmp( "color", attr ) )
                        {
                            if( *val == '#' )
                            {
                                unsigned long col = strtol(val+1, NULL, 16);
                                 p_ssa_style->p_style->i_font_color = (col & 0x00ffffff);
                                 p_ssa_style->p_style->i_font_alpha = (col >> 24) & 0xff;
                                 p_ssa_style->p_style->i_features |= STYLE_HAS_FONT_COLOR
                                                                   | STYLE_HAS_FONT_ALPHA;
                            }
                        }
                        else if( !strcasecmp( "outline-color", attr ) )
                        {
                            if( *val == '#' )
                            {
                                unsigned long col = strtol(val+1, NULL, 16);
                                p_ssa_style->p_style->i_outline_color = (col & 0x00ffffff);
                                p_ssa_style->p_style->i_outline_alpha = (col >> 24) & 0xff;
                                p_ssa_style->p_style->i_features |= STYLE_HAS_OUTLINE_COLOR
                                                                  | STYLE_HAS_OUTLINE_ALPHA;
                            }
                        }
                        else if( !strcasecmp( "outline-level", attr ) )
                        {
                            p_ssa_style->p_style->i_outline_width = atoi( val );
                        }
                        else if( !strcasecmp( "shadow-color", attr ) )
                        {
                            if( *val == '#' )
                            {
                                unsigned long col = strtol(val+1, NULL, 16);
                                p_ssa_style->p_style->i_shadow_color = (col & 0x00ffffff);
                                p_ssa_style->p_style->i_shadow_alpha = (col >> 24) & 0xff;
                                p_ssa_style->p_style->i_features |= STYLE_HAS_SHADOW_COLOR
                                                                  | STYLE_HAS_SHADOW_ALPHA;
                            }
                        }
                        else if( !strcasecmp( "shadow-level", attr ) )
                        {
                            p_ssa_style->p_style->i_shadow_width = atoi( val );
                        }
                        else if( !strcasecmp( "spacing", attr ) )
                        {
                            p_ssa_style->p_style->i_spacing = atoi( val );
                        }
                    }
                }
                else if( !strcasecmp( "position", node ) && (i_style_level == 2) )
                {
                    const char *attr, *val;
                    while( (attr = xml_ReaderNextAttr( p_xml_reader, &val )) )
                    {
                        if( !strcasecmp( "alignment", attr ) )
                        {
                            if( !strcasecmp( "TopLeft", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_LEFT;
                            else if( !strcasecmp( "TopCenter", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_TOP;
                            else if( !strcasecmp( "TopRight", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_TOP | SUBPICTURE_ALIGN_RIGHT;
                            else if( !strcasecmp( "MiddleLeft", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_LEFT;
                            else if( !strcasecmp( "MiddleCenter", val ) )
                                p_ssa_style->i_align = 0;
                            else if( !strcasecmp( "MiddleRight", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_RIGHT;
                            else if( !strcasecmp( "BottomLeft", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_LEFT;
                            else if( !strcasecmp( "BottomCenter", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_BOTTOM;
                            else if( !strcasecmp( "BottomRight", val ) )
                                p_ssa_style->i_align = SUBPICTURE_ALIGN_BOTTOM | SUBPICTURE_ALIGN_RIGHT;
                        }
                        else if( !strcasecmp( "horizontal-margin", attr ) )
                        {
                            if( strchr( val, '%' ) )
                            {
                                p_ssa_style->i_margin_h = 0;
                                p_ssa_style->i_margin_percent_h = atoi( val );
                            }
                            else
                            {
                                p_ssa_style->i_margin_h = atoi( val );
                                p_ssa_style->i_margin_percent_h = 0;
                            }
                        }
                        else if( !strcasecmp( "vertical-margin", attr ) )
                        {
                            if( strchr( val, '%' ) )
                            {
                                p_ssa_style->i_margin_v = 0;
                                p_ssa_style->i_margin_percent_v = atoi( val );
                            }
                            else
                            {
                                p_ssa_style->i_margin_v = atoi( val );
                                p_ssa_style->i_margin_percent_v = 0;
                            }
                        }
                    }
                }
                break;
        }
    }
    free( p_ssa_style );
}



static subpicture_region_t *ParseUSFString( decoder_t *p_dec,
                                            char *psz_subtitle )
{
    decoder_sys_t        *p_sys = p_dec->p_sys;
    subpicture_region_t  *p_region_first = NULL;
    subpicture_region_t  *p_region_upto  = p_region_first;

    while( *psz_subtitle )
    {
        if( *psz_subtitle == '<' )
        {
            char *psz_end = NULL;


            if(( !strncasecmp( psz_subtitle, "<karaoke ", 9 )) ||
                    ( !strncasecmp( psz_subtitle, "<karaoke>", 9 )))
            {
                psz_end = strcasestr( psz_subtitle, "</karaoke>" );

                if( psz_end )
                {
                    subpicture_region_t  *p_text_region;

                    char *psz_flat = NULL;
                    char *psz_knodes = strndup( &psz_subtitle[9], psz_end - &psz_subtitle[9] );
                    if( psz_knodes )
                    {
                        /* remove timing <k> tags */
                        psz_flat = CreatePlainText( psz_knodes );
                        free( psz_knodes );
                        if( psz_flat )
                        {
                            p_text_region = CreateTextRegion( p_dec,
                                                              psz_flat,
                                                              p_sys->i_align );
                            if( p_text_region )
                            {
                                free( p_text_region->p_text->psz_text );
                                p_text_region->p_text->psz_text = psz_flat;
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
                            else free( psz_flat );
                        }
                    }

                    psz_end += strcspn( psz_end, ">" ) + 1;
                }
            }
            else if(( !strncasecmp( psz_subtitle, "<image ", 7 )) ||
                    ( !strncasecmp( psz_subtitle, "<image>", 7 )))
            {
                subpicture_region_t *p_image_region = NULL;

                psz_end = strcasestr( psz_subtitle, "</image>" );
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
                        p_image_region = LoadEmbeddedImage( p_dec,
                                            psz_filename, i_transparent );
                        free( psz_filename );
                    }
                }

                if( psz_end ) psz_end += strcspn( psz_end, ">" ) + 1;

                if( p_image_region )
                {
                    SetupPositions( p_image_region, psz_subtitle );

                    p_image_region->p_next   = NULL;
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
            else
            {
                subpicture_region_t  *p_text_region;

                psz_end = psz_subtitle + strlen( psz_subtitle );

                p_text_region = CreateTextRegion( p_dec,
                                                  psz_subtitle,
                                                  p_sys->i_align );

                if( p_text_region )
                {
                    free( p_text_region->p_text->psz_text );
                    p_text_region->p_text->psz_text = CreatePlainText( psz_subtitle );
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
    xml_reader_t  *p_xml_reader = NULL;

    p_sub = vlc_stream_MemoryNew( VLC_OBJECT(p_dec),
                              p_dec->fmt_in.p_extra,
                              p_dec->fmt_in.i_extra,
                              true );
    if( !p_sub )
        return;

    p_xml_reader = xml_ReaderCreate( p_dec, p_sub );
    if( likely(p_xml_reader) )
    {
        const char *node;

        /* Look for Root Node */
        if( xml_ReaderNextNode( p_xml_reader, &node ) == XML_READER_STARTELEM
         && !strcasecmp( "usfsubtitles", node ) )
            ParseUSFHeaderTags( p_dec, p_xml_reader );

        xml_ReaderDelete( p_xml_reader );
    }
    vlc_stream_Delete( p_sub );
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

        /* Security fix: Account for the case where input ends early */
        if( *psz_subtitle == '\0' ) break;

        psz_subtitle++;
    }
    *psz_text++ = '\0';

    char *psz = realloc( psz_text_start, psz_text - psz_text_start );
    return likely(psz != NULL) ? psz : psz_text_start;
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
        char  spc = ' ';
        int   i_whitespace = strspn( s, "\t\r\n " );

        /* Favour '\n' over other whitespaces - if one of these
         * occurs in the whitespace use a '\n' as our value,
         * otherwise just use a ' '
         */
        for( int k = 0; k < i_whitespace; k++ )
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
                                               const char *psz_filename,
                                               int i_transparent_color )
{
    decoder_sys_t         *p_sys = p_dec->p_sys;
    subpicture_region_t   *p_region;
    video_format_t         fmt_out;
    picture_t             *p_pic = NULL;

    for( int k = 0; k < p_sys->i_images; k++ )
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

    fmt_out.i_chroma = VLC_CODEC_YUVA;
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
    assert( p_pic->format.i_chroma == VLC_CODEC_YUVA );
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

        assert( p_region->fmt.i_chroma == VLC_CODEC_YUVA );
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

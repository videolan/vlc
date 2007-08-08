/*****************************************************************************
 * subsusf.c : USF subtitles decoder
 *****************************************************************************
 * Copyright (C) 2000-2006 the VideoLAN team
 * $Id: subsdec.c 20996 2007-08-05 20:01:21Z jb $
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *          Samuel Hocevar <sam@zoy.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Bernie Purcell <b dot purcell at adbglobal dot com>
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

#include "subsdec.h"

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
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
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
                        msg_Err( p_dec, "out of memory" );
                        free( psz_node );
                        break;
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
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
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
                                if( p_style->font_style.psz_fontname )
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
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
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
                        if( psz_name )  free( psz_name );
                        if( psz_value ) free( psz_value );
                    }
                }

                free( psz_node );
                break;
        }
    }
    if( p_style ) free( p_style );
}



subpicture_region_t *ParseUSFString( decoder_t *p_dec,
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
void ParseUSFHeader( decoder_t *p_dec )
{
    stream_t      *p_sub = NULL;
    xml_t         *p_xml = NULL;
    xml_reader_t  *p_xml_reader = NULL;

    p_sub = stream_MemoryNew( VLC_OBJECT(p_dec),
                              p_dec->fmt_in.p_extra,
                              p_dec->fmt_in.i_extra,
                              VLC_TRUE );
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



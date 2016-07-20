/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2015 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Sigmund Augdal Helberg <dnumgis@videolan.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Bernie Purcell <bitmap@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *          Felix Paul Kühne <fkuehne@videolan.org>
 *          Salah-Eddin Shaban <salshaaban@gmail.com>
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
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_filter.h>                                      /* filter_sys_t */

# include <vlc_xml.h>
# include <vlc_stream.h>

#include "../platform_fonts.h"

#define ANDROID_SYSTEM_FONTS    "file:///system/etc/system_fonts.xml"
#define ANDROID_FALLBACK_FONTS  "file:///system/etc/fallback_fonts.xml"
#define ANDROID_VENDOR_FONTS    "file:///vendor/etc/fallback_fonts.xml"
#define ANDROID_FONT_PATH       "/system/fonts"

static int Android_ParseFamily( filter_t *p_filter, xml_reader_t *p_xml )
{
    filter_sys_t     *p_sys       = p_filter->p_sys;
    vlc_dictionary_t *p_dict      = &p_sys->family_map;
    vlc_family_t     *p_family    = NULL;
    char             *psz_lc      = NULL;
    int               i_counter   = 0;
    bool              b_bold      = false;
    bool              b_italic    = false;
    const char       *p_node      = NULL;
    int               i_type      = 0;

    while( ( i_type = xml_ReaderNextNode( p_xml, &p_node ) ) > 0 )
    {
        switch( i_type )
        {
        case XML_READER_STARTELEM:
            /*
             * Multiple names can reference the same family in Android. When
             * the first name is encountered we set p_family to the vlc_family_t
             * in the master list matching this name, and if no such family
             * exists we create a new one and add it to the master list.
             * If the master list does contain a family with that name it's one
             * of the font attachments, and the family will end up having embedded
             * fonts and system fonts.
             */
            if( !strcasecmp( "name", p_node ) )
            {
                i_type = xml_ReaderNextNode( p_xml, &p_node );

                if( i_type != XML_READER_TEXT || !p_node || !*p_node )
                {
                    msg_Warn( p_filter, "Android_ParseFamily: empty name" );
                    continue;
                }

                psz_lc = ToLower( p_node );
                if( unlikely( !psz_lc ) )
                    return VLC_ENOMEM;

                if( !p_family )
                {
                    p_family = vlc_dictionary_value_for_key( p_dict, psz_lc );
                    if( p_family == kVLCDictionaryNotFound )
                    {
                        p_family =
                            NewFamily( p_filter, psz_lc, &p_sys->p_families, NULL, NULL );

                        if( unlikely( !p_family ) )
                        {
                            free( psz_lc );
                            return VLC_ENOMEM;
                        }

                    }
                }

                if( vlc_dictionary_value_for_key( p_dict, psz_lc ) == kVLCDictionaryNotFound )
                    vlc_dictionary_insert( p_dict, psz_lc, p_family );
                free( psz_lc );
            }
            /*
             * If p_family has not been set by the time we encounter the first file,
             * it means this family has no name, and should be used only as a fallback.
             * We create a new family for it in the master list with the name "fallback-xx"
             * and later add it to the "default" fallback list.
             */
            else if( !strcasecmp( "file", p_node ) )
            {
                i_type = xml_ReaderNextNode( p_xml, &p_node );

                if( i_type != XML_READER_TEXT || !p_node || !*p_node )
                {
                    ++i_counter;
                    continue;
                }

                if( !p_family )
                    p_family = NewFamily( p_filter, NULL, &p_sys->p_families,
                                          &p_sys->family_map, NULL );

                if( unlikely( !p_family ) )
                    return VLC_ENOMEM;

                switch( i_counter )
                {
                case 0:
                    b_bold = false;
                    b_italic = false;
                    break;
                case 1:
                    b_bold = true;
                    b_italic = false;
                    break;
                case 2:
                    b_bold = false;
                    b_italic = true;
                    break;
                case 3:
                    b_bold = true;
                    b_italic = true;
                    break;
                default:
                    msg_Warn( p_filter, "Android_ParseFamily: too many files" );
                    return VLC_EGENERIC;
                }

                char *psz_fontfile = NULL;
                if( asprintf( &psz_fontfile, "%s/%s", ANDROID_FONT_PATH, p_node ) < 0
                 || !NewFont( psz_fontfile, 0, b_bold, b_italic, p_family ) )
                    return VLC_ENOMEM;

                ++i_counter;
            }
            break;

        case XML_READER_ENDELEM:
            if( !strcasecmp( "family", p_node ) )
            {
                if( !p_family )
                {
                    msg_Warn( p_filter, "Android_ParseFamily: empty family" );
                    return VLC_EGENERIC;
                }

                /*
                 * If the family name has "fallback" in it, add it to the
                 * "default" fallback list.
                 */
                if( strcasestr( p_family->psz_name, FB_NAME ) )
                {
                    vlc_family_t *p_fallback =
                        NewFamily( p_filter, p_family->psz_name,
                                   NULL, &p_sys->fallback_map, FB_LIST_DEFAULT );

                    if( unlikely( !p_fallback ) )
                        return VLC_ENOMEM;

                    p_fallback->p_fonts = p_family->p_fonts;
                }

                return VLC_SUCCESS;
            }
            break;
        }
    }

    msg_Warn( p_filter, "Android_ParseFamily: Corrupt font configuration file" );
    return VLC_EGENERIC;
}

static int Android_ParseSystemFonts( filter_t *p_filter, const char *psz_path )
{
    int i_ret = VLC_SUCCESS;
    stream_t *p_stream = vlc_stream_NewMRL( p_filter, psz_path );

    if( !p_stream )
        return VLC_EGENERIC;

    xml_reader_t *p_xml = xml_ReaderCreate( p_filter, p_stream );

    if( !p_xml )
    {
        vlc_stream_Delete( p_stream );
        return VLC_EGENERIC;
    }

    const char *p_node;
    int i_type;
    while( ( i_type = xml_ReaderNextNode( p_xml, &p_node ) ) > 0 )
    {
        if( i_type == XML_READER_STARTELEM && !strcasecmp( "family", p_node ) )
        {
            if( ( i_ret = Android_ParseFamily( p_filter, p_xml ) ) )
                break;
        }
    }

    xml_ReaderDelete( p_xml );
    vlc_stream_Delete( p_stream );
    return i_ret;
}

int Android_Prepare( filter_t *p_filter )
{
    if( Android_ParseSystemFonts( p_filter, ANDROID_SYSTEM_FONTS ) == VLC_ENOMEM )
        return VLC_EGENERIC;
    if( Android_ParseSystemFonts( p_filter, ANDROID_FALLBACK_FONTS ) == VLC_ENOMEM )
        return VLC_EGENERIC;
    if( Android_ParseSystemFonts( p_filter, ANDROID_VENDOR_FONTS ) == VLC_ENOMEM )
        return VLC_EGENERIC;

    return VLC_SUCCESS;
}

const vlc_family_t *Android_GetFamily( filter_t *p_filter, const char *psz_family )
{
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_lc = ToLower( psz_family );
    if( unlikely( !psz_lc ) )
        return NULL;

    vlc_family_t *p_family =
            vlc_dictionary_value_for_key( &p_sys->family_map, psz_lc );

    free( psz_lc );

    if( p_family == kVLCDictionaryNotFound )
        return NULL;

    return p_family;
}

vlc_family_t *Android_GetFallbacks( filter_t *p_filter, const char *psz_family,
                                    uni_char_t codepoint )
{
    VLC_UNUSED( codepoint );

    vlc_family_t *p_fallbacks = NULL;
    filter_sys_t *p_sys = p_filter->p_sys;
    char *psz_lc = ToLower( psz_family );
    if( unlikely( !psz_lc ) )
        return NULL;

    p_fallbacks = vlc_dictionary_value_for_key( &p_sys->fallback_map, psz_lc );

    free( psz_lc );

    if( p_fallbacks == kVLCDictionaryNotFound )
        return NULL;

    return p_fallbacks;
}

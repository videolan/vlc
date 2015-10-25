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

#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
# include <Carbon/Carbon.h>
#endif
#include <sys/param.h>                         /* for MAXPATHLEN */

#include "../platform_fonts.h"

#if !TARGET_OS_IPHONE
char* MacLegacy_Select( filter_t *p_filter, const char* psz_fontname,
                        bool b_bold, bool b_italic,
                        int *i_idx, uni_char_t codepoint )
{
    VLC_UNUSED( b_bold );
    VLC_UNUSED( b_italic );
    VLC_UNUSED( codepoint );
    FSRef ref;
    unsigned char path[MAXPATHLEN];
    char * psz_path;

    CFStringRef  cf_fontName;
    ATSFontRef   ats_font_id;

    *i_idx = 0;

    if( psz_fontname == NULL )
        return NULL;

    msg_Dbg( p_filter, "looking for %s", psz_fontname );
    cf_fontName = CFStringCreateWithCString( kCFAllocatorDefault, psz_fontname, kCFStringEncodingUTF8 );

    ats_font_id = ATSFontFindFromName( cf_fontName, kATSOptionFlagsIncludeDisabledMask );

    if ( ats_font_id == 0 || ats_font_id == 0xFFFFFFFFUL )
    {
        msg_Dbg( p_filter, "ATS couldn't find %s by name, checking family", psz_fontname );
        ats_font_id = ATSFontFamilyFindFromName( cf_fontName, kATSOptionFlagsDefault );

        if ( ats_font_id == 0 || ats_font_id == 0xFFFFFFFFUL )
        {
            msg_Dbg( p_filter, "ATS couldn't find either %s nor its family, checking PS name", psz_fontname );
            ats_font_id = ATSFontFindFromPostScriptName( cf_fontName, kATSOptionFlagsDefault );

            if ( ats_font_id == 0 || ats_font_id == 0xFFFFFFFFUL )
            {
                msg_Err( p_filter, "ATS couldn't find %s (no font name, family or PS name)", psz_fontname );
                CFRelease( cf_fontName );
                return NULL;
            }
        }
    }
    CFRelease( cf_fontName );

    if ( noErr != ATSFontGetFileReference( ats_font_id, &ref ) )
    {
        msg_Err( p_filter, "ATS couldn't get file ref for %s", psz_fontname );
        return NULL;
    }

    /* i_idx calculation by searching preceding fontIDs */
    /* with same FSRef                                       */
    {
        ATSFontRef  id2 = ats_font_id - 1;
        FSRef       ref2;

        while ( id2 > 0 )
        {
            if ( noErr != ATSFontGetFileReference( id2, &ref2 ) )
                break;
            if ( noErr != FSCompareFSRefs( &ref, &ref2 ) )
                break;

            id2 --;
        }
        *i_idx = ats_font_id - ( id2 + 1 );
    }

    if ( noErr != FSRefMakePath( &ref, path, sizeof(path) ) )
    {
        msg_Err( p_filter, "failure when getting path from FSRef" );
        return NULL;
    }
    msg_Dbg( p_filter, "found %s", path );

    psz_path = strdup( (char *)path );

    return psz_path;
}
#endif

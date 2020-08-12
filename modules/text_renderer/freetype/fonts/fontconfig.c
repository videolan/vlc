/*****************************************************************************
 * freetype.c : Put text on the video, using freetype2
 *****************************************************************************
 * Copyright (C) 2002 - 2015 VLC authors and VideoLAN
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

#include <assert.h>
#include <stdint.h>

#include <vlc_common.h>
#include <vlc_filter.h>                     /* filter_sys_t */
#include <vlc_dialog.h>                     /* FcCache dialog */

#include <fontconfig/fontconfig.h>

#include "../platform_fonts.h"
#include "backends.h"

static FcConfig *config;
static uintptr_t refs;
static vlc_mutex_t lock = VLC_STATIC_MUTEX;

int FontConfig_Prepare( vlc_font_select_t *fs )
{
    vlc_tick_t ts;

    vlc_mutex_lock( &lock );
    if( refs++ > 0 )
    {
        vlc_mutex_unlock( &lock );
        return VLC_SUCCESS;
    }

    msg_Dbg( fs->p_obj, "Building font databases.");
    ts = vlc_tick_now();

#ifndef _WIN32
    config = FcInitLoadConfigAndFonts();
    if( unlikely(config == NULL) )
        refs = 0;

#else
    unsigned int i_dialog_id = 0;
    dialog_progress_bar_t *p_dialog = NULL;
    config = FcInitLoadConfig();

    int i_ret =
        vlc_dialog_display_progress( fs->p_obj, true, 0.0, NULL,
                                     _("Building font cache"),
                                     _("Please wait while your font cache is rebuilt.\n"
                                     "This should take less than a few minutes.") );

    i_dialog_id = i_ret > 0 ? i_ret : 0;

    if( FcConfigBuildFonts( config ) == FcFalse )
        return VLC_ENOMEM;

    if( i_dialog_id != 0 )
        vlc_dialog_cancel( fs->p_obj, i_dialog_id );

#endif

    vlc_mutex_unlock( &lock );
    msg_Dbg( fs->p_obj, "Took %" PRId64 " microseconds", vlc_tick_now() - ts );

    return (config != NULL) ? VLC_SUCCESS : VLC_EGENERIC;
}

void FontConfig_Unprepare( vlc_font_select_t *fs )
{
    VLC_UNUSED(fs);
    vlc_mutex_lock( &lock );
    assert( refs > 0 );
    if( --refs == 0 )
        FcConfigDestroy( config );

    vlc_mutex_unlock( &lock );
}

static void FontConfig_AddFromFcPattern( FcPattern *p_pat,  vlc_family_t *p_family )
{
    int bold;
    int italic;
    FcChar8* val_s;
    int i_index = 0;
    char *psz_fontfile = NULL;

    if( FcResultMatch != FcPatternGetInteger( p_pat, FC_INDEX, 0, &i_index ) )
        i_index = 0;

    if( FcResultMatch != FcPatternGetInteger( p_pat, FC_WEIGHT, 0, &bold ) )
        bold = FC_WEIGHT_NORMAL;
    if( bold < FC_WEIGHT_NORMAL )
        return;

    if( FcResultMatch != FcPatternGetInteger( p_pat, FC_SLANT, 0, &italic ) )
        italic = FC_SLANT_ROMAN;

    if( FcResultMatch != FcPatternGetString( p_pat, FC_FILE, 0, &val_s ) )
        return;

    psz_fontfile = strdup( (const char*)val_s );
    if( psz_fontfile )
        NewFont( psz_fontfile, i_index,
                 bold > FC_WEIGHT_NORMAL,
                 italic != FC_SLANT_ROMAN,
                 p_family );
}

static void FontConfig_FillFaces( vlc_family_t *p_family )
{
    FcPattern *pat = FcPatternCreate();
    if ( !pat )
        return;

    FcObjectSet *os = FcObjectSetBuild ( FC_FILE, FC_SLANT, FC_WEIGHT,
                                         FC_INDEX, FC_WIDTH, (char *) 0 );
    if( os )
    {
        FcPatternAddString( pat, FC_FAMILY, (const FcChar8 *) p_family->psz_name );
        FcPatternAddBool( pat, FC_OUTLINE, FcTrue );

        FcFontSet *fs = FcFontList( 0, pat, os );
        FcObjectSetDestroy( os );
        if( fs )
        {
            for( int pass=0; pass<2 && !p_family->p_fonts; pass++ )
            {
                for ( int i = 0; i < fs->nfont; i++ )
                {
                    int i_width = 0;
                    FcPattern *p_pat = fs->fonts[i];

                    /* we relax the WIDTH condition if we did not get any match */
                    if( pass == 0 &&
                        FcResultMatch == FcPatternGetInteger( p_pat, FC_WIDTH, 0, &i_width ) &&
                        i_width != FC_WIDTH_NORMAL )
                        continue;

                    FontConfig_AddFromFcPattern( p_pat, p_family );
                }
            }
            FcFontSetDestroy( fs );
        }
    }
    FcPatternDestroy( pat );
}

int FontConfig_SelectAmongFamilies( vlc_font_select_t *fs, const fontfamilies_t *families,
                                    const vlc_family_t **pp_result )
{
    FcResult result = FcResultMatch;
    FcPattern *pat, *p_matchpat;
    FcChar8* val_s;

    /* Create a pattern and fill it */
    pat = FcPatternCreate();
    if (!pat)
        return VLC_EGENERIC;

    /* */
    const char *psz_lcname;
    vlc_vector_foreach( psz_lcname, &families->vec )
        FcPatternAddString( pat, FC_FAMILY, (const FcChar8*) psz_lcname );
    FcPatternAddBool( pat, FC_OUTLINE, FcTrue );

    /* */
    FcDefaultSubstitute( pat );
    if( !FcConfigSubstitute( config, pat, FcMatchPattern ) )
    {
        FcPatternDestroy( pat );
        return VLC_EGENERIC;
    }

    /* Find the best font for the pattern, destroy the pattern */
    p_matchpat = FcFontMatch( config, pat, &result );
    if( !p_matchpat )
        return VLC_EGENERIC;
    FcPatternDestroy( pat );
    if( result == FcResultNoMatch )
    {
        *pp_result = NULL;
        return VLC_SUCCESS;
    }

    if( FcResultMatch != FcPatternGetString( p_matchpat, FC_FAMILY, 0, &val_s ) )
    {
        FcPatternDestroy( p_matchpat );
        return VLC_EGENERIC;
    }

    char *psz_fnlc = LowercaseDup((const char *)val_s);
    vlc_family_t *p_family = vlc_dictionary_value_for_key( &fs->family_map, psz_fnlc );
    if( p_family == kVLCDictionaryNotFound )
    {
        p_family = NewFamily( fs, psz_fnlc, &fs->p_families,
                              &fs->family_map, psz_fnlc );
        if( !p_family )
        {
            free( psz_fnlc );
            FcPatternDestroy( p_matchpat );
            return VLC_EGENERIC;
        }
    }

    free(psz_fnlc);
    FcPatternDestroy( p_matchpat );

    if( p_family ) /* Populate with fonts */
        FontConfig_FillFaces( p_family );

    *pp_result = p_family;

    return VLC_SUCCESS;
}

int FontConfig_GetFamily( vlc_font_select_t *fs, const char *psz_lcname,
                          const vlc_family_t **pp_result )
{
    fontfamilies_t families;
    families.psz_key = psz_lcname;
    vlc_vector_init( &families.vec );
    vlc_vector_push( &families.vec, (char *) psz_lcname );
    int ret = FontConfig_SelectAmongFamilies( fs, &families, pp_result );
    vlc_vector_clear( &families.vec );
    return ret;
}

int FontConfig_GetFallbacksAmongFamilies( vlc_font_select_t *fs, const fontfamilies_t *families,
                                          uni_char_t codepoint, vlc_family_t **pp_result )
{

    VLC_UNUSED( codepoint );
    vlc_family_t *p_family =
            vlc_dictionary_value_for_key( &fs->fallback_map, families->psz_key );
    if( p_family != kVLCDictionaryNotFound )
    {
        *pp_result = p_family;
        return VLC_SUCCESS;
    }
    p_family = NULL;

    FcPattern  *p_pattern = FcPatternCreate();
    if (!p_pattern)
        return VLC_EGENERIC;

    const char *psz_lcname;
    vlc_vector_foreach( psz_lcname, &families->vec )
        FcPatternAddString( p_pattern, FC_FAMILY, (const FcChar8*) psz_lcname );
    FcPatternAddBool( p_pattern, FC_OUTLINE, FcTrue );

    vlc_family_t *p_current = NULL;
    if( FcConfigSubstitute( config, p_pattern, FcMatchPattern ) == FcTrue )
    {
        FcDefaultSubstitute( p_pattern );
        FcResult result;
        FcFontSet* p_font_set = FcFontSort( config, p_pattern, FcTrue, NULL, &result );
        if( p_font_set )
        {
            for( int i = 0; i < p_font_set->nfont; ++i )
            {
                char* psz_name = NULL;
                if( FcPatternGetString( p_font_set->fonts[i],
                                    FC_FAMILY, 0, ( FcChar8** ) &psz_name ) )
                    continue;

                if( !p_current || strcasecmp( p_current->psz_name, psz_name ) )
                {
                    p_current = NewFamilyFromMixedCase( fs, psz_name,
                                                        &p_family, NULL, NULL );
                    if( unlikely( !p_current ) )
                        continue;
                }
            }
            FcFontSetDestroy( p_font_set );
        }
    }
    FcPatternDestroy( p_pattern );

    if( p_family )
        vlc_dictionary_insert( &fs->fallback_map, families->psz_key, p_family );

    *pp_result = p_family;
    return VLC_SUCCESS;
}

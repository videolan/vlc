/*****************************************************************************
 * kate_categories.c : maps well known category tags to translated strings.
 *****************************************************************************
 * Copyright (C) 2009 ogg.k.ogg.k@googlemail.com
 * $Id$
 *
 * Authors: ogg.k.ogg.k@googlemail.com
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

#include <stddef.h>
#include <string.h>
#include "kate_categories.h"

static const struct {
  const char *psz_tag;
  const char *psz_i18n;
} Katei18nCategories[] = {
    /* From Silvia's Mozilla list */
    { "CC",      N_("Closed captions") },
    { "SUB",     N_("Subtitles") },
    { "TAD",     N_("Textual audio descriptions") },
    { "KTV",     N_("Karaoke") },
    { "TIK",     N_("Ticker text") },
    { "AR",      N_("Active regions") },
    { "NB",      N_("Semantic annotations") },
    { "META",    N_("Metadata") },
    { "TRX",     N_("Transcript") },
    { "LRC",     N_("Lyrics") },
    { "LIN",     N_("Linguistic markup") },
    { "CUE",     N_("Cue points") },

    /* Grandfathered */
    { "subtitles", N_("Subtitles") },
    { "spu-subtitles", N_("Subtitles (images)") },
    { "lyrics", N_("Lyrics") },

    /* Kate specific */
    { "K-SPU", N_("Subtitles (images)") },
    { "K-SLD-T", N_("Slides (text)") },
    { "K-SLD-I", N_("Slides (images)") },
};

const char *FindKateCategoryName( const char *psz_tag )
{
    size_t i;

    for( i = 0; i < sizeof(Katei18nCategories)/sizeof(Katei18nCategories[0]); i++ )
    {
        if( !strcmp( psz_tag, Katei18nCategories[i].psz_tag ) )
            return Katei18nCategories[i].psz_i18n;
    }
    return N_("Unknown category");
}



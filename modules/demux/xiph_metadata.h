/*****************************************************************************
 * xiph_metadata.h: Vorbis Comment parser
 *****************************************************************************
 * Copyright © 2008-2013 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#include <vlc_charset.h>
#include <vlc_strings.h>

# ifdef __cplusplus
extern "C" {
# endif

input_attachment_t* ParseFlacPicture( const uint8_t *p_data, int i_data,
    int i_attachments, int *i_cover_score, int *i_cover_idx );

void vorbis_ParseComment( vlc_meta_t **pp_meta,
        const uint8_t *p_data, int i_data,
        int *i_attachments, input_attachment_t ***attachments,
        int *i_cover_score, int *i_cover_idx,
        int *i_seekpoint, seekpoint_t ***ppp_seekpoint );

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

const char *FindKateCategoryName( const char *psz_tag );

# ifdef __cplusplus
}
# endif


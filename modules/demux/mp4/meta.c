/*****************************************************************************
 * meta.c: mp4 meta handling
 *****************************************************************************
 * Copyright (C) 2001-2004, 2010, 2014 VLC authors and VideoLAN
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

#include "mp4.h"

#include "id3genres.h"                             /* for ATOM_gnre */

#include <vlc_meta.h>
#include <vlc_charset.h>

#include <assert.h>

static const struct
{
    const uint32_t xa9_type;
    const vlc_meta_type_t meta_type;
} xa9typetometa[] = {
    { ATOM_0x40PRM, vlc_meta_EncodedBy }, /* Adobe Premiere */
    { ATOM_0x40PRQ, vlc_meta_EncodedBy }, /* Adobe Qt */
    { ATOM_0xa9nam, vlc_meta_Title }, /* Full name */
    { ATOM_0xa9aut, vlc_meta_Artist },
    { ATOM_0xa9ART, vlc_meta_Artist },
    { ATOM_0xa9cpy, vlc_meta_Copyright },
    { ATOM_0xa9day, vlc_meta_Date }, /* Creation Date */
    { ATOM_0xa9des, vlc_meta_Description }, /* Description */
    { ATOM_0xa9gen, vlc_meta_Genre }, /* Genre */
    { ATOM_0xa9alb, vlc_meta_Album }, /* Album */
    { ATOM_0xa9trk, vlc_meta_TrackNumber }, /* Track */
    { ATOM_0xa9cmt, vlc_meta_Description }, /* Comment */
    { ATOM_0xa9url, vlc_meta_URL }, /* URL */
    { ATOM_0xa9too, vlc_meta_EncodedBy }, /* Encoder Tool */
    { ATOM_0xa9enc, vlc_meta_EncodedBy }, /* Encoded By */
    { ATOM_0xa9pub, vlc_meta_Publisher },
    { ATOM_0xa9dir, vlc_meta_Director },
    { ATOM_MCPS,    vlc_meta_EncodedBy }, /* Cleaner Pro */
    { 0, 0 },
};

static const struct
{
    const uint32_t xa9_type;
    const char metadata[25];
} xa9typetoextrameta[] = {
    { ATOM_0xa9wrt, N_("Writer") },
    { ATOM_0xa9com, N_("Composer") },
    { ATOM_0xa9prd, N_("Producer") },
    { ATOM_0xa9inf, N_("Information") },
    { ATOM_0xa9dis, N_("Disclaimer") },
    { ATOM_0xa9req, N_("Requirements") },
    { ATOM_0xa9fmt, N_("Original Format") },
    { ATOM_0xa9dsa, N_("Display Source As") },
    { ATOM_0xa9hst, N_("Host Computer") },
    { ATOM_0xa9prf, N_("Performers") },
    { ATOM_0xa9ope, N_("Original Performer") },
    { ATOM_0xa9src, N_("Providers Source Content") },
    { ATOM_0xa9wrn, N_("Warning") },
    { ATOM_0xa9swr, N_("Software") },
    { ATOM_0xa9lyr, N_("Lyrics") },
    { ATOM_0xa9mak, N_("Record Company") },
    { ATOM_0xa9mod, N_("Model") },
    { ATOM_0xa9PRD, N_("Product") },
    { ATOM_0xa9grp, N_("Grouping") },
    { ATOM_0xa9gen, N_("Genre") },
    { ATOM_0xa9st3, N_("Sub-Title") },
    { ATOM_0xa9arg, N_("Arranger") },
    { ATOM_0xa9ard, N_("Art Director") },
    { ATOM_0xa9cak, N_("Copyright Acknowledgement") },
    { ATOM_0xa9con, N_("Conductor") },
    { ATOM_0xa9des, N_("Song Description") },
    { ATOM_0xa9lnt, N_("Liner Notes") },
    { ATOM_0xa9phg, N_("Phonogram Rights") },
    { ATOM_0xa9pub, N_("Publisher") },
    { ATOM_0xa9sne, N_("Sound Engineer") },
    { ATOM_0xa9sol, N_("Soloist") },
    { ATOM_0xa9thx, N_("Thanks") },
    { ATOM_0xa9xpd, N_("Executive Producer") },
    { ATOM_aART,    N_("Album Artist") },
    { ATOM_flvr,    N_("Encoding Params") },
    { ATOM_vndr,    N_("Vendor") },
    { ATOM_xid_,    N_("Catalog Number") },
    { 0, "" },
};

static const struct
{
    const char *psz_naming;
    const vlc_meta_type_t meta_type;
} com_apple_quicktime_tometa[] = {
    { "displayname",     vlc_meta_NowPlaying },
    { "software",        vlc_meta_EncodedBy },
    { "Encoded_With",    vlc_meta_EncodedBy },
    { "album",           vlc_meta_Album },
    { "artist",          vlc_meta_Artist },
    { "comment",         vlc_meta_Description },
    { "description",     vlc_meta_Description },
    { "copyright",       vlc_meta_Copyright },
    { "creationdate",    vlc_meta_Date },
    { "director",        vlc_meta_Director },
    { "genre",           vlc_meta_Genre },
    { "publisher",       vlc_meta_Publisher },
    { NULL,              0 },
};

static const struct
{
    const char *psz_naming;
    const char *psz_metadata;
} com_apple_quicktime_toextrameta[] = {
    { "information",     N_("Information") },
    { "keywords",        N_("Keywords") },
    { "make",            N_("Vendor") },
    { NULL,              NULL },
};

inline static char * StringConvert( const MP4_Box_data_data_t *p_data )
{
    if ( !p_data || !p_data->i_blob )
        return NULL;

    switch( p_data->e_wellknowntype )
    {
    case DATA_WKT_UTF8:
    case DATA_WKT_UTF8_SORT:
        return FromCharset( "UTF-8", p_data->p_blob, p_data->i_blob );
    case DATA_WKT_UTF16:
    case DATA_WKT_UTF16_SORT:
        return FromCharset( "UTF-16BE", p_data->p_blob, p_data->i_blob );
    case DATA_WKT_SJIS:
        return FromCharset( "SHIFT-JIS", p_data->p_blob, p_data->i_blob );
    default:
        return NULL;
    }
}

static char * ExtractString( MP4_Box_t *p_box )
{
    if ( p_box->i_type == ATOM_data )
        return StringConvert( p_box->data.p_data );

    MP4_Box_t *p_data = MP4_BoxGet( p_box, "data" );
    if ( p_data )
        return StringConvert( BOXDATA(p_data) );
    else if ( p_box->data.p_string && p_box->data.p_string->psz_text )
    {
        char *psz_utf = strdup( p_box->data.p_string->psz_text );
        if (likely( psz_utf ))
            EnsureUTF8( psz_utf );
        return psz_utf;
    }
    else
        return NULL;
}

static bool MatchXA9Type( vlc_meta_t *p_meta, uint32_t i_type, MP4_Box_t *p_box )
{
    bool b_matched = false;

    for( unsigned i = 0; !b_matched && xa9typetometa[i].xa9_type; i++ )
    {
        if( i_type == xa9typetometa[i].xa9_type )
        {
            b_matched = true;
            char *psz_utf = ExtractString( p_box );
            if( psz_utf )
            {
                 vlc_meta_Set( p_meta, xa9typetometa[i].meta_type, psz_utf );
                 free( psz_utf );
            }
            break;
        }
    }

    for( unsigned i = 0; !b_matched && xa9typetoextrameta[i].xa9_type; i++ )
    {
        if( i_type == xa9typetoextrameta[i].xa9_type )
        {
            b_matched = true;
            char *psz_utf = ExtractString( p_box );
            if( psz_utf )
            {
                 vlc_meta_AddExtra( p_meta, _(xa9typetoextrameta[i].metadata), psz_utf );
                 free( psz_utf );
            }
            break;
        }
    }

    return b_matched;
}

static bool Matchcom_apple_quicktime( vlc_meta_t *p_meta, const char *psz_naming, MP4_Box_t *p_box )
{
    bool b_matched = false;

    for( unsigned i = 0; !b_matched && com_apple_quicktime_tometa[i].psz_naming; i++ )
    {
        if( !strcmp( psz_naming, com_apple_quicktime_tometa[i].psz_naming ) )
        {
            b_matched = true;
            char *psz_utf = ExtractString( p_box );
            if( psz_utf )
            {
                 vlc_meta_Set( p_meta, com_apple_quicktime_tometa[i].meta_type, psz_utf );
                 free( psz_utf );
            }
            break;
        }
    }

    for( unsigned i = 0; !b_matched && com_apple_quicktime_toextrameta[i].psz_naming; i++ )
    {
        if( !strcmp( psz_naming, com_apple_quicktime_toextrameta[i].psz_naming ) )
        {
            b_matched = true;
            char *psz_utf = ExtractString( p_box );
            if( psz_utf )
            {
                 vlc_meta_AddExtra( p_meta, _(com_apple_quicktime_toextrameta[i].psz_metadata), psz_utf );
                 free( psz_utf );
            }
            break;
        }
    }

    return b_matched;
}

static void SetupmdirMeta( vlc_meta_t *p_meta, MP4_Box_t *p_box )
{
    bool b_matched = true;
    const MP4_Box_t *p_data = MP4_BoxGet( p_box, "data" );
    /* XXX Becarefull p_udta can have box that are not 0xa9xx */
    switch( p_box->i_type )
    {
    case ATOM_atID:
    {
        if ( p_data && BOXDATA(p_data) && BOXDATA(p_data)->i_blob >= 4 &&
             BOXDATA(p_data)->e_wellknowntype == DATA_WKT_BE_SIGNED )
        {
            char psz_utf[11];
            snprintf( psz_utf, sizeof( psz_utf ), "%"PRId32,
                      GetDWBE(BOXDATA(p_data)->p_blob) );
            vlc_meta_AddExtra( p_meta, N_("iTunes Account ID"), psz_utf );
        }
        break;
    }
    case ATOM_cnID:
    {
        if ( p_data && BOXDATA(p_data) && BOXDATA(p_data)->i_blob >= 4 &&
             BOXDATA(p_data)->e_wellknowntype == DATA_WKT_BE_SIGNED )
        {
            char psz_utf[11];
            snprintf( psz_utf, sizeof( psz_utf ), "%"PRId32,
                      GetDWBE(BOXDATA(p_data)->p_blob) );
            vlc_meta_AddExtra( p_meta, N_("iTunes Catalog ID"), psz_utf );
        }
        break;
    }
    case ATOM_disk:
    {
        if ( p_data && BOXDATA(p_data) && BOXDATA(p_data)->i_blob >= 6 &&
             BOXDATA(p_data)->e_wellknowntype == DATA_WKT_RESERVED )
        {
            char psz_utf[5 + 5 + 4];
            snprintf( psz_utf, sizeof( psz_utf ), "%"PRIu16" / %"PRIu16,
                      GetWBE(&BOXDATA(p_data)->p_blob[2]),
                      GetWBE(&BOXDATA(p_data)->p_blob[4]) );
            vlc_meta_AddExtra( p_meta, N_("Disc"), psz_utf );
        }
        break;
    }
    case ATOM_gnre:
    {
        if ( p_data && BOXDATA(p_data) && BOXDATA(p_data)->i_blob >= 2 &&
             BOXDATA(p_data)->e_wellknowntype == DATA_WKT_RESERVED )
        {
            const uint16_t i_genre = GetWBE(BOXDATA(p_data)->p_blob);
            if( i_genre && i_genre <= NUM_GENRES )
                vlc_meta_SetGenre( p_meta, ppsz_genres[i_genre - 1] );
        }
        break;
    }
    case ATOM_rtng:
    {
        if ( p_data && BOXDATA(p_data) && BOXDATA(p_data)->i_blob >= 1 )
        {
            const char *psz_rating;
            switch( *BOXDATA(p_data)->p_blob )
            {
            case 0x4:
                psz_rating = N_("Explicit");
                break;
            case 0x2:
                psz_rating = N_("Clean");
                break;
            default:
            case 0x0:
                psz_rating = N_("None");
                break;
            }
            vlc_meta_AddExtra( p_meta, N_("Rating"), psz_rating );
        }
        break;
    }
    case ATOM_trkn:
    {
        if ( p_data && BOXDATA(p_data) && BOXDATA(p_data)->i_blob >= 4 &&
             BOXDATA(p_data)->e_wellknowntype == DATA_WKT_RESERVED )
        {
            char psz_trck[6];
            snprintf( psz_trck, sizeof( psz_trck ), "%"PRIu16, GetWBE(&BOXDATA(p_data)->p_blob[2]) );
            vlc_meta_SetTrackNum( p_meta, psz_trck );
            if( BOXDATA(p_data)->i_blob >= 8 && GetWBE(&BOXDATA(p_data)->p_blob[4]) )
            {
                snprintf( psz_trck, sizeof( psz_trck ), "%"PRIu16, GetWBE(&BOXDATA(p_data)->p_blob[4]) );
                vlc_meta_Set( p_meta, vlc_meta_TrackTotal, psz_trck );
            }
        }
        break;
    }

    default:
        b_matched = false;
        break;
    }

    if ( !b_matched )
        MatchXA9Type( p_meta, p_box->i_type, p_box );
}

static void SetupmdtaMeta( vlc_meta_t *p_meta, MP4_Box_t *p_box, MP4_Box_t *p_keys )
{
    if ( !p_keys || !BOXDATA(p_keys) || BOXDATA(p_keys)->i_entry_count == 0 )
        return;
    if ( !p_box->i_index || p_box->i_index > BOXDATA(p_keys)->i_entry_count )
        return;

    const char *psz_naming = BOXDATA(p_keys)->p_entries[p_box->i_index - 1].psz_value;
    const uint32_t i_namespace = BOXDATA(p_keys)->p_entries[p_box->i_index - 1].i_namespace;

    if( i_namespace == HANDLER_mdta )
    {
        if ( !strncmp( "com.apple.quicktime.", psz_naming, 20 ) )
        {
            Matchcom_apple_quicktime( p_meta, psz_naming + 20, p_box );
        }
    }
    else if ( i_namespace == ATOM_udta )
    {
        /* Regular atom inside... could that be even more complex ??? */
        char *psz_utf = ExtractString( p_box );
        if ( psz_utf )
        {
            if ( strlen(psz_utf) == 4 )
            {
                MatchXA9Type( p_meta,
                              VLC_FOURCC(psz_utf[0],psz_utf[1],psz_utf[2],psz_utf[3]),
                              p_box );
            }
            free( psz_utf );
        }
    }
}

void SetupMeta( vlc_meta_t *p_meta, MP4_Box_t *p_udta )
{
    uint32_t i_handler = 0;
    if ( p_udta->p_father )
        i_handler = p_udta->i_handler;

    for( MP4_Box_t *p_box = p_udta->p_first; p_box; p_box = p_box->p_next )
    {
        switch( i_handler )
        {
            case HANDLER_mdta:
            {
                MP4_Box_t *p_keys = MP4_BoxGet( p_udta->p_father, "keys" );
                SetupmdtaMeta( p_meta, p_box, p_keys );
                break;
            }

            case HANDLER_mdir:
            default:
                SetupmdirMeta( p_meta, p_box );
                break;
        }
    }
}


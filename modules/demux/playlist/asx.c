/*****************************************************************************
 * asx.c : ASX playlist format import
 *****************************************************************************
 * Copyright (C) 2005-2006 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

/* See also: http://msdn.microsoft.com/library/en-us/wmplay10/mmp_sdk/windowsmediametafilereference.asp
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_demux.h>

#include <ctype.h>
#include <vlc_charset.h>
#include "playlist.h"
#include <vlc_meta.h>

struct demux_sys_t
{
    char    *psz_prefix;
    char    *psz_data;
    int64_t i_data_len;
    bool b_utf8;
    bool b_skip_ads;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);

static int StoreString( demux_t *p_demux, char **ppsz_string,
                        const char *psz_source_start,
                        const char *psz_source_end )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    unsigned len = psz_source_end - psz_source_start;

    free( *ppsz_string );

    char *buf = *ppsz_string = malloc ((len * (1 + !p_sys->b_utf8)) + 1);
    if (buf == NULL)
        return VLC_ENOMEM;

    if( p_sys->b_utf8 )
    {
        memcpy (buf, psz_source_start, len);
        (*ppsz_string)[len] = '\0';
        EnsureUTF8 (*ppsz_string);
    }
    else
    {
        /* Latin-1 -> UTF-8 */
        for (unsigned i = 0; i < len; i++)
        {
            unsigned char c = psz_source_start[i];
            if (c & 0x80)
            {
                *buf++ = 0xc0 | (c >> 6);
                *buf++ = 0x80 | (c & 0x3f);
            }
            else
                *buf++ = c;
        }
        *buf++ = '\0';

        buf = realloc (*ppsz_string, buf - *ppsz_string);
        if( buf )
            *ppsz_string = buf;
    }
    return VLC_SUCCESS;
}

static char *SkipBlanks(char *s, size_t i_strlen )
{
    while( i_strlen > 0 ) {
        switch( *s )
        {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                --i_strlen;
                ++s;
                break;
            default:
                i_strlen = 0;
        }
    }
    return s;
}

static int ParseTime(char *s, size_t i_strlen)
{
    // need to parse hour:minutes:sec.fraction string
    int result = 0;
    int val;
    const char *end = s + i_strlen;
    // skip leading spaces if any
    s = SkipBlanks(s, i_strlen);

    val = 0;
    while( (s < end) && isdigit((unsigned char)*s) )
    {
        int newval = val*10 + (*s - '0');
        if( newval < val )
        {
            // overflow
            val = 0;
            break;
        }
        val = newval;
        ++s;
    }
    result = val;
    s = SkipBlanks(s, end-s);
    if( *s == ':' )
    {
        ++s;
        s = SkipBlanks(s, end-s);
        result = result * 60;
        val = 0;
        while( (s < end) && isdigit((unsigned char)*s) )
        {
            int newval = val*10 + (*s - '0');
            if( newval < val )
            {
                // overflow
                val = 0;
                break;
            }
            val = newval;
            ++s;
        }
        result += val;
        s = SkipBlanks(s, end-s);
        if( *s == ':' )
        {
            ++s;
            s = SkipBlanks(s, end-s);
            result = result * 60;
            val = 0;
            while( (s < end) && isdigit((unsigned char)*s) )
            {
                int newval = val*10 + (*s - '0');
                if( newval < val )
                {
                    // overflow
                    val = 0;
                    break;
                }
                val = newval;
                ++s;
            }
            result += val;
            // TODO: one day, we may need to parse fraction for sub-second resolution
        }
    }
    return result;
}

/*****************************************************************************
 * Import_ASX: main import function
 *****************************************************************************/
int Import_ASX( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    const uint8_t *p_peek;
    CHECK_PEEK( p_peek, 10 );

    // skip over possible leading empty lines and empty spaces
    p_peek = (uint8_t *)SkipBlanks((char *)p_peek, 6);

    if( POKE( p_peek, "<asx", 4 ) || demux_IsPathExtension( p_demux, ".asx" ) ||
        demux_IsPathExtension( p_demux, ".wax" ) || demux_IsPathExtension( p_demux, ".wvx" ) ||
        demux_IsForced( p_demux, "asx-open" ) )
    {
        ;
    }
    else
        return VLC_EGENERIC;

    STANDARD_DEMUX_INIT_MSG( "found valid ASX playlist" );
    p_demux->p_sys->psz_prefix = FindPrefix( p_demux );
    p_demux->p_sys->psz_data = NULL;
    p_demux->p_sys->i_data_len = -1;
    p_demux->p_sys->b_utf8 = false;
    p_demux->p_sys->b_skip_ads =
        var_InheritBool( p_demux, "playlist-skip-ads" );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void Close_ASX( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    free( p_sys->psz_prefix );
    free( p_sys->psz_data );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char        *psz_parse = NULL;
    char        *psz_backup = NULL;
    bool  b_entry = false;
    input_item_t *p_current_input = GetCurrentItem(p_demux);

    /* init txt */
    if( p_sys->i_data_len < 0 )
    {
        int64_t i_pos = 0;
        p_sys->i_data_len = stream_Size( p_demux->s ) + 1; /* This is a cheat to prevent unnecessary realloc */
        if( p_sys->i_data_len <= 0 || p_sys->i_data_len > 16384 ) p_sys->i_data_len = 1024;
        p_sys->psz_data = xmalloc( p_sys->i_data_len +1);

        /* load the complete file */
        for( ;; )
        {
            int i_read = stream_Read( p_demux->s, &p_sys->psz_data[i_pos], p_sys->i_data_len - i_pos );
            p_sys->psz_data[i_pos + i_read] = '\0';

            if( i_read < p_sys->i_data_len - i_pos ) break; /* Done */

            i_pos += i_read;
            p_sys->i_data_len <<= 1 ;
            p_sys->psz_data = xrealloc( p_sys->psz_data,
                                   p_sys->i_data_len * sizeof( char * ) + 1 );
        }
        if( p_sys->i_data_len <= 0 ) return -1;
    }

    input_item_node_t *p_subitems = input_item_node_Create( p_current_input );

    psz_parse = p_sys->psz_data;
    /* Find first element */
    if( ( psz_parse = strcasestr( psz_parse, "<ASX" ) ) )
    {
        /* ASX element */
        char *psz_string = NULL;
        int i_strlen = 0;

        char *psz_base_asx = NULL;
        char *psz_title_asx = NULL;
        char *psz_artist_asx = NULL;
        char *psz_copyright_asx = NULL;
        char *psz_moreinfo_asx = NULL;
        char *psz_abstract_asx = NULL;

        char *psz_base_entry = NULL;
        char *psz_title_entry = NULL;
        char *psz_artist_entry = NULL;
        char *psz_copyright_entry = NULL;
        char *psz_moreinfo_entry = NULL;
        char *psz_abstract_entry = NULL;
        int i_entry_count = 0;
        bool b_skip_entry = false;

        char *psz_href = NULL;
        int i_starttime = 0;
        int i_duration = 0;

        psz_parse = strcasestr( psz_parse, ">" );

        /* counter for single ad item */
        input_item_t *uniq_entry_ad_backup = NULL;
        int i_inserted_entries = 0;

        while( psz_parse && ( psz_parse = strcasestr( psz_parse, "<" ) ) )
        {
            if( !strncasecmp( psz_parse, "<!--", 4 ) )
            {
                /* this is a comment */
                if( ( psz_parse = strcasestr( psz_parse, "-->" ) ) )
                    psz_parse+=3;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<PARAM ", 7 ) )
            {
                bool b_encoding_flag = false;
                psz_parse = SkipBlanks(psz_parse+7, (unsigned)-1);
                if( !strncasecmp( psz_parse, "name", 4 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;
                            msg_Dbg( p_demux, "param name strlen: %d", i_strlen);
                            psz_string = xmalloc( i_strlen + 1);
                            memcpy( psz_string, psz_backup, i_strlen );
                            psz_string[i_strlen] = '\0';
                            msg_Dbg( p_demux, "param name: %s", psz_string);
                            b_encoding_flag = !strcasecmp( psz_string, "encoding" );
                            free( psz_string );
                        }
                        else continue;
                    }
                    else continue;
                }
                psz_parse++;
                if( !strncasecmp( psz_parse, "value", 5 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;
                            msg_Dbg( p_demux, "param value strlen: %d", i_strlen);
                            psz_string = xmalloc( i_strlen +1);
                            memcpy( psz_string, psz_backup, i_strlen );
                            psz_string[i_strlen] = '\0';
                            msg_Dbg( p_demux, "param value: %s", psz_string);
                            if( b_encoding_flag && !strcasecmp( psz_string, "utf-8" ) ) p_sys->b_utf8 = true;
                            free( psz_string );
                        }
                        else continue;
                    }
                    else continue;
                }
                if( ( psz_parse = strcasestr( psz_parse, "/>" ) ) )
                    psz_parse += 2;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<BANNER", 7 ) )
            {
                /* We skip this element */
                if( ( psz_parse = strcasestr( psz_parse, "</BANNER>" ) ) )
                    psz_parse += 9;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<PREVIEWDURATION", 16 ) ||
                     !strncasecmp( psz_parse, "<LOGURL", 7 ) ||
                     !strncasecmp( psz_parse, "<Skin", 5 ) )
            {
                /* We skip this element */
                if( ( psz_parse = strcasestr( psz_parse, "/>" ) ) )
                    psz_parse += 2;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<BASE ", 6 ) )
            {
                psz_parse = SkipBlanks(psz_parse+6, (unsigned)-1);
                if( !strncasecmp( psz_parse, "HREF", 4 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            StoreString( p_demux, (b_entry ? &psz_base_entry : &psz_base_asx), psz_backup, psz_parse );
                        }
                        else continue;
                    }
                    else continue;
                }
                if( ( psz_parse = strcasestr( psz_parse, "/>" ) ) )
                    psz_parse += 2;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<TITLE>", 7 ) )
            {
                psz_backup = psz_parse+=7;
                if( ( psz_parse = strcasestr( psz_parse, "</TITLE>" ) ) )
                {
                    StoreString( p_demux, (b_entry ? &psz_title_entry : &psz_title_asx), psz_backup, psz_parse );
                    psz_parse += 8;
                }
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<Author>", 8 ) )
            {
                psz_backup = psz_parse+=8;
                if( ( psz_parse = strcasestr( psz_parse, "</Author>" ) ) )
                {
                    StoreString( p_demux, (b_entry ? &psz_artist_entry : &psz_artist_asx), psz_backup, psz_parse );
                    psz_parse += 9;
                }
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<Copyright", 10 ) )
            {
                psz_backup = psz_parse+=11;
                if( ( psz_parse = strcasestr( psz_parse, "</Copyright>" ) ) )
                {
                    StoreString( p_demux, (b_entry ? &psz_copyright_entry : &psz_copyright_asx), psz_backup, psz_parse );
                    psz_parse += 12;
                }
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<MoreInfo ", 10 ) )
            {
                psz_parse = SkipBlanks(psz_parse+10, (unsigned)-1);
                if( !strncasecmp( psz_parse, "HREF", 4 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            StoreString( p_demux, (b_entry ? &psz_moreinfo_entry : &psz_moreinfo_asx), psz_backup, psz_parse );
                        }
                        else continue;
                    }
                    else continue;
                }
                if( ( psz_backup = strcasestr( psz_parse, "/>" ) ) )
                    psz_parse = psz_backup + 2;
                else if( ( psz_backup = strcasestr( psz_parse, "</MoreInfo>") ) )
                    psz_parse = psz_backup + 11;
                else
                {
                    psz_parse = NULL;
                    continue;
                }
            }
            else if( !strncasecmp( psz_parse, "<ABSTRACT>", 10 ) )
            {
                psz_backup = psz_parse+=10;
                if( ( psz_parse = strcasestr( psz_parse, "</ABSTRACT>" ) ) )
                {
                    StoreString( p_demux, (b_entry ? &psz_abstract_entry : &psz_abstract_asx), psz_backup, psz_parse );
                    psz_parse += 11;
                }
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<EntryRef ", 10 ) )
            {
                psz_parse = SkipBlanks(psz_parse+10, (unsigned)-1);
                if( !strncasecmp( psz_parse, "HREF", 4 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;
                            psz_string = xmalloc( i_strlen +1);
                            memcpy( psz_string, psz_backup, i_strlen );
                            psz_string[i_strlen] = '\0';
                            input_item_t *p_input;
                            p_input = input_item_New( psz_string, psz_title_asx );
                            input_item_CopyOptions( p_current_input, p_input );
                            input_item_node_AppendItem( p_subitems, p_input );
                            vlc_gc_decref( p_input );
                            free( psz_string );
                        }
                        else continue;
                    }
                    else continue;
                }
                if( ( psz_parse = strcasestr( psz_parse, "/>" ) ) )
                    psz_parse += 2;
            }
            else if( !strncasecmp( psz_parse, "</Entry>", 8 ) )
            {
                input_item_t *p_entry = NULL;
                char *psz_name = NULL;

                char * ppsz_options[2];
                int i_options = 0;

                /* add a new entry */
                psz_parse+=8;
                if( !b_entry )
                {
                    msg_Err( p_demux, "end of entry without start?" );
                    continue;
                }

                if( !psz_href )
                {
                    msg_Err( p_demux, "entry without href?" );
                    continue;
                }
                /* An skip entry is an ad only if other entries exist without skip */
                if( p_sys->b_skip_ads && b_skip_entry && i_inserted_entries != 0 )
                {
                    char *psz_current_input_name = input_item_GetName( p_current_input );
                    msg_Dbg( p_demux, "skipped entry %d %s (%s)",
                             i_entry_count,
                             ( psz_title_entry ? psz_title_entry : psz_current_input_name ), psz_href );
                    free( psz_current_input_name );
                }
                else
                {
                    if( i_starttime || i_duration )
                    {
                        if( i_starttime )
                        {
                            if( asprintf(ppsz_options+i_options, ":start-time=%d", i_starttime) == -1 )
                                *(ppsz_options+i_options) = NULL;
                            else
                                ++i_options;
                        }
                        if( i_duration )
                        {
                            if( asprintf(ppsz_options+i_options, ":stop-time=%d", i_starttime + i_duration) == -1 )
                                *(ppsz_options+i_options) = NULL;
                            else
                                ++i_options;
                        }
                    }

                    /* create the new entry */
                    char *psz_current_input_name = input_item_GetName( p_current_input );
                    if( asprintf( &psz_name, "%d %s", i_entry_count, ( psz_title_entry ? psz_title_entry : psz_current_input_name ) ) != -1 )
                    {
                        char *psz_mrl = ProcessMRL( psz_href, p_demux->p_sys->psz_prefix );
                        p_entry = input_item_NewExt( psz_mrl, psz_name,
                                                     i_options, (const char * const *)ppsz_options, VLC_INPUT_OPTION_TRUSTED, -1 );
                        free( psz_name );
                        free( psz_mrl );
                        input_item_CopyOptions( p_current_input, p_entry );
                        while( i_options )
                        {
                            psz_name = ppsz_options[--i_options];
                            free( psz_name );
                        }
                        psz_name = NULL;

                        if( psz_title_entry ) input_item_SetTitle( p_entry, psz_title_entry );
                        if( psz_artist_entry ) input_item_SetArtist( p_entry, psz_artist_entry );
                        if( psz_copyright_entry ) input_item_SetCopyright( p_entry, psz_copyright_entry );
                        if( psz_moreinfo_entry ) input_item_SetURL( p_entry, psz_moreinfo_entry );
                        if( psz_abstract_entry ) input_item_SetDescription( p_entry, psz_abstract_entry );

                        i_inserted_entries++;
                        if( p_sys->b_skip_ads && b_skip_entry )
                        {
                            // We put the entry as a backup for unique ad case
                            uniq_entry_ad_backup = p_entry;
                        }
                        else
                        {
                            if( uniq_entry_ad_backup != NULL )
                            {
                                vlc_gc_decref( uniq_entry_ad_backup );
                                uniq_entry_ad_backup = NULL;
                            }
                            input_item_node_AppendItem( p_subitems, p_entry );
                            vlc_gc_decref( p_entry );
                        }
                    }
                    free( psz_current_input_name );
                }

                /* cleanup entry */;
                FREENULL( psz_href );
                FREENULL( psz_title_entry );
                FREENULL( psz_base_entry );
                FREENULL( psz_artist_entry );
                FREENULL( psz_copyright_entry );
                FREENULL( psz_moreinfo_entry );
                FREENULL( psz_abstract_entry );
                b_entry = false;
            }
            else if( !strncasecmp( psz_parse, "<Entry", 6 ) )
            {
                char *psz_clientskip;
                psz_parse+=6;
                if( b_entry )
                {
                    msg_Err( p_demux, "We already are in an entry section" );
                    continue;
                }
                i_entry_count += 1;
                b_entry = true;
                psz_clientskip = strcasestr( psz_parse, "clientskip=\"no\"" );
                psz_parse = strcasestr( psz_parse, ">" );

                /* If clientskip was enabled ... this is an ad */
                b_skip_entry = (NULL != psz_clientskip) && (psz_clientskip < psz_parse);

                // init entry details
                FREENULL(psz_href);
                i_starttime = 0;
                i_duration = 0;
            }
            else if( !strncasecmp( psz_parse, "<Ref ", 5 ) )
            {
                psz_parse = SkipBlanks(psz_parse+5, (unsigned)-1);
                if( !b_entry )
                {
                    msg_Err( p_demux, "A ref outside an entry section" );
                    continue;
                }

                if( !strncasecmp( psz_parse, "HREF", 4 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        psz_backup = SkipBlanks(psz_backup, (unsigned)-1);
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            char *psz_tmp;
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;

                            if( psz_href )
                            {
                                /* we have allready one href in this entry, lets make new input from it and
                                continue with new href, don't free meta/options*/
                                input_item_t *p_entry = NULL;
                                char *psz_name = input_item_GetName( p_current_input );

                                char *psz_mrl = ProcessMRL( psz_href, p_demux->p_sys->psz_prefix );
                                p_entry = input_item_NewExt( psz_mrl, psz_name,
                                                     0, NULL, VLC_INPUT_OPTION_TRUSTED, -1 );
                                free( psz_mrl );
                                input_item_CopyOptions( p_current_input, p_entry );
                                if( psz_title_entry ) input_item_SetTitle( p_entry, psz_title_entry );
                                if( psz_artist_entry ) input_item_SetArtist( p_entry, psz_artist_entry );
                                if( psz_copyright_entry ) input_item_SetCopyright( p_entry, psz_copyright_entry );
                                if( psz_moreinfo_entry ) input_item_SetURL( p_entry, psz_moreinfo_entry );
                                if( psz_abstract_entry ) input_item_SetDescription( p_entry, psz_abstract_entry );
                                input_item_node_AppendItem( p_subitems, p_entry );
                                vlc_gc_decref( p_entry );
                            }

                            free( psz_href );
                            psz_href = xmalloc( i_strlen +1);
                            memcpy( psz_href, psz_backup, i_strlen );
                            psz_href[i_strlen] = '\0';
                            psz_tmp = psz_href + (i_strlen-1);
                            while( psz_tmp >= psz_href &&
                                 ( *psz_tmp == '\r' || *psz_tmp == '\n' ) )
                            {
                                *psz_tmp = '\0';
                                psz_tmp++;
                            }
                        }
                        else continue;
                    }
                    else continue;
                }
                if( ( psz_parse = strcasestr( psz_parse, ">" ) ) )
                    psz_parse++;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<starttime ", 11 ) )
            {
                psz_parse = SkipBlanks(psz_parse+11, (unsigned)-1);
                if( !b_entry )
                {
                    msg_Err( p_demux, "starttime outside an entry section" );
                    continue;
                }

                if( !strncasecmp( psz_parse, "value", 5 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;

                            i_starttime = ParseTime(psz_backup, i_strlen);
                        }
                        else continue;
                    }
                    else continue;
                }
                if( ( psz_parse = strcasestr( psz_parse, ">" ) ) )
                    psz_parse++;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "<duration ", 11 ) )
            {
                psz_parse = SkipBlanks(psz_parse+5, (unsigned)-1);
                if( !b_entry )
                {
                    msg_Err( p_demux, "duration outside an entry section" );
                    continue;
                }

                if( !strncasecmp( psz_parse, "value", 5 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;

                            i_duration = ParseTime(psz_backup, i_strlen);
                        }
                        else continue;
                    }
                    else continue;
                }
                if( ( psz_parse = strcasestr( psz_parse, ">" ) ) )
                    psz_parse++;
                else continue;
            }
            else if( !strncasecmp( psz_parse, "</ASX", 5 ) )
            {
                if( psz_title_asx ) input_item_SetTitle( p_current_input, psz_title_asx );
                if( psz_artist_asx ) input_item_SetArtist( p_current_input, psz_artist_asx );
                if( psz_copyright_asx ) input_item_SetCopyright( p_current_input, psz_copyright_asx );
                if( psz_moreinfo_asx ) input_item_SetURL( p_current_input, psz_moreinfo_asx );
                if( psz_abstract_asx ) input_item_SetDescription( p_current_input, psz_abstract_asx );
                FREENULL( psz_base_asx );
                FREENULL( psz_title_asx );
                FREENULL( psz_artist_asx );
                FREENULL( psz_copyright_asx );
                FREENULL( psz_moreinfo_asx );
                FREENULL( psz_abstract_asx );
                psz_parse++;
            }
            else psz_parse++;
        }
        if ( uniq_entry_ad_backup != NULL )
        {
            msg_Dbg( p_demux, "added unique entry even if ad");
            /* If ASX contains a unique entry, we add it, it is probably not an ad */
            input_item_node_AppendItem( p_subitems, uniq_entry_ad_backup );
            vlc_gc_decref( uniq_entry_ad_backup);
        }
#if 0
/* FIXME Unsupported elements */
            PARAM
            EVENT
            REPEAT
            ENDMARK
            STARTMARK
#endif
    }

    input_item_node_PostAndDelete( p_subitems );

    vlc_gc_decref(p_current_input);
    return 0; /* Needed for correct operation of go back */
}

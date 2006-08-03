/*****************************************************************************
 * asx.c : ASX playlist format import
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
 *
 * Authors: Derk-Jan Hartman <hartman at videolan dot org>
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

/* See also: http://msdn.microsoft.com/library/en-us/wmplay10/mmp_sdk/windowsmediametafilereference.asp
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <ctype.h>                                              /* isspace() */

#include <vlc/vlc.h>
#include <vlc/input.h>

#include <errno.h>                                                 /* ENOMEM */
#include "playlist.h"
#include "vlc_meta.h"

struct demux_sys_t
{
    char    *psz_prefix;
    char    *psz_data;
    int64_t i_data_len;
    vlc_bool_t b_utf8;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int Demux( demux_t *p_demux);
static int Control( demux_t *p_demux, int i_query, va_list args );

static int StoreString( demux_t *p_demux, char **ppsz_string, char *psz_source_start, char *psz_source_end )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    int i_strlen = psz_source_end-psz_source_start;
    if( i_strlen < 1 )
        return VLC_EGENERIC;

    if( *ppsz_string ) free( *ppsz_string );
    *ppsz_string = malloc( i_strlen*sizeof( char ) +1);
    memcpy( *ppsz_string, psz_source_start, i_strlen );
    (*ppsz_string)[i_strlen] = '\0';

    if( p_sys->b_utf8 )
        EnsureUTF8( *ppsz_string );
    else
    {
        char *psz_temp;
        psz_temp = FromLocaleDup( *ppsz_string );
        if( psz_temp )
        {
            free( *ppsz_string );
            *ppsz_string = psz_temp;
        } else EnsureUTF8( *ppsz_string );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Import_ASX: main import function
 *****************************************************************************/
int E_(Import_ASX)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    uint8_t *p_peek;
    CHECK_PEEK( p_peek, 10 );
    
    if( POKE( p_peek, "<asx", 4 ) || isExtension( p_demux, ".asx" ) ||
        isExtension( p_demux, ".wax" ) || isExtension( p_demux, ".wvx" ) ||
        isDemux( p_demux, "asx-open" ) )
    {
        ;
    }
    else
        return VLC_EGENERIC;
    
    STANDARD_DEMUX_INIT_MSG( "found valid ASX playlist" );
    p_demux->p_sys->psz_prefix = E_(FindPrefix)( p_demux );
    p_demux->p_sys->psz_data = NULL;
    p_demux->p_sys->i_data_len = -1;
    p_demux->p_sys->b_utf8 = VLC_FALSE;
    
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Deactivate: frees unused data
 *****************************************************************************/
void E_(Close_ASX)( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t *)p_this;
    demux_sys_t *p_sys = p_demux->p_sys;

    if( p_sys->psz_prefix ) free( p_sys->psz_prefix );
    if( p_sys->psz_data ) free( p_sys->psz_data );
    free( p_sys );
}

static int Demux( demux_t *p_demux )
{
    demux_sys_t *p_sys = p_demux->p_sys;
    char        *psz_parse = NULL;
    char        *psz_backup = NULL;
    vlc_bool_t  b_entry = VLC_FALSE;

    INIT_PLAYLIST_STUFF;

    /* init txt */
    if( p_sys->i_data_len < 0 )
    {
        int64_t i_pos = 0;
        p_sys->i_data_len = stream_Size( p_demux->s ) +1; /* This is a cheat to prevent unnecessary realloc */
        if( p_sys->i_data_len <= 0 && p_sys->i_data_len < 16384 ) p_sys->i_data_len = 1024;
        p_sys->psz_data = malloc( p_sys->i_data_len * sizeof(char) +1);
        
        /* load the complete file */
        for( ;; )
        {
            int i_read = stream_Read( p_demux->s, &p_sys->psz_data[i_pos], p_sys->i_data_len - i_pos );
            p_sys->psz_data[i_read] = '\0';
           
            if( i_read < p_sys->i_data_len - i_pos ) break; /* Done */
            
            i_pos += i_read;
            p_sys->i_data_len += 1024;
            p_sys->psz_data = realloc( p_sys->psz_data, p_sys->i_data_len * sizeof( char * ) +1 );
        }
        if( p_sys->i_data_len <= 0 ) return VLC_EGENERIC;
    }

    psz_parse = p_sys->psz_data;
    /* Find first element */
    if( ( psz_parse = strcasestr( psz_parse, "<ASX" ) ) )
    {
        /* ASX element */
        char *psz_string = NULL;
        int i_strlen = 0;

        char *psz_base_asx = NULL;
        char *psz_title_asx = NULL;
        char *psz_author_asx = NULL;
        char *psz_copyright_asx = NULL;
        char *psz_moreinfo_asx = NULL;
        char *psz_abstract_asx = NULL;
        
        char *psz_base_entry = NULL;
        char *psz_title_entry = NULL;
        char *psz_author_entry = NULL;
        char *psz_copyright_entry = NULL;
        char *psz_moreinfo_entry = NULL;
        char *psz_abstract_entry = NULL;
        int i_entry_count = 0;
    
        psz_parse = strcasestr( psz_parse, ">" );

        while( ( psz_parse = strcasestr( psz_parse, "<" ) ) && psz_parse && *psz_parse )
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
                vlc_bool_t b_encoding_flag = VLC_FALSE;
                psz_parse+=7;
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
                            psz_string = malloc( i_strlen *sizeof( char ) +1);
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
                            psz_string = malloc( i_strlen *sizeof( char ) +1);
                            memcpy( psz_string, psz_backup, i_strlen );
                            psz_string[i_strlen] = '\0';
                            msg_Dbg( p_demux, "param value: %s", psz_string);
                            if( b_encoding_flag && !strcasecmp( psz_string, "utf-8" ) ) p_sys->b_utf8 = VLC_TRUE;
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
                psz_parse+=6;
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
                    StoreString( p_demux, (b_entry ? &psz_author_entry : &psz_author_asx), psz_backup, psz_parse );
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
                psz_parse+=10;
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
                if( ( psz_parse = strcasestr( psz_parse, "/>" ) ) )
                    psz_parse += 2;
                else continue;
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
                psz_parse+=10;
                if( !strncasecmp( psz_parse, "HREF", 4 ) )
                {
                    if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                    {
                        psz_backup = ++psz_parse;
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;
                            psz_string = malloc( i_strlen*sizeof( char ) +1);
                            memcpy( psz_string, psz_backup, i_strlen );
                            psz_string[i_strlen] = '\0';
                            p_input = input_ItemNew( p_playlist, psz_string, psz_title_asx );
                            vlc_input_item_CopyOptions( p_current->p_input, p_input );
                            playlist_AddWhereverNeeded( p_playlist, p_input, p_current,
                                 p_item_in_category, (i_parent_id > 0 )? VLC_TRUE : VLC_FALSE,
                                 PLAYLIST_APPEND );
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
            else if( !strncasecmp( psz_parse, "</Entry>", 8 ) )
            {
                /* add a new entry */
                psz_parse+=8;
                if( !b_entry )
                {
                    msg_Err( p_demux, "end of entry without start?" );
                    continue;
                }
                /* cleanup entry */
                FREENULL( psz_title_entry )
                FREENULL( psz_base_entry )
                FREENULL( psz_author_entry )
                FREENULL( psz_copyright_entry )
                FREENULL( psz_moreinfo_entry )
                FREENULL( psz_abstract_entry )
                b_entry = VLC_FALSE;
            }
            else if( !strncasecmp( psz_parse, "<Entry>", 7 ) )
            {
                psz_parse+=7;
                if( b_entry )
                {
                    msg_Err( p_demux, "We already are in an entry section" );
                    continue;
                }
                i_entry_count += 1;
                b_entry = VLC_TRUE;
            }
            else if( !strncasecmp( psz_parse, "<Ref ", 5 ) )
            {
                psz_parse+=5;
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
                        if( ( psz_parse = strcasestr( psz_parse, "\"" ) ) )
                        {
                            input_item_t *p_entry = NULL;
                            char *psz_name = NULL;
                            i_strlen = psz_parse-psz_backup;
                            if( i_strlen < 1 ) continue;
                            psz_string = malloc( i_strlen*sizeof( char ) +1);
                            memcpy( psz_string, psz_backup, i_strlen );
                            psz_string[i_strlen] = '\0';

                            /* create the new entry */
                            asprintf( &psz_name, "%d %s", i_entry_count, ( psz_title_entry ? psz_title_entry : p_current->p_input->psz_name ) );
                            p_entry = input_ItemNew( p_playlist, psz_string, psz_name );
                            FREENULL( psz_name );
                            
                            vlc_input_item_CopyOptions( p_current->p_input, p_entry );
                            p_entry->p_meta = vlc_meta_New();
                            if( psz_title_entry ) vlc_meta_SetTitle( p_entry->p_meta, psz_title_entry );
                            if( psz_author_entry ) vlc_meta_SetAuthor( p_entry->p_meta, psz_author_entry );
                            if( psz_copyright_entry ) vlc_meta_SetCopyright( p_entry->p_meta, psz_copyright_entry );
                            if( psz_moreinfo_entry ) vlc_meta_SetURL( p_entry->p_meta, psz_moreinfo_entry );
                            if( psz_abstract_entry ) vlc_meta_SetDescription( p_entry->p_meta, psz_abstract_entry );
                            
                            playlist_AddWhereverNeeded( p_playlist, p_entry, p_current,
                                p_item_in_category, (i_parent_id > 0 )? VLC_TRUE : VLC_FALSE,
                                PLAYLIST_APPEND );
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
            else if( !strncasecmp( psz_parse, "</ASX", 5 ) )
            {
                vlc_mutex_lock( &p_current->p_input->lock );
                if( !p_current->p_input->p_meta ) p_current->p_input->p_meta = vlc_meta_New();
                if( psz_title_asx ) vlc_meta_SetTitle( p_current->p_input->p_meta, psz_title_asx );
                if( psz_author_asx ) vlc_meta_SetAuthor( p_current->p_input->p_meta, psz_author_asx );
                if( psz_copyright_asx ) vlc_meta_SetCopyright( p_current->p_input->p_meta, psz_copyright_asx );
                if( psz_moreinfo_asx ) vlc_meta_SetURL( p_current->p_input->p_meta, psz_moreinfo_asx );
                if( psz_abstract_asx ) vlc_meta_SetDescription( p_current->p_input->p_meta, psz_abstract_asx );
                vlc_mutex_unlock( &p_current->p_input->lock );
                FREENULL( psz_base_asx );
                FREENULL( psz_title_asx );
                FREENULL( psz_author_asx );
                FREENULL( psz_copyright_asx );
                FREENULL( psz_moreinfo_asx );
                FREENULL( psz_abstract_asx );
                psz_parse++;
            }
            else psz_parse++;
        }
#if 0
/* FIXME Unsupported elements */
            PARAM
            EVENT
            REPEAT
            DURATION
            ENDMARK
            STARTMARK
            STARTTIME
#endif
    }
    HANDLE_PLAY_AND_RELEASE;
    return VLC_SUCCESS;
}

static int Control( demux_t *p_demux, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

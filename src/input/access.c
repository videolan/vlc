/*****************************************************************************
 * access.c
 *****************************************************************************
 * Copyright (C) 1999-2004 VideoLAN
 * $Id: demux.c 7546 2004-04-29 13:53:29Z gbazin $
 *
 * Author: Laurent Aimar <fenrir@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include <stdlib.h>
#include <vlc/vlc.h>
#include <vlc/input.h>

#include "ninput.h"

int access_vaControl( input_thread_t *p_input, int i_query, va_list args )
{
    if( p_input->pf_access_control )
    {
        return p_input->pf_access_control( p_input, i_query, args );
    }
    return VLC_EGENERIC;
}

int access_Control( input_thread_t *p_input, int i_query, ...  )
{
    va_list args;
    int     i_result;

    va_start( args, i_query );
    i_result = access_vaControl( p_input, i_query, args );
    va_end( args );

    return i_result;
}

int access_vaControlDefault( input_thread_t *p_input, int i_query, va_list args )
{
    return VLC_EGENERIC;
}

/*****************************************************************************
 * access2_New:
 *****************************************************************************/
access_t *__access2_New( vlc_object_t *p_obj, char *psz_mrl )
{
    msg_Err( p_obj, "access2_New not yet implemented" );
    return NULL;
#if 0
    access_t *p_access = vlc_object_create( p_obj, VLC_OBJECT_ACCESS );

    char    *psz_dup = strdup( psz_mrl ? psz_mrl : "" );
    char    *psz = strchr( psz_dup, ':' );
    char    *psz_module;

    if( p_demux == NULL )
    {
        free( psz_dup );
        return NULL;
    }

    /* Parse URL */
    p_demux->psz_access = NULL;
    p_demux->psz_demux  = NULL;
    p_demux->psz_path   = NULL;

    if( psz )
    {
        *psz++ = '\0';

        if( psz[0] == '/' && psz[1] == '/' )
        {
            psz += 2;
        }
        p_demux->psz_path = strdup( psz );

        psz = strchr( psz_dup, '/' );
        if( psz )
        {
            *psz++ = '\0';
            p_demux->psz_access = strdup( psz_dup );
            p_demux->psz_demux  = strdup( psz );
        }
    }
    else
    {
        p_demux->psz_path = strdup( psz_mrl );
    }
    free( psz_dup );


    if( p_demux->psz_access == NULL )
    {
        p_demux->psz_access = strdup( "" );
    }
    if( p_demux->psz_demux == NULL )
    {
        p_demux->psz_demux = strdup( "" );
    }
    if( p_demux->psz_path == NULL )
    {
        p_demux->psz_path = strdup( "" );
    }
    msg_Dbg( p_obj, "demux2_New: '%s' -> access='%s' demux='%s' path='%s'",
             psz_mrl,
             p_demux->psz_access, p_demux->psz_demux, p_demux->psz_path );

    p_demux->s          = s;
    p_demux->out        = out;

    p_demux->pf_demux   = NULL;
    p_demux->pf_control = NULL;
    p_demux->p_sys      = NULL;

    psz_module = p_demux->psz_demux;
    if( *psz_module == '\0' && strrchr( p_demux->psz_path, '.' ) )
    {
        /* XXX: add only file without any problem here and with strong detection.
         *  - no .mp3, .a52, ... (aac is added as it works only by file ext anyway
         *  - wav can't be added 'cause of a52 and dts in them as raw audio
         */
        static struct { char *ext; char *demux; } exttodemux[] =
        {
            { "aac",  "aac" },
            { "aiff", "aiff" },
            { "asf",  "asf" }, { "wmv",  "asf" }, { "wma",  "asf" },
            { "avi",  "avi" },
            { "au",   "au" },
            { "flac", "flac" },
            { "dv",   "dv" },
            { "m3u",  "m3u" },
            { "mkv",  "mkv" }, { "mka",  "mkv" }, { "mks",  "mkv" },
            { "mp4",  "mp4" }, { "m4a",  "mp4" }, { "mov",  "mp4" }, { "moov", "mp4" },
            { "mod",  "mod" }, { "xm",   "mod" },
            { "nsv",  "nsv" },
            { "ogg",  "ogg" }, { "ogm",  "ogg" },
            { "pva",  "pva" },
            { "rm",   "rm" },
            { "",  "" },
        };

        char *psz_ext = strrchr( p_demux->psz_path, '.' ) + 1;
        int  i;

        for( i = 0; exttodemux[i].ext != NULL; i++ )
        {
            if( !strcasecmp( psz_ext, exttodemux[i].ext ) )
            {
                psz_module = exttodemux[i].demux;
                break;
            }
        }
    }

    /* Before module_Need (for var_Create...) */
    vlc_object_attach( p_demux, p_obj );

    p_demux->p_module =
        module_Need( p_demux, "demux2", psz_module,
                     !strcmp( psz_module, p_demux->psz_demux ) ? VLC_TRUE : VLC_FALSE );

    if( p_demux->p_module == NULL )
    {
        vlc_object_detach( p_demux );
        free( p_demux->psz_path );
        free( p_demux->psz_demux );
        free( p_demux->psz_access );
        vlc_object_destroy( p_demux );
        return NULL;
    }

    return p_demux;
#endif
}

/*****************************************************************************
 * demux2_Delete:
 *****************************************************************************/
void access2_Delete( access_t *p_access )
{
    module_Unneed( p_access, p_access->p_module );
    vlc_object_detach( p_access );

    free( p_access->psz_access );
    free( p_access->psz_path );
    free( p_access->psz_demux );

    vlc_object_destroy( p_access );
}

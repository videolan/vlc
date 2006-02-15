/*****************************************************************************
 * vlm.c: libvlc new API VLM handling functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id: playlist.c 14265 2006-02-12 17:31:39Z zorglub $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#include <libvlc_internal.h>
#include <vlc/libvlc.h>

#include <vlc/vlc.h>
#include <vlc_input.h>
#include <vlc_vlm.h>

void InitVLM( libvlc_instance_t *p_instance )
{
    if( p_instance->p_vlm ) return;
    p_instance->p_vlm = vlm_New( p_instance->p_vlc );
}

#define CHECK_VLM { if( !p_instance->p_vlm ) InitVLM( p_instance ); \
                    if( !p_instance->p_vlm ) {\
                      libvlc_exception_raise( p_exception, \
                                         "Unable to create VLM" ); return; } }

#define GET_MEDIA { p_media = vlm_MediaSearch( p_instance->p_vlm, psz_name );\
                   if( !p_media ) \
                   { \
                        libvlc_exception_raise( p_exception, \
                                                "Media %s does not exist", \
                                                psz_name ); return; } }

void libvlc_vlm_add_broadcast( libvlc_instance_t *p_instance, char *psz_name,
                               char *psz_input, char *psz_output,
                               int i_options, char **ppsz_options,
                               int b_enabled, int b_loop,
                               libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    CHECK_VLM;

    p_media = vlm_MediaNew( p_instance->p_vlm, psz_name, BROADCAST_TYPE );
    if( !p_media )
    {
        libvlc_exception_raise( p_exception, "Media %s creation failed",
                                psz_name );
        return;
    }
    libvlc_vlm_change_media( p_instance, psz_name, psz_input, psz_output,
                             i_options, ppsz_options, b_enabled, b_loop,
                             p_exception );

}

void libvlc_vlm_del_media( libvlc_instance_t *p_instance, char *psz_name,
                           libvlc_exception_t *p_exception )
{
    char *psz_message;
    vlm_message_t *answer;
    CHECK_VLM;
    asprintf( &psz_message, "del %s", psz_name );
    vlm_ExecuteCommand( p_instance->p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_exception_raise( p_exception, "Unable to delete %s",
                                psz_name );
    }
    free( psz_message);
}

void libvlc_vlm_set_enabled( libvlc_instance_t *p_instance, char *psz_name,
                             int b_enabled, libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    CHECK_VLM;
    GET_MEDIA;
    if( b_enabled != 0 ) b_enabled = 1;
    p_media->b_enabled = b_enabled;
}

void libvlc_vlm_set_loop( libvlc_instance_t *p_instance, char *psz_name,
                          int b_loop, libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    CHECK_VLM;
    GET_MEDIA;
    if( b_loop != 0 ) b_loop = 1;
    p_media->b_loop = b_loop;
}

void libvlc_vlm_set_output( libvlc_instance_t *p_instance, char *psz_name,
                            char *psz_output,  libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
    GET_MEDIA;

    vlc_mutex_lock( &p_instance->p_vlm->lock );
    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "output", psz_output );
    if( i_ret )
    { libvlc_exception_raise( p_exception, "Unable to set output" ); return;}
    vlc_mutex_unlock( &p_instance->p_vlm->lock );
}

void libvlc_vlm_set_input( libvlc_instance_t *p_instance, char *psz_name,
                           char *psz_input,  libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
    GET_MEDIA;

    vlc_mutex_lock( &p_instance->p_vlm->lock );

    vlm_MediaSetup( p_instance->p_vlm, p_media, "inputdel", "all" );
    if( i_ret )
    { libvlc_exception_raise( p_exception, "Unable to change input" ); return;}
    vlm_MediaSetup( p_instance->p_vlm, p_media, "input", psz_input );
    if( i_ret )
    { libvlc_exception_raise( p_exception, "Unable to change input" ); return;}

    vlc_mutex_unlock( &p_instance->p_vlm->lock );
}

void libvlc_vlm_add_input( libvlc_instance_t *p_instance, char *psz_name,
                           char *psz_input,  libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
    GET_MEDIA;

    vlc_mutex_lock( &p_instance->p_vlm->lock );

    vlm_MediaSetup( p_instance->p_vlm, p_media, "input", psz_input );
    if( i_ret )
    { libvlc_exception_raise( p_exception, "Unable to change input" ); return;}

    vlc_mutex_unlock( &p_instance->p_vlm->lock );
}




void libvlc_vlm_change_media( libvlc_instance_t *p_instance, char *psz_name,
                              char *psz_input, char *psz_output, int i_options,
                              char **ppsz_options, int b_enabled, int b_loop,
                              libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
    GET_MEDIA;
    if( b_enabled != 0 ) b_enabled = 1;
    if( b_loop != 0 ) b_loop = 1;

    vlc_mutex_lock( &p_instance->p_vlm->lock );
    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "output", psz_output );
    if( i_ret ) libvlc_exception_raise( p_exception, "Unable to set output" );
    p_media->b_enabled = b_enabled;
    p_media->b_loop = b_loop;

    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "output", psz_output );
    if( i_ret )
    { libvlc_exception_raise( p_exception, "Unable to set output" ); return;}
    vlm_MediaSetup( p_instance->p_vlm, p_media, "inputdel", "all" );
    if( i_ret )
    { libvlc_exception_raise( p_exception, "Unable to change input" ); return;}
    vlm_MediaSetup( p_instance->p_vlm, p_media, "input", psz_input );
    if( i_ret )
    { libvlc_exception_raise( p_exception, "Unable to change input" ); return;}

    vlc_mutex_unlock( &p_instance->p_vlm->lock );
}

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

#include "libvlc_internal.h"
#include <vlc/libvlc.h>
#include <vlc_es.h>
#include <vlc_input.h>
#include "../input/vlm_internal.h"

static void InitVLM( libvlc_instance_t *p_instance )
{
#ifdef ENABLE_VLM
    if( p_instance->p_vlm ) return;
    p_instance->p_vlm = vlm_New( p_instance->p_libvlc_int );
#else
    p_instance->p_vlm = NULL;
#endif
}

#define CHECK_VLM { if( !p_instance->p_vlm ) InitVLM( p_instance ); \
                    if( !p_instance->p_vlm ) {\
                  libvlc_exception_raise( p_exception, \
                  "Unable to create VLM. It might be disabled." ); return; } }

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
#ifdef ENABLE_VLM
    asprintf( &psz_message, "del %s", psz_name );
    vlm_ExecuteCommand( p_instance->p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_exception_raise( p_exception, "Unable to delete %s",
                                psz_name );
    }
    free( psz_message);
#endif
}

void libvlc_vlm_set_enabled( libvlc_instance_t *p_instance, char *psz_name,
                             int b_enabled, libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    CHECK_VLM;
#ifdef ENABLE_VLM
    GET_MEDIA;
    if( b_enabled != 0 ) b_enabled = 1;
    p_media->b_enabled = b_enabled;
#endif
}

void libvlc_vlm_set_loop( libvlc_instance_t *p_instance, char *psz_name,
                          int b_loop, libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    CHECK_VLM;
#ifdef ENABLE_VLM
    GET_MEDIA;
    if( b_loop != 0 ) b_loop = 1;
    p_media->b_loop = b_loop;
#endif
}

void libvlc_vlm_set_output( libvlc_instance_t *p_instance, char *psz_name,
                            char *psz_output,  libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
#ifdef ENABLE_VLM
    GET_MEDIA;

    vlc_mutex_lock( &p_instance->p_vlm->lock );
    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "output", psz_output );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to set output" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }
    vlc_mutex_unlock( &p_instance->p_vlm->lock );
#endif
}

void libvlc_vlm_set_input( libvlc_instance_t *p_instance, char *psz_name,
                           char *psz_input,  libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
#ifdef ENABLE_VLM
    vlc_mutex_lock( &p_instance->p_vlm->lock );
    GET_MEDIA;

    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "inputdel", "all" );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to change input" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }
    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "input", psz_input );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to change input" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }
    vlc_mutex_unlock( &p_instance->p_vlm->lock );
#endif
}

void libvlc_vlm_add_input( libvlc_instance_t *p_instance, char *psz_name,
                           char *psz_input,  libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
#ifdef ENABLE_VLM
    vlc_mutex_lock( &p_instance->p_vlm->lock );
    GET_MEDIA;

    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "input", psz_input );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to change input" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }

    vlc_mutex_unlock( &p_instance->p_vlm->lock );
#endif
}


void libvlc_vlm_change_media( libvlc_instance_t *p_instance, char *psz_name,
                              char *psz_input, char *psz_output, int i_options,
                              char **ppsz_options, int b_enabled, int b_loop,
                              libvlc_exception_t *p_exception )
{
    vlm_media_t *p_media;
    int i_ret;
    CHECK_VLM;
#ifdef ENABLE_VLM
    vlc_mutex_lock( &p_instance->p_vlm->lock );
    GET_MEDIA;
    if( b_enabled != 0 ) b_enabled = 1;
    if( b_loop != 0 ) b_loop = 1;

    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "output", psz_output );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to set output" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }
    p_media->b_enabled = b_enabled;
    p_media->b_loop = b_loop;

    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "output", psz_output );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to set output" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }
    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "inputdel", "all" );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to change input" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }
    i_ret = vlm_MediaSetup( p_instance->p_vlm, p_media, "input", psz_input );
    if( i_ret )
    {
        libvlc_exception_raise( p_exception, "Unable to change input" );
        vlc_mutex_unlock( &p_instance->p_vlm->lock );
        return;
    }

    vlc_mutex_unlock( &p_instance->p_vlm->lock );
#endif
}

void libvlc_vlm_play_media( libvlc_instance_t *p_instance, char *psz_name,
                            libvlc_exception_t *p_exception )
    
{
    char *psz_message;
    vlm_message_t *answer;
    CHECK_VLM;
#ifdef ENABLE_VLM
    asprintf( &psz_message, "control %s play", psz_name );
    vlm_ExecuteCommand( p_instance->p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_exception_raise( p_exception, "Unable to play %s",
                                psz_name );
    }
    free( psz_message);
#endif
}

void libvlc_vlm_stop_media( libvlc_instance_t *p_instance, char *psz_name,
                            libvlc_exception_t *p_exception )
    
{
    char *psz_message;
    vlm_message_t *answer;
    CHECK_VLM;
#ifdef ENABLE_VLM
    asprintf( &psz_message, "control %s stop", psz_name );
    vlm_ExecuteCommand( p_instance->p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_exception_raise( p_exception, "Unable to stop %s",
                                psz_name );
    }
    free( psz_message);
#endif
}

void libvlc_vlm_pause_media( libvlc_instance_t *p_instance, char *psz_name,
                            libvlc_exception_t *p_exception )
    
{
    char *psz_message;
    vlm_message_t *answer;
    CHECK_VLM;
#ifdef ENABLE_VLM
    asprintf( &psz_message, "control %s pause", psz_name );
    vlm_ExecuteCommand( p_instance->p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_exception_raise( p_exception, "Unable to pause %s",
                                psz_name );
    }
    free( psz_message );
#endif
}

void libvlc_vlm_seek_media( libvlc_instance_t *p_instance, char *psz_name,
                            float f_percentage, libvlc_exception_t *p_exception )
    
{
    char *psz_message;
    vlm_message_t *answer;
    CHECK_VLM;
#ifdef ENABLE_VLM
    asprintf( &psz_message, "control %s seek %f", psz_name, f_percentage );
    vlm_ExecuteCommand( p_instance->p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_exception_raise( p_exception, "Unable to seek %s to %f",
                                psz_name, f_percentage );
    }
    free( psz_message );
#endif
}

#ifdef ENABLE_VLM
#define LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( attr, returnType, getType, default)\
returnType libvlc_vlm_get_media_## attr( libvlc_instance_t *p_instance, \
                        char *psz_name, int i_instance, \
                        libvlc_exception_t *p_exception ) \
{ \
    vlm_media_instance_t *p_media_instance; \
    CHECK_VLM; \
    vlm_media_t *p_media; \
    p_media = vlm_MediaSearch( p_instance->p_vlm, psz_name ); \
    if ( p_media == NULL ) \
    { \
        libvlc_exception_raise( p_exception, "Unable to find media %s", \
                                psz_name); \
    } \
    else \
    { \
        if ( i_instance < p_media->i_instance ) \
        { \
            p_media_instance = p_media->instance[ i_instance ]; \
            return var_Get ## getType( p_media_instance->p_input, #attr );\
        } \
        else \
        { \
            libvlc_exception_raise( p_exception, "Media index %i out of range",\
                                    i_instance); \
        } \
    } \
    return default; \
}
#else
#define LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( attr, returnType, getType, default)\
returnType libvlc_vlm_get_media_## attr( libvlc_instance_t *p_instance, \
                        char *psz_name, int i_instance, libvlc_exception_t *p_exception ) \
{ \
    char *psz_message; \
    vlm_message_t *answer; \
    CHECK_VLM; \
    return default; \
}
#endif

LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( position, float, Float, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( time, int, Integer, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( length, int, Integer, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( rate, int, Integer, -1);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( title, int, Integer, 0);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( chapter, int, Integer, 0);
LIBVLC_VLM_GET_MEDIA_ATTRIBUTE( seekable, int, Bool, 0);

#undef LIBVLC_VLM_GET_MEDIA_ATTRIBUTE

/* local function to be used in libvlc_vlm_show_media only */
static char* recurse_answer( char* psz_prefix, vlm_message_t *p_answer ) {
    char* psz_childprefix;
    char* psz_response="";
    char* response_tmp;
    int i;
    vlm_message_t *aw_child, **paw_child;

    asprintf( &psz_childprefix, "%s%s.", psz_prefix, p_answer->psz_name );

    if ( p_answer->i_child )
    {
        paw_child = p_answer->child;
        aw_child = *( paw_child );
        for( i = 0; i < p_answer->i_child; i++ )
        {
            asprintf( &response_tmp, "%s%s%s:%s\n",
                      psz_response, psz_prefix, aw_child->psz_name,
                      aw_child->psz_value );
            free( psz_response );
            psz_response = response_tmp;
            if ( aw_child->i_child )
            {
                asprintf(&response_tmp, "%s%s", psz_response,
                         recurse_answer(psz_childprefix, aw_child));
                free( psz_response );
                psz_response = response_tmp;
            }
            paw_child++;
            aw_child = *( paw_child );
        }
    }
    free( psz_childprefix );
    return psz_response;
}

char* libvlc_vlm_show_media( libvlc_instance_t *p_instance, char *psz_name,
                             libvlc_exception_t *p_exception )
{
    char *psz_message;
    vlm_message_t *answer;
    char *psz_response;

    CHECK_VLM;
#ifdef ENABLE_VLM
    asprintf( &psz_message, "show %s", psz_name );
    asprintf( &psz_response, "", psz_name );
    vlm_ExecuteCommand( p_instance->p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_exception_raise( p_exception, "Unable to call show %s: %s",
                                psz_name, answer->psz_value );
    }
    else
    {
        if ( answer->child )
        {
            psz_response = recurse_answer( "", answer );
        }
    }
    free( psz_message );
    return(psz_response );
#endif
    return NULL;
}

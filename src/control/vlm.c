/*****************************************************************************
 * vlm.c: libvlc new API VLM handling functions
 *****************************************************************************
 * Copyright (C) 2005 the VideoLAN team
 * $Id$
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
#include <vlc_vlm.h>

#if 0
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
}
#else

char* libvlc_vlm_show_media( libvlc_instance_t *p_instance,
                             const char *psz_name,
                             libvlc_exception_t *p_exception )
{
    (void)p_instance;
    /* FIXME is it needed ? */
    libvlc_exception_raise( p_exception, "Unable to call show %s", psz_name );
    return NULL;
}

#endif /* 0 */

static int libvlc_vlm_init( libvlc_instance_t *p_instance,
                            libvlc_exception_t *p_exception )
{
    if( !p_instance->p_vlm )
        p_instance->p_vlm = vlm_New( p_instance->p_libvlc_int );

    if( !p_instance->p_vlm )
    {
        libvlc_exception_raise( p_exception,
                                "Unable to create VLM." );
        return VLC_EGENERIC;
    }
    return VLC_SUCCESS;
}
#define VLM_RET(p,ret) do {                                     \
    if( libvlc_vlm_init( p_instance, p_exception ) ) return ret;\
    (p) = p_instance->p_vlm;                                    \
  } while(0)
#define VLM(p) VLM_RET(p,)

static vlm_media_instance_t *libvlc_vlm_get_media_instance( libvlc_instance_t *p_instance,
                                                            const char *psz_name,
                                                            int i_minstance_idx,
                                                            libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    vlm_media_instance_t **pp_minstance;
    vlm_media_instance_t *p_minstance;
    int i_minstance;
    int64_t id;

    VLM_RET(p_vlm, NULL);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_GET_MEDIA_INSTANCES, id, &pp_minstance, &i_minstance ) )
    {
        libvlc_exception_raise( p_exception, "Unable to get %s instances", psz_name );
        return NULL;
    }
    p_minstance = NULL;
    if( i_minstance_idx >= 0 && i_minstance_idx < i_minstance )
    {
        p_minstance = pp_minstance[i_minstance_idx];
        TAB_REMOVE( i_minstance, pp_minstance, p_minstance );
    }
    while( i_minstance > 0 )
        vlm_media_instance_Delete( pp_minstance[--i_minstance] );
    TAB_CLEAN( i_minstance, pp_minstance );
    return p_minstance;
}


void libvlc_vlm_release( libvlc_instance_t *p_instance, libvlc_exception_t *p_exception)
{
    vlm_t *p_vlm;

    VLM(p_vlm);

    vlm_Delete( p_vlm );
}

void libvlc_vlm_add_broadcast( libvlc_instance_t *p_instance,
                               const char *psz_name,
                               const char *psz_input,
                               const char *psz_output, int i_options,
                               const char * const *ppsz_options,
                               int b_enabled, int b_loop,
                               libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    vlm_media_t m;
    int n;

    VLM(p_vlm);

    vlm_media_Init( &m );
    m.psz_name = strdup( psz_name );
    m.b_enabled = b_enabled;
    m.b_vod = false;
    m.broadcast.b_loop = b_loop;
    if( psz_input )
        TAB_APPEND( m.i_input, m.ppsz_input, strdup(psz_input) );
    if( psz_output )
        m.psz_output = strdup( psz_output );
    for( n = 0; n < i_options; n++ )
        TAB_APPEND( m.i_option, m.ppsz_option, strdup(ppsz_options[n]) );

    n = vlm_Control( p_vlm, VLM_ADD_MEDIA, &m, NULL );
    vlm_media_Clean( &m );
    if( n )
        libvlc_exception_raise( p_exception, "Media %s creation failed", psz_name );
}

void libvlc_vlm_add_vod( libvlc_instance_t *p_instance, const char *psz_name,
                         const char *psz_input, int i_options,
                         const char * const *ppsz_options, int b_enabled,
                         const char *psz_mux, libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    vlm_media_t m;
    int n;

    VLM(p_vlm);

    vlm_media_Init( &m );
    m.psz_name = strdup( psz_name );
    m.b_enabled = b_enabled;
    m.b_vod = true;
    m.vod.psz_mux = psz_mux ? strdup( psz_mux ) : NULL;
    if( psz_input )
        TAB_APPEND( m.i_input, m.ppsz_input, strdup(psz_input) );
    for( n = 0; n < i_options; n++ )
        TAB_APPEND( m.i_option, m.ppsz_option, strdup(ppsz_options[n]) );

    n = vlm_Control( p_vlm, VLM_ADD_MEDIA, &m, NULL );
    vlm_media_Clean( &m );
    if( n )
        libvlc_exception_raise( p_exception, "Media %s creation failed", psz_name );
}

void libvlc_vlm_del_media( libvlc_instance_t *p_instance, const char *psz_name,
                           libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM(p_vlm);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_DEL_MEDIA, id ) )
    {
        libvlc_exception_raise( p_exception, "Unable to delete %s", psz_name );
    }
}

#define VLM_CHANGE(psz_error, code ) do {   \
    vlm_media_t *p_media;   \
    vlm_t *p_vlm;           \
    int64_t id;             \
    VLM(p_vlm);             \
    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||    \
        vlm_Control( p_vlm, VLM_GET_MEDIA, id, &p_media ) ) {       \
        libvlc_exception_raise( p_exception, psz_error, psz_name ); \
        return;             \
    }                       \
    if( !p_media ) goto error;                                      \
                            \
    code;                   \
                            \
    if( vlm_Control( p_vlm, VLM_CHANGE_MEDIA, p_media ) ) {         \
        vlm_media_Delete( p_media );                                \
        goto error;         \
    }                       \
    vlm_media_Delete( p_media );                                    \
    return;                 \
  error:                    \
    libvlc_exception_raise( p_exception, psz_error, psz_name );\
  } while(0)

void libvlc_vlm_set_enabled( libvlc_instance_t *p_instance,
                             const char *psz_name, int b_enabled,
                             libvlc_exception_t *p_exception )
{
#define VLM_CHANGE_CODE { p_media->b_enabled = b_enabled; }
    VLM_CHANGE( "Unable to delete %s", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

void libvlc_vlm_set_loop( libvlc_instance_t *p_instance, const char *psz_name,
                          int b_loop, libvlc_exception_t *p_exception )
{
#define VLM_CHANGE_CODE { p_media->broadcast.b_loop = b_loop; }
    VLM_CHANGE( "Unable to change %s loop property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

void libvlc_vlm_set_mux( libvlc_instance_t *p_instance, const char *psz_name,
                         const char *psz_mux, libvlc_exception_t *p_exception )
{
#define VLM_CHANGE_CODE { if( p_media->b_vod ) { \
                            free( p_media->vod.psz_mux ); \
                            p_media->vod.psz_mux = psz_mux ? strdup( psz_mux ) : NULL; \
                          } }
    VLM_CHANGE( "Unable to change %s mux property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

void libvlc_vlm_set_output( libvlc_instance_t *p_instance,
                            const char *psz_name, const char *psz_output,
                            libvlc_exception_t *p_exception )
{
#define VLM_CHANGE_CODE { free( p_media->psz_output ); \
                          p_media->psz_output = strdup( psz_output ); }
    VLM_CHANGE( "Unable to change %s output property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

void libvlc_vlm_set_input( libvlc_instance_t *p_instance,
                           const char *psz_name, const char *psz_input,
                           libvlc_exception_t *p_exception )
{
#define VLM_CHANGE_CODE { while( p_media->i_input > 0 ) \
                            free( p_media->ppsz_input[--p_media->i_input] );\
                          TAB_APPEND( p_media->i_input, p_media->ppsz_input, strdup(psz_input) ); }
    VLM_CHANGE( "Unable to change %s input property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

void libvlc_vlm_add_input( libvlc_instance_t *p_instance,
                           const char *psz_name, const char *psz_input,
                           libvlc_exception_t *p_exception )
{
#define VLM_CHANGE_CODE { TAB_APPEND( p_media->i_input, p_media->ppsz_input, strdup(psz_input) ); }
    VLM_CHANGE( "Unable to change %s input property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

void libvlc_vlm_change_media( libvlc_instance_t *p_instance,
                              const char *psz_name, const char *psz_input,
                              const char *psz_output, int i_options,
                              const char * const *ppsz_options, int b_enabled,
                              int b_loop, libvlc_exception_t *p_exception )
{
#define VLM_CHANGE_CODE { int n;        \
    p_media->b_enabled = b_enabled;     \
    p_media->broadcast.b_loop = b_loop; \
    while( p_media->i_input > 0 )       \
        free( p_media->ppsz_input[--p_media->i_input] );    \
    if( psz_input )                     \
        TAB_APPEND( p_media->i_input, p_media->ppsz_input, strdup(psz_input) ); \
    free( p_media->psz_output );        \
    p_media->psz_output = psz_output ? strdup( psz_output ) : NULL; \
    while( p_media->i_option > 0 )     \
        free( p_media->ppsz_option[--p_media->i_option] );        \
    for( n = 0; n < i_options; n++ )    \
        TAB_APPEND( p_media->i_option, p_media->ppsz_option, strdup(ppsz_options[n]) );   \
  }
    VLM_CHANGE( "Unable to change %s properties", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

void libvlc_vlm_play_media( libvlc_instance_t *p_instance,
                            const char *psz_name,
                            libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM(p_vlm);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_START_MEDIA_BROADCAST_INSTANCE, id, NULL, 0 ) )
    {
        libvlc_exception_raise( p_exception, "Unable to play %s", psz_name );
    }
}

void libvlc_vlm_stop_media( libvlc_instance_t *p_instance,
                            const char *psz_name,
                            libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM(p_vlm);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_STOP_MEDIA_INSTANCE, id, NULL ) )
    {
        libvlc_exception_raise( p_exception, "Unable to stop %s", psz_name );
    }
}

void libvlc_vlm_pause_media( libvlc_instance_t *p_instance,
                             const char *psz_name,
                             libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM(p_vlm);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_PAUSE_MEDIA_INSTANCE, id, NULL ) )
    {
        libvlc_exception_raise( p_exception, "Unable to pause %s", psz_name );
    }
}

void libvlc_vlm_seek_media( libvlc_instance_t *p_instance,
                            const char *psz_name, float f_percentage,
                            libvlc_exception_t *p_exception )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM(p_vlm);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_SET_MEDIA_INSTANCE_POSITION, id, NULL, f_percentage ) )
    {
        libvlc_exception_raise( p_exception, "Unable to seek %s to %f", psz_name, f_percentage );
    }
}

float libvlc_vlm_get_media_instance_position( libvlc_instance_t *p_instance,
                                              const char *psz_name,
                                              int i_instance,
                                              libvlc_exception_t *p_exception )
{
    float result = -1;
    vlm_media_instance_t *p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                        i_instance, p_exception );
    if( p_mi )
    {
        result = p_mi->d_position;
        vlm_media_instance_Delete( p_mi );
        return result;
    }
    libvlc_exception_raise( p_exception, "Unable to get position attribute" );
    return result;
}

int libvlc_vlm_get_media_instance_time( libvlc_instance_t *p_instance,
                                        const char *psz_name, int i_instance,
                                        libvlc_exception_t *p_exception )
{
    int result = -1;
    vlm_media_instance_t *p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                        i_instance, p_exception );
    if( p_mi )
    {
        result = p_mi->i_time;
        vlm_media_instance_Delete( p_mi );
        return result;
    }
    libvlc_exception_raise( p_exception, "Unable to get time attribute" );
    return result;
}

int libvlc_vlm_get_media_instance_length( libvlc_instance_t *p_instance,
                                          const char *psz_name,
                                          int i_instance,
                                          libvlc_exception_t *p_exception )
{
    int result = -1;
    vlm_media_instance_t *p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                        i_instance, p_exception );
    if( p_mi )
    {
        result = p_mi->i_length;
        vlm_media_instance_Delete( p_mi );
        return result;
    }
    libvlc_exception_raise( p_exception, "Unable to get length attribute" );
    return result;
}

int libvlc_vlm_get_media_instance_rate( libvlc_instance_t *p_instance,
                                        const char *psz_name, int i_instance,
                                        libvlc_exception_t *p_exception )
{
    int result = -1;
    vlm_media_instance_t *p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                        i_instance, p_exception );
    if( p_mi )
    {
        result = p_mi->i_rate;
        vlm_media_instance_Delete( p_mi );
        return result;
    }
    libvlc_exception_raise( p_exception, "Unable to get rate attribute" );
    return result;
}

int libvlc_vlm_get_media_instance_title( libvlc_instance_t *p_instance,
                                         const char *psz_name, int i_instance,
                                         libvlc_exception_t *p_exception )
{
    int result = 0;
    vlm_media_instance_t *p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                        i_instance, p_exception );
    if( p_mi )
    {
        vlm_media_instance_Delete( p_mi );
        return result;
    }
    libvlc_exception_raise( p_exception, "Unable to get title attribute" );
    return result;
}

int libvlc_vlm_get_media_instance_chapter( libvlc_instance_t *p_instance,
                                           const char *psz_name,
                                           int i_instance,
                                           libvlc_exception_t *p_exception )
{
    int result = 0;
    vlm_media_instance_t *p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                        i_instance, p_exception );
    if( p_mi )
    {
        vlm_media_instance_Delete( p_mi );
        return result;
    }
    libvlc_exception_raise( p_exception, "Unable to get chapter attribute" );
    return result;
}

int libvlc_vlm_get_media_instance_seekable( libvlc_instance_t *p_instance,
                                            const char *psz_name,
                                            int i_instance,
                                            libvlc_exception_t *p_exception )
{
    bool result = 0;
    vlm_media_instance_t *p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                        i_instance, p_exception );
    if( p_mi )
    {
        vlm_media_instance_Delete( p_mi );
        return result;
    }
    libvlc_exception_raise( p_exception, "Unable to get seekable attribute" );
    return result;
}

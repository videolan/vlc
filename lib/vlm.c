/*****************************************************************************
 * vlm.c: libvlc new API VLM handling functions
 *****************************************************************************
 * Copyright (C) 2005 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <vlc/libvlc.h>
#include <vlc/libvlc_vlm.h>
#include <vlc_es.h>
#include <vlc_input.h>
#include <vlc_vlm.h>
#include <assert.h>

#include "libvlc_internal.h"

/* VLM events callback. Transmit to libvlc */
static int VlmEvent( vlc_object_t *p_this, const char * name,
                     vlc_value_t old_val, vlc_value_t newval, void *param )
{
    VLC_UNUSED(p_this);
    VLC_UNUSED(name);
    VLC_UNUSED(old_val);    
    vlm_event_t *event = (vlm_event_t*)newval.p_address;
    libvlc_event_manager_t *p_event_manager = (libvlc_event_manager_t *) param;
    libvlc_event_t libvlc_event;

    libvlc_event.u.vlm_media_event.psz_instance_name = NULL;
    libvlc_event.u.vlm_media_event.psz_media_name = event->psz_name;

    switch( event->i_type )
    {
    case VLM_EVENT_MEDIA_ADDED:
        libvlc_event.type = libvlc_VlmMediaAdded;
        break;
    case VLM_EVENT_MEDIA_REMOVED:
        libvlc_event.type = libvlc_VlmMediaRemoved;
        break;
    case VLM_EVENT_MEDIA_CHANGED:
        libvlc_event.type = libvlc_VlmMediaChanged;
        break;
    case VLM_EVENT_MEDIA_INSTANCE_STARTED:
        libvlc_event.type = libvlc_VlmMediaInstanceStarted;
        break;
    case VLM_EVENT_MEDIA_INSTANCE_STOPPED:
        libvlc_event.type = libvlc_VlmMediaInstanceStopped;
        break;
    case VLM_EVENT_MEDIA_INSTANCE_STATE:
        libvlc_event.u.vlm_media_event.psz_instance_name =
            event->psz_instance_name;
        switch( event->input_state )
        {
        case INIT_S:
            libvlc_event.type = libvlc_VlmMediaInstanceStatusInit;
            break;
        case OPENING_S:
            libvlc_event.type =
                libvlc_VlmMediaInstanceStatusOpening;
            break;
        case PLAYING_S:
            libvlc_event.type =
                libvlc_VlmMediaInstanceStatusPlaying;
            break;
        case PAUSE_S:
            libvlc_event.type = libvlc_VlmMediaInstanceStatusPause;
            break;
        case END_S:
            libvlc_event.type = libvlc_VlmMediaInstanceStatusEnd;
            break;
        case ERROR_S:
            libvlc_event.type = libvlc_VlmMediaInstanceStatusError;
            break;
        default:
            return 0;
        }
        break;
    default:
        return 0;
    }
    libvlc_event_send( p_event_manager, &libvlc_event );
    return 0;
}

static void libvlc_vlm_release_internal( libvlc_instance_t *p_instance )
{
    vlm_t *p_vlm = p_instance->libvlc_vlm.p_vlm;
    if( !p_instance->libvlc_vlm.p_vlm )
        return;
    /* We need to remove medias in order to receive events */
    vlm_Control( p_vlm, VLM_CLEAR_MEDIAS );
    vlm_Control( p_vlm, VLM_CLEAR_SCHEDULES );

    var_DelCallback( (vlc_object_t *)p_vlm, "intf-event", VlmEvent,
                     p_instance->libvlc_vlm.p_event_manager );
    p_instance->libvlc_vlm.pf_release = NULL;
    libvlc_event_manager_release( p_instance->libvlc_vlm.p_event_manager );
    p_instance->libvlc_vlm.p_event_manager = NULL;
    vlm_Delete( p_vlm );
    p_instance->libvlc_vlm.p_vlm = NULL;
}

static int libvlc_vlm_init( libvlc_instance_t *p_instance )
{
    if( !p_instance->libvlc_vlm.p_event_manager )
    {
        p_instance->libvlc_vlm.p_event_manager =
            libvlc_event_manager_new( p_instance->libvlc_vlm.p_vlm, p_instance );
        if( unlikely(p_instance->libvlc_vlm.p_event_manager == NULL) )
            return VLC_ENOMEM;
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaAdded );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaRemoved );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaChanged );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStarted );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStopped );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStatusInit );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStatusOpening );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStatusPlaying );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStatusPause );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStatusEnd );
        libvlc_event_manager_register_event_type(
            p_instance->libvlc_vlm.p_event_manager,
            libvlc_VlmMediaInstanceStatusError );
    }

    if( !p_instance->libvlc_vlm.p_vlm )
    {
        p_instance->libvlc_vlm.p_vlm = vlm_New( p_instance->p_libvlc_int );
        if( !p_instance->libvlc_vlm.p_vlm )
        {
            libvlc_printerr( "VLM not supported or out of memory" );
            return VLC_EGENERIC;
        }
        var_AddCallback( (vlc_object_t *)p_instance->libvlc_vlm.p_vlm,
                         "intf-event", VlmEvent,
                         p_instance->libvlc_vlm.p_event_manager );
        p_instance->libvlc_vlm.pf_release = libvlc_vlm_release_internal;
    }

    return VLC_SUCCESS;
}

void libvlc_vlm_release( libvlc_instance_t *p_instance )
{
    libvlc_vlm_release_internal( p_instance );
}

#define VLM_RET(p,ret) do { \
    if( libvlc_vlm_init( p_instance ) ) \
        return (ret); \
    (p) = p_instance->libvlc_vlm.p_vlm; \
  } while(0)

static vlm_media_instance_t *
libvlc_vlm_get_media_instance( libvlc_instance_t *p_instance,
                               const char *psz_name, int i_minstance_idx )
{
    vlm_t *p_vlm;
    vlm_media_instance_t **pp_minstance;
    vlm_media_instance_t *p_minstance;
    int i_minstance;
    int64_t id;

    VLM_RET(p_vlm, NULL);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_GET_MEDIA_INSTANCES, id, &pp_minstance,
                     &i_minstance ) )
    {
        libvlc_printerr( "%s: media instances not found", psz_name );
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

/* local function to be used in libvlc_vlm_show_media only */
static char* recurse_answer( vlm_message_t *p_answer, const char* psz_delim,
                             const int i_list ) {
    char* psz_childdelim = NULL;
    char* psz_nametag = NULL;
    char* psz_response = strdup( "" );
    char *psz_tmp;
    int i_success = 0;
    int i;
    vlm_message_t *aw_child, **paw_child;

    i_success = asprintf( &psz_childdelim, "%s\t", psz_delim);
    if( i_success == -1 )
        return psz_response;

    paw_child = p_answer->child;
    aw_child = *( paw_child );
    /* Iterate over children */
    for( i = 0; i < p_answer->i_child; i++ )
    {
        /* Spare comma if it is the last element */
        char c_comma = ',';
        if( i == (p_answer->i_child - 1) )
            c_comma = ' ';

        /* Append name of child node, if not in a list */
        if( !i_list )
        {
            i_success = asprintf( &psz_tmp, "%s\"%s\": ",
                          psz_response, aw_child->psz_name );
            if( i_success == -1 ) break;
            free( psz_response );
            psz_response = psz_tmp;
        }

        /* If child node has children, */
        if( aw_child->i_child )
        {
            /* If the parent node is a list (hence the child node is
             * inside a list), create a property of its name as if it
             * had a name value node
             */
            free( psz_nametag );
            if( i_list )
            {
                i_success = asprintf( &psz_nametag, "\"name\": \"%s\",%s",
                              aw_child->psz_name, psz_childdelim );
                if( i_success == -1 )
                {
                    psz_nametag = NULL;
                    break;
                }
            }
            else
            {
                psz_nametag = strdup( "" );
            }
            /* If the child is a list itself, format it accordingly and
             * recurse through the child's children, telling them that
             * they are inside a list.
             */
            if( strcmp( aw_child->psz_name, "media" ) == 0 ||
                strcmp( aw_child->psz_name, "inputs" ) == 0 ||
                strcmp( aw_child->psz_name, "options" ) == 0 )
            {
                char *psz_recurse = recurse_answer( aw_child, psz_childdelim, 1 ),
                i_success = asprintf( &psz_tmp, "%s[%s%s%s]%c%s",
                                      psz_response, psz_childdelim, psz_recurse,
                                      psz_delim, c_comma, psz_delim );
                free( psz_recurse );
                if( i_success == -1 ) break;
                free( psz_response );
                psz_response = psz_tmp;
            }
            /* Not a list, so format the child as a JSON object and
             * recurse through the child's children
             */
            else
            {
                char *psz_recurse = recurse_answer( aw_child, psz_childdelim, 0 ),
                i_success = asprintf( &psz_tmp, "%s{%s%s%s%s}%c%s",
                                      psz_response, psz_childdelim, psz_nametag,
                                      psz_recurse, psz_delim, c_comma, psz_delim );
                free( psz_recurse );
                if( i_success == -1 ) break;
                free( psz_response );
                psz_response = psz_tmp;
            }
        }
        /* Otherwise - when no children are present - the node is a
         * value node. So print the value string
         */
        else
        {
            /* If value is equivalent to NULL, print it as null */
            if( aw_child->psz_value == NULL
                || strcmp( aw_child->psz_value, "(null)" ) == 0 )
            {
                i_success = asprintf( &psz_tmp, "%snull%c%s",
                                      psz_response, c_comma, psz_delim );
                if( i_success == -1 ) break;
                free( psz_response );
                psz_response = psz_tmp;
            }
            /* Otherwise print the value in quotation marks */
            else
            {
                i_success = asprintf( &psz_tmp, "%s\"%s\"%c%s",
                                      psz_response, aw_child->psz_value,
                                      c_comma, psz_delim );
                if( i_success == -1 ) break;
                free( psz_response );
                psz_response = psz_tmp;
            }
        }
        /* getting next child */
        paw_child++;
        aw_child = *( paw_child );
    }
    free( psz_nametag );
    free( psz_childdelim );
    if( i_success == -1 )
    {
        free( psz_response );
        psz_response = strdup( "" );
    }
    return psz_response;
}

const char* libvlc_vlm_show_media( libvlc_instance_t *p_instance,
                                   const char *psz_name )
{
    char *psz_message = NULL;
    vlm_message_t *answer = NULL;
    char *psz_response = NULL;
    const char *psz_fmt = NULL;
    const char *psz_delimiter = NULL;
    int i_list;
    vlm_t *p_vlm = NULL;

    VLM_RET(p_vlm, NULL);

    assert( psz_name );

    if( asprintf( &psz_message, "show %s", psz_name ) == -1 )
        return NULL;

    vlm_ExecuteCommand( p_vlm, psz_message, &answer );
    if( answer->psz_value )
    {
        libvlc_printerr( "Unable to call show %s: %s",
                         psz_name, answer->psz_value );
    }
    else if ( answer->child )
    {   /* in case everything was requested  */
        if ( strcmp( psz_name, "" ) == 0 )
        {
            psz_fmt = "{\n\t%s\n}\n";
            psz_delimiter = "\n\t";
            i_list = 0;
        }
        else
        {
            psz_fmt = "%s\n";
            psz_delimiter = "\n";
            i_list = 1;
        }
        char *psz_tmp = recurse_answer( answer, psz_delimiter, i_list );
        if( asprintf( &psz_response, psz_fmt, psz_tmp ) == -1 )
        {
            libvlc_printerr( "Out of memory" );
            psz_response = NULL;
        }
        free( psz_tmp );
    }
    vlm_MessageDelete( answer );
    free( psz_message );
    return( psz_response );
}


int libvlc_vlm_add_broadcast( libvlc_instance_t *p_instance,
                              const char *psz_name,
                              const char *psz_input,
                              const char *psz_output, int i_options,
                              const char * const *ppsz_options,
                              int b_enabled, int b_loop )
{
    vlm_t *p_vlm;
    vlm_media_t m;
    int n;

    VLM_RET(p_vlm, -1);

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
    {
        libvlc_printerr( "Media %s creation failed", psz_name );
        return -1;
    }
    return 0;
}

int libvlc_vlm_add_vod( libvlc_instance_t *p_instance, const char *psz_name,
                        const char *psz_input, int i_options,
                        const char * const *ppsz_options, int b_enabled,
                        const char *psz_mux )
{
    vlm_t *p_vlm;
    vlm_media_t m;
    int n;

    VLM_RET(p_vlm, -1);

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
    {
        libvlc_printerr( "Media %s creation failed", psz_name );
        return -1;
    }
    return 0;
}

int libvlc_vlm_del_media( libvlc_instance_t *p_instance, const char *psz_name )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM_RET(p_vlm, -1);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_DEL_MEDIA, id ) )
    {
        libvlc_printerr( "Unable to delete %s", psz_name );
        return -1;
    }
    return 0;
}

static vlm_media_t *get_media( libvlc_instance_t *p_instance,
                               vlm_t **restrict pp_vlm, const char *name )
{
    vlm_media_t *p_media;
    vlm_t *p_vlm;
    int64_t id;

    VLM_RET(p_vlm, NULL);
    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, name, &id ) ||
        vlm_Control( p_vlm, VLM_GET_MEDIA, id, &p_media ) )
        return NULL;
    *pp_vlm = p_vlm;
    return p_media;
}

#define VLM_CHANGE(psz_error, code ) do {   \
    vlm_t *p_vlm;           \
    vlm_media_t *p_media = get_media( p_instance, &p_vlm, psz_name ); \
    if( p_media != NULL ) { \
        code;               \
        if( vlm_Control( p_vlm, VLM_CHANGE_MEDIA, p_media ) )       \
            p_vlm = NULL;                                           \
        vlm_media_Delete( p_media );                                \
        if( p_vlm != NULL ) \
            return 0;       \
    }                       \
    libvlc_printerr( psz_error, psz_name );                         \
    return -1;              \
  } while(0)

int libvlc_vlm_set_enabled( libvlc_instance_t *p_instance,
                            const char *psz_name, int b_enabled )
{
#define VLM_CHANGE_CODE { p_media->b_enabled = b_enabled; }
    VLM_CHANGE( "Unable to delete %s", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

int libvlc_vlm_set_loop( libvlc_instance_t *p_instance, const char *psz_name,
                         int b_loop )
{
#define VLM_CHANGE_CODE { p_media->broadcast.b_loop = b_loop; }
    VLM_CHANGE( "Unable to change %s loop property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

int libvlc_vlm_set_mux( libvlc_instance_t *p_instance, const char *psz_name,
                        const char *psz_mux )
{
#define VLM_CHANGE_CODE { if( p_media->b_vod ) { \
                            free( p_media->vod.psz_mux ); \
                            p_media->vod.psz_mux = psz_mux \
                                 ? strdup( psz_mux ) : NULL; \
                          } }
    VLM_CHANGE( "Unable to change %s mux property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

int libvlc_vlm_set_output( libvlc_instance_t *p_instance,
                           const char *psz_name, const char *psz_output )
{
#define VLM_CHANGE_CODE { free( p_media->psz_output ); \
                          p_media->psz_output = strdup( psz_output ); }
    VLM_CHANGE( "Unable to change %s output property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

int libvlc_vlm_set_input( libvlc_instance_t *p_instance,
                          const char *psz_name, const char *psz_input )
{
#define VLM_CHANGE_CODE { while( p_media->i_input > 0 ) \
                            free( p_media->ppsz_input[--p_media->i_input] );\
                          TAB_APPEND( p_media->i_input, p_media->ppsz_input, \
                                      strdup(psz_input) ); }
    VLM_CHANGE( "Unable to change %s input property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

int libvlc_vlm_add_input( libvlc_instance_t *p_instance,
                          const char *psz_name, const char *psz_input )
{
#define VLM_CHANGE_CODE { TAB_APPEND( p_media->i_input, p_media->ppsz_input, \
                          strdup(psz_input) ); }
    VLM_CHANGE( "Unable to change %s input property", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

int libvlc_vlm_change_media( libvlc_instance_t *p_instance,
                             const char *psz_name, const char *psz_input,
                             const char *psz_output, int i_options,
                             const char * const *ppsz_options, int b_enabled,
                             int b_loop )
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
        TAB_APPEND( p_media->i_option, p_media->ppsz_option, \
                    strdup(ppsz_options[n]) );   \
  }
    VLM_CHANGE( "Unable to change %s properties", VLM_CHANGE_CODE );
#undef VLM_CHANGE_CODE
}

int libvlc_vlm_play_media( libvlc_instance_t *p_instance,
                           const char *psz_name )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM_RET(p_vlm, -1);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_START_MEDIA_BROADCAST_INSTANCE, id, NULL, 0 ) )
    {
        libvlc_printerr( "Unable to play %s", psz_name );
        return -1;
    }
    return 0;
}

int libvlc_vlm_stop_media( libvlc_instance_t *p_instance,
                           const char *psz_name )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM_RET(p_vlm, -1);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_STOP_MEDIA_INSTANCE, id, NULL ) )
    {
        libvlc_printerr( "Unable to stop %s", psz_name );
        return -1;
    }
    return 0;
}

int libvlc_vlm_pause_media( libvlc_instance_t *p_instance,
                            const char *psz_name )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM_RET(p_vlm, -1);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_PAUSE_MEDIA_INSTANCE, id, NULL ) )
    {
        libvlc_printerr( "Unable to pause %s", psz_name );
        return -1;
    }
    return 0;
}

int libvlc_vlm_seek_media( libvlc_instance_t *p_instance,
                           const char *psz_name, float f_percentage )
{
    vlm_t *p_vlm;
    int64_t id;

    VLM_RET(p_vlm, -1);

    if( vlm_Control( p_vlm, VLM_GET_MEDIA_ID, psz_name, &id ) ||
        vlm_Control( p_vlm, VLM_SET_MEDIA_INSTANCE_POSITION, id, NULL,
                     f_percentage ) )
    {
        libvlc_printerr( "Unable to seek %s to %f%%", psz_name, f_percentage );
        return -1;
    }
    return 0;
}

float libvlc_vlm_get_media_instance_position( libvlc_instance_t *p_instance,
                                              const char *psz_name,
                                              int i_instance )
{
    vlm_media_instance_t *p_mi;
    float result = -1.;

    p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name, i_instance );
    if( p_mi )
    {
        result = p_mi->d_position;
        vlm_media_instance_Delete( p_mi );
    }
    return result;
}

int libvlc_vlm_get_media_instance_time( libvlc_instance_t *p_instance,
                                        const char *psz_name, int i_instance )
{
    vlm_media_instance_t *p_mi;
    int result = -1;

    p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name, i_instance );
    if( p_mi )
    {
        result = p_mi->i_time;
        vlm_media_instance_Delete( p_mi );
    }
    return result;
}

int libvlc_vlm_get_media_instance_length( libvlc_instance_t *p_instance,
                                          const char *psz_name,
                                          int i_instance )
{
    vlm_media_instance_t *p_mi;
    int result = -1;

    p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name, i_instance );
    if( p_mi )
    {
        result = p_mi->i_length;
        vlm_media_instance_Delete( p_mi );
    }
    return result;
}

int libvlc_vlm_get_media_instance_rate( libvlc_instance_t *p_instance,
                                        const char *psz_name, int i_instance )
{
    vlm_media_instance_t *p_mi;
    int result = -1;

    p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name, i_instance );
    if( p_mi )
    {
        result = p_mi->i_rate;
        vlm_media_instance_Delete( p_mi );
    }
    return result;
}

#if 0
int libvlc_vlm_get_media_instance_title( libvlc_instance_t *p_instance,
                                         const char *psz_name, int i_instance )
{
    vlm_media_instance_t *p_mi;

    p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name, i_instance );
    if( p_mi )
        vlm_media_instance_Delete( p_mi );
    return p_mi ? 0 : -1;
}

int libvlc_vlm_get_media_instance_chapter( libvlc_instance_t *p_instance,
                                           const char *psz_name,
                                           int i_instance )
{
    vlm_media_instance_t *p_mi;

    p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name,
                                          i_instance );
    if( p_mi )
        vlm_media_instance_Delete( p_mi );
    return p_mi ? 0 : -1;
}

int libvlc_vlm_get_media_instance_seekable( libvlc_instance_t *p_instance,
                                            const char *psz_name,
                                            int i_instance )
{
    vlm_media_instance_t *p_mi;

    p_mi = libvlc_vlm_get_media_instance( p_instance, psz_name, i_instance );
    if( p_mi )
        vlm_media_instance_Delete( p_mi );
    return p_mi ? 0 : -1;
}
#endif

libvlc_event_manager_t *
libvlc_vlm_get_event_manager( libvlc_instance_t *p_instance )
{
    vlm_t *p_vlm;
    VLM_RET( p_vlm, NULL);
    return p_instance->libvlc_vlm.p_event_manager;
}

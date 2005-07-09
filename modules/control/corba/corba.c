/*****************************************************************************
 * corba.c : CORBA (ORBit) remote control plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 the VideoLAN team
 * $Id$
 *
 * Authors: Olivier Aubert <oaubert@lisi.univ-lyon1.fr>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
/* For CORBA */
#include "MediaControl.h"
#include "orbit/poa/portableserver-poa-type.h"
#include "mediacontrol-core.h"

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>

#include <errno.h>
#include <unistd.h>

/* FIXME: replace this to ~/.vlc/vlc-ior.ref thanks to
   config_GetHomeDir( ) */
#ifndef __WIN32__
#define VLC_IOR_FILE "/tmp/vlc-ior.ref"
#else
#define VLC_IOR_FILE "vlc-ior-ref"
#endif

#define MC_TRY exception = mediacontrol_exception_init( exception )
#define MC_EXCEPT( return_value )  \
  if ( exception->code )\
  { \
      corba_raise( ev, exception ); \
      mediacontrol_exception_free( exception ); \
      return return_value; \
  } else { mediacontrol_exception_free( exception ); }

#define handle_exception( m ) if( ev->_major != CORBA_NO_EXCEPTION ) \
    { \
        msg_Err( servant->p_intf, m ); \
        return; \
    }

#define handle_exception_no_servant( p,m ) if( ev->_major != CORBA_NO_EXCEPTION ) \
    { \
        msg_Err( p, m ); \
        return; \
    }

static void corba_raise( CORBA_Environment *ev, mediacontrol_Exception *exception )
{
    char *corba_exception=NULL;
    char* i_type = NULL;
 
    switch( exception->code )
    {
    case mediacontrol_InternalException:
        corba_exception = ( char* )VLC_InternalException__alloc();
        i_type = ex_VLC_InternalException;
        break;
    case mediacontrol_PlaylistException:
        corba_exception = ( char* )VLC_PlaylistException__alloc();
        i_type = ex_VLC_PlaylistException;
        break;
    case mediacontrol_InvalidPosition:
        corba_exception = ( char* )VLC_InvalidPosition__alloc();
        i_type = ex_VLC_InvalidPosition;
        break;
    case mediacontrol_PositionKeyNotSupported:
        corba_exception = ( char* )VLC_PositionKeyNotSupported__alloc();
        i_type = ex_VLC_PositionKeyNotSupported;
        break;
    case mediacontrol_PositionOriginNotSupported:
        corba_exception = ( char* )VLC_PositionOriginNotSupported__alloc();
        i_type = ex_VLC_PositionOriginNotSupported;
        break;
    }
    ( (VLC_InternalException* )corba_exception )->message = CORBA_string_dup( exception->message );
    CORBA_exception_set( ev, CORBA_USER_EXCEPTION, i_type, corba_exception );
    return;
}

static mediacontrol_Position* corba_position_corba_to_c( const VLC_Position* position )
{
    mediacontrol_Position* retval;
    
    retval = ( mediacontrol_Position* )malloc( sizeof( mediacontrol_Position ) );
    if( ! retval )
        return NULL;
    retval->origin = position->origin;
    retval->key    = position->key;
    retval->value  = position->value;
    return retval;
}

static VLC_Position* corba_position_c_to_corba( const mediacontrol_Position* position )
{
    VLC_Position* retval;

    retval = ( VLC_Position* )malloc( sizeof( VLC_Position ) );
    if( ! retval )
        return NULL;
    retval->origin = position->origin;
    retval->key    = position->key;
    retval->value  = position->value;
    return retval;
}

/*****************************************************************************
 * intf_sys_t: description and status of corba interface
 *****************************************************************************/
struct intf_sys_t
{
    CORBA_ORB                 orb;
    GMainLoop*                corbaloop;
    mediacontrol_Instance     *mc;
    msg_subscription_t* p_sub;  /* message bank subscription */
};

/*** App-specific servant structures ***/

/* We can add attributes to this structure, which is both a pointer on a
   specific structure, and on a POA_VLC_MediaControl ( servant ). Cf
   http://developer.gnome.org/doc/guides/corba/html/corba-poa-example.html */

typedef struct
{
    POA_VLC_MediaControl servant;
    PortableServer_POA poa;
    /* Ajouter ici les attributs utiles */
    mediacontrol_Instance     *mc;
    intf_thread_t             *p_intf;
} impl_POA_VLC_MediaControl;

/* Beginning of the CORBA code generated in Mediacontrol-skelimpl.c */
/* BEGIN INSERT */
/*** Implementation stub prototypes ***/

static void impl_VLC_MediaControl__destroy( impl_POA_VLC_MediaControl *
                                            servant, CORBA_Environment * ev );

static VLC_Position
impl_VLC_MediaControl_get_media_position( impl_POA_VLC_MediaControl * servant,
                                          const VLC_PositionOrigin an_origin,
                                          const VLC_PositionKey a_key,
                                          CORBA_Environment * ev );

static void
impl_VLC_MediaControl_set_media_position( impl_POA_VLC_MediaControl * servant,
                                          const VLC_Position * a_position,
                                          CORBA_Environment * ev );

static void
impl_VLC_MediaControl_start( impl_POA_VLC_MediaControl * servant,
                             const VLC_Position * a_position,
                             CORBA_Environment * ev );

static void
impl_VLC_MediaControl_pause( impl_POA_VLC_MediaControl * servant,
                             const VLC_Position * a_position,
                             CORBA_Environment * ev );

static void
impl_VLC_MediaControl_resume( impl_POA_VLC_MediaControl * servant,
                              const VLC_Position * a_position,
                              CORBA_Environment * ev );

static void
impl_VLC_MediaControl_stop( impl_POA_VLC_MediaControl * servant,
                            const VLC_Position * a_position,
                            CORBA_Environment * ev );

static void
impl_VLC_MediaControl_exit( impl_POA_VLC_MediaControl * servant,
                            CORBA_Environment * ev );

static void
impl_VLC_MediaControl_playlist_add_item( impl_POA_VLC_MediaControl * servant,
                                         const CORBA_char * a_file,
                                         CORBA_Environment * ev );

static void
impl_VLC_MediaControl_playlist_clear( impl_POA_VLC_MediaControl * servant,
                                      CORBA_Environment * ev );

static VLC_PlaylistSeq
*impl_VLC_MediaControl_playlist_get_list( impl_POA_VLC_MediaControl *
                                          servant, CORBA_Environment * ev );

static VLC_RGBPicture
*impl_VLC_MediaControl_snapshot( impl_POA_VLC_MediaControl * servant,
                                 const VLC_Position * a_position,
                                 CORBA_Environment * ev );

static VLC_RGBPictureSeq
*impl_VLC_MediaControl_all_snapshots( impl_POA_VLC_MediaControl * servant,
                                      CORBA_Environment * ev );

static void
impl_VLC_MediaControl_display_text( impl_POA_VLC_MediaControl * servant,
                                    const CORBA_char * message,
                                    const VLC_Position * begin,
                                    const VLC_Position * end,
                                    CORBA_Environment * ev );

static VLC_StreamInformation
*impl_VLC_MediaControl_get_stream_information( impl_POA_VLC_MediaControl *
                                               servant,
                                               CORBA_Environment * ev );

static CORBA_unsigned_short
impl_VLC_MediaControl_sound_get_volume( impl_POA_VLC_MediaControl * servant,
                                        CORBA_Environment * ev );

static void
impl_VLC_MediaControl_sound_set_volume( impl_POA_VLC_MediaControl * servant,
                                        const CORBA_unsigned_short volume,
                                        CORBA_Environment * ev );

/*** epv structures ***/

static PortableServer_ServantBase__epv impl_VLC_MediaControl_base_epv = {
    NULL,                       /* _private data */
    ( gpointer ) & impl_VLC_MediaControl__destroy,      /* finalize routine */
    NULL,                       /* default_POA routine */
};
static POA_VLC_MediaControl__epv impl_VLC_MediaControl_epv = {
    NULL,                       /* _private */

    ( gpointer ) & impl_VLC_MediaControl_get_media_position,

    ( gpointer ) & impl_VLC_MediaControl_set_media_position,

    ( gpointer ) & impl_VLC_MediaControl_start,

    ( gpointer ) & impl_VLC_MediaControl_pause,

    ( gpointer ) & impl_VLC_MediaControl_resume,

    ( gpointer ) & impl_VLC_MediaControl_stop,

    ( gpointer ) & impl_VLC_MediaControl_exit,

    ( gpointer ) & impl_VLC_MediaControl_playlist_add_item,

    ( gpointer ) & impl_VLC_MediaControl_playlist_clear,

    ( gpointer ) & impl_VLC_MediaControl_playlist_get_list,

    ( gpointer ) & impl_VLC_MediaControl_snapshot,

    ( gpointer ) & impl_VLC_MediaControl_all_snapshots,

    ( gpointer ) & impl_VLC_MediaControl_display_text,

    ( gpointer ) & impl_VLC_MediaControl_get_stream_information,

    ( gpointer ) & impl_VLC_MediaControl_sound_get_volume,

    ( gpointer ) & impl_VLC_MediaControl_sound_set_volume,

};

/*** vepv structures ***/

static POA_VLC_MediaControl__vepv impl_VLC_MediaControl_vepv = {
    &impl_VLC_MediaControl_base_epv,
    &impl_VLC_MediaControl_epv,
};

/*** Stub implementations ***/

static VLC_MediaControl
impl_VLC_MediaControl__create( PortableServer_POA poa, CORBA_Environment * ev )
{
    VLC_MediaControl retval;
    impl_POA_VLC_MediaControl *newservant;
    PortableServer_ObjectId *objid;

    newservant = g_new0( impl_POA_VLC_MediaControl, 1 );
    newservant->servant.vepv = &impl_VLC_MediaControl_vepv;
    newservant->poa =
        ( PortableServer_POA ) CORBA_Object_duplicate( (CORBA_Object ) poa, ev );
    POA_VLC_MediaControl__init( (PortableServer_Servant ) newservant, ev );
    /* Before servant is going to be activated all
     * private attributes must be initialized.  */

    /* ------ init private attributes here ------ */
    newservant->mc = NULL;
    /* ------ ---------- end ------------- ------ */

    objid = PortableServer_POA_activate_object( poa, newservant, ev );
    CORBA_free( objid );
    retval = PortableServer_POA_servant_to_reference( poa, newservant, ev );

    return retval;
}

static void
impl_VLC_MediaControl__destroy( impl_POA_VLC_MediaControl * servant,
                                CORBA_Environment * ev )
{
    CORBA_Object_release( (CORBA_Object ) servant->poa, ev );

    /* No further remote method calls are delegated to 
     * servant and you may free your private attributes. */
    /* ------ free private attributes here ------ */
    /* ------ ---------- end ------------- ------ */

    POA_VLC_MediaControl__fini( (PortableServer_Servant ) servant, ev );
}

/* END INSERT */

/* Beginning of the CORBA functions that we define */

/* Returns the current position in the stream. The returned value can
   be relative or absolute ( according to PositionOrigin ) and the unit
   is set by PositionKey */
static VLC_Position
impl_VLC_MediaControl_get_media_position( impl_POA_VLC_MediaControl * servant,
                                          const VLC_PositionOrigin an_origin,
                                          const VLC_PositionKey a_key,
                                          CORBA_Environment * ev )
{
    VLC_Position* retval = NULL;
    mediacontrol_Position *p_pos;
    mediacontrol_Exception *exception = NULL;

    MC_TRY;
    p_pos = mediacontrol_get_media_position( servant->mc, an_origin, a_key, exception );
    MC_EXCEPT( *retval );

    retval = corba_position_c_to_corba( p_pos );
    free( p_pos );
    return *retval;
}

/* Sets the media position */
static void
impl_VLC_MediaControl_set_media_position( impl_POA_VLC_MediaControl * servant,
                                          const VLC_Position * a_position,
                                          CORBA_Environment * ev )
{
    mediacontrol_Position *p_pos;
    mediacontrol_Exception *exception = NULL;

    p_pos = corba_position_corba_to_c( a_position );
  
    MC_TRY;
    mediacontrol_set_media_position( servant->mc, p_pos, exception );
    MC_EXCEPT();
    free( p_pos );

    return;
}

/* Starts playing a stream */
static void
impl_VLC_MediaControl_start( impl_POA_VLC_MediaControl * servant,
                             const VLC_Position * a_position, CORBA_Environment * ev )
{
    mediacontrol_Position *p_pos;
    mediacontrol_Exception *exception = NULL;

    p_pos = corba_position_corba_to_c( a_position );
  
    MC_TRY;
    mediacontrol_start( servant->mc, p_pos, exception );
    MC_EXCEPT();

    free( p_pos );
    return;
}

static void
impl_VLC_MediaControl_pause( impl_POA_VLC_MediaControl * servant,
                             const VLC_Position * a_position, CORBA_Environment * ev )
{
    mediacontrol_Position *p_pos;
    mediacontrol_Exception *exception = NULL;

    p_pos = corba_position_corba_to_c( a_position );
  
    MC_TRY;
    mediacontrol_pause( servant->mc, p_pos, exception );
    MC_EXCEPT();

    free( p_pos );
    return;
}

static void
impl_VLC_MediaControl_resume( impl_POA_VLC_MediaControl * servant,
                              const VLC_Position * a_position, CORBA_Environment * ev )
{
    mediacontrol_Position *p_pos;
    mediacontrol_Exception *exception = NULL;

    p_pos = corba_position_corba_to_c( a_position );
  
    MC_TRY;
    mediacontrol_resume( servant->mc, p_pos, exception );
    MC_EXCEPT();

    free( p_pos );
    return;
}

static void
impl_VLC_MediaControl_stop( impl_POA_VLC_MediaControl * servant,
                            const VLC_Position * a_position, CORBA_Environment * ev )
{
    mediacontrol_Position *p_pos;
    mediacontrol_Exception *exception = NULL;

    p_pos = corba_position_corba_to_c( a_position );
  
    MC_TRY;
    mediacontrol_pause( servant->mc, p_pos, exception );
    MC_EXCEPT();

    free( p_pos );
    return;
}

static void
impl_VLC_MediaControl_exit( impl_POA_VLC_MediaControl * servant,
                            CORBA_Environment * ev )
{
    mediacontrol_exit( servant->mc );
    return;
}

static void
impl_VLC_MediaControl_playlist_add_item( impl_POA_VLC_MediaControl * servant,
                                         const CORBA_char * psz_file,
                                         CORBA_Environment * ev )
{
    mediacontrol_Exception *exception = NULL;
  
    MC_TRY;
    mediacontrol_playlist_add_item( servant->mc, psz_file, exception );
    MC_EXCEPT();

    return;
}

static void
impl_VLC_MediaControl_playlist_clear( impl_POA_VLC_MediaControl * servant,
                                      CORBA_Environment * ev )
{
    mediacontrol_Exception *exception = NULL;
  
    MC_TRY;
    mediacontrol_playlist_clear( servant->mc, exception );
    MC_EXCEPT();

    return;
}

static VLC_PlaylistSeq *
impl_VLC_MediaControl_playlist_get_list( impl_POA_VLC_MediaControl * servant,
                                         CORBA_Environment * ev )
{
    VLC_PlaylistSeq *retval = NULL;
    mediacontrol_Exception *exception = NULL;
    mediacontrol_PlaylistSeq* p_ps;
    int i_index;
   
    MC_TRY;
    p_ps = mediacontrol_playlist_get_list( servant->mc, exception );
    MC_EXCEPT( retval );

    retval = VLC_PlaylistSeq__alloc();
    retval->_buffer = VLC_PlaylistSeq_allocbuf( p_ps->size );
    retval->_length = p_ps->size;
  
    for( i_index = 0 ; i_index < p_ps->size ; i_index++ )
    {
        retval->_buffer[i_index] = CORBA_string_dup( p_ps->data[i_index] );
    }
    CORBA_sequence_set_release( retval, TRUE );
  
    mediacontrol_PlaylistSeq__free( p_ps );
    return retval;
}

VLC_RGBPicture*
createRGBPicture( mediacontrol_RGBPicture* p_pic )
{
    VLC_RGBPicture *retval;
  
    retval = VLC_RGBPicture__alloc();
    if( retval )
    {
        retval->width  = p_pic->width;
        retval->height = p_pic->height;
        retval->type   = p_pic->type;
        retval->date   = p_pic->date;
      
        retval->data._maximum = p_pic->size;
        retval->data._length = p_pic->size;
        retval->data._buffer = VLC_ByteSeq_allocbuf( p_pic->size );
        memcpy( retval->data._buffer, p_pic->data, p_pic->size );
        /* CORBA_sequence_set_release( &( retval->data ), FALSE ); */
    }
    return retval;
}

static VLC_RGBPicture *
impl_VLC_MediaControl_snapshot( impl_POA_VLC_MediaControl * servant,
                                const VLC_Position * a_position,
                                CORBA_Environment * ev )
{
    VLC_RGBPicture *retval = NULL;
    mediacontrol_RGBPicture* p_pic = NULL;
    mediacontrol_Position *p_pos;
    mediacontrol_Exception *exception = NULL;

    p_pos = corba_position_corba_to_c( a_position );
  
    MC_TRY;
    p_pic = mediacontrol_snapshot( servant->mc, p_pos, exception );
    MC_EXCEPT( retval );
  
    retval = createRGBPicture( p_pic );
    mediacontrol_RGBPicture__free( p_pic );
    return retval;
}

static VLC_RGBPictureSeq *
impl_VLC_MediaControl_all_snapshots( impl_POA_VLC_MediaControl * servant,
                                     CORBA_Environment * ev )
{
    VLC_RGBPictureSeq *retval = NULL;
    mediacontrol_RGBPicture** p_piclist = NULL;
    mediacontrol_RGBPicture** p_tmp = NULL;
    mediacontrol_Exception *exception = NULL;
    int i_size = 0;
    int i_index;
  
    MC_TRY;
    p_piclist = mediacontrol_all_snapshots( servant->mc, exception );
    MC_EXCEPT( retval );

    for( p_tmp = p_piclist ; *p_tmp != NULL ; p_tmp++ )
        i_size++;
  
    retval = VLC_RGBPictureSeq__alloc();
    retval->_buffer = VLC_RGBPictureSeq_allocbuf( i_size );
    retval->_length = i_size;

    for( i_index = 0 ; i_index < i_size ; i_index++ )
    {
        mediacontrol_RGBPicture *p_pic = p_piclist[i_index];
        VLC_RGBPicture *p_rgb;
      
        p_rgb = &( retval->_buffer[i_index] );
      
        p_rgb->width  = p_pic->width;
        p_rgb->height = p_pic->height;
        p_rgb->type   = p_pic->type;
        p_rgb->date   = p_pic->date;
      
        p_rgb->data._maximum = p_pic->size;
        p_rgb->data._length  = p_pic->size;
        p_rgb->data._buffer  = VLC_ByteSeq_allocbuf( p_pic->size );
        memcpy( p_rgb->data._buffer, p_pic->data, p_pic->size );
        mediacontrol_RGBPicture__free( p_pic );
    }
  
    free( p_piclist );
    return retval;
}

static void
impl_VLC_MediaControl_display_text( impl_POA_VLC_MediaControl * servant,
                                    const CORBA_char * message,
                                    const VLC_Position * begin,
                                    const VLC_Position * end,
                                    CORBA_Environment * ev )
{
    mediacontrol_Position *p_begin = NULL;
    mediacontrol_Position *p_end = NULL;
    mediacontrol_Exception *exception = NULL;

    p_begin = corba_position_corba_to_c( begin );
    p_end = corba_position_corba_to_c( end );
    MC_TRY;
    mediacontrol_display_text( servant->mc, message, p_begin, p_end, exception );
    MC_EXCEPT();

    free( p_begin );
    free( p_end );
    return;
}

static VLC_StreamInformation *
impl_VLC_MediaControl_get_stream_information( impl_POA_VLC_MediaControl *
                                              servant, CORBA_Environment * ev )
{
    mediacontrol_Exception *exception = NULL;
    mediacontrol_StreamInformation *p_si = NULL;
    VLC_StreamInformation *retval = NULL;

    MC_TRY;
    p_si = mediacontrol_get_stream_information( servant->mc, mediacontrol_MediaTime, exception );
    MC_EXCEPT( retval );

    retval = VLC_StreamInformation__alloc();
    if( ! retval )
    {
        return NULL;
    }

    retval->streamstatus = p_si->streamstatus;
    retval->url          = CORBA_string_dup( p_si->url );
    retval->position     = p_si->position;
    retval->length       = p_si->length;
  
    free( p_si->url );
    free( p_si );
    return retval;
}

static CORBA_unsigned_short
impl_VLC_MediaControl_sound_get_volume( impl_POA_VLC_MediaControl * servant,
                                        CORBA_Environment * ev )
{
    CORBA_short retval = 0;
    mediacontrol_Exception *exception = NULL;
  
    MC_TRY;
    retval = mediacontrol_sound_get_volume( servant->mc, exception );
    MC_EXCEPT( retval );

    return retval;
}

static void
impl_VLC_MediaControl_sound_set_volume( impl_POA_VLC_MediaControl * servant,
                                        const CORBA_unsigned_short volume,
                                        CORBA_Environment * ev )
{
    mediacontrol_Exception *exception = NULL;
  
    MC_TRY;
    mediacontrol_sound_set_volume( servant->mc, volume, exception );
    MC_EXCEPT();
}

/* ( Real ) end of the CORBA code generated in Mediacontrol-skelimpl.c */

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static void Run          ( intf_thread_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
set_category( CAT_INTERFACE );
set_subcategory( SUBCAT_INTERFACE_CONTROL );
add_category_hint( N_( "Corba control" ), NULL, VLC_FALSE );

set_description( _( "corba control module" ) );
set_capability( "interface", 10 );
add_integer( "corba-reactivity", 5000, NULL, "Internal reactivity factor", "Internal reactivity factor ( gtk timeout is INTF_IDLE_SLEEP / factor )", VLC_TRUE );
set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * intf_Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = ( intf_thread_t * )p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "Out of memory" );
        return VLC_ENOMEM;
    }

    /* Initialize the fields of the p_intf struct */
    p_intf->pf_run = Run;

    p_intf->p_sys->mc = NULL;
    p_intf->p_sys->orb = NULL;
    p_intf->p_sys->corbaloop = NULL;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * intf_Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = ( intf_thread_t * )p_this;
    CORBA_Environment*        ev = NULL;

    ev = CORBA_exception__alloc();
    CORBA_ORB_shutdown( p_intf->p_sys->orb, FALSE, ev );
    handle_exception_no_servant( p_intf, "Error in Close" );

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*
  Function called regularly to handle various tasks( mainly CORBA calls )
*/
static gboolean Manage( gpointer p_interface )
{
    intf_thread_t *p_intf = ( intf_thread_t* )p_interface;
    CORBA_boolean b_work_pending;
    CORBA_Environment* ev;

    ev = CORBA_exception__alloc();

    /* CORBA */
    b_work_pending = CORBA_ORB_work_pending( p_intf->p_sys->orb, ev );
    if( ev->_major != CORBA_NO_EXCEPTION )
    {
        msg_Err( p_intf, "Exception in CORBA events check loop" );
        return FALSE;
    }
  
    vlc_mutex_lock( &p_intf->change_lock );

    if( b_work_pending )
        CORBA_ORB_perform_work( p_intf->p_sys->orb, ev );
  
    if( p_intf->b_die )
    {
        vlc_mutex_unlock( &p_intf->change_lock );
        CORBA_ORB_shutdown( p_intf->p_sys->orb, TRUE, ev );
        g_main_loop_quit( p_intf->p_sys->corbaloop );
        /* Just in case */
        return( TRUE );
    }

    vlc_mutex_unlock( &p_intf->change_lock );

    return TRUE;
}

/*****************************************************************************
 * Run: main loop
 *****************************************************************************
 * this part of the interface is in a separate thread so that we can call
 * g_main_loop_run() from within it without annoying the rest of the program.
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    CORBA_Environment*        ev = NULL;
    PortableServer_POA        root_poa;
    PortableServer_POAManager root_poa_manager;
    guint                     i_event_source;
    CORBA_char*               psz_objref;
    impl_POA_VLC_MediaControl *servant = NULL;
    VLC_MediaControl          corba_instance;
    mediacontrol_Instance     *mc_instance;
    mediacontrol_Exception    *exception = NULL;
    int i_argc = 1;
    char* ppsz_argv[] = { "mc" };
    int i_reactivity;

    ev = CORBA_exception__alloc();

    p_intf->p_sys->orb = CORBA_ORB_init( &i_argc, ppsz_argv, "orbit-local-orb", ev );

    /* Should be cleaner this way ( cf
       http://www.fifi.org/doc/gnome-dev-doc/html/C/orbitgtk.html ) but it
       functions well enough in the ugly way so that I do not bother
       cleaning it */
    /* p_intf->p_sys->orb = gnome_CORBA_init ( "VLC", NULL, &argc, &argv, 0, NULL, ev ); */

    handle_exception_no_servant( p_intf, "Exception during CORBA_ORB_init" );

    root_poa = ( PortableServer_POA )CORBA_ORB_resolve_initial_references( p_intf->p_sys->orb, "RootPOA", ev );
    handle_exception( "Exception during RootPOA initialization" );

    corba_instance = impl_VLC_MediaControl__create( root_poa, ev );
    handle_exception( "Exception during MediaControl initialization" );

    servant = ( impl_POA_VLC_MediaControl* )PortableServer_POA_reference_to_servant( root_poa, corba_instance, ev );
    handle_exception( "Exception during MediaControl access" );

    MC_TRY;
    mc_instance = mediacontrol_new_from_object((vlc_object_t* )p_intf, exception );
    MC_EXCEPT();

    p_intf->p_sys->mc = mc_instance;

    servant->p_intf = p_intf;
    servant->mc = p_intf->p_sys->mc;

    psz_objref = CORBA_ORB_object_to_string( p_intf->p_sys->orb, corba_instance, ev );
    handle_exception( "Exception during IOR generation" );

    msg_Warn( p_intf, "MediaControl IOR :" );
    msg_Warn( p_intf, psz_objref );

    /* We write the IOR in a file. */
    {
        FILE* fp;
        fp = fopen( VLC_IOR_FILE, "w" );
        if( fp == NULL )
        {
            msg_Err( p_intf, "Cannot write the IOR to %s ( %d ).", VLC_IOR_FILE, errno );
        }
        else
        {
            fprintf( fp, "%s", psz_objref );
            fclose( fp );
            msg_Warn( p_intf, "IOR written to %s", VLC_IOR_FILE );
        }
    }
  
    root_poa_manager = PortableServer_POA__get_the_POAManager( root_poa, ev );
    handle_exception( "Exception during POAManager resolution" );

    PortableServer_POAManager_activate( root_poa_manager, ev );
    handle_exception( "Exception during POAManager activation" );

    msg_Info( p_intf, "corba remote control interface initialized" );

    /*
    // Tentative de gestion du nommage...
    {
    CosNaming_NamingContext name_service;
    CosNaming_NameComponent name_component[3] = {{"GNOME", "subcontext"},
    {"Servers", "subcontext"},
    {"vlc", "server"} };
    CosNaming_Name name = {3, 3, name_component, CORBA_FALSE};

    name_service = CORBA_ORB_resolve_initial_references( p_intf->p_sys->orb,
    "NameService",
    ev );
    handle_exception( "Error: could not get name service: %s\n",
    CORBA_exception_id( ev ) );
    msg_Warn( p_intf, "Name service OK" );

    CosNaming_NamingContext_bind( name_service, &name, p_intf->p_sys->mc, ev );
    handle_exception( "Error: could not register object: %s\n",
    CORBA_exception_id( ev ) );
    }
    */

    /* The time factor should be 1/1000 but it is a little too
       slow. Make it 1/10000 */
    i_reactivity = config_GetInt( p_intf, "corba-reactivity" );
    i_event_source = g_timeout_add( INTF_IDLE_SLEEP / i_reactivity, Manage, p_intf );
    p_intf->p_sys->corbaloop = g_main_loop_new( NULL, FALSE );
    g_main_loop_run( p_intf->p_sys->corbaloop );

    /* Cleaning */
    g_source_remove( i_event_source );
    unlink( VLC_IOR_FILE );

    /* Make sure we exit ( In case other interfaces have been spawned ) */
    mediacontrol_exit( p_intf->p_sys->mc );

    return;
}

/*****************************************************************************
 * corba.c : CORBA (ORBit) remote control plugin for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: corba.c,v 1.1 2003/07/07 16:59:00 sam Exp $
 *
 * Authors: Olivier Aubert <oaubert at lisi dot univ-lyon1 dot fr>
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
#include "mediacontrol.h"
#include "orbit/poa/portableserver-poa-type.h"
#define VLC_IOR_FILE "/tmp/vlc-ior.ref"

#define handle_exception(m) if(ev->_major != CORBA_NO_EXCEPTION) \
    { \
      msg_Err (servant->p_intf, m); \
      return; \
    }


#define handle_exception_no_servant(p,m) if(ev->_major != CORBA_NO_EXCEPTION) \
    { \
      msg_Err (p, m); \
      return; \
    }

#include <vlc/vlc.h>
#include <vlc/intf.h>
#include <vlc/vout.h>
#include <vlc/aout.h>

#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
#    include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <sys/types.h>

/*****************************************************************************
 * intf_sys_t: description and status of corba interface
 *****************************************************************************/
struct intf_sys_t
{
  CORBA_ORB                 orb;
  VLC_MediaControl          mc;
  PortableServer_POA        root_poa;
  PortableServer_POAManager root_poa_manager;
  GMainLoop*                corbaloop;

  vlc_bool_t          b_playing;

  input_thread_t *    p_input;                /* The input thread */

  msg_subscription_t* p_sub;                  /* message bank subscription */
};

/* Convert an offset into seconds. Taken from input_ext-intf.c.
   The 50 hardcoded constant comes from the definition of i_mux_rate :
   i_mux_rate : the rate we read the stream (in units of 50 bytes/s) ;
   0 if undef */
long long offsetToSeconds (input_thread_t *p_input, off_t l_offset)
{
  long long l_res;

  l_res = -1;
  if (p_input != NULL && p_input->stream.i_mux_rate != 0)
    {
      l_res = (long long) l_offset / 50 / p_input->stream.i_mux_rate;
    }
  return l_res;
}

/* Convert an offset into milliseconds */
long long offsetToMilliseconds (input_thread_t *p_input, off_t l_offset)
{
  long long l_res;

  l_res = -1;
  if (p_input != NULL && p_input->stream.i_mux_rate != 0)
    {
      l_res = (long long) 1000 * l_offset / 50 / p_input->stream.i_mux_rate;
    }
  return l_res;
}

/* Convert seconds to an offset */
off_t secondsToOffset (input_thread_t *p_input, long long l_seconds)
{
  off_t l_res;

  l_res = -1;

  if (p_input != NULL)
    {
      l_res = (off_t) l_seconds * 50 * p_input->stream.i_mux_rate;
    }
  return l_res;
}


/* Convert milliseconds to an offset */
off_t millisecondsToOffset (input_thread_t *p_input, long long l_milliseconds)
{
  off_t l_res;

  l_res = -1;
  if (p_input != NULL)
    {
      l_res = (off_t) l_milliseconds * 50 * p_input->stream.i_mux_rate / 1000;
    }
  return l_res;
}

/* Returns the current offset. */
off_t currentOffset (input_thread_t *p_input)
{
  off_t l_offset;

  if( p_input == NULL )
    {
      return -1;
    }

  /* offset contient la valeur en unités arbitraires (cf
     include/input_ext-intf.h) */
  vlc_mutex_lock( &p_input->stream.stream_lock );

#define A p_input->stream.p_selected_area
  l_offset = A->i_tell + A->i_start;
#undef A
  vlc_mutex_unlock( &p_input->stream.stream_lock );

  return l_offset;
}

/*** App-specific servant structures ***/

/* We can add attributes to this structure, which is both a pointer on a
   specific structure, and on a POA_VLC_MediaControl (servant). Cf
   http://developer.gnome.org/doc/guides/corba/html/corba-poa-example.html */

typedef struct
{
  POA_VLC_MediaControl servant;
  PortableServer_POA poa;
  /* Ajouter ici les attributs utiles */
  intf_thread_t *p_intf;
}
impl_POA_VLC_MediaControl;

/* Beginning of the CORBA code generated in Mediacontrol-skelimpl.c */
/* BEGIN INSERT */

/*** Implementation stub prototypes ***/

static void impl_VLC_MediaControl__destroy(impl_POA_VLC_MediaControl *
                                           servant, CORBA_Environment * ev);

static VLC_Position
impl_VLC_MediaControl_get_media_position(impl_POA_VLC_MediaControl * servant,
                                         const VLC_PositionOrigin an_origin,
                                         const VLC_PositionKey a_key,
                                         CORBA_Environment * ev);

static void
impl_VLC_MediaControl_set_media_position(impl_POA_VLC_MediaControl * servant,
                                         const VLC_Position * a_position,
                                         CORBA_Environment * ev);

static void
impl_VLC_MediaControl_start(impl_POA_VLC_MediaControl * servant,
                            const VLC_Position * a_position,
                            CORBA_Environment * ev);

static void
impl_VLC_MediaControl_pause(impl_POA_VLC_MediaControl * servant,
                            const VLC_Position * a_position,
                            CORBA_Environment * ev);

static void
impl_VLC_MediaControl_resume(impl_POA_VLC_MediaControl * servant,
                             const VLC_Position * a_position,
                             CORBA_Environment * ev);

static void
impl_VLC_MediaControl_stop(impl_POA_VLC_MediaControl * servant,
                           const VLC_Position * a_position,
                           CORBA_Environment * ev);

static void
impl_VLC_MediaControl_exit(impl_POA_VLC_MediaControl * servant,
                           CORBA_Environment * ev);

static void
impl_VLC_MediaControl_add_to_playlist(impl_POA_VLC_MediaControl * servant,
                                      const CORBA_char * a_file,
                                      CORBA_Environment * ev);

static VLC_PlaylistSeq
   *impl_VLC_MediaControl_get_playlist(impl_POA_VLC_MediaControl * servant,
                                       CORBA_Environment * ev);

/*** epv structures ***/

static PortableServer_ServantBase__epv impl_VLC_MediaControl_base_epv = {
   NULL,                        /* _private data */
   NULL,                        /* finalize routine */
   NULL,                        /* default_POA routine */
};
static POA_VLC_MediaControl__epv impl_VLC_MediaControl_epv = {
   NULL,                        /* _private */

   (gpointer) & impl_VLC_MediaControl_get_media_position,

   (gpointer) & impl_VLC_MediaControl_set_media_position,

   (gpointer) & impl_VLC_MediaControl_start,

   (gpointer) & impl_VLC_MediaControl_pause,

   (gpointer) & impl_VLC_MediaControl_resume,

   (gpointer) & impl_VLC_MediaControl_stop,

   (gpointer) & impl_VLC_MediaControl_exit,

   (gpointer) & impl_VLC_MediaControl_add_to_playlist,

   (gpointer) & impl_VLC_MediaControl_get_playlist,

};

/*** vepv structures ***/

static POA_VLC_MediaControl__vepv impl_VLC_MediaControl_vepv = {
   &impl_VLC_MediaControl_base_epv,
   &impl_VLC_MediaControl_epv,
};

/*** Stub implementations ***/

static VLC_MediaControl
impl_VLC_MediaControl__create(PortableServer_POA poa, CORBA_Environment * ev)
{
   VLC_MediaControl retval;
   impl_POA_VLC_MediaControl *newservant;
   PortableServer_ObjectId *objid;

   newservant = g_new0(impl_POA_VLC_MediaControl, 1);
   newservant->servant.vepv = &impl_VLC_MediaControl_vepv;
   newservant->poa = poa;
   POA_VLC_MediaControl__init((PortableServer_Servant) newservant, ev);
   objid = PortableServer_POA_activate_object(poa, newservant, ev);
   CORBA_free(objid);
   retval = PortableServer_POA_servant_to_reference(poa, newservant, ev);

   return retval;
}

static void
impl_VLC_MediaControl__destroy(impl_POA_VLC_MediaControl * servant,
                               CORBA_Environment * ev)
{
   PortableServer_ObjectId *objid;

   objid = PortableServer_POA_servant_to_id(servant->poa, servant, ev);
   PortableServer_POA_deactivate_object(servant->poa, objid, ev);
   CORBA_free(objid);

   POA_VLC_MediaControl__fini((PortableServer_Servant) servant, ev);
   g_free(servant);
}

/* END INSERT */
/* Beginning of the CORBA functions that we define */

/* Returns the current position in the stream. The returned value can
   be relative or absolute (according to PositionOrigin) and the unit
   is set by PositionKey */
static VLC_Position
impl_VLC_MediaControl_get_media_position(impl_POA_VLC_MediaControl * servant,
                                     const VLC_PositionOrigin an_origin,
                                     const VLC_PositionKey a_key,
                                     CORBA_Environment * ev)
{
  VLC_Position retval;
  off_t l_offset;
  VLC_PositionKeyNotSupported *exception;
  input_thread_t * p_input = servant->p_intf->p_sys->p_input;

  /*  msg_Warn (servant->p_intf, "Calling MediaControl::get_media_position"); */

  retval.origin = an_origin;
  retval.key = a_key;

  if ( an_origin == VLC_RelativePosition
       || an_origin == VLC_ModuloPosition )
    {
      /* Relative or ModuloPosition make no sense */
      /* FIXME: should we return 0 or raise an exception ? */
      retval.value = 0;
      return retval;
    }

  if ( p_input == NULL )
    {
      /* FIXME: should we return 0 or raise an exception ? */
      retval.value = 0;
      return retval;
    }

  /* We are asked for an AbsolutePosition. */
  /* Cf plugins/gtk/gtk_display.c */

  /* The lock is taken by the currentOffset function */
  l_offset = currentOffset (p_input);

  if (a_key == VLC_ByteCount)
    {
      retval.value = l_offset;
      return retval;
    }
  if (a_key == VLC_MediaTime)
    {
      retval.value = offsetToSeconds (p_input, l_offset);
      return retval;
    }
  if (a_key == VLC_SampleCount)
    {
      /* Raising exceptions in C : cf the good explanations in
         http://developer.gnome.org/doc/guides/corba/html/corba-module-complete-helloworld.html
      */
      exception = VLC_PositionKeyNotSupported__alloc ();
      memcpy (&exception->key, &a_key, sizeof (a_key));
      CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
                           ex_VLC_PositionKeyNotSupported,
                           exception);
      retval.value = 0;
      return retval;
    }

  /* http://catb.org/~esr/jargon/html/entry/can't-happen.html */
  return retval;
}

/* Sets the media position */
static void
impl_VLC_MediaControl_set_media_position(impl_POA_VLC_MediaControl * servant,
                                         const VLC_Position * a_position,
                                         CORBA_Environment * ev)
{
  VLC_InvalidPosition *pe_exception;
  VLC_PositionKeyNotSupported *pe_key_exception;
  off_t l_offset_destination = 0;
  int i_whence = 0;
  input_thread_t * p_input = servant->p_intf->p_sys->p_input;

  msg_Warn (servant->p_intf, "Calling MediaControl::set_media_position");

  if( p_input == NULL )
      return;

  if ( !p_input->stream.b_seekable )
    {
      pe_exception = VLC_InvalidPosition__alloc ();
      memcpy (&pe_exception->key, &a_position->key, sizeof (&a_position->key));
      CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
                           ex_VLC_InvalidPosition,
                           pe_exception);
      return;
    }

  switch ( a_position->key )
    {
    case VLC_SampleCount:
      /* The SampleCount unit is still a bit mysterious... */
      pe_key_exception = VLC_PositionKeyNotSupported__alloc ();
      memcpy (&pe_key_exception->key, &a_position->key, sizeof (&a_position->key));
      CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
                           ex_VLC_PositionKeyNotSupported,
                           pe_key_exception);
      return;
      break;
    case VLC_MediaTime:
      i_whence |= INPUT_SEEK_SECONDS;
      break;
    case VLC_ByteCount:
      i_whence |= INPUT_SEEK_BYTES;
      break;
    default:
      i_whence |= INPUT_SEEK_BYTES;
      break;
    }

  switch ( a_position->origin)
    {
    case VLC_RelativePosition:
      i_whence |= INPUT_SEEK_CUR;
      break;
    case VLC_ModuloPosition:
      i_whence |= INPUT_SEEK_END;
      break;
    case VLC_AbsolutePosition:
      i_whence |= INPUT_SEEK_SET;
      break;
    default:
      i_whence |= INPUT_SEEK_SET;
      break;
    }

  l_offset_destination = a_position->value;

  /* msg_Warn (servant->p_intf, "Offset destination : %d", l_offset_destination); */
  /* Now we can set the position. The lock is taken in the input_Seek
     function (cf input_ext-intf.c) */
  input_Seek (p_input, l_offset_destination, i_whence);
  return;
}

/* Starts playing a stream */
static void
impl_VLC_MediaControl_start(impl_POA_VLC_MediaControl * servant,
                            const VLC_Position * a_position, CORBA_Environment * ev)
{
  intf_thread_t *  p_intf = servant->p_intf;
  playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                             FIND_ANYWHERE );

  msg_Warn (servant->p_intf, "Calling MediaControl::start");

  if( p_playlist == NULL )
    {
      /* FIXME: we should raise an appropriate exception, but we must
         define it in the IDL first */
      msg_Err (servant->p_intf, "Error: no playlist available.");
      return;
    }

    vlc_mutex_lock( &p_playlist->object_lock );
    if( p_playlist->i_size )
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        playlist_Play( p_playlist );
        vlc_object_release( p_playlist );
    }
    else
    {
        vlc_mutex_unlock( &p_playlist->object_lock );
        vlc_object_release( p_playlist );
        msg_Err (servant->p_intf, "Error: playlist empty.");
    }

  return;
}

static void
impl_VLC_MediaControl_pause(impl_POA_VLC_MediaControl * servant,
                        const VLC_Position * a_position, CORBA_Environment * ev)
{
  input_thread_t *p_input = servant->p_intf->p_sys->p_input;

  msg_Warn (servant->p_intf, "Calling MediaControl::pause");

  if( p_input != NULL )
    {
      input_SetStatus( p_input, INPUT_STATUS_PAUSE );
    }

    return;
}

static void
impl_VLC_MediaControl_resume(impl_POA_VLC_MediaControl * servant,
                         const VLC_Position * a_position, CORBA_Environment * ev)
{
  input_thread_t *p_input = servant->p_intf->p_sys->p_input;

  msg_Warn (servant->p_intf, "Calling MediaControl::resume");

  if( p_input != NULL )
    {
      input_SetStatus( p_input, INPUT_STATUS_PAUSE );
    }

    return;
}

static void
impl_VLC_MediaControl_stop(impl_POA_VLC_MediaControl * servant,
                       const VLC_Position * a_position, CORBA_Environment * ev)
{
  intf_thread_t *  p_intf = servant->p_intf;
  playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                             FIND_ANYWHERE );

  msg_Warn (servant->p_intf, "Calling MediaControl::stop");

  if( p_playlist != NULL )
    {
      playlist_Stop( p_playlist );
      vlc_object_release( p_playlist );
    }

  return;
}

static void
impl_VLC_MediaControl_exit(impl_POA_VLC_MediaControl * servant,
                           CORBA_Environment * ev)
{
  msg_Warn (servant->p_intf, "Calling MediaControl::exit");

  vlc_mutex_lock( &servant->p_intf->change_lock );
  servant->p_intf->b_die = TRUE;
  vlc_mutex_unlock( &servant->p_intf->change_lock );
}

static void
impl_VLC_MediaControl_add_to_playlist(impl_POA_VLC_MediaControl * servant,
                                      const CORBA_char * psz_file,
                                      CORBA_Environment * ev)
{
  intf_thread_t *  p_intf = servant->p_intf;
  playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                             FIND_ANYWHERE );

  msg_Warn (servant->p_intf, "Calling MediaControl::add_to_playlist %s", psz_file);

  if ( p_playlist == NULL )
    {
      msg_Err (servant->p_intf, "Error: no playlist defined");
      /* FIXME: should return an exception */
      return;
    }

  playlist_Add (p_playlist, psz_file, PLAYLIST_REPLACE, 0);
  vlc_object_release( p_playlist );

  return;
}

static VLC_PlaylistSeq *
impl_VLC_MediaControl_get_playlist(impl_POA_VLC_MediaControl * servant,
                                   CORBA_Environment * ev)
{
   VLC_PlaylistSeq *retval;
   int i_index;
   intf_thread_t *  p_intf = servant->p_intf;
   playlist_t * p_playlist = vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                              FIND_ANYWHERE );
   int i_playlist_size;

   msg_Warn (servant->p_intf, "Calling MediaControl::get_playlist");

   vlc_mutex_lock( &p_playlist->object_lock );
   i_playlist_size = p_playlist->i_size;

   retval = VLC_PlaylistSeq__alloc ();
   retval->_buffer = VLC_PlaylistSeq_allocbuf (i_playlist_size);
   retval->_length = i_playlist_size;

   for (i_index = 0 ; i_index < i_playlist_size ; i_index++)
     {
       retval->_buffer[i_index] =
         CORBA_string_dup (p_playlist->pp_items[i_index]->psz_name);
     }
   vlc_mutex_unlock( &p_playlist->object_lock );
   vlc_object_release( p_playlist );

   CORBA_sequence_set_release (retval, TRUE);
   return retval;
}

/* (Real) end of the CORBA code generated in Mediacontrol-skelimpl.c */

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
    add_category_hint( N_("Corba control"), NULL, VLC_FALSE );
    set_description( _("corba control module") );
    set_capability( "interface", 10 );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * intf_Open: initialize and create stuff
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
  intf_thread_t *p_intf = (intf_thread_t *)p_this;

  /* Allocate instance and initialize some members */
  p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
  if( p_intf->p_sys == NULL )
    {
      msg_Err( p_intf, "out of memory" );
      return VLC_ENOMEM;
    }

  /* Initialize the fields of the p_intf struct */
  p_intf->pf_run = Run;
  p_intf->p_sys->b_playing = VLC_FALSE;
  p_intf->p_sys->p_input = NULL;

  p_intf->p_sys->orb = NULL;
  p_intf->p_sys->mc = NULL;
  p_intf->p_sys->root_poa = NULL;
  p_intf->p_sys->root_poa_manager = NULL;
  p_intf->p_sys->corbaloop = NULL;

  return VLC_SUCCESS;
}



/*****************************************************************************
 * intf_Close: destroy interface
 *****************************************************************************/
static void Close( vlc_object_t *p_this )
{
  intf_thread_t *p_intf = (intf_thread_t *)p_this;
  CORBA_Environment*        ev = NULL;

  ev = CORBA_exception__alloc ();
  CORBA_ORB_shutdown (p_intf->p_sys->orb, FALSE, ev);
  handle_exception_no_servant (p_intf, "Erreur dans Close");

  if( p_intf->p_sys->p_input )
    {
      vlc_object_release( p_intf->p_sys->p_input );
    }

  /* Destroy structure */
  free( p_intf->p_sys );
}

/*
  Function called regularly to handle various tasks (mainly CORBA calls)
 */
static gboolean Manage (gpointer p_interface)
{
  intf_thread_t *p_intf = (intf_thread_t*)p_interface;
  CORBA_boolean b_work_pending;
  CORBA_Environment* ev;

  ev = CORBA_exception__alloc ();

  /* CORBA */
  b_work_pending = CORBA_ORB_work_pending (p_intf->p_sys->orb, ev);
  if(ev->_major != CORBA_NO_EXCEPTION)
    {
      msg_Err (p_intf, "Exception dans la vérif d'événements CORBA");
      return FALSE;
    }

  vlc_mutex_lock( &p_intf->change_lock );

  /* Update the input */
  if( p_intf->p_sys->p_input == NULL )
    {
      p_intf->p_sys->p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT,
                                                FIND_ANYWHERE );
    }
  else if( p_intf->p_sys->p_input->b_dead )
    {
      vlc_object_release( p_intf->p_sys->p_input );
      p_intf->p_sys->p_input = NULL;
    }

  if( p_intf->p_sys->p_input )
    {
      input_thread_t  *p_input = p_intf->p_sys->p_input;

      vlc_mutex_lock( &p_input->stream.stream_lock );

      if ( !p_input->b_die )
        {
          /* New input or stream map change */
          if( p_input->stream.b_changed )
            {
              /* FIXME: We should notify our client that the input changed */
              /* E_(GtkModeManage)( p_intf ); */
              p_intf->p_sys->b_playing = 1;
            }
        }
      vlc_mutex_unlock( &p_input->stream.stream_lock );
    }
  else if( p_intf->p_sys->b_playing && !p_intf->b_die )
    {
      /* FIXME: We should notify our client that the input changed */
      /* E_(GtkModeManage)( p_intf ); */
      p_intf->p_sys->b_playing = 0;
    }

  /* CORBA calls handling. Beware: no lock is taken (since p_pinput
     can be null) */
  if (b_work_pending)
    CORBA_ORB_perform_work (p_intf->p_sys->orb, ev);

  if( p_intf->b_die )
    {
      vlc_mutex_unlock( &p_intf->change_lock );
      g_main_loop_quit (p_intf->p_sys->corbaloop);
      /* Just in case */
      return( FALSE );
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
static void Run ( intf_thread_t *p_intf )
{
  CORBA_Environment*        ev = NULL;
  guint                     i_event_source;
  CORBA_char*               psz_objref;
  impl_POA_VLC_MediaControl *servant = NULL;
  int i_argc = 1;
  char* ppsz_argv[] = { "mc" };

  msg_Warn (p_intf, "Entering Run");

  ev = CORBA_exception__alloc ();

  /* To be able to use CORBA in a MT app */
  linc_set_threaded (TRUE);

  p_intf->p_sys->orb = CORBA_ORB_init(&i_argc, ppsz_argv, "orbit-local-orb", ev);

  /* Should be cleaner this way (cf
     http://www.fifi.org/doc/gnome-dev-doc/html/C/orbitgtk.html) but it
     functions well enough in the ugly way so that I do not bother
     cleaning it */
  /* p_intf->p_sys->orb = gnome_CORBA_init ("VLC", NULL, &argc, &argv, 0, NULL, ev); */

  handle_exception_no_servant (p_intf, "Exception during CORBA_ORB_init");

  p_intf->p_sys->root_poa = (PortableServer_POA)CORBA_ORB_resolve_initial_references(p_intf->p_sys->orb, "RootPOA", ev);
  handle_exception ("Exception during RootPOA initialization");

  p_intf->p_sys->mc = impl_VLC_MediaControl__create(p_intf->p_sys->root_poa, ev);
  handle_exception ("Exception during MediaControl initialization");

  servant = (impl_POA_VLC_MediaControl*)PortableServer_POA_reference_to_servant(p_intf->p_sys->root_poa, p_intf->p_sys->mc, ev);
  handle_exception ("Exception during MediaControl access");

  servant->p_intf = p_intf;

  psz_objref = CORBA_ORB_object_to_string(p_intf->p_sys->orb, p_intf->p_sys->mc, ev);
  handle_exception ("Exception during IOR generation");

  msg_Warn (p_intf, "MediaControl IOR :");
  msg_Warn (p_intf, psz_objref);

  /* We write the IOR in a file. */
  {
    FILE* fp;
    fp = fopen (VLC_IOR_FILE, "w");
    if (fp == NULL)
      {
        msg_Err (servant->p_intf, "Cannot write the IOR to %s (%d).", VLC_IOR_FILE, errno);
      }
    else
      {
        fprintf (fp, "%s", psz_objref);
        fclose (fp);
        msg_Warn (servant->p_intf, "IOR written to %s", VLC_IOR_FILE);
      }
  }

  msg_Warn (p_intf, "get_the_POAManager (state  %s)", p_intf->p_sys->root_poa);
  p_intf->p_sys->root_poa_manager = PortableServer_POA__get_the_POAManager(p_intf->p_sys->root_poa, ev);
  handle_exception ("Exception during POAManager resolution");

  msg_Warn (p_intf, "Activating POAManager");
  PortableServer_POAManager_activate(p_intf->p_sys->root_poa_manager, ev);
  handle_exception ("Exception during POAManager activation");

  msg_Info(p_intf, "corba remote control interface initialized" );

  /*
    // Tentative de gestion du nommage...
  {
    CosNaming_NamingContext name_service;
    CosNaming_NameComponent name_component[3] = {{"GNOME", "subcontext"},
                                                 {"Servers", "subcontext"},
                                                 {"vlc", "server"} };
    CosNaming_Name name = {3, 3, name_component, CORBA_FALSE};

    name_service = CORBA_ORB_resolve_initial_references (p_intf->p_sys->orb,
                                                         "NameService",
                                                         ev);
    handle_exception ("Error: could not get name service: %s\n",
                   CORBA_exception_id(ev));
    msg_Warn (p_intf, "Name service OK");

    CosNaming_NamingContext_bind (name_service, &name, p_intf->p_sys->mc, ev);
      handle_exception ("Error: could not register object: %s\n",
      CORBA_exception_id(ev));
  }
    */

  /* The time factor should be 1/1000 but it is a little too
     slow. Make it 1/10000 */
  i_event_source = g_timeout_add (INTF_IDLE_SLEEP / 10000,
                                Manage,
                                p_intf);
  msg_Warn (p_intf, "Entering mainloop");

  p_intf->p_sys->corbaloop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (p_intf->p_sys->corbaloop);

  /* Cleaning */
  g_source_remove( i_event_source );
  unlink (VLC_IOR_FILE);

  msg_Warn (p_intf, "Normal termination of VLC corba plugin");
  return;
}

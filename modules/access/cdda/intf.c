/*****************************************************************************
 * intf.c: Video CD interface to handle user interaction and still time
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: intf.c,v 1.1 2003/11/26 03:35:26 rocky Exp $
 *
 * Authors: Rocky Bernstein <rocky@panix.com>
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
#include <vlc/vlc.h>
#include <vlc/intf.h>

#include "stream_control.h"
#include "input_ext-intf.h"
#include "input_ext-dec.h"
#include "vlc_keys.h"

#include "cdda.h"

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    input_thread_t    * p_input;
    cdda_data_t       * p_cdda;
    vlc_bool_t          b_click, b_move, b_key_pressed;
};

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  InitThread     ( intf_thread_t *p_intf );
static int  KeyEvent       ( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

/* Exported functions */
static void RunIntf        ( intf_thread_t *p_intf );

/*****************************************************************************
 * OpenIntf: initialize dummy interface
 *****************************************************************************/
int E_(OpenIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( 1 );
    };

    p_intf->pf_run = RunIntf;

    var_AddCallback( p_intf->p_vlc, "key-pressed", KeyEvent, p_intf );

    return( 0 );
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void E_(CloseIntf) ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Destroy structure */
    free( p_intf->p_sys );
}

/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void RunIntf( intf_thread_t *p_intf )
{
    vlc_object_t      * p_vout = NULL;
    cdda_data_t       * p_cdda;
    input_thread_t    * p_input;
    
    /* What you add to the last input number entry. It accumulates all of
       the 10_ADD keypresses */
    int number_addend = 0; 
    
    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize intf" );
        return;
    }

    p_input = p_intf->p_sys->p_input;
    p_cdda   = p_intf->p_sys->p_cdda = 
      (cdda_data_t *) p_input->p_access_data;

    dbg_print( INPUT_DBG_CALL, "intf initialized" );

    /* Main loop */
    while( !p_intf->b_die )
    {
      vlc_mutex_lock( &p_intf->change_lock );

      /*
       * keyboard event
       */
      if( p_vout && p_intf->p_sys->b_key_pressed )
        {
	  vlc_value_t val;
	  int i, i_action = -1;
	  struct hotkey *p_hotkeys = p_intf->p_vlc->p_hotkeys;

	  p_intf->p_sys->b_key_pressed = VLC_FALSE;
          
	  /* Find action triggered by hotkey (if any) */
	  var_Get( p_intf->p_vlc, "key-pressed", &val );

	  dbg_print( INPUT_DBG_EVENT, "Key pressed %d", val.i_int );

	  for( i = 0; p_hotkeys[i].psz_action != NULL; i++ )
            {
	      if( p_hotkeys[i].i_key == val.i_int )
                {
		  i_action = p_hotkeys[i].i_action;
                }
            }
	  
	  if( i_action != -1) {
	    switch (i_action) {
	      
	    case ACTIONID_NAV_LEFT: 
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_LEFT (%d)", 
			 number_addend );
	      do {
		if ( CDDAPlay( p_input, p_cdda->i_track-1 ) ) {
		  p_cdda->i_track--;
		} else {
		  break;
		}
	      }	while (number_addend-- > 0);
	      break;

	    case ACTIONID_NAV_RIGHT:
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_RIGHT (%d)",
			 number_addend );
	      do {
		if ( CDDAPlay( p_input, p_cdda->i_track+1 ) ) {
		  p_cdda->i_track++;
		} else {
		  break;
		}
	      } while (number_addend-- > 0);
	      break;

	    case ACTIONID_NAV_UP:
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_UP" );
	      do {
		;
	      } while (number_addend-- > 0);
	      break;

	    case ACTIONID_NAV_DOWN:
	      dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_DOWN"  );
	      break;

	    case ACTIONID_NAV_ACTIVATE: 
	      {
		dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_ACTIVATE" );
		if ( CDDAPlay( p_input, number_addend ) ) {
		  p_cdda->i_track = number_addend;
		} else {
		  break;
		}
		break;
	      }
	    }
	    number_addend = 0;
	  } else {
	    unsigned int digit_entered=0;

	    switch (val.i_int) {
	    case '9':
	      digit_entered++;
	    case '8':
	      digit_entered++;
	    case '7':
	      digit_entered++;
	    case '6':
	      digit_entered++;
	    case '5':
	      digit_entered++;
	    case '4':
	      digit_entered++;
	    case '3':
	      digit_entered++;
	    case '2':
	      digit_entered++;
	    case '1':
	      digit_entered++;
	    case '0':
	      {
		number_addend *= 10;
		number_addend += digit_entered;
		dbg_print( INPUT_DBG_EVENT, 
			   "Added %d. Number is now: %d\n", 
			   digit_entered, number_addend);
		break;
	      }
	    }
	  }
        }

      
      vlc_mutex_unlock( &p_intf->change_lock );
      
      if( p_vout == NULL )
        {
	  p_vout = vlc_object_find( p_intf->p_sys->p_input,
				    VLC_OBJECT_VOUT, FIND_ANYWHERE );
	  if( p_vout )
            {
	      var_AddCallback( p_vout, "key-pressed", KeyEvent, p_intf );
            }
        }
      
      
      /* Wait a bit */
      msleep( INTF_IDLE_SLEEP );
    }

    if( p_vout )
    {
        var_DelCallback( p_vout, "key-pressed", KeyEvent, p_intf );
        vlc_object_release( p_vout );
    }

    vlc_object_release( p_intf->p_sys->p_input );
}

/*****************************************************************************
 * InitThread:
 *****************************************************************************/
static int InitThread( intf_thread_t * p_intf )
{
    /* We might need some locking here */
    if( !p_intf->b_die )
    {
        input_thread_t * p_input;

        p_input = vlc_object_find( p_intf, VLC_OBJECT_INPUT, FIND_PARENT );

        /* Maybe the input just died */
        if( p_input == NULL )
        {
            return VLC_EGENERIC;
        }

        vlc_mutex_lock( &p_intf->change_lock );

        p_intf->p_sys->p_input = p_input;

        p_intf->p_sys->b_move = VLC_FALSE;
        p_intf->p_sys->b_click = VLC_FALSE;
        p_intf->p_sys->b_key_pressed = VLC_FALSE;

        vlc_mutex_unlock( &p_intf->change_lock );

        return VLC_SUCCESS;
    }
    else
    {
        return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * KeyEvent: callback for keyboard events
 *****************************************************************************/
static int KeyEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    vlc_mutex_lock( &p_intf->change_lock );

    p_intf->p_sys->b_key_pressed = VLC_TRUE;

    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

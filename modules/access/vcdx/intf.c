/*****************************************************************************
 * intf.c: Video CD interface to handle user interaction and still time
 *****************************************************************************
 * Copyright (C) 2002,2003 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
 *
 * Author: Rocky Bernstein <rocky@panix.com>
 *   from DVD code by Stéphane Borel <stef@via.ecp.fr>
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
#include <vlc/input.h>

#include "vlc_keys.h"

#include "vcd.h"
#include "vcdplayer.h"
#include "intf.h"

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
int VCDOpenIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    msg_Dbg( p_intf, "VCDOpenIntf" );

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        return( VLC_EGENERIC );
    };

    p_intf->pf_run = RunIntf;

    var_AddCallback( p_intf->p_vlc, "key-pressed", KeyEvent, p_intf );
    p_intf->p_sys->m_still_time = 0;
    p_intf->p_sys->b_infinite_still = 0;
    p_intf->p_sys->b_still = 0;

    return( VLC_SUCCESS );
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
void VCDCloseIntf ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    var_DelCallback( p_intf->p_vlc, "key-pressed", KeyEvent, p_intf );

    /* Destroy structure */
    free( p_intf->p_sys );
}


/*****************************************************************************
 * RunIntf: main loop
 *****************************************************************************/
static void 
RunIntf( intf_thread_t *p_intf )
{
    vlc_object_t      * p_vout = NULL;
    mtime_t             mtime = 0;
    mtime_t             mlast = 0;
    vcdplayer_t       * p_vcdplayer;
    input_thread_t    * p_input;
    access_t          * p_access;

    /* What you add to the last input number entry. It accumulates all of
       the 10_ADD keypresses */
    int number_addend = 0;

    if( InitThread( p_intf ) < 0 )
    {
        msg_Err( p_intf, "can't initialize intf" );
        return;
    }

    p_input = p_intf->p_sys->p_input;

    while ( !p_intf->p_sys->p_vcdplayer )
    {
        msleep( INTF_IDLE_SLEEP );
    }
    
    p_vcdplayer = p_intf->p_sys->p_vcdplayer;
    p_access    = p_vcdplayer->p_access;

    dbg_print( INPUT_DBG_CALL, "intf initialized" );

    /* Main loop */
    while( !p_intf->b_die )
    {
      vlc_mutex_lock( &p_intf->change_lock );

        /*
         * Have we timed-out in showing a still frame?
         */
        if( p_intf->p_sys->b_still && !p_intf->p_sys->b_infinite_still )
        {
            if( p_intf->p_sys->m_still_time > 0 )
            {
                /* Update remaining still time */
                dbg_print(INPUT_DBG_STILL, "updating still time");
                mtime = mdate();
                if( mlast )
                {
                    p_intf->p_sys->m_still_time -= mtime - mlast;
                }

                mlast = mtime;
            }
            else
            {
                /* Still time has elapsed; set to continue playing. */
                dbg_print(INPUT_DBG_STILL, "wait time done - setting play");
                var_SetInteger( p_intf->p_sys->p_input, "state", PLAYING_S );
                p_intf->p_sys->m_still_time = 0;
                p_intf->p_sys->b_still = 0;
                mlast = 0;
            }
        }

      /*
       * Do we have a keyboard event?
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
              dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_LEFT - prev (%d)",
                         number_addend );
              do {
                vcdplayer_play_prev( p_access );
              }        while (number_addend-- > 0);
              break;

            case ACTIONID_NAV_RIGHT:
              dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_RIGHT - next (%d)",
                         number_addend );
              do {
                vcdplayer_play_next( p_access );
              } while (number_addend-- > 0);
              break;

            case ACTIONID_NAV_UP:
              dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_UP - return" );
              do {
                vcdplayer_play_return( p_access );
              } while (number_addend-- > 0);
              break;

            case ACTIONID_NAV_DOWN:
              dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_DOWN - default"  );
              vcdplayer_play_default( p_access );
              break;

            case ACTIONID_NAV_ACTIVATE:
              {
                vcdinfo_itemid_t itemid;
                itemid.type=p_vcdplayer->play_item.type;

                dbg_print( INPUT_DBG_EVENT, "ACTIONID_NAV_ACTIVATE" );

                if ( vcdplayer_pbc_is_on( p_vcdplayer ) 
		     && number_addend != 0 ) {
                  lid_t next_num=vcdinfo_selection_get_lid(p_vcdplayer->vcd,
                                                           p_vcdplayer->i_lid,
                                                           number_addend);
                  if (VCDINFO_INVALID_LID != next_num) {
                    itemid.num  = next_num;
                    itemid.type = VCDINFO_ITEM_TYPE_LID;
                    vcdplayer_play( p_access, itemid );
                  }
                } else {
                  itemid.num = number_addend;
                  vcdplayer_play( p_access, itemid );
                }
                break;
              }
            }
            number_addend = 0;

            /* Any keypress gets rid of still frame waiting.
               FIXME - should handle just the ones that cause an action.
            */
            if( p_intf->p_sys->b_still )
              {
                dbg_print(INPUT_DBG_STILL, "Playing still after activate");
                var_SetInteger( p_intf->p_sys->p_input, "state", PLAYING_S );
                p_intf->p_sys->b_still = 0;
                p_intf->p_sys->b_infinite_still = 0;
                p_intf->p_sys->m_still_time = 0;
              }

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
                                    VLC_OBJECT_VOUT, FIND_CHILD );
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

        p_intf->p_sys->p_input     = p_input;
        p_intf->p_sys->p_vcdplayer = NULL;

        p_intf->p_sys->b_move  = VLC_FALSE;
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

/*****************************************************************************
 * vcdIntfStillTime: function provided to demux plugin to request
 * still images
 *****************************************************************************/
int vcdIntfStillTime( intf_thread_t *p_intf, uint8_t i_sec )
{
    vlc_mutex_lock( &p_intf->change_lock );

    p_intf->p_sys->b_still = 1;
    if( 255 == i_sec )
    {
        p_intf->p_sys->b_infinite_still = VLC_TRUE;
    }
    else 
    {
        p_intf->p_sys->m_still_time = MILLISECONDS_PER_SEC * i_sec;
    }
    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * vcdIntfStillTime: function provided to reset still image
 *****************************************************************************/
int vcdIntfResetStillTime( intf_thread_t *p_intf )
{
    vlc_mutex_lock( &p_intf->change_lock );
    p_intf->p_sys->m_still_time = 0;
    var_SetInteger( p_intf->p_sys->p_input, "state", PLAYING_S );
    vlc_mutex_unlock( &p_intf->change_lock );

    return VLC_SUCCESS;
}

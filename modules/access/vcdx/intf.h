/*****************************************************************************
 * intf.h: send info to intf.
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN (Centrale RÃ©seaux) and its contributors
 * $Id$
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
  input_thread_t *p_input;
  vcdplayer_t    *p_vcdplayer;

  vlc_bool_t      b_still;           /* True if we are in a still frame */
  vlc_bool_t      b_infinite_still;  /* True if still wait time is infinite */
  mtime_t         m_still_time;      /* Time in microseconds remaining
                                         to wait in still frame.
				     */
#if FINISHED
  vcdplay_ctrl_t      control;
#else
  int                 control;
#endif
  vlc_bool_t          b_click, b_move, b_key_pressed;
};

int vcdIntfStillTime( struct intf_thread_t * p_intf, uint8_t wait_time);
int vcdIntfResetStillTime( intf_thread_t *p_intf );


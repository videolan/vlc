/*****************************************************************************
 * plugins_export.h : exporting plugins structure
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
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
 * Prototypes
 *****************************************************************************/

/* audio output */
int     aout_SysOpen         ( aout_thread_t *p_aout );
int     aout_SysReset        ( aout_thread_t *p_aout );
int     aout_SysSetFormat    ( aout_thread_t *p_aout );
int     aout_SysSetChannels  ( aout_thread_t *p_aout );
int     aout_SysSetRate      ( aout_thread_t *p_aout );
long    aout_SysGetBufInfo   ( aout_thread_t *p_aout, long l_buffer_info );
void    aout_SysPlaySamples  ( aout_thread_t *p_aout, byte_t *buffer,
                               int i_size );
void    aout_SysClose        ( aout_thread_t *p_aout );

/* video output */
int     vout_SysCreate       ( vout_thread_t *p_vout, char *psz_display,
                               int i_root_window, void *p_data );
int     vout_SysInit         ( p_vout_thread_t p_vout );
void    vout_SysEnd          ( p_vout_thread_t p_vout );
void    vout_SysDestroy      ( p_vout_thread_t p_vout );
int     vout_SysManage       ( p_vout_thread_t p_vout );
void    vout_SysDisplay      ( p_vout_thread_t p_vout );
void    vout_SetPalette      ( p_vout_thread_t p_vout,
                               u16 *red, u16 *green, u16 *blue, u16 *transp );

/* interface */
int     intf_SysCreate       ( p_intf_thread_t p_intf );
void    intf_SysDestroy      ( p_intf_thread_t p_intf );
void    intf_SysManage       ( p_intf_thread_t p_intf );

/* YUV transformations */
int     yuv_SysInit          ( p_vout_thread_t p_vout );
int     yuv_SysReset         ( p_vout_thread_t p_vout );
void    yuv_SysEnd           ( p_vout_thread_t p_vout );


/*****************************************************************************
 * input_ext-plugins.h: structures of the input not exported to other modules,
 *                      but exported to plug-ins
 *****************************************************************************
 * Copyright (C) 1999-2002 VideoLAN
 * $Id: input_ext-plugins.h,v 1.42 2003/05/05 22:23:31 gbazin Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

/*
 * Communication plugin -> input
 */

/* FIXME: you've gotta move this move this, you've gotta move this move this */
#define PADDING_PACKET_SIZE 188 /* Size of the NULL packet inserted in case
                                 * of data loss (this should be < 188).      */
#define PADDING_PACKET_NUMBER 10 /* Number of padding packets top insert to
                                  * escape a decoder.                        */
#define INPUT_DEFAULT_BUFSIZE 65536 /* Default buffer size to use when none
                                     * is natural.                           */
#define NO_SEEK             -1

/*****************************************************************************
 * Prototypes from input_programs.c
 *****************************************************************************/
VLC_EXPORT( int,  input_InitStream,( input_thread_t *, size_t ) );
VLC_EXPORT( void, input_EndStream, ( input_thread_t * ) );
VLC_EXPORT( pgrm_descriptor_t *, input_FindProgram,( input_thread_t *, uint16_t ) );
VLC_EXPORT( pgrm_descriptor_t *, input_AddProgram, ( input_thread_t *, uint16_t, size_t ) );
VLC_EXPORT( void, input_DelProgram,( input_thread_t *, pgrm_descriptor_t * ) );
VLC_EXPORT( int, input_SetProgram,( input_thread_t *, pgrm_descriptor_t * ) );
VLC_EXPORT( input_area_t *, input_AddArea,( input_thread_t *, uint16_t, uint16_t ) );
VLC_EXPORT( void, input_DelArea,   ( input_thread_t *, input_area_t * ) );
VLC_EXPORT( es_descriptor_t *, input_FindES,( input_thread_t *, uint16_t ) );
VLC_EXPORT( es_descriptor_t *, input_AddES, ( input_thread_t *, pgrm_descriptor_t *, uint16_t, int, char const *, size_t ) );
VLC_EXPORT( void, input_DelES,     ( input_thread_t *, es_descriptor_t * ) );
VLC_EXPORT( int,  input_SelectES,  ( input_thread_t *, es_descriptor_t * ) );
VLC_EXPORT( int,  input_UnselectES,( input_thread_t *, es_descriptor_t * ) );

/*****************************************************************************
 * Prototypes from input_dec.c
 *****************************************************************************/
decoder_fifo_t * input_RunDecoder( input_thread_t *, es_descriptor_t * );
void input_EndDecoder( input_thread_t *, es_descriptor_t * );
VLC_EXPORT( void, input_DecodePES, ( decoder_fifo_t *, pes_packet_t * ) );
void input_EscapeDiscontinuity( input_thread_t * );
void input_EscapeAudioDiscontinuity( input_thread_t * );
VLC_EXPORT( void, input_NullPacket, ( input_thread_t *, es_descriptor_t * ) );

/*****************************************************************************
 * Prototypes from input_clock.c
 *****************************************************************************/
void input_ClockInit( pgrm_descriptor_t * );
VLC_EXPORT( int,  input_ClockManageControl, ( input_thread_t *, pgrm_descriptor_t *, mtime_t ) );
VLC_EXPORT( void, input_ClockManageRef, ( input_thread_t *, pgrm_descriptor_t *, mtime_t ) );
VLC_EXPORT( mtime_t, input_ClockGetTS, ( input_thread_t *, pgrm_descriptor_t *, mtime_t ) );

/*****************************************************************************
 * Prototypes from input_info.c
 *****************************************************************************/
VLC_EXPORT( input_info_category_t *, input_InfoCategory, ( input_thread_t *, char * ) );
VLC_EXPORT( int, input_AddInfo, ( input_info_category_t *, char *, char *, ... ) );
int input_DelInfo( input_thread_t * p_input ); /* no need to export this */
/*****************************************************************************
 * Prototypes from input_ext-plugins.h (buffers management)
 *****************************************************************************/
#define input_BuffersInit(a) __input_BuffersInit(VLC_OBJECT(a))
void * __input_BuffersInit( vlc_object_t * );
VLC_EXPORT( void, input_BuffersEnd, ( input_thread_t *, input_buffers_t * ) );

VLC_EXPORT( data_buffer_t *, input_NewBuffer,   ( input_buffers_t *, size_t ) );
VLC_EXPORT( void, input_ReleaseBuffer,          ( input_buffers_t *, data_buffer_t * ) );
VLC_EXPORT( data_packet_t *, input_ShareBuffer, ( input_buffers_t *, data_buffer_t * ) );
VLC_EXPORT( data_packet_t *, input_NewPacket,   ( input_buffers_t *, size_t ) );
VLC_EXPORT( void, input_DeletePacket,           ( input_buffers_t *, data_packet_t * ) );
VLC_EXPORT( pes_packet_t *, input_NewPES, ( input_buffers_t * ) );
VLC_EXPORT( ssize_t, input_FillBuffer,  ( input_thread_t * ) );
VLC_EXPORT( ssize_t, input_Peek,        ( input_thread_t *, byte_t **, size_t ) );
VLC_EXPORT( ssize_t, input_SplitBuffer, ( input_thread_t *, data_packet_t **, size_t ) );
VLC_EXPORT( int, input_AccessInit,      ( input_thread_t * ) );
VLC_EXPORT( void, input_AccessReinit,   ( input_thread_t * ) );
VLC_EXPORT( void, input_AccessEnd,      ( input_thread_t * ) );

/* no need to export this one */
data_packet_t *input_NewPacketForce( input_buffers_t *, size_t );

/*
 * Optional standard file descriptor operations (input_ext-plugins.h)
 */

/*****************************************************************************
 * input_socket_t: private access plug-in data
 *****************************************************************************/
struct input_socket_t
{
    /* Unbuffered file descriptor */
    int i_handle;
};


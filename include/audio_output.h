/*****************************************************************************
 * audio_output.h : audio output interface
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: audio_output.h,v 1.52 2002/08/07 21:36:55 massiot Exp $
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

/*****************************************************************************
 * audio_sample_format_t
 *****************************************************************************
 * This structure defines a format for audio samples.
 *****************************************************************************/
struct audio_sample_format_t
{
    int                 i_format;
    int                 i_rate;
    int                 i_channels;
};

#define AOUT_FMT_MU_LAW     0x00000001
#define AOUT_FMT_A_LAW      0x00000002
#define AOUT_FMT_IMA_ADPCM  0x00000004
#define AOUT_FMT_U8         0x00000008
#define AOUT_FMT_S16_LE     0x00000010            /* Little endian signed 16 */
#define AOUT_FMT_S16_BE     0x00000020               /* Big endian signed 16 */
#define AOUT_FMT_S8         0x00000040
#define AOUT_FMT_U16_LE     0x00000080                  /* Little endian U16 */
#define AOUT_FMT_U16_BE     0x00000100                     /* Big endian U16 */
#define AOUT_FMT_A52        0x00000400             /* ATSC A/52 (for SP/DIF) */
#define AOUT_FMT_FLOAT32    0x00000800
#define AOUT_FMT_FIXED32    0x00001000

#define AOUT_FMTS_IDENTICAL( p_first, p_second ) (                          \
    (p_first->i_format == p_second->i_format)                               \
      && (p_first->i_rate == p_second->i_rate)                              \
      && (p_first->i_channels == p_second->i_channels                       \
           || p_first->i_channels == -1 || p_second->i_channels == -1) )

#ifdef WORDS_BIGENDIAN
#   define AOUT_FMT_S16_NE AOUT_FMT_S16_BE
#   define AOUT_FMT_U16_NE AOUT_FMT_U16_BE
#else
#   define AOUT_FMT_S16_NE AOUT_FMT_S16_LE
#   define AOUT_FMT_U16_NE AOUT_FMT_U16_LE
#endif

/*****************************************************************************
 * aout_buffer_t : audio output buffer
 *****************************************************************************/
struct aout_buffer_t
{
    char *                  p_buffer;
    int                     i_alloc_type;
    size_t                  i_size;
    int                     i_nb_samples;
    mtime_t                 start_date, end_date;

    struct aout_buffer_t *  p_next;
};

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
/* From audio_output.c : */
#define aout_NewInstance(a) __aout_NewInstance(VLC_OBJECT(a))
VLC_EXPORT( aout_instance_t *, __aout_NewInstance,    ( vlc_object_t * ) );
VLC_EXPORT( void,              aout_DeleteInstance, ( aout_instance_t * ) );
VLC_EXPORT( aout_buffer_t *, aout_BufferNew, ( aout_instance_t *, aout_input_t *, size_t ) );
VLC_EXPORT( void, aout_BufferDelete, ( aout_instance_t *, aout_input_t *, aout_buffer_t * ) );
VLC_EXPORT( void, aout_BufferPlay, ( aout_instance_t *, aout_input_t *, aout_buffer_t * ) );

/* From input.c : */
#define aout_InputNew(a,b,c) __aout_InputNew(VLC_OBJECT(a),b,c)
VLC_EXPORT( aout_input_t *, __aout_InputNew, ( vlc_object_t *, aout_instance_t **, audio_sample_format_t * ) );
VLC_EXPORT( void, aout_InputDelete, ( aout_instance_t *, aout_input_t * ) );

/* From output.c : */
VLC_EXPORT( aout_buffer_t *, aout_OutputNextBuffer, ( aout_instance_t *, mtime_t ) );


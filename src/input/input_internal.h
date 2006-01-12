/*****************************************************************************
 * input_internal.h:
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _INPUT_INTERNAL_H
#define _INPUT_INTERNAL_H 1

enum input_control_e
{
    INPUT_CONTROL_SET_DIE,

    INPUT_CONTROL_SET_STATE,

    INPUT_CONTROL_SET_RATE,
    INPUT_CONTROL_SET_RATE_SLOWER,
    INPUT_CONTROL_SET_RATE_FASTER,

    INPUT_CONTROL_SET_POSITION,
    INPUT_CONTROL_SET_POSITION_OFFSET,

    INPUT_CONTROL_SET_TIME,
    INPUT_CONTROL_SET_TIME_OFFSET,

    INPUT_CONTROL_SET_PROGRAM,

    INPUT_CONTROL_SET_TITLE,
    INPUT_CONTROL_SET_TITLE_NEXT,
    INPUT_CONTROL_SET_TITLE_PREV,

    INPUT_CONTROL_SET_SEEKPOINT,
    INPUT_CONTROL_SET_SEEKPOINT_NEXT,
    INPUT_CONTROL_SET_SEEKPOINT_PREV,

    INPUT_CONTROL_SET_BOOKMARK,

    INPUT_CONTROL_SET_ES,

    INPUT_CONTROL_SET_AUDIO_DELAY,
    INPUT_CONTROL_SET_SPU_DELAY,

    INPUT_CONTROL_ADD_SLAVE,
};

/* Internal helpers */
static inline void input_ControlPush( input_thread_t *p_input,
                                      int i_type, vlc_value_t *p_val )
{
    vlc_mutex_lock( &p_input->lock_control );
    if( i_type == INPUT_CONTROL_SET_DIE )
    {
        /* Special case, empty the control */
        p_input->i_control = 1;
        p_input->control[0].i_type = i_type;
        memset( &p_input->control[0].val, 0, sizeof( vlc_value_t ) );
    }
    else
    {
        if( p_input->i_control >= INPUT_CONTROL_FIFO_SIZE )
        {
            msg_Err( p_input, "input control fifo overflow, trashing type=%d",
                     i_type );
            vlc_mutex_unlock( &p_input->lock_control );
            return;
        }
        p_input->control[p_input->i_control].i_type = i_type;
        if( p_val )
            p_input->control[p_input->i_control].val = *p_val;
        else
            memset( &p_input->control[p_input->i_control].val, 0,
                    sizeof( vlc_value_t ) );

        p_input->i_control++;
    }
    vlc_mutex_unlock( &p_input->lock_control );
}

/* var.c */
void input_ControlVarInit ( input_thread_t * );
void input_ControlVarClean( input_thread_t * );
void input_ControlVarNavigation( input_thread_t * );
void input_ControlVarTitle( input_thread_t *, int i_title );

void input_ConfigVarInit ( input_thread_t * );

/* stream.c */
stream_t *stream_AccessNew( access_t *p_access, vlc_bool_t );
void stream_AccessDelete( stream_t *s );
void stream_AccessReset( stream_t *s );
void stream_AccessUpdate( stream_t *s );

/* decoder.c FIXME make it public ?*/
void       input_DecoderDiscontinuity( decoder_t * p_dec );
vlc_bool_t input_DecoderEmpty( decoder_t * p_dec );
void       input_DecoderPreroll( decoder_t *p_dec, int64_t i_preroll_end );

/* es_out.c */
es_out_t  *input_EsOutNew( input_thread_t * );
void       input_EsOutDelete( es_out_t * );
es_out_id_t *input_EsOutGetFromID( es_out_t *, int i_id );
void       input_EsOutDiscontinuity( es_out_t *, vlc_bool_t b_audio );
void       input_EsOutSetDelay( es_out_t *, int i_cat, int64_t );
vlc_bool_t input_EsOutDecodersEmpty( es_out_t * );

/* clock.c */
enum /* Synchro states */
{
    SYNCHRO_OK     = 0,
    SYNCHRO_START  = 1,
    SYNCHRO_REINIT = 2,
};

typedef struct
{
    /* Synchronization information */
    mtime_t                 delta_cr;
    mtime_t                 cr_ref, sysdate_ref;
    mtime_t                 last_sysdate;
    mtime_t                 last_cr; /* reference to detect unexpected stream
                                      * discontinuities                      */
    mtime_t                 last_pts;
    int                     i_synchro_state;

    vlc_bool_t              b_master;

    /* Config */
    int                     i_cr_average;
    int                     i_delta_cr_residue;
} input_clock_t;

void input_ClockInit( input_clock_t *, vlc_bool_t b_master, int i_cr_average );
void    input_ClockSetPCR( input_thread_t *, input_clock_t *, mtime_t );
mtime_t input_ClockGetTS( input_thread_t *, input_clock_t *, mtime_t );

/* Subtitles */
char **subtitles_Detect( input_thread_t *, char* path, char *fname );
void MRLSplit( vlc_object_t *, char *, char **, char **, char ** );

#endif

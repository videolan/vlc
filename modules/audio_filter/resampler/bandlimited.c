/*****************************************************************************
 * bandlimited.c : band-limited interpolation resampler
 *****************************************************************************
 * Copyright (C) 2002 the VideoLAN team
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
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
 * Preamble:
 *
 * This implementation of the band-limited interpolationis based on the
 * following paper:
 * http://ccrma-www.stanford.edu/~jos/resample/resample.html
 *
 * It uses a Kaiser-windowed sinc-function low-pass filter and the width of the
 * filter is 13 samples.
 *
 *****************************************************************************/
#include <stdlib.h>                                      /* malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>
#include "audio_output.h"
#include "aout_internal.h"
#include "bandlimited.h"

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Close     ( vlc_object_t * );
static void DoWork    ( aout_instance_t *, aout_filter_t *, aout_buffer_t *,
                        aout_buffer_t * );

static void FilterFloatUP( float Imp[], float ImpD[], uint16_t Nwing,
                           float *f_in, float *f_out, uint32_t ui_remainder,
                           uint32_t ui_output_rate, int16_t Inc,
                           int i_nb_channels );

static void FilterFloatUD( float Imp[], float ImpD[], uint16_t Nwing,
                           float *f_in, float *f_out, uint32_t ui_remainder,
                           uint32_t ui_output_rate, uint32_t ui_input_rate,
                           int16_t Inc, int i_nb_channels );

/*****************************************************************************
 * Local structures
 *****************************************************************************/
struct aout_filter_sys_t
{
    int32_t *p_buf;                        /* this filter introduces a delay */
    int i_buf_size;

    int i_old_rate;
    double d_old_factor;
    int i_old_wing;

    unsigned int i_remainder;                /* remainder of previous sample */

    audio_date_t end_date;
};

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_category( CAT_AUDIO );
    set_subcategory( SUBCAT_AUDIO_MISC );
    set_description( _("audio filter for band-limited interpolation resampling") );
    set_capability( "audio filter", 20 );
    set_callbacks( Create, Close );
vlc_module_end();

/*****************************************************************************
 * Create: allocate linear resampler
 *****************************************************************************/
static int Create( vlc_object_t *p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    double d_factor;
    int i_filter_wing;

    if ( p_filter->input.i_rate == p_filter->output.i_rate
          || p_filter->input.i_format != p_filter->output.i_format
          || p_filter->input.i_physical_channels
              != p_filter->output.i_physical_channels
          || p_filter->input.i_original_channels
              != p_filter->output.i_original_channels
          || p_filter->input.i_format != VLC_FOURCC('f','l','3','2') )
    {
        return VLC_EGENERIC;
    }

#if !defined( SYS_DARWIN )
    if( !config_GetInt( p_this, "hq-resampling" ) )
    {
        return VLC_EGENERIC;
    }
#endif

    /* Allocate the memory needed to store the module's structure */
    p_filter->p_sys = malloc( sizeof(struct aout_filter_sys_t) );
    if( p_filter->p_sys == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    /* Calculate worst case for the length of the filter wing */
    d_factor = (double)p_filter->output.i_rate
                        / p_filter->input.i_rate;

    if( d_factor < (double)1.0 )
    {
        return VLC_EGENERIC;
    }

    i_filter_wing = ((SMALL_FILTER_NMULT + 1)/2.0)
                      * __MAX(1.0, 1.0/d_factor) + 10;
    p_filter->p_sys->i_buf_size = aout_FormatNbChannels( &p_filter->input ) *
        sizeof(int32_t) * 2 * i_filter_wing;

    /* Allocate enough memory to buffer previous samples */
    p_filter->p_sys->p_buf = malloc( p_filter->p_sys->i_buf_size );
    if( p_filter->p_sys->p_buf == NULL )
    {
        msg_Err( p_filter, "out of memory" );
        return VLC_ENOMEM;
    }

    p_filter->p_sys->i_old_wing = 0;
    p_filter->pf_do_work = DoWork;

    /* We don't want a new buffer to be created because we're not sure we'll
     * actually need to resample anything. */
    p_filter->b_in_place = VLC_TRUE;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: free our resources
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_filter_t * p_filter = (aout_filter_t *)p_this;
    free( p_filter->p_sys->p_buf );
    free( p_filter->p_sys );
}

/*****************************************************************************
 * DoWork: convert a buffer
 *****************************************************************************/
static void DoWork( aout_instance_t * p_aout, aout_filter_t * p_filter,
                    aout_buffer_t * p_in_buf, aout_buffer_t * p_out_buf )
{
    float *p_in, *p_in_orig, *p_out = (float *)p_out_buf->p_buffer;

    int i_nb_channels = aout_FormatNbChannels( &p_filter->input );
    int i_in_nb = p_in_buf->i_nb_samples;
    int i_in, i_out = 0;
    double d_factor, d_scale_factor, d_old_scale_factor;
    int i_filter_wing;
#if 0
    int i;
#endif

    /* Check if we really need to run the resampler */
    if( p_aout->mixer.mixer.i_rate == p_filter->input.i_rate )
    {
        if( //p_filter->b_continuity && /* What difference does it make ? :) */
            p_filter->p_sys->i_old_wing &&
            p_in_buf->i_size >=
              p_in_buf->i_nb_bytes + p_filter->p_sys->i_old_wing *
              p_filter->input.i_bytes_per_frame )
        {
            /* output the whole thing with the samples from last time */
            memmove( ((float *)(p_in_buf->p_buffer)) +
                     i_nb_channels * p_filter->p_sys->i_old_wing,
                     p_in_buf->p_buffer, p_in_buf->i_nb_bytes );
            memcpy( p_in_buf->p_buffer, p_filter->p_sys->p_buf +
                    i_nb_channels * p_filter->p_sys->i_old_wing,
                    p_filter->p_sys->i_old_wing *
                    p_filter->input.i_bytes_per_frame );

            p_out_buf->i_nb_samples = p_in_buf->i_nb_samples +
                p_filter->p_sys->i_old_wing;

            p_out_buf->start_date = aout_DateGet( &p_filter->p_sys->end_date );
            p_out_buf->end_date =
                aout_DateIncrement( &p_filter->p_sys->end_date,
                                    p_out_buf->i_nb_samples );

            p_out_buf->i_nb_bytes = p_out_buf->i_nb_samples *
                p_filter->input.i_bytes_per_frame;
        }
        p_filter->b_continuity = VLC_FALSE;
        p_filter->p_sys->i_old_wing = 0;
        return;
    }

    if( !p_filter->b_continuity )
    {
        /* Continuity in sound samples has been broken, we'd better reset
         * everything. */
        p_filter->b_continuity = VLC_TRUE;
        p_filter->p_sys->i_remainder = 0;
        aout_DateInit( &p_filter->p_sys->end_date, p_filter->output.i_rate );
        aout_DateSet( &p_filter->p_sys->end_date, p_in_buf->start_date );
        p_filter->p_sys->i_old_rate   = p_filter->input.i_rate;
        p_filter->p_sys->d_old_factor = 1;
        p_filter->p_sys->i_old_wing   = 0;
    }

#if 0
    msg_Err( p_filter, "old rate: %i, old factor: %f, old wing: %i, i_in: %i",
             p_filter->p_sys->i_old_rate, p_filter->p_sys->d_old_factor,
             p_filter->p_sys->i_old_wing, i_in_nb );
#endif

    /* Prepare the source buffer */
    i_in_nb += (p_filter->p_sys->i_old_wing * 2);
#ifdef HAVE_ALLOCA
    p_in = p_in_orig = (float *)alloca( i_in_nb *
                                        p_filter->input.i_bytes_per_frame );
#else
    p_in = p_in_orig = (float *)malloc( i_in_nb *
                                        p_filter->input.i_bytes_per_frame );
#endif
    if( p_in == NULL )
    {
        return;
    }

    /* Copy all our samples in p_in */
    if( p_filter->p_sys->i_old_wing )
    {
        p_aout->p_vlc->pf_memcpy( p_in, p_filter->p_sys->p_buf,
                                  p_filter->p_sys->i_old_wing * 2 *
                                  p_filter->input.i_bytes_per_frame );
    }
    p_aout->p_vlc->pf_memcpy( p_in + p_filter->p_sys->i_old_wing * 2 *
                              i_nb_channels, p_in_buf->p_buffer,
                              p_in_buf->i_nb_samples *
                              p_filter->input.i_bytes_per_frame );

    /* Make sure the output buffer is reset */
    memset( p_out, 0, p_out_buf->i_size );

    /* Calculate the new length of the filter wing */
    d_factor = (double)p_aout->mixer.mixer.i_rate / p_filter->input.i_rate;
    i_filter_wing = ((SMALL_FILTER_NMULT+1)/2.0) * __MAX(1.0,1.0/d_factor) + 1;

    /* Account for increased filter gain when using factors less than 1 */
    d_old_scale_factor = SMALL_FILTER_SCALE *
        p_filter->p_sys->d_old_factor + 0.5;
    d_scale_factor = SMALL_FILTER_SCALE * d_factor + 0.5;

    /* Apply the old rate until we have enough samples for the new one */
    i_in = p_filter->p_sys->i_old_wing;
    p_in += p_filter->p_sys->i_old_wing * i_nb_channels;
    for( ; i_in < i_filter_wing &&
           (i_in + p_filter->p_sys->i_old_wing) < i_in_nb; i_in++ )
    {
        if( p_filter->p_sys->d_old_factor == 1 )
        {
            /* Just copy the samples */
            memcpy( p_out, p_in, 
                    p_filter->input.i_bytes_per_frame );          
            p_in += i_nb_channels;
            p_out += i_nb_channels;
            i_out++;
            continue;
        }

        while( p_filter->p_sys->i_remainder < p_filter->output.i_rate )
        {

            if( p_filter->p_sys->d_old_factor >= 1 )
            {
                /* FilterFloatUP() is faster if we can use it */

                /* Perform left-wing inner product */
                FilterFloatUP( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in, p_out,
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate,
                               -1, i_nb_channels );
                /* Perform right-wing inner product */
                FilterFloatUP( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in + i_nb_channels, p_out,
                               p_filter->output.i_rate -
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate,
                               1, i_nb_channels );

#if 0
                /* Normalize for unity filter gain */
                for( i = 0; i < i_nb_channels; i++ )
                {
                    *(p_out+i) *= d_old_scale_factor;
                }
#endif

                /* Sanity check */
                if( p_out_buf->i_size/p_filter->input.i_bytes_per_frame
                    <= (unsigned int)i_out+1 )
                {
                    p_out += i_nb_channels;
                    i_out++;
                    p_filter->p_sys->i_remainder += p_filter->input.i_rate;
                    break;
                }
            }
            else
            {
                /* Perform left-wing inner product */
                FilterFloatUD( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in, p_out,
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate, p_filter->input.i_rate,
                               -1, i_nb_channels );
                /* Perform right-wing inner product */
                FilterFloatUD( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in + i_nb_channels, p_out,
                               p_filter->output.i_rate -
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate, p_filter->input.i_rate,
                               1, i_nb_channels );
            }

            p_out += i_nb_channels;
            i_out++;

            p_filter->p_sys->i_remainder += p_filter->input.i_rate;
        }

        p_in += i_nb_channels;
        p_filter->p_sys->i_remainder -= p_filter->output.i_rate;
    }

    /* Apply the new rate for the rest of the samples */
    if( i_in < i_in_nb - i_filter_wing )
    {
        p_filter->p_sys->i_old_rate   = p_filter->input.i_rate;
        p_filter->p_sys->d_old_factor = d_factor;
        p_filter->p_sys->i_old_wing   = i_filter_wing;
    }
    for( ; i_in < i_in_nb - i_filter_wing; i_in++ )
    {
        while( p_filter->p_sys->i_remainder < p_filter->output.i_rate )
        {

            if( d_factor >= 1 )
            {
                /* FilterFloatUP() is faster if we can use it */

                /* Perform left-wing inner product */
                FilterFloatUP( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in, p_out,
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate,
                               -1, i_nb_channels );

                /* Perform right-wing inner product */
                FilterFloatUP( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in + i_nb_channels, p_out,
                               p_filter->output.i_rate -
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate,
                               1, i_nb_channels );

#if 0
                /* Normalize for unity filter gain */
                for( i = 0; i < i_nb_channels; i++ )
                {
                    *(p_out+i) *= d_old_scale_factor;
                }
#endif
                /* Sanity check */
                if( p_out_buf->i_size/p_filter->input.i_bytes_per_frame
                    <= (unsigned int)i_out+1 )
                {
                    p_out += i_nb_channels;
                    i_out++;
                    p_filter->p_sys->i_remainder += p_filter->input.i_rate;
                    break;
                }
            }
            else
            {
                /* Perform left-wing inner product */
                FilterFloatUD( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in, p_out,
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate, p_filter->input.i_rate,
                               -1, i_nb_channels );
                /* Perform right-wing inner product */
                FilterFloatUD( SMALL_FILTER_FLOAT_IMP, SMALL_FILTER_FLOAT_IMPD,
                               SMALL_FILTER_NWING, p_in + i_nb_channels, p_out,
                               p_filter->output.i_rate -
                               p_filter->p_sys->i_remainder,
                               p_filter->output.i_rate, p_filter->input.i_rate,
                               1, i_nb_channels );
            }

            p_out += i_nb_channels;
            i_out++;

            p_filter->p_sys->i_remainder += p_filter->input.i_rate;
        }

        p_in += i_nb_channels;
        p_filter->p_sys->i_remainder -= p_filter->output.i_rate;
    }

    /* Buffer i_filter_wing * 2 samples for next time */
    if( p_filter->p_sys->i_old_wing )
    {
        memcpy( p_filter->p_sys->p_buf,
                p_in_orig + (i_in_nb - 2 * p_filter->p_sys->i_old_wing) *
                i_nb_channels, (2 * p_filter->p_sys->i_old_wing) *
                p_filter->input.i_bytes_per_frame );
    }

#if 0
    msg_Err( p_filter, "p_out size: %i, nb bytes out: %i", p_out_buf->i_size,
             i_out * p_filter->input.i_bytes_per_frame );
#endif

    /* Free the temp buffer */
#ifndef HAVE_ALLOCA
    free( p_in_orig );
#endif

    /* Finalize aout buffer */
    p_out_buf->i_nb_samples = i_out;
    p_out_buf->start_date = aout_DateGet( &p_filter->p_sys->end_date );
    p_out_buf->end_date = aout_DateIncrement( &p_filter->p_sys->end_date,
                                              p_out_buf->i_nb_samples );

    p_out_buf->i_nb_bytes = p_out_buf->i_nb_samples *
        i_nb_channels * sizeof(int32_t);

}

void FilterFloatUP( float Imp[], float ImpD[], uint16_t Nwing, float *p_in,
                    float *p_out, uint32_t ui_remainder,
                    uint32_t ui_output_rate, int16_t Inc, int i_nb_channels )
{
    float *Hp, *Hdp, *End;
    float t, temp;
    uint32_t ui_linear_remainder;
    int i;

    Hp = &Imp[(ui_remainder<<Nhc)/ui_output_rate];
    Hdp = &ImpD[(ui_remainder<<Nhc)/ui_output_rate];

    End = &Imp[Nwing];

    ui_linear_remainder = (ui_remainder<<Nhc) -
                            (ui_remainder<<Nhc)/ui_output_rate*ui_output_rate;

    if (Inc == 1)               /* If doing right wing...              */
    {                           /* ...drop extra coeff, so when Ph is  */
        End--;                  /*    0.5, we don't do too many mult's */
        if (ui_remainder == 0)  /* If the phase is zero...           */
        {                       /* ...then we've already skipped the */
            Hp += Npc;          /*    first sample, so we must also  */
            Hdp += Npc;         /*    skip ahead in Imp[] and ImpD[] */
        }
    }

    while (Hp < End) {
        t = *Hp;                /* Get filter coeff */
                                /* t is now interp'd filter coeff */
        t += *Hdp * ui_linear_remainder / ui_output_rate / Npc;
        for( i = 0; i < i_nb_channels; i++ )
        {
            temp = t;
            temp *= *(p_in+i);  /* Mult coeff by input sample */
            *(p_out+i) += temp; /* The filter output */
        }
        Hdp += Npc;             /* Filter coeff differences step */
        Hp += Npc;              /* Filter coeff step */
        p_in += (Inc * i_nb_channels); /* Input signal step */
    }
}

void FilterFloatUD( float Imp[], float ImpD[], uint16_t Nwing, float *p_in,
                    float *p_out, uint32_t ui_remainder,
                    uint32_t ui_output_rate, uint32_t ui_input_rate,
                    int16_t Inc, int i_nb_channels )
{
    float *Hp, *Hdp, *End;
    float t, temp;
    uint32_t ui_linear_remainder;
    int i, ui_counter = 0;

    Hp = Imp + (ui_remainder<<Nhc) / ui_input_rate;
    Hdp = ImpD  + (ui_remainder<<Nhc) / ui_input_rate;

    End = &Imp[Nwing];

    if (Inc == 1)               /* If doing right wing...              */
    {                           /* ...drop extra coeff, so when Ph is  */
        End--;                  /*    0.5, we don't do too many mult's */
        if (ui_remainder == 0)  /* If the phase is zero...           */
        {                       /* ...then we've already skipped the */
            Hp = Imp +          /* first sample, so we must also  */
                  (ui_output_rate << Nhc) / ui_input_rate;
            Hdp = ImpD +        /* skip ahead in Imp[] and ImpD[] */
                  (ui_output_rate << Nhc) / ui_input_rate;
            ui_counter++;
        }
    }

    while (Hp < End) {
        t = *Hp;                /* Get filter coeff */
                                /* t is now interp'd filter coeff */
        ui_linear_remainder =
          ((ui_output_rate * ui_counter + ui_remainder)<< Nhc) -
          ((ui_output_rate * ui_counter + ui_remainder)<< Nhc) /
          ui_input_rate * ui_input_rate;
        t += *Hdp * ui_linear_remainder / ui_input_rate / Npc;
        for( i = 0; i < i_nb_channels; i++ )
        {
            temp = t;
            temp *= *(p_in+i);  /* Mult coeff by input sample */
            *(p_out+i) += temp; /* The filter output */
        }

        ui_counter++;

        /* Filter coeff step */
        Hp = Imp + ((ui_output_rate * ui_counter + ui_remainder)<< Nhc)
                    / ui_input_rate;
        /* Filter coeff differences step */
        Hdp = ImpD + ((ui_output_rate * ui_counter + ui_remainder)<< Nhc)
                     / ui_input_rate;

        p_in += (Inc * i_nb_channels); /* Input signal step */
    }
}

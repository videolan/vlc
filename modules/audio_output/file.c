/*****************************************************************************
 * file.c : audio output which writes the samples to a file
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: file.c,v 1.7 2002/08/19 23:07:30 sam Exp $
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
 * Preamble
 *****************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <vlc/vlc.h>
#include <vlc/aout.h>

#include "aout_internal.h"

#define FRAME_SIZE 2048
#define A52_FRAME_NB 1536

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int     Open        ( vlc_object_t * );
static void    Close       ( vlc_object_t * );
static int     SetFormat   ( aout_instance_t * );
static void    Play        ( aout_instance_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define FORMAT_TEXT N_("output format")
#define FORMAT_LONGTEXT N_("one of \"u8\", \"s8\", \"u16\", \"s16\"," \
                           " \"u16_le\", \"s16_le\", \"u16_be\"," \
                           " \"s16_be\", \"fixed32\", \"float32\" or \"spdif\"")

static char *format_list[] = { "u8", "s8", "u16", "s16", "u16_le", "s16_le",
                               "u16_be", "s16_be", "fixed32", "float32",
                               "spdif", NULL };
static int format_int[] = { AOUT_FMT_U8, AOUT_FMT_S8, AOUT_FMT_U16_NE,
                            AOUT_FMT_S16_NE, AOUT_FMT_U16_LE, AOUT_FMT_S16_LE,
                            AOUT_FMT_U16_BE, AOUT_FMT_S16_BE, AOUT_FMT_FIXED32,
                            AOUT_FMT_FLOAT32, AOUT_FMT_SPDIF };

#define PATH_TEXT N_("path of the output file")
#define PATH_LONGTEXT N_("By default samples.raw")

vlc_module_begin();
    add_category_hint( N_("Audio"), NULL );
    add_string_from_list( "format", "s16", format_list, NULL,
                          FORMAT_TEXT, FORMAT_LONGTEXT );
    add_string( "path", "samples.raw", NULL, PATH_TEXT, PATH_LONGTEXT );
    set_description( _("file output module") );
    set_capability( "audio output", 0 );
    add_shortcut( "file" );
    set_callbacks( Open, Close );
vlc_module_end();

/*****************************************************************************
 * Open: open a dummy audio device
 *****************************************************************************/
static int Open( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;
    FILE * p_file;
    char * psz_name = config_GetPsz( p_this, "path" );

    p_file = fopen( psz_name, "wb" );
    p_aout->output.p_sys = (void *)p_file;
    free( psz_name );
    if ( p_file == NULL ) return -1;

    p_aout->output.pf_setformat = SetFormat;
    p_aout->output.pf_play = Play;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close: close our file
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    fclose( (FILE *)p_aout->output.p_sys );
}

/*****************************************************************************
 * SetFormat: pretend to set the dsp output format
 *****************************************************************************/
static int SetFormat( aout_instance_t * p_aout )
{
    char * psz_format = config_GetPsz( p_aout, "format" );
    char ** ppsz_compare = format_list;
    int i = 0;

    while ( *ppsz_compare != NULL )
    {
        if ( !strncmp( *ppsz_compare, psz_format, strlen(*ppsz_compare) ) )
        {
            break;
        }
        ppsz_compare++; i++;
    }

    if ( *ppsz_compare == NULL )
    {
        msg_Err( p_aout, "Cannot understand the format string (%s)",
                 psz_format );
        return -1;
    }

    p_aout->output.output.i_format = format_int[i];
    if ( p_aout->output.output.i_format == AOUT_FMT_SPDIF )
    {
        p_aout->output.i_nb_samples = A52_FRAME_NB;
        p_aout->output.output.i_bytes_per_frame = AOUT_SPDIF_SIZE;
        p_aout->output.output.i_frame_length = A52_FRAME_NB;
    }
    else
    {
        p_aout->output.i_nb_samples = FRAME_SIZE;
    }
    return 0;
}

/*****************************************************************************
 * Play: pretend to play a sound
 *****************************************************************************/
static void Play( aout_instance_t * p_aout )
{
    aout_buffer_t * p_buffer = aout_FifoPop( p_aout, &p_aout->output.fifo );
    if( fwrite( p_buffer->p_buffer, p_buffer->i_nb_bytes, 1,
                (FILE *)p_aout->output.p_sys ) != 1 )
    {
        msg_Err( p_aout, "write error (%s)", strerror(errno) );
    }

    aout_BufferFree( p_buffer );
}


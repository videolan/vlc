/*****************************************************************************
 * adec_test.c: MPEG Layer I-II audio decoder test program
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 * $Id: adec_test.c,v 1.1 2001/11/13 12:09:18 henri Exp $
 *
 * Authors: Michel Kaempf <maxx@via.ecp.fr>
 *          Michel Lespinasse <walken@via.ecp.fr>
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
 * TODO :
 *
 * - optimiser les NeedBits() et les GetBits() du code là où c'est possible ;
 * - vlc_cond_signal() / vlc_cond_wait() ;
 *
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "int_types.h"
#include "mpeg_adec_generic.h"
#include "mpeg_adec.h"

#define ADEC_FRAME_SIZE (2*1152)

int main (void)
{
    audiodec_t decoder;
    adec_sync_info_t sync_info;
    adec_byte_stream_t * stream;
    s16 buffer [ADEC_FRAME_SIZE];

    int framenum;

    memset (&decoder, 0, sizeof (decoder));
    if (adec_init (&decoder))
    {
        return 1;
    }

    stream = adec_byte_stream (&decoder);
    stream->p_byte = NULL;
    stream->p_end = NULL;
    stream->info = stdin;

    framenum = 0;

    while (1)
    {
        int i;

        if (adec_sync_frame (&decoder, &sync_info))
        {
            return 1;
        }
        if (adec_decode_frame (&decoder, buffer))
        {
            return 1;
        }

#if 1
        for (i = 0; i < (2*1152); i++)
        {
            fprintf ( stderr, "%04X\n",(u16)buffer[i] );
        }
#endif
    }

    return 0;
}

void adec_byte_stream_next (adec_byte_stream_t * p_byte_stream)
{
    static u8 buffer [1024];
    static u8 dummy = 0;
    FILE * fd;
    int size;

    fd = p_byte_stream->info;
    size = fread (buffer, 1, 1024, fd);
    if (size)
    {
        p_byte_stream->p_byte = buffer;
        p_byte_stream->p_end = buffer + size;
    }
    else
    {   /* end of stream, read dummy zeroes */
        p_byte_stream->p_byte = &dummy;
        p_byte_stream->p_end = &dummy + 1;
    }
}

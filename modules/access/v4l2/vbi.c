/*****************************************************************************
 * vbi.c : Video4Linux2 VBI input module for vlc
 *****************************************************************************
 * Copyright (C) 2012 the VideoLAN team
 *
 * Author: Devin Heitmueller <dheitmueller at kernellabs dot com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_fs.h>
#include <vlc_demux.h>

#include "v4l2.h"

#ifdef ZVBI_COMPILED

vbi_capture* OpenVBIDev( vlc_object_t *p_obj, const char *psz_device)
{
    vbi_capture *cap;
    char* errstr = NULL;

    //Can put more in here. See osd.c in zvbi package.
    unsigned int services = VBI_SLICED_CAPTION_525;

    int rawfd = vlc_open (psz_device, O_RDWR);
    if (rawfd == -1)
    {
        msg_Err ( p_obj, "cannot open device '%s': %m", psz_device );
        return NULL;
    }

    cap = vbi_capture_v4l2k_new (psz_device,
                                 rawfd,
                                 /* buffers */ 5,
                                 &services,
                                 /* strict */ 1,
                                 &errstr,
                                 /* verbose */ 1);
    if (cap == NULL)
    {
        msg_Err( p_obj, "Cannot capture vbi data with v4l2 interface (%s)",
                 errstr );
        free(errstr);
    }

    return cap;
}

void GrabVBI( demux_t *p_demux, vbi_capture *vbi_cap,
              es_out_id_t **p_es_subt, int num_streams)
{
    block_t     *p_block=NULL;
    vbi_capture_buffer *sliced_bytes;
    struct timeval timeout={0,0}; /* poll */
    int n_lines;
    int canc = vlc_savecancel ();

    int r = vbi_capture_pull_sliced (vbi_cap, &sliced_bytes, &timeout);
    switch (r) {
        case -1:
            msg_Err( p_demux, "Error reading VBI (%m)" );
            break;
        case  0: /* nothing avail */
            break;
        case  1: /* got data */
            n_lines = sliced_bytes->size / sizeof(vbi_sliced);
            if (n_lines)
            {
                int sliced_size = 2; /* Number of bytes per sliced line */

                int size = (sliced_size + 1) * n_lines;
                if( !( p_block = block_Alloc( size ) ) )
                {
                    msg_Err( p_demux, "cannot get block" );
                }
                else
                {
                    int field;
                    uint8_t* data = p_block->p_buffer;
                    vbi_sliced *sliced_array = sliced_bytes->data;
                    for(field=0; field<n_lines; field++)
                    {
                        *data = field;
                        data++;
                        memcpy(data, sliced_array[field].data, sliced_size);
                        data += sliced_size;
                    }
                    p_block->i_buffer = size;
                    p_block->i_pts = mdate();
                }
            }
    }

    if( p_block )
    {
        for (int i = 0; i < num_streams; i++)
        {
            block_t *p_sblock;
            if (p_es_subt[i] == NULL)
                continue;
            p_sblock = block_Duplicate(p_block);
            if (p_sblock)
                es_out_Send(p_demux->out, p_es_subt[i], p_sblock);
        }
        block_Release(p_block);
    }

    vlc_restorecancel (canc);
}
#endif

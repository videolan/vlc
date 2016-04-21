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
# include <libzvbi.h>
# define VBI_NUM_CC_STREAMS 4

struct vlc_v4l2_vbi
{
    vbi_capture *cap;
    es_out_id_t *es[VBI_NUM_CC_STREAMS];
};

vlc_v4l2_vbi_t *OpenVBI (demux_t *demux, const char *psz_device)
{
    vlc_v4l2_vbi_t *vbi = malloc (sizeof (*vbi));
    if (unlikely(vbi == NULL))
        return NULL;

    int rawfd = vlc_open (psz_device, O_RDWR);
    if (rawfd == -1)
    {
        msg_Err (demux, "cannot open device '%s': %s", psz_device,
                 vlc_strerror_c(errno));
        goto err;
    }

    //Can put more in here. See osd.c in zvbi package.
    unsigned int services = VBI_SLICED_CAPTION_525;
    char *errstr = NULL;

    vbi->cap = vbi_capture_v4l2k_new (psz_device, rawfd,
                                      /* buffers */ 5,
                                      &services,
                                      /* strict */ 1,
                                      &errstr,
                                      /* verbose */ 1);
    if (vbi->cap == NULL)
    {
        msg_Err (demux, "cannot capture VBI data: %s", errstr);
        free (errstr);
        vlc_close (rawfd);
        goto err;
    }

    for (unsigned i = 0; i < VBI_NUM_CC_STREAMS; i++)
    {
        es_format_t fmt;

        es_format_Init (&fmt, SPU_ES, VLC_FOURCC('c', 'c', '1' + i, ' '));
        if (asprintf (&fmt.psz_description, "Closed captions %d", i + 1) >= 0)
        {
            msg_Dbg (demux, "new spu es %4.4s", (char *)&fmt.i_codec);
            vbi->es[i] = es_out_Add (demux->out, &fmt);
        }
    }

    /* Do a single read and throw away the results so that ZVBI calls
       the STREAMON ioctl() */
    GrabVBI(demux, vbi);

    return vbi;
err:
    free (vbi);
    return NULL;
}

int GetFdVBI (vlc_v4l2_vbi_t *vbi)
{
    return vbi_capture_fd(vbi->cap);
}

void GrabVBI (demux_t *p_demux, vlc_v4l2_vbi_t *vbi)
{
    vbi_capture_buffer *sliced_bytes;
    struct timeval timeout={0,0}; /* poll */
    int canc = vlc_savecancel ();

    int r = vbi_capture_pull_sliced (vbi->cap, &sliced_bytes, &timeout);
    switch (r) {
        case -1:
            msg_Err (p_demux, "error reading VBI: %s", vlc_strerror_c(errno));
        case  0: /* nothing avail */
            break;
        case  1: /* got data */
        {
            int n_lines = sliced_bytes->size / sizeof(vbi_sliced);
            if (!n_lines)
                break;

            int sliced_size = 2; /* Number of bytes per sliced line */
            int size = (sliced_size + 1) * n_lines;
            block_t *p_block = block_Alloc (size);
            if (unlikely(p_block == NULL))
                break;

            uint8_t* data = p_block->p_buffer;
            vbi_sliced *sliced_array = sliced_bytes->data;
            for (int field = 0; field < n_lines; field++)
            {
                *data = field;
                data++;
                memcpy(data, sliced_array[field].data, sliced_size);
                data += sliced_size;
            }
            p_block->i_pts = mdate();

            for (unsigned i = 0; i < VBI_NUM_CC_STREAMS; i++)
            {
                if (vbi->es[i] == NULL)
                    continue;

                block_t *dup = block_Duplicate(p_block);
                if (likely(dup != NULL))
                    es_out_Send(p_demux->out, vbi->es[i], dup);
            }
            block_Release(p_block);
        }
    }
    vlc_restorecancel (canc);
}

void CloseVBI (vlc_v4l2_vbi_t *vbi)
{
    vlc_close (vbi_capture_fd (vbi->cap));
    vbi_capture_delete (vbi->cap);
    free (vbi);
}
#endif

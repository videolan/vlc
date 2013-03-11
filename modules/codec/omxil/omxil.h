/*****************************************************************************
 * omxil.h: helper functions
 *****************************************************************************
 * Copyright (C) 2010 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef RPI_OMX
#define OMX_SKIP64BIT
#endif

/*****************************************************************************
 * Includes
 *****************************************************************************/
#include "OMX_Core.h"
#include "OMX_Index.h"
#include "OMX_Component.h"
#include "OMX_Video.h"

#include "omxil_utils.h"
#include "omxil_core.h"

/*****************************************************************************
 * decoder_sys_t : omxil decoder descriptor
 *****************************************************************************/
typedef struct OmxPort
{
    bool b_valid;
    OMX_U32 i_port_index;
    OMX_HANDLETYPE omx_handle;
    OMX_PARAM_PORTDEFINITIONTYPE definition;
    es_format_t *p_fmt;

    unsigned int i_frame_size;
    unsigned int i_frame_stride;
    unsigned int i_frame_stride_chroma_div;

    unsigned int i_buffers;
    OMX_BUFFERHEADERTYPE **pp_buffers;

    struct fifo_t
    {
      vlc_mutex_t         lock;
      vlc_cond_t          wait;

      OMX_BUFFERHEADERTYPE *p_first;
      OMX_BUFFERHEADERTYPE **pp_last;

      int offset;

    } fifo;

    OmxFormatParam format_param;

    OMX_BOOL b_reconfigure;
    OMX_BOOL b_update_def;
    OMX_BOOL b_direct;
    OMX_BOOL b_flushed;

} OmxPort;

struct decoder_sys_t
{
    OMX_HANDLETYPE omx_handle;

    bool b_enc;

    char psz_component[OMX_MAX_STRINGNAME_SIZE];
    char ppsz_components[MAX_COMPONENTS_LIST_SIZE][OMX_MAX_STRINGNAME_SIZE];
    unsigned int components;

    OmxEventQueue event_queue;

    OmxPort *p_ports;
    unsigned int ports;
    OmxPort in;
    OmxPort out;

    bool b_error;

    date_t end_date;

    size_t i_nal_size_length; /* Length of the NAL size field for H264 */
    int b_use_pts;

};

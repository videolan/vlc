/*****************************************************************************
 * ts_metadata.c : TS demuxer metadata handling
 *****************************************************************************
 * Copyright (C) 2016 - VideoLAN Authors
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_meta.h>
#include <vlc_es_out.h>

#include "ts_metadata.h"
#include "../meta_engine/ID3Tag.h"
#include "../meta_engine/ID3Meta.h"

static int ID3TAG_Parse_Handler( uint32_t i_tag, const uint8_t *p_payload,
                                 size_t i_payload, void *p_priv )
{
    vlc_meta_t *p_meta = (vlc_meta_t *) p_priv;

    (void) ID3HandleTag( p_payload, i_payload, i_tag, p_meta, NULL );

    return VLC_SUCCESS;
}

void ProcessMetadata( es_out_t *out, uint32_t i_format, uint16_t i_program,
                      const uint8_t *p_buffer, size_t i_buffer )
{
    if( i_format == VLC_FOURCC('I', 'D', '3', ' ') )
    {
        vlc_meta_t *p_meta = vlc_meta_New();
        if( p_meta )
        {
            (void) ID3TAG_Parse( p_buffer, i_buffer, ID3TAG_Parse_Handler, p_meta );
            es_out_Control( out, ES_OUT_SET_GROUP_META, i_program, p_meta );
            vlc_meta_Delete( p_meta );
        }
    }
}

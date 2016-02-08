/*****************************************************************************
 * ts_psip_dvbpsi_fixes.c : TS demux Broken/missing dvbpsi PSIP handling
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

#ifndef _DVBPSI_DVBPSI_H_
 #include <dvbpsi/dvbpsi.h>
#endif
#include <dvbpsi/psi.h>
#include <dvbpsi/descriptor.h>
#include <dvbpsi/atsc_stt.h>

#include "ts_psip_dvbpsi_fixes.h"

#include "ts_psip.h"
#include "ts_decoders.h"

/* Our own STT Decoder since DVBPlague deduplicates tables for this fixed version table */
dvbpsi_atsc_stt_t * DVBPlague_STT_Decode( const dvbpsi_psi_section_t* p_section )
{
    size_t i_payload = p_section->p_payload_end - p_section->p_payload_start;
    if( i_payload >= 8 )
    {
        dvbpsi_atsc_stt_t *p_stt = dvbpsi_atsc_NewSTT( ATSC_STT_TABLE_ID, 0x00, 0x00, true );
        if(unlikely(!p_stt))
            return NULL;

        p_stt->i_system_time = GetDWBE( &p_section->p_payload_start[1] );
        p_stt->i_gps_utc_offset = p_section->p_payload_start[5];
        p_stt->i_daylight_savings = GetWBE( &p_section->p_payload_start[6] );

        return p_stt;
    }

    return NULL;
}

/*****************************************************************************
 * dvbpsi_compat.h: Compatibility headerfile
 *****************************************************************************
 * Copyright (C) 2013 VideoLAN Association
 *
 * Authors: Jean-Paul Saman <jpsaman@videolan.org>
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

#ifndef DVBPSI_COMPAT_H
#define DVBPSI_COMPAT_H

/*
 * dvbpsi compatibility macros:
 * dvbpsi version 1.0.0 and above returns a struct 'dvbpsi_t' as handle
 */
#define DVBPSI_VERSION_WANTED(major,minor,bugfix) (((major)<<16)+((minor)<<8)+(bugfix))

#if (DVBPSI_VERSION_INT >= DVBPSI_VERSION_WANTED(1,0,0))
# define dvbpsi_handle dvbpsi_t*
# define dvbpsi_PushPacket(handle,data) dvbpsi_packet_push((handle),(data))
/* PAT */
# define dvbpsi_InitPAT(pat,id,version,next) dvbpsi_pat_init((pat),(id),(version),(bool)(next))
# define dvbpsi_PATAddProgram(pat,nr,pid)    dvbpsi_pat_program_add((pat),(nr),(pid))
# define dvbpsi_EmptyPAT(pat)                dvbpsi_pat_empty((pat))
# define dvbpsi_DeletePAT(table)             dvbpsi_pat_delete((table))
# define dvbpsi_DetachPAT(pat)               dvbpsi_pat_detach((pat))
/* PMT */
# define dvbpsi_InitPMT(pmt,program,version,next,pcr) \
         dvbpsi_pmt_init((pmt),(program),(version),(bool)(next),(pcr))
# define dvbpsi_PMTAddDescriptor(pmt,tag,length,data) \
         dvbpsi_pmt_descriptor_add((pmt),(tag),(length),(data))
# define dvbpsi_PMTAddES(pmt,type,pid) \
         dvbpsi_pmt_es_add((pmt),(type),(pid))
# define dvbpsi_PMTESAddDescriptor(es,tag,length,data) \
         dvbpsi_pmt_es_descriptor_add((es),(tag),(length),(data))
# define dvbpsi_EmptyPMT(pmt) dvbpsi_pmt_empty((pmt))
# define dvbpsi_DeletePMT(table)        dvbpsi_pmt_delete((table))
# define dvbpsi_DetachPMT(pmt)          dvbpsi_pmt_detach((pmt))
/* SDT */
# define dvbpsi_InitSDT(sdt,id,version,curnext,netid) \
         dvbpsi_sdt_init((sdt),(id),(0),(version),(bool)(curnext),(netid))
# define dvbpsi_SDTAddService(sdt,id,schedule,present,status,ca) \
         dvbpsi_sdt_service_add((sdt),(id),(bool)(schedule),(bool)(present),(status),(bool)(ca))
# define dvbpsi_EmptySDT(sdt) dvbpsi_sdt_empty((sdt))
# define dvbpsi_DeleteSDT(table)        dvbpsi_sdt_delete((table))
/* TOT */
# define dvbpsi_DeleteTOT(table)        dvbpsi_tot_delete((table))
/* EIT */
# define dvbpsi_DeleteEIT(table)        dvbpsi_eit_delete((table))
/* NIT */
# define dvbpsi_DeleteNIT(table)        dvbpsi_nit_delete((table))

static void dvbpsi_messages(dvbpsi_t *p_dvbpsi, const dvbpsi_msg_level_t level, const char* msg)
{
    vlc_object_t *obj = (vlc_object_t *)p_dvbpsi->p_sys;

    /* See dvbpsi.h for the definition of these log levels.*/
    switch(level)
    {
        case DVBPSI_MSG_ERROR: msg_Err( obj, "%s", msg ); break;
        case DVBPSI_MSG_WARN:  msg_Warn( obj, "%s", msg ); break;
        case DVBPSI_MSG_DEBUG: msg_Dbg( obj, "%s", msg ); break;
        default: msg_Info( obj, "%s", msg ); break;
    }
}
#endif

#endif

/**
 * Copyright (C) 2000 Björn Englund <d4bjorn@dtek.chalmers.se>,
 *                    Håkan Hjort <d95hjort@dtek.chalmers.se>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef IFO_PRINT_H_INCLUDED
#define IFO_PRINT_H_INCLUDED

#include <dvdread/ifo_types.h>
#include <dvdread/dvd_reader.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This file provides example functions for printing information about the IFO
 * file to stdout.
 */

/**
 * Print the complete parsing information for the given file.
 */
void ifoPrint(dvd_reader_t *dvd, int title);

void ifoPrint_VMGI_MAT(vmgi_mat_t *vmgi_mat);
void ifoPrint_VTSI_MAT(vtsi_mat_t *vtsi_mat);

void ifoPrint_PTL_MAIT(ptl_mait_t *ptl_mait);
void ifoPrint_VTS_ATRT(vts_atrt_t *vts_atrt);
void ifoPrint_TT_SRPT(tt_srpt_t *vmg_ptt_srpt);
void ifoPrint_VTS_PTT_SRPT(vts_ptt_srpt_t *vts_ptt_srpt);
void ifoPrint_PGC(pgc_t *pgc);
void ifoPrint_PGCIT(pgcit_t *pgcit);
void ifoPrint_PGCI_UT(pgci_ut_t *pgci_ut);
void ifoPrint_C_ADT(c_adt_t *c_adt);
void ifoPrint_VOBU_ADMAP(vobu_admap_t *vobu_admap);

#ifdef __cplusplus
};
#endif
#endif /* IFO_PRINT_H_INCLUDED */

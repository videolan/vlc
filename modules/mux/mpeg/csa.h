/*****************************************************************************
 * csa.h
 *****************************************************************************
 * Copyright (C) 2004 Laurent Aimar
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
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

#ifndef _CSA_H
#define _CSA_H 1

typedef struct csa_t csa_t;
#define csa_New     E_(__csa_New)
#define csa_Delete  E_(__csa_Delete)
#define csa_SetCW  E_(__csa_SetCW)
#define csa_Decrypt E_(__csa_decrypt)
#define csa_Encrypt E_(__csa_encrypt)

csa_t *csa_New();
void   csa_Delete( csa_t * );

void   csa_SetCW( csa_t *, uint8_t o_ck[8], uint8_t e_ck[8] );

void   csa_Decrypt( csa_t *, uint8_t *pkt, int i_pkt_size );
void   csa_Encrypt( csa_t *, uint8_t *pkt, int i_pkt_size, int b_odd );

#endif /* _CSA_H */

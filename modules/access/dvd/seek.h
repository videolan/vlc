/* dvd_seek.h: DVD access plugin.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: seek.h,v 1.2 2002/12/06 16:34:04 sam Exp $
 *
 * Author: Stéphane Borel <stef@via.ecp.fr>
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

int CellIsInterleaved( thread_dvd_data_t * );
int CellAngleOffset  ( thread_dvd_data_t *, int );
int CellPrg2Map      ( thread_dvd_data_t * );
int CellFirstSector  ( thread_dvd_data_t * );
int CellLastSector   ( thread_dvd_data_t * );

int NextCellPrg      ( thread_dvd_data_t * );
int Lb2CellPrg       ( thread_dvd_data_t * );
int Lb2CellMap       ( thread_dvd_data_t * );
int LbMaxOnce        ( thread_dvd_data_t * );

int CellPrg2Chapter  ( thread_dvd_data_t * );
int NextChapter      ( thread_dvd_data_t * );
int DVDSetChapter    ( thread_dvd_data_t *, unsigned int );


/*****************************************************************************
 * ts_hotfixes.h : MPEG PMT/PAT less streams fixups
 *****************************************************************************
 * Copyright (C) 2014-2016 - VideoLAN Authors
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
#ifndef VLC_TS_HOTFIXES_H
#define VLC_TS_HOTFIXES_H

#define MIN_PAT_INTERVAL CLOCK_FREQ // DVB is 500ms

void ProbePES( demux_t *p_demux, ts_pid_t *pid, const uint8_t *p_pesstart, size_t i_data, bool b_adaptfield );
void MissingPATPMTFixup( demux_t *p_demux );

#endif

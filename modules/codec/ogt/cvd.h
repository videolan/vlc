/*****************************************************************************
 * cvd.h : CVD subtitles decoder thread interface
 *****************************************************************************
 * Copyright (C) 2003 VideoLAN
 * $Id: cvd.h,v 1.2 2004/01/04 04:56:21 rocky Exp $
 *
 * Author: Rocky Bernstein
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


/*****************************************************************************
 * Prototypes
 *****************************************************************************/
void E_(ParseHeader)( decoder_t *, uint8_t *, block_t *  );
void E_(ParsePacket)( decoder_t * );
void E_(ParseMetaInfo)( decoder_t *p_dec  );

/*****************************************************************************
 * input_es.h: thread structure of the ES plugin
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: input_es.h,v 1.3 2001/12/27 03:47:09 massiot Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#define ES_PACKET_SIZE 2048
#define ES_READ_ONCE 50
#define MAX_PACKETS_IN_FIFO 50

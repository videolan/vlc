/*****************************************************************************
 * avi_file.c : AVI file Stream input module for vlc
 *****************************************************************************
 * Copyright (C) 2001 VideoLAN
 * $Id: libLE.c,v 1.1 2002/04/23 23:44:36 fenrir Exp $
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

/*
 * Data reading functions
 */

static u16 __GetWordLittleEndianFromBuff( byte_t *p_buff )
{
    u16 i;
    i = (*p_buff) + ( *(p_buff + 1) <<8 );
    return ( i );
}

static u32 __GetDoubleWordLittleEndianFromBuff( byte_t *p_buff )
{
    u32 i;
    i = (*p_buff) + ( *(p_buff + 1) <<8 ) + ( *(p_buff + 2) <<16 ) + ( *(p_buff + 3) <<24 );
    return ( i );
}

static void __SetWordLittleEndianToBuff( byte_t *p_buff, u16 i)
{
    *(p_buff)     = (i & 0xFF);
    *(p_buff + 1) = ( ( i >>8 ) & 0xFF);
    return;
}

static void __SetDoubleWordLittleEndianToBuff( byte_t *p_buff, u32 i)
{
    *(p_buff)     = ( i & 0xFF );
    *(p_buff + 1) = (( i >>8 ) & 0xFF);
    *(p_buff + 2) = (( i >>16 ) & 0xFF);
    *(p_buff + 3) = (( i >>24 ) & 0xFF);
    return;
}



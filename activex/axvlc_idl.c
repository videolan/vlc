/*****************************************************************************
 * axvlc_idl.c: ActiveX control for VLC
 *****************************************************************************
 * Copyright (C) 2005 VideoLAN
 *
 * Authors: Damien Fouilleul <Damien.Fouilleul@laposte.net>
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

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IID_DEFINED__
#define __IID_DEFINED__

typedef struct _IID
{
    unsigned long x;
    unsigned short s1;
    unsigned short s2;
    unsigned char  c[8];
} IID;

#endif // __IID_DEFINED__

#ifndef CLSID_DEFINED
#define CLSID_DEFINED
typedef IID CLSID;
#endif // CLSID_DEFINED

const IID LIBID_AXVLC = {0xDF2BBE39,0x40A8,0x433b,{0xA2,0x79,0x07,0x3F,0x48,0xDA,0x94,0xB6}};


const IID IID_IVLCControl = {0xC2FA41D0,0xB113,0x476e,{0xAC,0x8C,0x9B,0xD1,0x49,0x99,0xC1,0xC1}};


const IID DIID_DVLCEvents = {0xDF48072F,0x5EF8,0x434e,{0x9B,0x40,0xE2,0xF3,0xAE,0x75,0x9B,0x5F}};


const CLSID CLSID_VLCPlugin = {0xE23FE9C6,0x778E,0x49D4,{0xB5,0x37,0x38,0xFC,0xDE,0x48,0x87,0xD8}};


#ifdef __cplusplus
}
#endif


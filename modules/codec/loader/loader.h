/*
 * Win32 binary loader interface
 * $Id$
 *
 * Copyright 2000 Eugene Kuznetsov (divx@euro.ru)
 * Copyright (C) the Wine project
 *
 * Originally distributed under LPGL 2.1 (or later) by the Wine project.
 *
 * Modified for use with MPlayer, detailed CVS changelog at
 * http://www.mplayerhq.hu/cgi-bin/cvsweb.cgi/main/
 *
 * File now distributed as part of VLC media player with no modifications.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#ifndef _LOADER_H
#define _LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "wine/windef.h"
#include "wine/driver.h"
#include "wine/mmreg.h"
#include "wine/vfw.h"
#include "wine/msacm.h"

unsigned int _GetPrivateProfileIntA(const char* appname, const char* keyname, int default_value, const char* filename);
int _GetPrivateProfileStringA(const char* appname, const char* keyname,
	const char* def_val, char* dest, unsigned int len, const char* filename);
int _WritePrivateProfileStringA(const char* appname, const char* keyname,
	const char* string, const char* filename);

INT WINAPI LoadStringA( HINSTANCE instance, UINT resource_id,
                            LPSTR buffer, INT buflen );

#ifdef __cplusplus
}
#endif
#endif /* __LOADER_H */


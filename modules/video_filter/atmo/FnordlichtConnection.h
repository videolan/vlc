/*
 * FnordlichtConnection.h: class to access a FnordlichtLight Hardware
 * - the description could be found
 * here: http://github.com/fd0/fnordlicht/raw/master/doc/PROTOCOL
 *
 * (C) Kai Lauterbach (klaute at gmail.com)
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
 *
 * $Id$
 */
#ifndef _FnordlichtConnection_h_
#define _FnordlichtConnection_h_

#include "AtmoDefs.h"
#include "AtmoConnection.h"
#include "AtmoConfig.h"

#if defined(_WIN32)
#   include <windows.h>
#endif


class CFnordlichtConnection : public CAtmoConnection
{
    private:
        HANDLE m_hComport;

        ATMO_BOOL sync(void);
        ATMO_BOOL stop(unsigned char addr);
        ATMO_BOOL reset(unsigned char addr);
        ATMO_BOOL start_bootloader(unsigned char addr);
        ATMO_BOOL boot_enter_application(unsigned char addr);

#if defined(_WIN32)
        DWORD  m_dwLastWin32Error;
    public:
        DWORD getLastError() { return m_dwLastWin32Error; }
#endif

    public:
        CFnordlichtConnection (CAtmoConfig *cfg);
        virtual ~CFnordlichtConnection (void);

        virtual ATMO_BOOL OpenConnection();

        virtual void CloseConnection();

        virtual ATMO_BOOL isOpen(void);

        virtual ATMO_BOOL SendData(pColorPacket data);

        virtual int getAmountFnordlichter();

        virtual const char *getDevicePath() { return "fnordlicht"; }

#if !defined(_ATMO_VLC_PLUGIN_)
        virtual char *getChannelName(int ch);
        virtual ATMO_BOOL ShowConfigDialog(HINSTANCE hInst, HWND parent,
                                            CAtmoConfig *cfg);
#endif

        virtual ATMO_BOOL CreateDefaultMapping(CAtmoChannelAssignment *ca);
};

#endif

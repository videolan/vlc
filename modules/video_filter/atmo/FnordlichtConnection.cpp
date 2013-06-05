/*
 * FnordlichtConnection.cpp: class to access a FnordlichtLight Hardware
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoDefs.h"
#include "FnordlichtConnection.h"

#if !defined(_ATMO_VLC_PLUGIN_)
# include "FnordlichtConfigDialog.h"
#endif

#include <stdio.h>
#include <fcntl.h>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif

CFnordlichtConnection::CFnordlichtConnection(CAtmoConfig *cfg)
    : CAtmoConnection(cfg)
{
    m_hComport = INVALID_HANDLE_VALUE;
}

CFnordlichtConnection::~CFnordlichtConnection()
{
}

ATMO_BOOL CFnordlichtConnection::OpenConnection()
{
#if defined(_ATMO_VLC_PLUGIN_)
    char *serdevice = m_pAtmoConfig->getSerialDevice();
    if ( !serdevice )
        return ATMO_FALSE;
#else
    int portNummer = m_pAtmoConfig->getComport();
    m_dwLastWin32Error = 0;
    if ( portNummer < 1 )
        return ATMO_FALSE; // make no real sense;-)
#endif

    CloseConnection();

#if !defined(_ATMO_VLC_PLUGIN_)
    char serdevice[16];  // com4294967295
    sprintf(serdevice,"com%d",portNummer);
#endif

#if defined(_WIN32)

    m_hComport = CreateFileA(serdevice,
                    GENERIC_WRITE, 0, NULL,
                    OPEN_EXISTING, 0, NULL);
    if ( m_hComport == INVALID_HANDLE_VALUE )
    {
        // we have a problem here can't open com port...
        // somebody else may use it?
        // m_dwLastWin32Error = GetLastError();
        return ATMO_FALSE;
    }
    /* change serial settings (Speed, stopbits etc.) */
    DCB dcb; // für comport-parameter
    dcb.DCBlength = sizeof(DCB);
    GetCommState(m_hComport, &dcb); // ger current serialport settings
    dcb.BaudRate = 19200;       // set speed
    dcb.ByteSize = 8;          // set databits
    dcb.Parity   = NOPARITY;    // set parity
    dcb.StopBits = ONESTOPBIT;   // set one stop bit
    SetCommState(m_hComport, &dcb);    // apply settings

#else

    int bconst = B19200;
    m_hComport = open(serdevice,O_RDWR |O_NOCTTY);
    if ( m_hComport < 0 )
        return ATMO_FALSE;

    struct termios tio;
    memset(&tio,0,sizeof(tio));
    tio.c_cflag = (CS8 | CREAD | HUPCL | CLOCAL);
    tio.c_iflag = (INPCK | BRKINT);
    cfsetispeed(&tio, bconst);
    cfsetospeed(&tio, bconst);
    if( ! tcsetattr(m_hComport, TCSANOW, &tio) )
        tcflush(m_hComport, TCIOFLUSH);
    else {
        // can't change parms
        close(m_hComport);
        m_hComport = -1;
        return false;
    }

#endif

    // sync fnordlicht
    if ( sync() )
        // stop fading on all devices
        if ( stop(255) )
            return true; // fnordlicht initialized...

    return false; // something is going wrong...
}

void CFnordlichtConnection::CloseConnection()
{
    if ( m_hComport != INVALID_HANDLE_VALUE )
    {
        reset(255);

#if defined(_WIN32)
        CloseHandle(m_hComport);
#else
        close(m_hComport);
#endif
        m_hComport = INVALID_HANDLE_VALUE;
    }
}

ATMO_BOOL CFnordlichtConnection::isOpen(void)
{
    return (m_hComport != INVALID_HANDLE_VALUE);
}

/*
    def fade_rgb(addr, r, g, b, step, delay)
       $dev.write addr.chr
       $dev.write "\x01"
       $dev.write step.chr
       $dev.write delay.chr
       $dev.write r.chr
       $dev.write g.chr
       $dev.write b.chr
       $dev.write "\x00\x00\x00\x00\x00"
       $dev.write "\x00\x00\x00"
       $dev.flush
    end
*/
ATMO_BOOL CFnordlichtConnection::SendData(pColorPacket data)
{
    if ( m_hComport == INVALID_HANDLE_VALUE )
        return ATMO_FALSE;

    int amount = getAmountFnordlichter();
    unsigned char buffer[15];
    memset(&buffer, 0, sizeof(buffer) ); // zero buffer
    int iBytesWritten;

    Lock();

    buffer[1] = 0x01; // fade to rgb color
    buffer[2] = 0x80; // in two steps
    buffer[3] = 0x01; // 1ms pause between steps

    // send all packages to all fnordlicht's
    for( unsigned char i=0; i < amount; i++ )
    {
        int idx;
        if ( m_ChannelAssignment && i < m_NumAssignedChannels )
            idx = m_ChannelAssignment[i];
        else
            idx = -1; // no channel assigned to fnordlicht[i]

        if( idx >= 0 && idx <= data->numColors )
        {
            // fnordlicht address equals to a MoMo Channel
            buffer[0] = i; // fnordlicht address (0..254, 255 = broadcast)
            buffer[4] = data->zone[idx].r;
            buffer[5] = data->zone[idx].g;
            buffer[6] = data->zone[idx].b;
        }

#if defined(_WIN32)
        // send to COM-Port
        WriteFile( m_hComport, buffer, sizeof(buffer),
                    (DWORD*)&iBytesWritten, NULL );
#else
        iBytesWritten = write(m_hComport, buffer, sizeof(buffer));
        tcflush(m_hComport, TCIOFLUSH);
        tcdrain(m_hComport); // flush buffer
#endif

        if (iBytesWritten != sizeof(buffer))
        {
            Unlock();
            return ATMO_FALSE; // shouldn't be...
        }

    }

    Unlock();

    return ATMO_TRUE;
}


ATMO_BOOL CFnordlichtConnection::CreateDefaultMapping(CAtmoChannelAssignment *ca)
{
    if ( !ca )
        return ATMO_FALSE;
    ca->setSize( getAmountFnordlichter() );  // oder 4 ? depending on config!
    ca->setZoneIndex(0, 0); // Zone 5
    ca->setZoneIndex(1, 1);
    ca->setZoneIndex(2, 2);
    ca->setZoneIndex(3, 3);
    return ATMO_TRUE;
}

int CFnordlichtConnection::getAmountFnordlichter()
{
   return m_pAtmoConfig->getFnordlicht_Amount();
}

/*
    def sync(addr = 0)
       1.up to(15) do
          $dev.write "\e"
       end
       $dev.write addr.chr
       $dev.flush
    end
*/
ATMO_BOOL CFnordlichtConnection::sync(void)
{
    if ( m_hComport == INVALID_HANDLE_VALUE )
        return ATMO_FALSE;

    unsigned char buffer[16];

    int iBytesWritten;

    Lock();

    // fill buffer with 15 escape character
    memset(&buffer, 0x1b, sizeof(buffer)-1);

    buffer[sizeof(buffer)-1] = 0x00; // append one zero byte

#if defined(_WIN32)
        // send to COM-Port
        WriteFile( m_hComport, buffer, sizeof(buffer),
                    (DWORD*)&iBytesWritten, NULL );
#else
        iBytesWritten = write(m_hComport, buffer, sizeof(buffer));
        tcflush(m_hComport, TCIOFLUSH);
        tcdrain(m_hComport); // flush buffer
#endif

    Unlock();

    return (iBytesWritten == sizeof(buffer)) ? ATMO_TRUE : ATMO_FALSE;

}

/*
    def stop(addr, fading = 1)
       $dev.write addr.chr
       $dev.write "\x08"
       $dev.write fading.chr
       $dev.write "\x00\x00\x00\x00"
       $dev.write "\x00\x00\x00\x00\x00"
       $dev.write "\x00\x00\x00"
       $dev.flush
    end
*/
ATMO_BOOL CFnordlichtConnection::stop(unsigned char addr)
{
    if(m_hComport == INVALID_HANDLE_VALUE)
        return ATMO_FALSE;

    unsigned char buffer[15];
    memset(&buffer, 0, sizeof(buffer)); // zero buffer
    int iBytesWritten;

    Lock();

    buffer[0] = addr; // fnordlicht address (255 = broadcast)
    buffer[1] = 0x08; // stop command
    buffer[2] = 1;    // fading

#if defined(_WIN32)
        // send to COM-Port
        WriteFile( m_hComport, buffer, sizeof(buffer),
                    (DWORD*)&iBytesWritten, NULL );
#else
        iBytesWritten = write(m_hComport, buffer, sizeof(buffer));
        tcflush(m_hComport, TCIOFLUSH);
        tcdrain(m_hComport); // flush buffer
#endif

    Unlock();

    return (iBytesWritten == sizeof(buffer)) ? ATMO_TRUE : ATMO_FALSE;
}

/*
*/
ATMO_BOOL CFnordlichtConnection::reset(unsigned char addr)
{
    if(m_hComport == INVALID_HANDLE_VALUE)
        return ATMO_FALSE;

    stop(255);

    if ( sync() && start_bootloader(addr) )
    {
#if defined(_ATMO_VLC_PLUGIN_)
        do_sleep(200000); // wait 200ms
#else
        do_sleep(200); // wait 200ms
#endif
        if ( sync() && boot_enter_application(addr) )
                return ATMO_TRUE;
    }

    return ATMO_FALSE;
}

/*
    def start_bootloader(addr)
        $dev.write(addr.chr)
        $dev.write("\x80")
        $dev.write("\x6b\x56\x27\xfc")
        $dev.write("\x00\x00\x00\x00\x00\x00\x00\x00\x00")
        $dev.flush
    end
*/
ATMO_BOOL CFnordlichtConnection::start_bootloader(unsigned char addr)
{
    if(m_hComport == INVALID_HANDLE_VALUE)
        return ATMO_FALSE;

    unsigned char buffer[15];
    memset(&buffer, 0, sizeof(buffer)); // zero buffer
    int iBytesWritten;

    Lock();

    buffer[0] = addr; // fnordlicht address (255 = broadcast)
    buffer[1] = 0x80; // start_bootloader
    buffer[2] = 0x6b;
    buffer[3] = 0x56;
    buffer[4] = 0x27;
    buffer[5] = 0xfc;

#if defined(_WIN32)
        // send to COM-Port
        WriteFile( m_hComport, buffer, sizeof(buffer),
                    (DWORD*)&iBytesWritten, NULL );
#else
        iBytesWritten = write(m_hComport, buffer, sizeof(buffer));
        tcflush(m_hComport, TCIOFLUSH);
        tcdrain(m_hComport); // flush buffer
#endif

    Unlock();

    return (iBytesWritten == sizeof(buffer)) ? ATMO_TRUE : ATMO_FALSE;
}

/*
    def boot_enter_application(addr)
        $dev.write(addr.chr)
        $dev.write("\x87")
        $dev.write("\x00"*13)
        $dev.flush
    end
*/
ATMO_BOOL CFnordlichtConnection::boot_enter_application(unsigned char addr)
{
    if(m_hComport == INVALID_HANDLE_VALUE)
        return ATMO_FALSE;

    unsigned char buffer[15];
    memset(&buffer, 0, sizeof(buffer)); // zero buffer
    int iBytesWritten;

    Lock();

    buffer[0] = addr; // fnordlicht address (255 = broadcast)
    buffer[1] = 0x87; // boot_ender_application command

#if defined(_WIN32)
        // send to COM-Port
        WriteFile( m_hComport, buffer, sizeof(buffer),
                    (DWORD*)&iBytesWritten, NULL );
#else
        iBytesWritten = write(m_hComport, buffer, sizeof(buffer));
        tcflush(m_hComport, TCIOFLUSH);
        tcdrain(m_hComport); // flush buffer
#endif

    Unlock();

    return (iBytesWritten == sizeof(buffer)) ? ATMO_TRUE : ATMO_FALSE;
}

#if !defined(_ATMO_VLC_PLUGIN_)

char *CFnordlichtConnection::getChannelName(int ch)
{
    char buf[60];
    if( ch < 0 ) return NULL;
    if( ch >= getAmountFnordlichter() ) return NULL;

    sprintf(buf,"Number [%d]",ch);
    return strdup(buf);
    // sorry asprintf is not defined on Visual Studio :)
    // return (asprintf(&ret, "Number [%d]", ch) != -1) ? ret : NULL;

}

ATMO_BOOL CFnordlichtConnection::ShowConfigDialog(HINSTANCE hInst, HWND parent,
                                                    CAtmoConfig *cfg)
{
    CFnordlichtConfigDialog *dlg = new CFnordlichtConfigDialog(hInst,
                                                                parent,
                                                                cfg);

    INT_PTR result = dlg->ShowModal();

    delete dlg;

    if(result == IDOK)
        return ATMO_TRUE;
    else
        return ATMO_FALSE;
}

#endif

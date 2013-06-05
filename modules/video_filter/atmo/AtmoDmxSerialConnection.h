/*
 * AtmoDmxSerialConnection.h: Class for communication with the serial DMX Interface of dzionsko,
 * opens and configures the serial port
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoDmxSerialConnection_h_
#define _AtmoDmxSerialConnection_h_


#include "AtmoDefs.h"
#include "AtmoConnection.h"
#include "AtmoConfig.h"

#if defined(_WIN32)
#   include <windows.h>
#endif

class CAtmoDmxSerialConnection : public CAtmoConnection {
    private:
        HANDLE m_hComport;
        // DMX Channel Buffer including some Control Bytes for up to 256 DMX Channels
		unsigned char DMXout[259];
        // contains the DMX Start Adress of each Atmo-Dmx-Channel
        int *m_dmx_channels_base;

#if defined(_WIN32)
        DWORD  m_dwLastWin32Error;
    public:
        DWORD getLastError() { return m_dwLastWin32Error; }
#endif

    public:
       CAtmoDmxSerialConnection(CAtmoConfig *cfg);
       virtual ~CAtmoDmxSerialConnection(void);

  	   virtual ATMO_BOOL OpenConnection();

       virtual void CloseConnection();

       virtual ATMO_BOOL isOpen(void);

       virtual ATMO_BOOL SendData(pColorPacket data);

	   virtual ATMO_BOOL setChannelColor(int channel, tRGBColor color);

       virtual ATMO_BOOL setChannelValues(int numValues,unsigned char *channel_values);

       virtual int getNumChannels();

#if !defined(_ATMO_VLC_PLUGIN_)
       virtual char *getChannelName(int ch);
       virtual ATMO_BOOL ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg);
#endif

       virtual const char *getDevicePath() { return "dmx"; }

       virtual ATMO_BOOL CreateDefaultMapping(CAtmoChannelAssignment *ca);
};

#endif

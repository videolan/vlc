/*
 * AtmoMultiConnection.h: Class for communication with up to 4 - 4 channel classic atmolight controllers
 * so you can built a cheap solution for having up to 16 channels, but you need four comports or
 * USB Adapters
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifndef _AtmoMultiConnection_h_
#define _AtmoMultiConnection_h_

#include "AtmoDefs.h"
#include "AtmoConnection.h"
#include "AtmoConfig.h"

#if defined(_WIN32)
#   include <windows.h>
#endif


class CAtmoMultiConnection :  public CAtmoConnection
{
    private:
        HANDLE m_hComports[4];
        unsigned char m_output[4 * 4 * 3];

#if defined(_WIN32)
        DWORD  m_dwLastWin32Error;
    public:
        DWORD getLastError() { return m_dwLastWin32Error; }
#endif

        /*
          on windows devName is COM1 COM2 etc.
          on linux devname my be /dev/ttyS0 or /dev/ttyUSB0
        */
       HANDLE OpenDevice(char *devName);
       ATMO_BOOL internal_HardwareWhiteAdjust(HANDLE hComport,int global_gamma,
                                                     int global_contrast,
                                                     int contrast_red,
                                                     int contrast_green,
                                                     int contrast_blue,
                                                     int gamma_red,
                                                     int gamma_green,
                                                     int gamma_blue,
                                                     ATMO_BOOL storeToEeprom);

       ATMO_BOOL internal_SendData(HANDLE hComport, unsigned char *colorData);

    public:
       CAtmoMultiConnection(CAtmoConfig *cfg);
       virtual ~CAtmoMultiConnection(void);

  	   virtual ATMO_BOOL OpenConnection();

       virtual void CloseConnection();

       virtual ATMO_BOOL isOpen(void);

       virtual ATMO_BOOL SendData(pColorPacket data);

       virtual ATMO_BOOL setChannelColor(int channel, tRGBColor color);
       virtual ATMO_BOOL setChannelValues(int numValues,unsigned char *channel_values);

       virtual ATMO_BOOL HardwareWhiteAdjust(int global_gamma,
                                             int global_contrast,
                                             int contrast_red,
                                             int contrast_green,
                                             int contrast_blue,
                                             int gamma_red,
                                             int gamma_green,
                                             int gamma_blue,
                                             ATMO_BOOL storeToEeprom);

       virtual int getNumChannels();
       virtual const char *getDevicePath() { return "multiatmo"; }

#if !defined(_ATMO_VLC_PLUGIN_)
       virtual char *getChannelName(int ch);
       virtual ATMO_BOOL ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg);
#endif

       virtual ATMO_BOOL CreateDefaultMapping(CAtmoChannelAssignment *ca);
};

#endif

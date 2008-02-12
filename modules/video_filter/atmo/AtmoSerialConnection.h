/*
 * AtmoCom.h: Class for communication with the serial hardware of Atmo Light,
 * opens and configures the serial port
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoSerialConnection_h_
#define _AtmoSerialConnection_h_

#include "AtmoDefs.h"
#include "AtmoConnection.h"
#include "AtmoConfig.h"

#if defined(WIN32)
#   include <windows.h>
#endif


class CAtmoSerialConnection : public CAtmoConnection {
    private:
        HANDLE m_hComport;

#if defined(WIN32)
        DWORD  m_dwLastWin32Error;
    public:
        DWORD getLastError() { return m_dwLastWin32Error; }
#endif

    public:
       CAtmoSerialConnection(CAtmoConfig *cfg);
       virtual ~CAtmoSerialConnection(void);

  	   virtual ATMO_BOOL OpenConnection();

       virtual void CloseConnection();

       virtual ATMO_BOOL isOpen(void);

       virtual ATMO_BOOL SendData(unsigned char numChannels,
                                  int red[],
                                  int green[],
                                  int blue[]);

       virtual ATMO_BOOL SendData(tColorPacket data);

       virtual ATMO_BOOL HardwareWhiteAdjust(int global_gamma,
                                             int global_contrast,
                                             int contrast_red,
                                             int contrast_green,
                                             int contrast_blue,
                                             int gamma_red,
                                             int gamma_green,
                                             int gamma_blue,
                                             ATMO_BOOL storeToEeprom);
};

#endif

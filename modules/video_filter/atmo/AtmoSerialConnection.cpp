/*
 * AtmoSerialConnection.cpp: Class for communication with the serial hardware of
 * Atmo Light, opens and configures the serial port
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */


#include "AtmoDefs.h"
#include "AtmoSerialConnection.h"


#include <stdio.h>
#include <fcntl.h>

#include <sys/stat.h>

#if !defined(WIN32)
#include <termios.h>
#include <unistd.h>
#endif

/*
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <vdr/tools.h>
*/


CAtmoSerialConnection::CAtmoSerialConnection(CAtmoConfig *cfg) : CAtmoConnection(cfg) {
    m_hComport = INVALID_HANDLE_VALUE;
}

CAtmoSerialConnection::~CAtmoSerialConnection() {
   CloseConnection();
}

ATMO_BOOL CAtmoSerialConnection::OpenConnection() {
#if defined(_ATMO_VLC_PLUGIN_)
     char *serdevice = m_pAtmoConfig->getSerialDevice();
     if(!serdevice)
        return ATMO_FALSE;
#else
     int portNummer = m_pAtmoConfig->getComport();
     m_dwLastWin32Error = 0;
	 if(portNummer < 1) return ATMO_FALSE; // make no real sense;-)
#endif

	 CloseConnection();

#if !defined(_ATMO_VLC_PLUGIN_)
     char comport[16];  // com4294967295
     sprintf(comport,"com%d",portNummer);
#endif

#if defined(WIN32)

#  if defined(_ATMO_VLC_PLUGIN_)
     m_hComport = CreateFile(serdevice, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
#  else
     m_hComport = CreateFile(comport, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
#  endif
     if(m_hComport == INVALID_HANDLE_VALUE) {
//      we have a problem here can't open com port... somebody else may use it?
//	    m_dwLastWin32Error = GetLastError();
	    return ATMO_FALSE;
     }
     /* change serial settings (Speed, stopbits etc.) */
     DCB dcb; // für comport-parameter
     dcb.DCBlength = sizeof(DCB);
     GetCommState (m_hComport, &dcb); // ger current serialport settings
     dcb.BaudRate  = 38400;        // set speed
     dcb.ByteSize  = 8;            // set databits
     dcb.Parity    = NOPARITY;     // set parity
     dcb.StopBits  = ONESTOPBIT;   // set one stop bit
     SetCommState (m_hComport, &dcb);    // apply settings

#else

     int bconst = B38400;
#  if defined(_ATMO_VLC_PLUGIN_)
     m_hComport = open(serdevice,O_RDWR |O_NOCTTY);
#  else
     m_hComport = open(comport,O_RDWR | O_NOCTTY);
#  endif
     if(m_hComport < 0) {
	    return ATMO_FALSE;
     }

     struct termios tio;
     memset(&tio,0,sizeof(tio));
     tio.c_cflag = (CS8 | CREAD | HUPCL | CLOCAL);
     tio.c_iflag = (INPCK | BRKINT);
     cfsetispeed(&tio, bconst);
     cfsetospeed(&tio, bconst);
     if(!tcsetattr(m_hComport, TCSANOW, &tio)) {
         tcflush(m_hComport, TCIOFLUSH);
     } else {
         // can't change parms
        close(m_hComport);
        m_hComport = -1;
        return false;
     }

#endif

     return true;
}

void CAtmoSerialConnection::CloseConnection() {
  if(m_hComport!=INVALID_HANDLE_VALUE) {
#if defined(WIN32)
     CloseHandle(m_hComport);
#else
     close(m_hComport);
#endif
	 m_hComport = INVALID_HANDLE_VALUE;
  }
}

ATMO_BOOL CAtmoSerialConnection::isOpen(void) {
	 return (m_hComport != INVALID_HANDLE_VALUE);
}

ATMO_BOOL CAtmoSerialConnection::HardwareWhiteAdjust(int global_gamma,
                                                     int global_contrast,
                                                     int contrast_red,
                                                     int contrast_green,
                                                     int contrast_blue,
                                                     int gamma_red,
                                                     int gamma_green,
                                                     int gamma_blue,
                                                     ATMO_BOOL storeToEeprom) {
     if(m_hComport == INVALID_HANDLE_VALUE)
   	    return ATMO_FALSE;

     DWORD iBytesWritten;
/*
[0] = 255
[1] = 00
[2] = 00
[3] = 101

[4]  brightness  0..255 ?

[5]  Contrast Red     11 .. 100
[6]  Contrast  Green  11 .. 100
[7]  Contrast  Blue   11 .. 100

[8]   Gamma Red    11 .. 35
[9]   Gamma Red    11 .. 35
[10]  Gamma Red    11 .. 35

[11]  Globale Contrast  11 .. 100

[12]  Store Data: 199 (else 0)

*/
     unsigned char sendBuffer[16];
     sendBuffer[0] = 0xFF;
     sendBuffer[1] = 0x00;
     sendBuffer[2] = 0x00;
     sendBuffer[3] = 101;

     sendBuffer[4] = (global_gamma & 255);

     sendBuffer[5] = (contrast_red & 255);
     sendBuffer[6] = (contrast_green & 255);
     sendBuffer[7] = (contrast_blue & 255);

     sendBuffer[8]  = (gamma_red & 255);
     sendBuffer[9]  = (gamma_green & 255);
     sendBuffer[10] = (gamma_blue & 255);

     sendBuffer[11] = (global_contrast & 255);

     if(storeToEeprom == ATMO_TRUE)
        sendBuffer[12] = 199; // store to eeprom!
     else
        sendBuffer[12] = 0;

#if defined(WIN32)
     WriteFile(m_hComport, sendBuffer, 13, &iBytesWritten, NULL); // send to COM-Port
#else
     iBytesWritten = write(m_hComport, sendBuffer, 13);
     tcdrain(m_hComport);
#endif

     return (iBytesWritten == 13) ? ATMO_TRUE : ATMO_FALSE;
}


ATMO_BOOL CAtmoSerialConnection::SendData(tColorPacket data) {
   if(m_hComport == INVALID_HANDLE_VALUE)
	  return ATMO_FALSE;

   unsigned char buffer[19];
   DWORD iBytesWritten;

   buffer[0] = 0xFF;  // Start Byte
   buffer[1] = 0x00;  // Start channel 0
   buffer[2] = 0x00;  // Start channel 0
   buffer[3] = 15; //
   int iBuffer = 4;
   for(int i=0;i<5;i++) {
       if(m_ChannelAssignment[i]>=0) {
          buffer[iBuffer++] = data.channel[m_ChannelAssignment[i]].r;
          buffer[iBuffer++] = data.channel[m_ChannelAssignment[i]].g;
          buffer[iBuffer++] = data.channel[m_ChannelAssignment[i]].b;
       } else {
          buffer[iBuffer++] = 0;
          buffer[iBuffer++] = 0;
          buffer[iBuffer++] = 0;
       }
   }

#if defined(WIN32)
   WriteFile(m_hComport, buffer, 19, &iBytesWritten, NULL); // send to COM-Port
#else
   iBytesWritten = write(m_hComport, buffer, 19);
   tcdrain(m_hComport);
#endif

   return (iBytesWritten == 19) ? ATMO_TRUE : ATMO_FALSE;
}

ATMO_BOOL CAtmoSerialConnection::SendData(unsigned char numChannels,
                                          int red[],
                                          int green[],
                                          int blue[])
{
   if(m_hComport == INVALID_HANDLE_VALUE)
	  return ATMO_FALSE;

   DWORD bufSize = 4 + numChannels*3;
   unsigned char *buffer = new unsigned char[bufSize];
   DWORD iBytesWritten;

   buffer[0] = 0xFF;  // Start Byte
   buffer[1] = 0x00;  // Start Kanal 0
   buffer[2] = 0x00;  // Start Kanal 0
   buffer[3] = numChannels * 3; //
   int iBuffer = 4;
   for(int i=0;i<numChannels;i++) {
       if(m_ChannelAssignment[i]>=0) {
          buffer[iBuffer++] = red[m_ChannelAssignment[i]] & 255;
          buffer[iBuffer++] = green[m_ChannelAssignment[i]] & 255;
          buffer[iBuffer++] = blue[m_ChannelAssignment[i]] & 255;
       } else {
          buffer[iBuffer++] = 0;
          buffer[iBuffer++] = 0;
          buffer[iBuffer++] = 0;
       }
   }

#if defined(WIN32)
   WriteFile(m_hComport, buffer, bufSize, &iBytesWritten, NULL);
#else
   iBytesWritten = write(m_hComport, buffer, bufSize);
   tcdrain(m_hComport);
#endif

   delete[] buffer;

   return (iBytesWritten == bufSize) ? ATMO_TRUE : ATMO_FALSE;
}


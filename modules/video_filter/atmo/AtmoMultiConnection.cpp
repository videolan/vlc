/*
 * AtmoMultiConnection.cpp: Class for communication with up to 4 - 4 channel classic atmolight controllers
 * so you can built a cheap solution for having up to 16 channels
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoDefs.h"
#include "AtmoMultiConnection.h"

#if !defined(_ATMO_VLC_PLUGIN_)
#include "AtmoMultiConfigDialog.h"
#endif

#include <stdio.h>
#include <fcntl.h>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif



CAtmoMultiConnection::CAtmoMultiConnection(CAtmoConfig *cfg) : CAtmoConnection(cfg)
{
   m_hComports[0] = INVALID_HANDLE_VALUE;
   m_hComports[1] = INVALID_HANDLE_VALUE;
   m_hComports[2] = INVALID_HANDLE_VALUE;
   m_hComports[3] = INVALID_HANDLE_VALUE;
   memset(&m_output, 0, sizeof(m_output));
}

CAtmoMultiConnection::~CAtmoMultiConnection(void)
{
}

HANDLE CAtmoMultiConnection::OpenDevice(char *devName)
{
     HANDLE hComport;

#if !defined(_ATMO_VLC_PLUGIN_)
     m_dwLastWin32Error = 0;
#endif

#if defined(_WIN32)
     hComport = CreateFileA(devName, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
     if(hComport == INVALID_HANDLE_VALUE) {
#if !defined(_ATMO_VLC_PLUGIN_)
	    m_dwLastWin32Error = GetLastError();
#endif
	    return INVALID_HANDLE_VALUE;
     }
     /* change serial settings (Speed, stopbits etc.) */
     DCB dcb; // für comport-parameter
     dcb.DCBlength = sizeof(DCB);
     GetCommState (hComport, &dcb); // ger current serialport settings
     dcb.BaudRate  = 38400;        // set speed
     dcb.ByteSize  = 8;            // set databits
     dcb.Parity    = NOPARITY;     // set parity
     dcb.StopBits  = ONESTOPBIT;   // set one stop bit
     SetCommState (hComport, &dcb);    // apply settings

#else

     int bconst = B38400;
     hComport = open(devName,O_RDWR |O_NOCTTY);
     if(hComport < 0) {
	    return INVALID_HANDLE_VALUE;;
     }
     struct termios tio;
     memset(&tio,0,sizeof(tio));
     tio.c_cflag = (CS8 | CREAD | HUPCL | CLOCAL);
     tio.c_iflag = (INPCK | BRKINT);
     cfsetispeed(&tio, bconst);
     cfsetospeed(&tio, bconst);
     if(!tcsetattr(hComport, TCSANOW, &tio)) {
         tcflush(hComport, TCIOFLUSH);
     } else {
         // can't change parms
        close(hComport);
        return INVALID_HANDLE_VALUE;
     }
#endif

     return hComport;
}

ATMO_BOOL CAtmoMultiConnection::OpenConnection()
{
    int z = 0;
#if defined(_ATMO_VLC_PLUGIN_)

    for(int c = 0; c < 4; c++ ) {
        char *devName = m_pAtmoConfig->getSerialDevice( c );
        if( !EMPTY_STR( devName ) )
        {
            m_hComports[z] = OpenDevice( devName );
            if(m_hComports[z] == INVALID_HANDLE_VALUE) {
                while(z) {
                      z--;
#if defined(_WIN32)
                      CloseHandle( m_hComports[z] );
#else
                      close( m_hComports[z] );
#endif
                      m_hComports[z] = INVALID_HANDLE_VALUE;
                }
                return ATMO_FALSE;
            }
            z++;
        }
    }


#else

    char devName[16];

    for(int c = 0; c < 4; c++ ) {
        int comportnr = m_pAtmoConfig->getComport(c);
        if(comportnr > 0)
        {
            sprintf(devName,"com%d",comportnr);
            m_hComports[z] = OpenDevice(devName);
            if(m_hComports[z] == INVALID_HANDLE_VALUE) {
                while(z) {
                      z--;
                      CloseHandle( m_hComports[z] );
                      m_hComports[z] = INVALID_HANDLE_VALUE;
                }
                return ATMO_FALSE;
            }
            z++;
        }
    }
#endif
    return ATMO_TRUE;
}

void CAtmoMultiConnection::CloseConnection() {
    for(int i = 0; i < 4; i++ ) {
        if(m_hComports[i] != INVALID_HANDLE_VALUE) {
#if defined(_WIN32)
           CloseHandle( m_hComports[i] );
#else
           close( m_hComports[i] );
#endif
	       m_hComports[i] = INVALID_HANDLE_VALUE;
        }
    }
}

ATMO_BOOL CAtmoMultiConnection::isOpen(void) {
     int z = 0;
     for(int i = 0; i < 4; i++ )
         if(m_hComports[i] != INVALID_HANDLE_VALUE) z++;

	 return (z > 0);
}

int CAtmoMultiConnection::getNumChannels()
{
    int z = 0;
#if defined(_ATMO_VLC_PLUGIN_)
    char *psz_dev;
    for(int i=0;i<4;i++) {
        psz_dev = m_pAtmoConfig->getSerialDevice( i );
        if( !EMPTY_STR( psz_dev ) )
            z+=4;
    }
#else
    for(int i=0;i<4;i++)
        if(m_pAtmoConfig->getComport(i)>0)
           z+=4;
#endif
    return z;
}

ATMO_BOOL CAtmoMultiConnection::CreateDefaultMapping(CAtmoChannelAssignment *ca)
{
  if(!ca) return ATMO_FALSE;
  int z = getNumChannels();
  ca->setSize( z );
  // 1 : 1 mapping vorschlagen...
  for(int i = 0; i < z ; i++ ) {
      ca->setZoneIndex( i, i );
  }
  return ATMO_TRUE;
}


ATMO_BOOL CAtmoMultiConnection::internal_HardwareWhiteAdjust(HANDLE hComport,
                                                     int global_gamma,
                                                     int global_contrast,
                                                     int contrast_red,
                                                     int contrast_green,
                                                     int contrast_blue,
                                                     int gamma_red,
                                                     int gamma_green,
                                                     int gamma_blue,
                                                     ATMO_BOOL storeToEeprom) {
  if(hComport == INVALID_HANDLE_VALUE)
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

#if defined(_WIN32)
     WriteFile(hComport, sendBuffer, 13, &iBytesWritten, NULL); // send to COM-Port
#else
     iBytesWritten = write(hComport, sendBuffer, 13);
     tcdrain(hComport);
#endif

     return (iBytesWritten == 13) ? ATMO_TRUE : ATMO_FALSE;
}


ATMO_BOOL CAtmoMultiConnection::HardwareWhiteAdjust( int global_gamma,
                                                     int global_contrast,
                                                     int contrast_red,
                                                     int contrast_green,
                                                     int contrast_blue,
                                                     int gamma_red,
                                                     int gamma_green,
                                                     int gamma_blue,
                                                     ATMO_BOOL storeToEeprom)
{
    for(int z = 0 ; z < 4; z++ ) {
        if(m_hComports[z]!= INVALID_HANDLE_VALUE)
           if(internal_HardwareWhiteAdjust(m_hComports[z], global_gamma, global_contrast,
                                           contrast_red, contrast_green, contrast_blue,
                                           gamma_red, gamma_green, gamma_blue,
                                           storeToEeprom) == ATMO_FALSE)
                                                return ATMO_FALSE;
    }
    return ATMO_TRUE;
}

ATMO_BOOL CAtmoMultiConnection::internal_SendData(HANDLE hComport, unsigned char *colorData)
{
 if(m_hComports[0] == INVALID_HANDLE_VALUE)
    return ATMO_FALSE;

  unsigned char buffer[19];
  DWORD iBytesWritten;

  buffer[0] = 0xFF;  // Start Byte
  buffer[1] = 0x00;  // Start channel 0
  buffer[2] = 0x00;  // Start channel 0
  buffer[3] = 15; //
  buffer[4] = 0; // Summe Red
  buffer[5] = 0; // Summe Green
  buffer[6] = 0; // Summe Blue
  memcpy(&buffer[7], colorData, 4 * 3);

#if defined(_WIN32)
   WriteFile(hComport, buffer, 19, &iBytesWritten, NULL); // send to COM-Port
#else
   iBytesWritten = write(hComport, buffer, 19);
   tcdrain(hComport);
#endif

 return (iBytesWritten == 19) ? ATMO_TRUE : ATMO_FALSE;
}

ATMO_BOOL CAtmoMultiConnection::SendData(pColorPacket data)
{
   if(m_hComports[0] == INVALID_HANDLE_VALUE)
	  return ATMO_FALSE;

   int numChannels = this->getNumChannels();

   int idx;
   int iBuffer = 0;
   ATMO_BOOL result = ATMO_TRUE;

   Lock();

   for(int i = 0; i < numChannels ; i++) {
       if(m_ChannelAssignment && (i < m_NumAssignedChannels))
          idx = m_ChannelAssignment[i];
       else
          idx = -1;
       if((idx>=0) && (idx<data->numColors)) {
          m_output[iBuffer] = data->zone[idx].r;
          m_output[iBuffer+1] = data->zone[idx].g;
          m_output[iBuffer+2] = data->zone[idx].b;
       }
       iBuffer+=3;
   }
   for(int i = 0;i < 4; i++)
       if(m_hComports[i] != INVALID_HANDLE_VALUE)
          result = result & internal_SendData(m_hComports[i], &m_output[i*4*3]);

   Unlock();

   return result;
}

ATMO_BOOL CAtmoMultiConnection::setChannelColor(int channel, tRGBColor color)
{
   if(m_hComports[0] == INVALID_HANDLE_VALUE)
	  return ATMO_FALSE;
   if((channel < 0) || (channel >= getNumChannels()))
      return ATMO_FALSE;

   ATMO_BOOL result = ATMO_TRUE;

   Lock();
   channel*=3;
   m_output[channel++] = color.r;
   m_output[channel++] = color.g;
   m_output[channel]   = color.b;
   for(int i = 0; i < 4; i++)
       if(m_hComports[i] != INVALID_HANDLE_VALUE)
          result = result & internal_SendData(m_hComports[i], &m_output[i*4*3]);
   Unlock();

   return result;
}

ATMO_BOOL CAtmoMultiConnection::setChannelValues(int numValues,unsigned char *channel_values)
{
  if(m_hComports[0] == INVALID_HANDLE_VALUE)
     return ATMO_FALSE;

  if((numValues & 1) || !channel_values)
     return ATMO_FALSE; // numValues must be even!

  ATMO_BOOL result = ATMO_TRUE;


  Lock();
  size_t Index = 0;
  for (int i = 0; i < numValues; i+=2) {
       Index = (size_t)channel_values[i];
       if(Index < sizeof(m_output))
          m_output[Index] = channel_values[i + 1];
  }
  for(int i = 0; i < 4; i++)
      if(m_hComports[i] != INVALID_HANDLE_VALUE)
         result = result & internal_SendData(m_hComports[i], &m_output[i*4*3]);

  Unlock();
  return result;
}

#if !defined(_ATMO_VLC_PLUGIN_)

char *CAtmoMultiConnection::getChannelName(int ch)
{
  int devnum = ch / 4;
  int kanal  = ch % 4;
  char buf[60];
  switch(kanal) {
         case 0: {
                   sprintf(buf,"Atmo[%d.%d] Links (%d)", devnum, kanal, ch);
                   break;
                 }
         case 1: {
                   sprintf(buf,"Atmo[%d.%d] Rechts (%d)", devnum, kanal, ch);
                   break;
                 }
         case 2: {
                   sprintf(buf,"Atmo[%d.%d] Oben (%d)", devnum, kanal, ch);
                   break;
                 }
         case 3: {
                   sprintf(buf,"Atmo[%d.%d] Unten (%d)", devnum, kanal, ch);
                   break;
                 }
  }
  return strdup(buf);
}

ATMO_BOOL CAtmoMultiConnection::ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg)
{
    CAtmoMultiConfigDialog *dlg = new CAtmoMultiConfigDialog(hInst, parent, cfg);

    INT_PTR result = dlg->ShowModal();

    delete dlg;

    if(result == IDOK)
      return ATMO_TRUE;
    else
      return ATMO_FALSE;
}

#endif


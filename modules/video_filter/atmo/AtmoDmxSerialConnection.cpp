/*
 * AtmoDmxSerialConnection.cpp: Class for communication with a Simple DMX Dongle/Controller
 * for hardware see also:
 * http://www.dzionsko.de/elektronic/index.htm
 * http://www.ulrichradig.de/ (search for dmx on his page)
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoDefs.h"
#include "AtmoDmxSerialConnection.h"
#include "DmxTools.h"

#if !defined(_ATMO_VLC_PLUGIN_)
#include "DmxConfigDialog.h"
#endif

#include <stdio.h>
#include <fcntl.h>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif


CAtmoDmxSerialConnection::CAtmoDmxSerialConnection(CAtmoConfig *cfg) : CAtmoConnection(cfg) {
    m_hComport = INVALID_HANDLE_VALUE;

    memset(&DMXout, 0, sizeof(DMXout));
    DMXout[0] = 0x5A;     // DMX Command Start Byte
    DMXout[1] = 0xA1;     // DMX Controlcommand for 256 channels
    DMXout[258] = 0xA5;   // end of block

    m_dmx_channels_base = ConvertDmxStartChannelsToInt( cfg->getDMX_RGB_Channels(), cfg->getDMX_BaseChannels());
}


CAtmoDmxSerialConnection::~CAtmoDmxSerialConnection() {
    delete m_dmx_channels_base;
}

ATMO_BOOL CAtmoDmxSerialConnection::OpenConnection() {
#if defined(_ATMO_VLC_PLUGIN_)
     char *serdevice = m_pAtmoConfig->getSerialDevice();
     if(!serdevice)
        return ATMO_FALSE;
#else
     int portNummer = m_pAtmoConfig->getComport();
     m_dwLastWin32Error = 0;
	 if(portNummer < 1) return ATMO_FALSE; // make no real sense;-)
#endif

     if(!m_dmx_channels_base)
        return ATMO_FALSE;

	 CloseConnection();

#if !defined(_ATMO_VLC_PLUGIN_)
     char serdevice[16];  // com4294967295
     sprintf(serdevice,"com%d",portNummer);
#endif

#if defined(_WIN32)

     m_hComport = CreateFileA(serdevice, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
     if(m_hComport == INVALID_HANDLE_VALUE) {
//      we have a problem here can't open com port... somebody else may use it?
//	    m_dwLastWin32Error = GetLastError();
	    return ATMO_FALSE;
     }
     /* change serial settings (Speed, stopbits etc.) */
     DCB dcb; // für comport-parameter
     dcb.DCBlength = sizeof(DCB);
     GetCommState (m_hComport, &dcb); // ger current serialport settings
     dcb.BaudRate  = 115200;        // set speed
     dcb.ByteSize  = 8;            // set databits
     dcb.Parity    = NOPARITY;     // set parity
     dcb.StopBits  = ONESTOPBIT;   // set one stop bit
     SetCommState (m_hComport, &dcb);    // apply settings

#else

     int bconst = B115200;
     m_hComport = open(serdevice, O_RDWR |O_NOCTTY);
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

void CAtmoDmxSerialConnection::CloseConnection() {
  if(m_hComport!=INVALID_HANDLE_VALUE) {
#if defined(_WIN32)
     CloseHandle(m_hComport);
#else
     close(m_hComport);
#endif
	 m_hComport = INVALID_HANDLE_VALUE;
  }
}

ATMO_BOOL CAtmoDmxSerialConnection::isOpen(void) {
	 return (m_hComport != INVALID_HANDLE_VALUE);
}

ATMO_BOOL CAtmoDmxSerialConnection::SendData(pColorPacket data) {
   if(m_hComport == INVALID_HANDLE_VALUE)
	  return ATMO_FALSE;

   int iBuffer = 2;
   DWORD iBytesWritten;

   Lock();

   int idx, z = 0;

   for(int i=0;i<getNumChannels();i++) {
       if(m_ChannelAssignment && (i < m_NumAssignedChannels))
         idx = m_ChannelAssignment[i];
       else
         idx = -1;

       if((idx>=0) && (idx<data->numColors)) {
          if( m_dmx_channels_base[z] >= 0 )
              iBuffer = m_dmx_channels_base[z] + 2;
          else
              iBuffer += 3;

          DMXout[iBuffer]   = data->zone[ idx ].r;
          DMXout[iBuffer+1] = data->zone[ idx ].g;
          DMXout[iBuffer+2] = data->zone[ idx ].b;
       }

       if( m_dmx_channels_base[z] >= 0 )
          z++;
   }

#if defined(_WIN32)
   WriteFile(m_hComport, DMXout, 259, &iBytesWritten, NULL); // send to COM-Port
#else
   iBytesWritten = write(m_hComport, DMXout, 259);
   tcdrain(m_hComport);
#endif
   Unlock();

   return (iBytesWritten == 259) ? ATMO_TRUE : ATMO_FALSE;
}


ATMO_BOOL CAtmoDmxSerialConnection::setChannelValues(int numValues,unsigned char *channel_values)
{
	DWORD iBytesWritten;
    if((numValues & 1) || !channel_values)
       return ATMO_FALSE; // numValues must be even!

    /*
       the array shall contain
    */

	Lock();
    int dmxIndex = 0;

    for (int i = 0; i < numValues; i+=2) {
         dmxIndex = ((int)channel_values[i]) + 2;
         DMXout[dmxIndex] = channel_values[i+1];
    }
#if defined(_WIN32)
	WriteFile(m_hComport, DMXout, 259, &iBytesWritten, NULL);
#else
    iBytesWritten = write(m_hComport, DMXout, 259);
    tcdrain(m_hComport);
#endif

    Unlock();

	return (iBytesWritten == 259) ? ATMO_TRUE : ATMO_FALSE;
}


ATMO_BOOL CAtmoDmxSerialConnection::setChannelColor(int channel, tRGBColor color)
{
	DWORD iBytesWritten;
	
    Lock();

    DMXout[channel+0+2]=color.r;
	DMXout[channel+1+2]=color.g;
	DMXout[channel+2+2]=color.b;

#if defined(_WIN32)
	WriteFile(m_hComport, DMXout, 259, &iBytesWritten, NULL);
#else
    iBytesWritten = write(m_hComport, DMXout, 259);
    tcdrain(m_hComport);
#endif


    Unlock();

	return (iBytesWritten == 259) ? ATMO_TRUE : ATMO_FALSE;
}

ATMO_BOOL CAtmoDmxSerialConnection::CreateDefaultMapping(CAtmoChannelAssignment *ca)
{
    if(!ca) return ATMO_FALSE;
    ca->setSize( getNumChannels() );
    for(int i = 0; i < getNumChannels(); i++)
        ca->setZoneIndex(i , i);
    return ATMO_TRUE;
}

#if !defined(_ATMO_VLC_PLUGIN_)

char *CAtmoDmxSerialConnection::getChannelName(int ch)
{
  if(ch < 0) return NULL;
  char buf[30];

  switch(ch) {
      case 0:
          sprintf(buf,"Summenkanal [%d]",ch);
          break;
      case 1:
          sprintf(buf,"Linker Kanal [%d]",ch);
          break;
      case 2:
          sprintf(buf,"Rechter Kanal [%d]",ch);
          break;
      case 3:
          sprintf(buf,"Oberer Kanal [%d]",ch);
          break;
      case 4:
          sprintf(buf,"Unterer Kanal [%d]",ch);
          break;
      default:
          sprintf(buf,"Kanal [%d]",ch);
          break;
  }

  return strdup(buf);
}

ATMO_BOOL CAtmoDmxSerialConnection::ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg)
{
    CDmxConfigDialog *dlg = new CDmxConfigDialog(hInst, parent, cfg);

    INT_PTR result = dlg->ShowModal();

    delete dlg;

    if(result == IDOK)
      return ATMO_TRUE;
    else
      return ATMO_FALSE;
}

#endif


int CAtmoDmxSerialConnection::getNumChannels()
{
    return m_pAtmoConfig->getDMX_RGB_Channels();
}


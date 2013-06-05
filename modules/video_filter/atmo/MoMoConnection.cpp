/*
 * MoMoConnection.cpp: Class for communication with the serial hardware of
 * Atmo Light, opens and configures the serial port
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoDefs.h"
#include "MoMoConnection.h"

#if !defined(_ATMO_VLC_PLUGIN_)
# include "MoMoConfigDialog.h"
#endif

#include <stdio.h>
#include <fcntl.h>

#if !defined(_WIN32)
#include <termios.h>
#include <unistd.h>
#endif


CMoMoConnection::CMoMoConnection(CAtmoConfig *cfg) : CAtmoConnection(cfg) {
    m_hComport = INVALID_HANDLE_VALUE;
}

CMoMoConnection::~CMoMoConnection() {
}

ATMO_BOOL CMoMoConnection::OpenConnection() {
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
     dcb.BaudRate  = 9600;        // set speed
     dcb.ByteSize  = 8;            // set databits
     dcb.Parity    = NOPARITY;     // set parity
     dcb.StopBits  = ONESTOPBIT;   // set one stop bit
     SetCommState (m_hComport, &dcb);    // apply settings

#else

     int bconst = B9600;
     m_hComport = open(serdevice,O_RDWR |O_NOCTTY);
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

void CMoMoConnection::CloseConnection() {
  if(m_hComport!=INVALID_HANDLE_VALUE) {
#if defined(_WIN32)
     CloseHandle(m_hComport);
#else
     close(m_hComport);
#endif
	 m_hComport = INVALID_HANDLE_VALUE;
  }
}

ATMO_BOOL CMoMoConnection::isOpen(void) {
	 return (m_hComport != INVALID_HANDLE_VALUE);
}

ATMO_BOOL CMoMoConnection::SendData(pColorPacket data) {
   if(m_hComport == INVALID_HANDLE_VALUE)
	  return ATMO_FALSE;

   int channels = getNumChannels();
   DWORD bufSize = channels * 3;
   unsigned char *buffer = new unsigned char[ bufSize ];
   DWORD iBytesWritten;

   Lock();
   int i_red   = 0;
   int i_green = channels;
   int i_blue  = channels * 2;
   int idx;
   /*
     3 ch
     i_red = 0, i_green = 3, i_blue = 6
     4 ch
     i_red = 0, i_green = 4, i_blue = 8
   */

   for(int i=0; i < channels ; i++) {
       if(m_ChannelAssignment && (i < m_NumAssignedChannels))
          idx = m_ChannelAssignment[i];
       else
          idx = -1;
       if((idx>=0) && (idx<data->numColors)) {
          buffer[i_red++]   = data->zone[idx].r;
          buffer[i_green++] = data->zone[idx].g;
          buffer[i_blue++]  = data->zone[idx].b;
       } else {
          buffer[i_red++]   = 0;
          buffer[i_green++] = 0;
          buffer[i_blue++]  = 0;
       }
   }

#if defined(_WIN32)
   WriteFile(m_hComport, buffer, bufSize, &iBytesWritten, NULL); // send to COM-Port
#else
   iBytesWritten = write(m_hComport, buffer, bufSize);
   tcdrain(m_hComport);
#endif
   delete []buffer;

   Unlock();

   return (iBytesWritten == bufSize) ? ATMO_TRUE : ATMO_FALSE;
}


ATMO_BOOL CMoMoConnection::CreateDefaultMapping(CAtmoChannelAssignment *ca)
{
   if(!ca) return ATMO_FALSE;
   ca->setSize( getNumChannels() );  // oder 4 ? depending on config!
   ca->setZoneIndex(0, 0); // Zone 5
   ca->setZoneIndex(1, 1);
   ca->setZoneIndex(2, 2);
   ca->setZoneIndex(3, 3);
   return ATMO_TRUE;
}

int CMoMoConnection::getNumChannels()
{
   return m_pAtmoConfig->getMoMo_Channels();
}


#if !defined(_ATMO_VLC_PLUGIN_)

char *CMoMoConnection::getChannelName(int ch)
{
  if(ch < 0) return NULL;
  char buf[30];

  switch(ch) {
      case 0:
          sprintf(buf,"Channel [%d]",ch);
          break;
      case 1:
          sprintf(buf,"Channel [%d]",ch);
          break;
      case 2:
          sprintf(buf,"Channel [%d]",ch);
          break;
      case 3:
          sprintf(buf,"Channel [%d]",ch);
          break;
      default:
          sprintf(buf,"Channel [%d]",ch);
          break;
  }

  return strdup(buf);
}

ATMO_BOOL CMoMoConnection::ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg)
{
    CMoMoConfigDialog *dlg = new CMoMoConfigDialog(hInst, parent, cfg);

    INT_PTR result = dlg->ShowModal();

    delete dlg;

    if(result == IDOK)
      return ATMO_TRUE;
    else
      return ATMO_FALSE;
}

#endif

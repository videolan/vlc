/*
 * AtmoPacketQueue.cpp:  works as connection between the framegrabber (color-preprocessor)
 * and the live output thread. It works as a FIFO for the colorpackets - helps also
 * to synchronize between grabber and liveview threads.
 * especially if the grabber has another framerate as the liveview (25fps)
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoDefs.h"
#include "AtmoPacketQueue.h"

#if defined(_ATMO_VLC_PLUGIN_)
# include <vlc_common.h>
#define MAX_PACKET_TOO_LATE  -30000
#define MAX_PACKET_TOO_EARLY  30000
#define MIN_SLEEP_TIME        15000
#else
#define MAX_PACKET_TOO_LATE  -30
#define MAX_PACKET_TOO_EARLY  30
#define MIN_SLEEP_TIME        15
#endif


#if defined(_ATMO_VLC_PLUGIN_)

CAtmoPacketQueue::CAtmoPacketQueue()
{
  m_first = NULL;
  m_last = NULL;
  m_waitcounter   = 0;
  m_skipcounter   = 0;
  m_framecounter  = 0;
  m_nullpackets   = 0;

  m_avgWait  = 0;
  m_avgDelay = 0;

  vlc_cond_init( &m_PacketArrivedCond );
  vlc_mutex_init( &m_PacketArrivedLock );
  vlc_mutex_init( &m_Lock );
  m_PacketArrived = ATMO_FALSE;
}

#else

CAtmoPacketQueue::CAtmoPacketQueue(CAtmoPacketQueueStatus *statusMonitor)
{
  m_first = NULL;
  m_last = NULL;
  m_waitcounter   = 0;
  m_skipcounter   = 0;
  m_framecounter  = 0;
  m_nullpackets   = 0;

  m_avgWait  = 0;
  m_avgDelay = 0;

  m_StatusMonitor = statusMonitor;

  InitializeCriticalSection(&m_lock);
  m_hPacketArrivedEvent = CreateEvent(NULL,ATMO_FALSE,ATMO_FALSE,NULL);
}

#endif


CAtmoPacketQueue::~CAtmoPacketQueue(void)
{
  ClearQueue();

#if defined(_ATMO_VLC_PLUGIN_)

  vlc_cond_destroy( &m_PacketArrivedCond );
  vlc_mutex_destroy( &m_Lock );

#else

  DeleteCriticalSection( &m_lock );
  CloseHandle(m_hPacketArrivedEvent);
  if(m_StatusMonitor)
     m_StatusMonitor->destroyWindow();

#endif
}

void CAtmoPacketQueue::Lock()
{
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_lock( &m_Lock );
#else
    EnterCriticalSection( &m_lock );
#endif
}

void CAtmoPacketQueue::Unlock()
{
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_unlock( &m_Lock );
#else
    LeaveCriticalSection( &m_lock );
#endif
}

void CAtmoPacketQueue::SignalEvent()
{
#if defined(_ATMO_VLC_PLUGIN_)
   vlc_mutex_lock( &m_PacketArrivedLock );
   m_PacketArrived = ATMO_TRUE;
   vlc_cond_signal( &m_PacketArrivedCond );
   vlc_mutex_unlock( &m_PacketArrivedLock );
#else
   SetEvent( m_hPacketArrivedEvent );
#endif
}

void CAtmoPacketQueue::UnSignalEvent()
{
#if defined(_ATMO_VLC_PLUGIN_)

#else
   ResetEvent( m_hPacketArrivedEvent );
#endif
}

void CAtmoPacketQueue::AddPacket(pColorPacket newPacket)
{
  pColorPacketItem temp = new ColorPacketItem;
  temp->packet = newPacket;
  temp->next = NULL;
#if defined(_ATMO_VLC_PLUGIN_)
  temp->tickcount = mdate();
#else
  temp->tickcount = GetTickCount();
#endif

  Lock();
  if(m_last) {
     m_last->next = temp;
     m_last = temp;
  } else {
     m_last = temp;
     m_first = temp;
  }
  Unlock();
  SignalEvent();
}

pColorPacketItem CAtmoPacketQueue::GetNextPacketContainer()
{
  pColorPacketItem temp = NULL;

  Lock();
  if(m_first) {
     temp      = m_first;
     m_first   = m_first->next;
     if(!m_first)
        m_last = NULL;
     temp->next = NULL;
  }
  Unlock();

  return temp;
}

pColorPacket CAtmoPacketQueue::GetNextPacket()
{
  pColorPacketItem item = GetNextPacketContainer();
  if(item) {
     pColorPacket temp = item->packet;
     delete item;
     return(temp);
  } else
     return(NULL);
}

#if defined(_ATMO_VLC_PLUGIN_)
void CAtmoPacketQueue::ShowQueueStatus(vlc_object_t *p_this)
{
    /*
     show some statistics for the whole time...
    */
    msg_Dbg( p_this, "Skipped Packets: %d", m_skipcounter );
    if( m_skipcounter > 0 )
        msg_Dbg( p_this, "Average Delay: %d ms", (int)(m_avgDelay/m_skipcounter)/1000 );
    msg_Dbg( p_this, "Waited Packets: %d", m_waitcounter );
    if( m_waitcounter > 0 )
        msg_Dbg( p_this, "Average Wait: %d ms", (int)(m_avgWait/m_waitcounter)/1000 );
    msg_Dbg( p_this, "Used Packets: %d", m_framecounter );
    msg_Dbg( p_this, "Null Packets: %d", m_nullpackets );
}
#endif

#if defined(_ATMO_VLC_PLUGIN_)
pColorPacket CAtmoPacketQueue::GetNextPacket(mtime_t timecode, ATMO_BOOL withWait, vlc_object_t *p_this, mtime_t &packet_time)
#else
pColorPacket CAtmoPacketQueue::GetNextPacket(DWORD timecode, ATMO_BOOL withWait, DWORD &packet_time)
#endif
{
#if !defined(_ATMO_VLC_PLUGIN_)
    if(timecode & 0x80000000) // GetTickCount - delay < 0 ;-)
      return NULL;
#endif

   int timeDiff;

   while(1)
   {
     Lock();
     if(!m_first) {
        Unlock();
        break;
     }
     timeDiff    = m_first->tickcount - timecode;
     packet_time = m_first->tickcount;
     Unlock();

     if(timeDiff >= MAX_PACKET_TOO_EARLY) // packet should be process in 35ms or later (usually we are to early for it)
     {
       if( !withWait )
            break;
     }
     else
     {
         if(timeDiff <= MAX_PACKET_TOO_LATE) {
            // we are more than -35ms too late for this packet, skip it and throw it away!
#if defined(_ATMO_VLC_PLUGIN_)
            msg_Dbg( p_this, "getNextPacket skip late %d ms", timeDiff / 1000 );
#endif
            pColorPacket skip = GetNextPacket();
            delete (char *)skip;

            m_skipcounter++;
            m_avgDelay += abs(timeDiff);

            continue;
         }
     }

     if(withWait && timeDiff > MIN_SLEEP_TIME)
     {
          // if this is a sync call, to get in sync with frame source again we wait untils its time!
#if defined(_ATMO_VLC_PLUGIN_)
         msg_Dbg( p_this, "getNextPacket Sleep %d ms", timeDiff / 1000 );
#endif
         do_sleep( timeDiff );

         m_avgWait += timeDiff;
         m_waitcounter++;
     }

     m_framecounter++;
#if !defined(_ATMO_VLC_PLUGIN_)
     if(m_StatusMonitor)
     {
        if(withWait)
           m_StatusMonitor->UpdateValues(m_waitcounter, m_skipcounter, m_framecounter, m_nullpackets, m_avgWait, m_avgDelay);
     }
#endif

     return GetNextPacket();
   }

   m_nullpackets++;
#if !defined(_ATMO_VLC_PLUGIN_)
   if(m_StatusMonitor)
   {
      if(withWait)
         m_StatusMonitor->UpdateValues(m_waitcounter, m_skipcounter, m_framecounter, m_nullpackets, m_avgWait, m_avgDelay);
   }
#endif
   return NULL;
}

ATMO_BOOL CAtmoPacketQueue::WaitForNextPacket(DWORD timeout)
{
    UnSignalEvent();

#if !defined(_ATMO_VLC_PLUGIN_)

    return ( WaitForSingleObject( m_hPacketArrivedEvent, timeout ) == WAIT_OBJECT_0 );

#else

    mtime_t maxWait = mdate() + timeout * 1000;

    vlc_mutex_lock( &m_PacketArrivedLock );
    m_PacketArrived = ATMO_FALSE;
    while(vlc_cond_timedwait( &m_PacketArrivedCond, &m_PacketArrivedLock, maxWait) == 0)
    {
      /*
        condition was set -> but may be an old signal from previous AddPacket
        which is still left - so if m_PacketArrived is still false, wait again
      */
      if(mdate() >= maxWait)
         break;
      if( m_PacketArrived )
         break;
    }
    vlc_mutex_unlock( &m_PacketArrivedLock );
    return m_PacketArrived;

#endif
}

void CAtmoPacketQueue::ClearQueue()
{
  pColorPacketItem next;

  Lock();

  while(m_first)
  {
      next = m_first->next;
      delete (char *)(m_first->packet);
      delete m_first;
      m_first = next;
  }
  m_last = NULL;

  m_waitcounter   = 0;
  m_skipcounter   = 0;
  m_framecounter  = 0;

  m_avgWait  = 0;
  m_avgDelay = 0;
  m_nullpackets = 0;

  Unlock();
}

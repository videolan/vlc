/*
 * AtmoPacketQueue.h:  works as connection between the framegrabber (color-preprocessor)
 * and the live output thread. It works as a FIFO for the colorpackets - helps also
 * to synchronize between grabber and liveview threads.
 * especially if the grabber has another framerate as the liveview (25fps)
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifndef _AtmoPacketQueue_
#define _AtmoPacketQueue_

#include "AtmoDefs.h"
#include "AtmoThread.h"

#if defined(_ATMO_VLC_PLUGIN_)
#  include <vlc_common.h>
#  include <vlc_threads.h>
#else
# include "AtmoPacketQueueStatus.h"
#endif


struct ColorPacketItem {
    pColorPacket packet;
#if defined(_ATMO_VLC_PLUGIN_)
    mtime_t tickcount;
#else
    DWORD tickcount;
#endif
    ColorPacketItem *next;
};
typedef ColorPacketItem* pColorPacketItem;



class CAtmoPacketQueue
{
public:
#if defined(_ATMO_VLC_PLUGIN_)
    CAtmoPacketQueue();
#else
    CAtmoPacketQueue(CAtmoPacketQueueStatus *statusMonitor);
#endif
    ~CAtmoPacketQueue(void);

protected:
    int m_waitcounter;
    int m_skipcounter;
    int m_framecounter;
    int m_nullpackets;
    DWORD m_avgWait;
    DWORD m_avgDelay;

#if !defined(_ATMO_VLC_PLUGIN_)
    CAtmoPacketQueueStatus *m_StatusMonitor;
#endif

private:
    volatile pColorPacketItem m_first;
    volatile pColorPacketItem m_last;

#if defined(_ATMO_VLC_PLUGIN_)
    vlc_cond_t   m_PacketArrivedCond;
    vlc_mutex_t  m_PacketArrivedLock;
    volatile ATMO_BOOL m_PacketArrived;
    vlc_mutex_t  m_Lock;
#else
    CRITICAL_SECTION m_lock;
    HANDLE m_hPacketArrivedEvent;
#endif

private:
    void Lock();
    void Unlock();
    void SignalEvent();
    void UnSignalEvent();

private:
    pColorPacket GetNextPacket();
    pColorPacketItem GetNextPacketContainer();

public:
    void AddPacket(pColorPacket newPacket);

    // timecode = GetTickCount() - framedelay;
#if defined(_ATMO_VLC_PLUGIN_)
    void ShowQueueStatus(vlc_object_t *p_this);
    pColorPacket GetNextPacket(mtime_t timecode, ATMO_BOOL withWait, vlc_object_t *p_this, mtime_t &packet_time );
#else
    pColorPacket GetNextPacket(DWORD timecode, ATMO_BOOL withWait, DWORD &packet_time );
#endif

    void ClearQueue();

    ATMO_BOOL WaitForNextPacket(DWORD timeout);

};

#endif

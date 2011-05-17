/*
 * AtmoLiveView.cpp:  this effect outputs colors as result of a picture
 * content (most complex effect) see thread.c of the linux VDR version -
 * to fully understand what happens here..
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define __STDC_FORMAT_MACROS 1

#include "AtmoDefs.h"
#include "AtmoLiveView.h"
#include "AtmoOutputFilter.h"
#include "AtmoTools.h"

#if defined(_ATMO_VLC_PLUGIN_)
#  include <vlc_common.h>
#else
#  include "AtmoGdiDisplayCaptureInput.h"
#endif

#include "AtmoExternalCaptureInput.h"

#if defined(_ATMO_VLC_PLUGIN_)

CAtmoLiveView::CAtmoLiveView(CAtmoDynData *pAtmoDynData) :
               CThread(pAtmoDynData->getAtmoFilter())
{
    this->m_pAtmoDynData    = pAtmoDynData;
}

#else

CAtmoLiveView::CAtmoLiveView(CAtmoDynData *pAtmoDynData)
{
    this->m_pAtmoDynData  = pAtmoDynData;
}

#endif


CAtmoLiveView::~CAtmoLiveView(void)
{
}


DWORD CAtmoLiveView::Execute(void)
{
#if defined(_ATMO_VLC_PLUGIN_)
    vlc_object_t *m_pLog = m_pAtmoDynData->getAtmoFilter();
    mtime_t ticks;
    mtime_t t;
    mtime_t packet_time;
#else
    DWORD ticks;
    DWORD t;
    DWORD packet_time;
#endif
    int i_frame_counter = -1;

    pColorPacket ColorPacket;
    pColorPacket PreviousPacket = NULL;

    CAtmoConnection *pAtmoConnection = this->m_pAtmoDynData->getAtmoConnection();
    if((pAtmoConnection == NULL) || (pAtmoConnection->isOpen() == ATMO_FALSE)) return 0;

    CAtmoConfig *pAtmoConfig = this->m_pAtmoDynData->getAtmoConfig();

    /*
       this object does post processing of the pixel data
       like jump /scenechange detection fading over the colors
    */
    CAtmoOutputFilter *filter = new CAtmoOutputFilter( this->m_pAtmoDynData->getAtmoConfig() );
    CAtmoPacketQueue *pPacketQueue = this->m_pAtmoDynData->getLivePacketQueue();

    int frameDelay = pAtmoConfig->getLiveView_FrameDelay();

#if defined(_ATMO_VLC_PLUGIN_)
    /*
     because time function of vlc are working with us values instead of ms
    */
    frameDelay = frameDelay * 1000;
#endif

    /*
      wait for the first frame to go in sync with the other thread
    */
    t = get_time;

    if( pPacketQueue->WaitForNextPacket(3000) )
    {
        if( frameDelay > 0 )
            do_sleep( frameDelay );
#if defined(_ATMO_VLC_PLUGIN_)
        msg_Dbg( m_pLog, "First Packet got %"PRId64" ms", (get_time - t) / 1000  );
#endif
    }

    while(this->m_bTerminated == ATMO_FALSE)
    {
        i_frame_counter++;
        if(i_frame_counter == 50) i_frame_counter = 0;

        /* grab current Packet from InputQueue (working as FIFO)! */
#if defined(_ATMO_VLC_PLUGIN_)
        ColorPacket = pPacketQueue->GetNextPacket(get_time - frameDelay, (i_frame_counter == 0), m_pLog, packet_time);
#else
        ColorPacket = pPacketQueue->GetNextPacket(get_time - frameDelay, (i_frame_counter == 0), packet_time);
#endif
        if(ColorPacket)
        {
            /*
              create a packet copy - for later reuse if the input is slower than 25fps
            */
            if(PreviousPacket && (PreviousPacket->numColors == ColorPacket->numColors))
                CopyColorPacket(ColorPacket, PreviousPacket)
            else {
                delete (char *)PreviousPacket;
                DupColorPacket(PreviousPacket, ColorPacket )
            }
        } else {
            /*
              packet queue was empty for the given point of time
            */
            if(i_frame_counter == 0)
            {
#if defined(_ATMO_VLC_PLUGIN_)
                msg_Dbg( m_pLog, "wait for delayed packet..." );
#endif
                t = get_time;
                if( pPacketQueue->WaitForNextPacket(200) )
                {
                    if( frameDelay > 0 )
                        do_sleep( frameDelay );
#if defined(_ATMO_VLC_PLUGIN_)
                    msg_Dbg( m_pLog, "got delayed packet %"PRId64" ms", (mdate() - t) / 1000  );
#endif
                    continue;
                }
            }
            /*
              reuse previous color packet
            */
            DupColorPacket(ColorPacket, PreviousPacket)
        }

        ticks = get_time;

        if(ColorPacket)
        {
            /* pass it through the outputfilters! */
            // Info Filtering will possible free the colorpacket and alloc a new one!
            ColorPacket = filter->Filtering(ColorPacket);

            /* apply gamma correction - only if the hardware isnt capable doing this */
            ColorPacket = CAtmoTools::ApplyGamma(pAtmoConfig, ColorPacket);

            /*
            apply white calibration - only if it is not
            done by the hardware
            */
            if(pAtmoConfig->isUseSoftwareWhiteAdj())
                ColorPacket = CAtmoTools::WhiteCalibration(pAtmoConfig, ColorPacket);

            /* send color data to the the hardware... */
            pAtmoConnection->SendData(ColorPacket);

            delete (char *)ColorPacket;
        }

        /*
            calculate RunTime of thread abbove (doesn't work well - so
            this threads comes out of sync with Image producer and the
            framerate (25fps) drifts away
        */
#if defined(_ATMO_VLC_PLUGIN_)
        ticks = ((mdate() - ticks) + 999)/1000;
#else
        ticks = GetTickCount() - ticks;
#endif
        if(ticks < 40)
        {
            if( ThreadSleep( 40 - ticks ) == ATMO_FALSE )
                break;
        }
    }

#if defined(_ATMO_VLC_PLUGIN_)
    msg_Dbg( m_pLog, "DWORD CAtmoLiveView::Execute(void) terminates");
    pPacketQueue->ShowQueueStatus( m_pLog );
#endif

    delete (char *)PreviousPacket;

    delete filter;
    return 0;
}


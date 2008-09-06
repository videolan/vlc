/*
 * AtmoLiveView.cpp:  this effect outputs colors as result of a picture
 * content (most complex effect) see thread.c of the linux VDR version -
 * to fully understand what happens here..
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#include "AtmoDefs.h"
#include "AtmoLiveView.h"
#include "AtmoOutputFilter.h"
#include "AtmoTools.h"

#if defined(_ATMO_VLC_PLUGIN_)
# include <vlc_common.h>
#else
#  include "AtmoGdiDisplayCaptureInput.h"
#endif

#include "AtmoExternalCaptureInput.h"


#if defined(_ATMO_VLC_PLUGIN_)

CAtmoLiveView::CAtmoLiveView(CAtmoDynData *pAtmoDynData) :
               CThread(pAtmoDynData->getAtmoFilter())
{
    this->m_pAtmoDynData    = pAtmoDynData;
    m_pAtmoInput = NULL;
}

#else

CAtmoLiveView::CAtmoLiveView(CAtmoDynData *pAtmoDynData)
{
    this->m_pAtmoDynData  = pAtmoDynData;
    m_LiveViewSource = lvsGDI;
    m_CurrentLiveViewSource = lvsGDI;
    m_InputChangedEvent = CreateEvent(NULL,ATMO_FALSE,ATMO_FALSE,NULL);
    m_pAtmoInput = NULL;
    InitializeCriticalSection(&m_InputChangeCriticalSection);
}

#endif


CAtmoLiveView::~CAtmoLiveView(void)
{
#if !defined(_ATMO_VLC_PLUGIN_)
   DeleteCriticalSection(&m_InputChangeCriticalSection);
   CloseHandle(m_InputChangedEvent);
#endif
}



#if !defined(_ATMO_VLC_PLUGIN_)

STDMETHODIMP CAtmoLiveView::setLiveViewSource(enum ComLiveViewSource dwModus)
{
    if(dwModus != m_LiveViewSource) {
       m_LiveViewSource = dwModus;
       /*
         you may ask why I don't use a critical section here and directly acces the
         the variable of the Thread?
         Just because you would need very much / often entering / leaving the critical
         section ... and in this case It could be avoid ...

         assigning the value to the "mirror" variable m_LiveViewSource which is compare
         in every run of the thread with its current value ... if there is a change
         the thread can proceed switching the live source ... until this is done
         the thread calling this method is waiting...
       */

       // I don't expect that it will take longer than 500ms to switch...
       if(WaitForSingleObject(m_InputChangedEvent,500) == WAIT_TIMEOUT)
          return S_FALSE; // if not so the switch seems be have failed (badly)
    }
    return S_OK;
}

STDMETHODIMP CAtmoLiveView::getCurrentLiveViewSource(enum ComLiveViewSource *modus) {
     *modus = m_LiveViewSource;
     return S_OK;
}

#endif


DWORD CAtmoLiveView::Execute(void)
{
#if defined(_ATMO_VLC_PLUGIN_)
      mtime_t ticks;
#else
      DWORD ticks;
#endif
      int i_frame_counter = 0;
      CAtmoInput *newInput,*oldInput;
      tColorPacket ColorPacket;

      CAtmoConnection *pAtmoConnection = this->m_pAtmoDynData->getAtmoConnection();
      if((pAtmoConnection == NULL) || (pAtmoConnection->isOpen() == ATMO_FALSE)) return 0;

      CAtmoConfig *pAtmoConfig = this->m_pAtmoDynData->getAtmoConfig();

      /*
         this object does post processing of the pixel data
         like jump /scenechange detection fading over the colors
      */
      CAtmoOutputFilter *filter = new CAtmoOutputFilter(this->m_pAtmoDynData->getAtmoConfig());



#if defined(_ATMO_VLC_PLUGIN_)
      /* this thread is the data preprocess which gets the real 64x48 pixel
         and converts them into the RGB channel values - this is done in
         another thread to keep this thread at a constant timing - to that
         color output is updated 25 times a second
      */
      m_pAtmoInput = new CAtmoExternalCaptureInput(m_pAtmoDynData);
#else
      if(m_LiveViewSource == lvsGDI)
         m_pAtmoInput = new CAtmoGdiDisplayCaptureInput(m_pAtmoDynData);
      else
         m_pAtmoInput = new CAtmoExternalCaptureInput(m_pAtmoDynData);
#endif

      if(m_pAtmoInput->Open() == ATMO_TRUE)
      {
          /*
            wait for the first frame to go in sync with the other thread
          */
#if defined(_ATMO_VLC_PLUGIN_)
          msg_Dbg( m_pAtmoThread, "CAtmoLiveView::Execute(void)");
#endif
          m_pAtmoInput->WaitForNextFrame(500);

          while(this->m_bTerminated == ATMO_FALSE)
          {
              /*  atmoInput - capture Thread Running... */
#if defined(_ATMO_VLC_PLUGIN_)
                ticks = mdate();
#else
                ticks = GetTickCount();
#endif

                /* grab current Packet from Input! */
                ColorPacket = m_pAtmoInput->GetColorPacket();

                /* pass it through the outputfilters! */
                ColorPacket = filter->Filtering(ColorPacket);

                /* apply gamma later ;-) not implemented yet */
                ColorPacket = CAtmoTools::ApplyGamma(pAtmoConfig, ColorPacket);

                /*
                   apply white calibration - only if it is not
                   done by the hardware
                 */
                if(pAtmoConfig->isUseSoftwareWhiteAdj())
                   ColorPacket = CAtmoTools::WhiteCalibration(pAtmoConfig,
                                                              ColorPacket);

                /* send color data to the the hardware... */
                pAtmoConnection->SendData(ColorPacket);

                /*
                   experimental do sync every 100 Frames to the image producer
                   thread because GetTickCount precision is really poor ;-)
                */
                i_frame_counter++;
                if(i_frame_counter == 100) {
                   m_pAtmoInput->WaitForNextFrame(50);
                   i_frame_counter = 0;
#if !defined(WIN32)
/* kludge for pthreads? when running GDB debugger using the same condition variable
   to often results in haging wait timedout...
*/
#ifdef _ATMO_KLUDGE_
                   vlc_mutex_lock( &m_TerminateLock );
                   vlc_cond_destroy( &m_TerminateCond );
                   vlc_cond_init( &m_TerminateCond );
                   vlc_mutex_unlock( &m_TerminateLock );
#endif
#endif
                   continue;
                }


#if !defined(_ATMO_VLC_PLUGIN_)
                /*
                  Check if Input Source has changed - through an async
                  call from the com interface?
                */
                if(m_CurrentLiveViewSource != m_LiveViewSource) {
                   oldInput = m_pAtmoInput;
                   m_pAtmoInput = NULL;

                   if(m_LiveViewSource == lvsGDI) {
                      // create new GDI Input Source...
                      newInput = new CAtmoGdiDisplayCaptureInput(m_pAtmoDynData);
                      newInput->Open(); // should not fail now... hope is the best!
                   } else if(m_LiveViewSource == lvsExternal) {
                      newInput = new CAtmoExternalCaptureInput(m_pAtmoDynData);
                      newInput->Open();
                   }
                   m_CurrentLiveViewSource = m_LiveViewSource;

                   m_pAtmoInput = newInput;

                   oldInput->Close();
                   delete oldInput;

                   /*
                     signal the call to the method "setLiveViewSource" the source
                     was switched...
                   */
                   SetEvent(m_InputChangedEvent);
                   // do sync with input thread
                   m_pAtmoInput->WaitForNextFrame(100);
                   continue;
                }
#endif

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
                    // ThreadSleep -> AtmoThread.cpp
                    if(this->ThreadSleep(40 - ticks)==ATMO_FALSE)
                      break;
                }
          }

          /* shutdown the input processor thread */
          m_pAtmoInput->Close();
      }

      delete m_pAtmoInput;
      m_pAtmoInput = NULL;

#if !defined(_ATMO_VLC_PLUGIN_)
      /*
        if there is a pending call to setLiveViewSource let him surely return before
        destroying the thread and this class instance...
      */
      SetEvent(m_InputChangedEvent);
#endif
      delete filter;
      return 0;
}


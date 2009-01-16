/*
 * AtmoExternalCaptureInput.cpp: Datasource which gets its data via a COM object call
 * or some other external method.
 *
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
*/

#include "AtmoExternalCaptureInput.h"
#include "AtmoTools.h"

#ifndef INT64_C
#define INT64_C(c)  c ## LL
#endif

#if defined(_ATMO_VLC_PLUGIN_)

CAtmoExternalCaptureInput::CAtmoExternalCaptureInput(CAtmoDynData *pAtmoDynData) :
                           CAtmoInput(pAtmoDynData),
                           CThread(pAtmoDynData->getAtmoFilter())
{
    m_pCurrentFramePixels = NULL;
    vlc_cond_init( &m_WakeupCond );
    vlc_mutex_init( &m_WakeupLock );
    msg_Dbg( m_pAtmoThread, "CAtmoExternalCaptureInput created.");

}

#else

CAtmoExternalCaptureInput::CAtmoExternalCaptureInput(CAtmoDynData *pAtmoDynData) :
                           CAtmoInput(pAtmoDynData)
{
    m_hWakeupEvent = CreateEvent(NULL,ATMO_FALSE,ATMO_FALSE,NULL);
    m_pCurrentFramePixels = NULL;
}

#endif

CAtmoExternalCaptureInput::~CAtmoExternalCaptureInput(void)
{
   /* if there is still an unprocessed bufferpicture do kill it */
   if(m_pCurrentFramePixels != NULL)
      free(m_pCurrentFramePixels);

#if defined(_ATMO_VLC_PLUGIN_)
    vlc_cond_destroy( &m_WakeupCond );
    vlc_mutex_destroy(&m_WakeupLock);
    msg_Dbg( m_pAtmoThread, "CAtmoExternalCaptureInput destroyed.");
#else
    CloseHandle(m_hWakeupEvent);
#endif
}

ATMO_BOOL CAtmoExternalCaptureInput::Open()
{
    this->Run();
    return ATMO_TRUE;
}

// Closes the input-device.
// Returns true if the input-device was closed successfully.
ATMO_BOOL CAtmoExternalCaptureInput::Close(void)
{
    this->Terminate();
    return ATMO_TRUE;
}

tColorPacket CAtmoExternalCaptureInput::GetColorPacket(void)
{
    return this->m_ColorPacket;
}

/*
  this method will be called from another thread or possible the COM Server to feed
  new pixeldata into the calculation process it doest just the following:
  1: check if last buffer was allready processed (!m_pCurrentFramePixels)
  2. copy the bitmap info structure into the threads own one
  3. alloc memory for frame
  4. copy sourcepixeldata into own buffer...
  5. let the thread wake up and return imediately to the caller
  so that the real videoout wouldn't be stop for a too long time
*/
void CAtmoExternalCaptureInput::DeliverNewSourceDataPaket(BITMAPINFOHEADER *bmpInfoHeader,void *pixelData)
{
    /*
       normaly we should protect this area of code by critical_section or a mutex,
       but I think we can omit this here because the timing this method is called
       is really slow (in terms of the speed of a modern computer?)
       so it's nearly impossible that two frames are delivert in the same time
       the test needs and malloc needs...
    */
    if( !m_pCurrentFramePixels )
    {
        // Last Frame was processed... take this one...
        memcpy(&m_CurrentFrameHeader,bmpInfoHeader,bmpInfoHeader->biSize);
        int PixelDataSize = m_CurrentFrameHeader.biHeight * m_CurrentFrameHeader.biWidth;
        switch(m_CurrentFrameHeader.biBitCount) {
            case 8:  /* PixelDataSize = PixelDataSize; */ break;
            case 16: PixelDataSize = PixelDataSize * 2; break;
            case 24: PixelDataSize = PixelDataSize * 3; break;
            case 32: PixelDataSize = PixelDataSize * 4; break;
        }
        m_pCurrentFramePixels = malloc(PixelDataSize);
        memcpy(m_pCurrentFramePixels,pixelData,PixelDataSize);
    }
#if defined(_ATMO_VLC_PLUGIN_)
   vlc_mutex_lock( &m_WakeupLock );
   vlc_cond_signal( &m_WakeupCond );
   vlc_mutex_unlock( &m_WakeupLock );
#else
    SetEvent(m_hWakeupEvent);
#endif
}



/*
 the real thread Method which is processing the pixeldata into the hardware channel
 values - which are used by the thread AtmoLiveView...
*/
#if defined (_ATMO_VLC_PLUGIN_)

DWORD CAtmoExternalCaptureInput::Execute(void)
{
    msg_Dbg( m_pAtmoThread, "CAtmoExternalCaptureInput::Execute(void)");
    int i = 0;

    vlc_mutex_lock( &m_WakeupLock );

    while ((this->m_bTerminated == ATMO_FALSE) && (!vlc_object_alive (this->m_pAtmoThread) == false)) {
          int value = vlc_cond_timedwait(&m_WakeupCond, &m_WakeupLock, mdate() + INT64_C(75000));
          if(!value) {
             /* DeliverNewSourceDataPaket delivered new work for me... get it! */
             CalcColors(); // read picture and calculate colors
             this->m_FrameArrived = ATMO_TRUE;
          }
          i++;
          if(i == 100) {
             i = 0;
#if !defined(WIN32)
/* kludge for pthreads? using the same condition variable too often results in hanging the pthread
   call inside vlc_cond_timedwait...
*/
#ifdef _ATMO_KLUDGE_
             vlc_cond_destroy( &m_WakeupCond );
             vlc_cond_init( &m_WakeupCond );
#endif
#endif
          }
    }
    vlc_mutex_unlock( &m_WakeupLock );

    return 0;
}

#else

DWORD CAtmoExternalCaptureInput::Execute(void) {
    HANDLE handles[2];
    handles[0] = this->m_hTerminateEvent;
    handles[1] = m_hWakeupEvent;

    while (this->m_bTerminated == ATMO_FALSE) {
           DWORD event = WaitForMultipleObjects(2,handles,ATMO_FALSE,INFINITE);
           if(event == WAIT_OBJECT_0) {
              // Terminate Thread Event was set... say good bye...!
              break;
           }
           if(event == (WAIT_OBJECT_0+1)) {
              CalcColors(); // read picture and calculate colors
              this->m_FrameArrived = ATMO_TRUE;
           }
    }
    return 0;
}

#endif


void CAtmoExternalCaptureInput::WaitForNextFrame(DWORD timeout)
{
    this->m_FrameArrived = ATMO_FALSE;
    for(DWORD i=0;(i<timeout) && !m_FrameArrived;i++)
#if defined (_ATMO_VLC_PLUGIN_)
        msleep(1000);
#else
        Sleep(1);
#endif

    if(this->m_pAtmoDynData)
    {
        CAtmoConfig *cfg = this->m_pAtmoDynData->getAtmoConfig();
        if(cfg)
        {
            int delay = cfg->getLiveView_FrameDelay();
            if(delay > 0)
            {
#if defined (_ATMO_VLC_PLUGIN_)
              msleep(delay * 1000);
#else
              Sleep(delay);
#endif
            }
        }
    }
}

void CAtmoExternalCaptureInput::CalcColors() {
     // take data from m_CurrentFrameHeader and m_pCurrentFramePixels .. process for atmo ...
    tHSVColor HSV_Img[IMAGE_SIZE];
    tRGBColor pixelColor;
    int srcIndex,index = 0;
    memset(&HSV_Img,0,sizeof(HSV_Img));

     // Convert Data to HSV values.. bla bla....
    if(m_pCurrentFramePixels!=NULL)
    {
        if((m_CurrentFrameHeader.biWidth == CAP_WIDTH) && (m_CurrentFrameHeader.biHeight == CAP_HEIGHT))
        {

          // HSVI = HSV Image allready in right format just copy the easiest task
          // und weiterverarbeiten lassen
#ifdef _ATMO_VLC_PLUGIN_
          if(m_CurrentFrameHeader.biCompression ==  VLC_FOURCC('H','S','V','I'))
#else
          if(m_CurrentFrameHeader.biCompression ==  MakeDword('H','S','V','I'))
#endif
          {
              memcpy( &HSV_Img, m_pCurrentFramePixels, CAP_WIDTH * CAP_HEIGHT * sizeof(tHSVColor));
          }
          else if(m_CurrentFrameHeader.biCompression == BI_RGB)
          {
             if(m_CurrentFrameHeader.biBitCount == 16)
             {
                 unsigned short *buffer = (unsigned short *)m_pCurrentFramePixels;

                 for(int y=0;y<CAP_HEIGHT;y++)
                 {
                     srcIndex = y * CAP_WIDTH;
                     for(int x=0;x<CAP_WIDTH;x++)
                     {
                         pixelColor.b = (buffer[srcIndex] & 31) << 3;
                         pixelColor.g = ((buffer[srcIndex] >> 5) & 31) << 3;
                         pixelColor.r = ((buffer[srcIndex] >> 10) & 63) << 2;
                         srcIndex++;
                         HSV_Img[index++] = RGB2HSV(pixelColor);
                     }
                 }
             }
             else if(m_CurrentFrameHeader.biBitCount == 24)
             {
                 for(int y=0;y<CAP_HEIGHT;y++)
                 {
                     srcIndex = y * (CAP_WIDTH*3);
                     for(int x=0;x<CAP_WIDTH;x++)
                     {
                         pixelColor.b = ((unsigned char *)m_pCurrentFramePixels)[srcIndex++];
                         pixelColor.g = ((unsigned char *)m_pCurrentFramePixels)[srcIndex++];
                         pixelColor.r = ((unsigned char *)m_pCurrentFramePixels)[srcIndex++];
                         HSV_Img[index++] = RGB2HSV(pixelColor);
                     }
                 }
             }
             else if(m_CurrentFrameHeader.biBitCount == 32)
             {
                 for(int y=0;y<CAP_HEIGHT;y++)
                 {
                     srcIndex = y * (CAP_WIDTH*4);
                     for(int x=0;x<CAP_WIDTH;x++)
                     {
                         pixelColor.b = ((unsigned char *)m_pCurrentFramePixels)[srcIndex++];
                         pixelColor.g = ((unsigned char *)m_pCurrentFramePixels)[srcIndex++];
                         pixelColor.r = ((unsigned char *)m_pCurrentFramePixels)[srcIndex++];
                         srcIndex++;
                         HSV_Img[index++] = RGB2HSV(pixelColor);
                     }
                 }
             }
          }
       }

       /*
          if the image color format wasn't recognized - the output
          will be black (memset)
       */

       /*
          now convert the pixeldata into one RGB trippel for each channel,
          this is done by some very sophisticated methods and statistics ...

          the only thing I know - the pixel priority is controled by some
          gradients for each edge of the picture

          (sorry I don't know how it exactly works because the formulars
           are done by some one else...)
       */
       m_ColorPacket = CalcColorsAnalyzeHSV(this->m_pAtmoDynData->getAtmoConfig(), HSV_Img);

       /* remove the source buffe */
       free(m_pCurrentFramePixels);
       /*
          the buffer zereo so that deliver new data paket will wakeup the
          thread on  the next frame again
       */
       m_pCurrentFramePixels = NULL;
    }
}


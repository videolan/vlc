/*
 * AtmoExternalCaptureInput.cpp: Datasource which gets its data via a COM object call
 * or some other external method.
 *
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoExternalCaptureInput.h"
#include "AtmoTools.h"


#if defined(_ATMO_VLC_PLUGIN_)


#ifndef INT64_C
#define INT64_C(c)  c ## LL
#endif

CAtmoExternalCaptureInput::CAtmoExternalCaptureInput(CAtmoDynData *pAtmoDynData) :
                           CAtmoInput(pAtmoDynData)
{
    vlc_cond_init( &m_WakeupCond );
    vlc_mutex_init( &m_WakeupLock );
    m_pCurrentFramePixels = NULL;
    m_pLog = pAtmoDynData->getAtmoFilter();
}

#else

CAtmoExternalCaptureInput::CAtmoExternalCaptureInput(CAtmoDynData *pAtmoDynData) :
                           CAtmoInput(pAtmoDynData)
{
    m_hWakeupEvent = CreateEvent(NULL,0,0,NULL);
    InitializeCriticalSection( &m_BufferLock );
    m_pCurrentFramePixels = NULL;
}

#endif

CAtmoExternalCaptureInput::~CAtmoExternalCaptureInput(void)
{
   /* if there is still an unprocessed bufferpicture do kill it */
#if defined(_ATMO_VLC_PLUGIN_)
   vlc_mutex_lock( &m_WakeupLock );
   free( m_pCurrentFramePixels );
   vlc_mutex_unlock( &m_WakeupLock );

   vlc_cond_destroy( &m_WakeupCond );
   vlc_mutex_destroy( &m_WakeupLock );
#else
   EnterCriticalSection( &m_BufferLock );
   free( m_pCurrentFramePixels );
   LeaveCriticalSection( &m_BufferLock );
   CloseHandle(m_hWakeupEvent);
   DeleteCriticalSection( &m_BufferLock );
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
void CAtmoExternalCaptureInput::DeliverNewSourceDataPaket(VLC_BITMAPINFOHEADER *bmpInfoHeader,void *pixelData)
{
    /*
       normaly we should protect this area of code by critical_section or a mutex,
       but I think we can omit this here because the timing this method is called
       is really slow (in terms of the speed of a modern computer?)
       so it's nearly impossible that two frames are delivert in the same time
       the test needs and malloc needs...
    */
#if defined(_ATMO_VLC_PLUGIN_)
//    msg_Dbg( m_pLog, "DeliverNewSourceDataPaket start...");
    vlc_mutex_lock( &m_WakeupLock );
#else
    EnterCriticalSection( &m_BufferLock );
#endif
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
    vlc_cond_signal( &m_WakeupCond );
    vlc_mutex_unlock( &m_WakeupLock );
    // msg_Dbg( m_pLog, "DeliverNewSourceDataPaket done.");
#else
    SetEvent(m_hWakeupEvent);
    LeaveCriticalSection( &m_BufferLock );
#endif
}



/*
 the real thread Method which is processing the pixeldata into the hardware channel
 values - which are used by the thread AtmoLiveView...
*/
#if defined (_ATMO_VLC_PLUGIN_)

DWORD CAtmoExternalCaptureInput::Execute(void)
{
    while (this->m_bTerminated == ATMO_FALSE) {
          vlc_mutex_lock( &m_WakeupLock );
          vlc_cond_timedwait(&m_WakeupCond, &m_WakeupLock, mdate() + 75000 );

          /* DeliverNewSourceDataPaket delivered new work for me... get it! */
          if(m_pCurrentFramePixels)
             CalcColors(); // read picture and calculate colors
          vlc_mutex_unlock( &m_WakeupLock );
    }

    msg_Dbg( m_pLog, "DWORD CAtmoExternalCaptureInput::Execute(void) bailed out?");

    return 0;
}

#else

DWORD CAtmoExternalCaptureInput::Execute(void) {
    HANDLE handles[2];
    handles[0] = this->m_hTerminateEvent;
    handles[1] = m_hWakeupEvent;

    while (this->m_bTerminated == ATMO_FALSE) {
           DWORD event = WaitForMultipleObjects(2, &handles[0], FALSE, INFINITE);
           if(event == WAIT_OBJECT_0) {
              // Terminate Thread Event was set... say good bye...!
              break;
           }
           if(event == (WAIT_OBJECT_0+1)) {
              EnterCriticalSection( &m_BufferLock );
              if(m_pCurrentFramePixels)
                 CalcColors(); // read picture and calculate colors
              LeaveCriticalSection( &m_BufferLock );
           }
    }
    return 0;
}

#endif


void CAtmoExternalCaptureInput::CalcColors()
{
     // take data from m_CurrentFrameHeader and m_pCurrentFramePixels .. process for atmo ...
    tHSVColor HSV_Img[IMAGE_SIZE];
    tRGBColor pixelColor;
    int srcIndex,index = 0;
    memset(&HSV_Img,0,sizeof(HSV_Img));

    // msg_Dbg( m_pLog, "CalcColors start...");

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
      else {
        if the image color format wasn't recognized - the output
        will be black (memset)
        }
    */

    /* remove the source buffer */
    free( m_pCurrentFramePixels );
    m_pCurrentFramePixels = NULL;

    /*
        now convert the pixeldata into one RGB trippel for each channel,
        this is done by some very sophisticated methods and statistics ...

        the only thing I know - the pixel priority is controled by some
        gradients for each edge of the picture

        (sorry I don't know how it exactly works because the formulas
        are done by some one else...)
    */
    //msg_Dbg( m_pLog, "CalcColors ende AddPacket...");
    m_pAtmoDynData->getLivePacketQueue()->AddPacket( m_pAtmoColorCalculator->AnalyzeHSV( HSV_Img ) );
    //msg_Dbg( m_pLog, "CalcColors ende AddPacket...done.");
}


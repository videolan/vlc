/*
 * AtmoTools.cpp: Collection of tool and helperfunction
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#include "AtmoTools.h"
#include "AtmoLiveView.h"
#include "AtmoSerialConnection.h"

#if !defined(_ATMO_VLC_PLUGIN_)
#   include "AtmoColorChanger.h"
#   include "AtmoLeftRightColorChanger.h"
#   include "AtmoDummyConnection.h"
#   include "AtmoDmxSerialConnection.h"
#endif


CAtmoTools::CAtmoTools(void)
{
}

CAtmoTools::~CAtmoTools(void)
{
}

void CAtmoTools::ShowShutdownColor(CAtmoDynData *pDynData)
{
    pDynData->LockCriticalSection();

    CAtmoConnection *atmoConnection = pDynData->getAtmoConnection();
    CAtmoConfig *atmoConfig = pDynData->getAtmoConfig();
    if((atmoConnection != NULL) && (atmoConfig!=NULL)) {
       int r[ATMO_NUM_CHANNELS],g[ATMO_NUM_CHANNELS],b[ATMO_NUM_CHANNELS],i;
       // set a special color? on shutdown of the software? mostly may use black or so ...
       // if this function ist disabled ... atmo will continuing to show the last color...
       if(atmoConnection->isOpen() == ATMO_TRUE) {
	        if(atmoConfig->isSetShutdownColor() == 1) {
		        for(i=0;i<ATMO_NUM_CHANNELS;i++) {
			        r[i] = atmoConfig->getShutdownColor_Red();
			        g[i] = atmoConfig->getShutdownColor_Green();
			        b[i] = atmoConfig->getShutdownColor_Blue();
		        }
                atmoConnection->SendData(ATMO_NUM_CHANNELS,r,g,b);
	        }
       }
	}

    pDynData->UnLockCriticalSection();
}

EffectMode CAtmoTools::SwitchEffect(CAtmoDynData *pDynData, EffectMode newEffectMode)
{
    // may need a critical section??
    if(pDynData == NULL) {
       return emUndefined;
    }
    pDynData->LockCriticalSection();

    CAtmoConfig *atmoConfig = pDynData->getAtmoConfig();
    if(atmoConfig == NULL) {
       pDynData->UnLockCriticalSection();
       return emUndefined;
    }
    CAtmoConnection *atmoConnection = pDynData->getAtmoConnection();

    EffectMode oldEffectMode = atmoConfig->getEffectMode();
    CThread *currentEffect = pDynData->getEffectThread();

    // stop and delete/cleanup current Effect Thread...
    pDynData->setEffectThread(NULL);
    if(currentEffect!=NULL) {
       currentEffect->Terminate();
       delete currentEffect;
       currentEffect = NULL;
    }

    if((atmoConnection!=NULL) && (atmoConnection->isOpen()==ATMO_TRUE)) {
        // neuen EffectThread nur mit aktiver Connection starten...

        switch(newEffectMode) {
            case emDisabled:
                break;

            case emStaticColor:
                    // get values from config - and put them to all channels?
                    int r[ATMO_NUM_CHANNELS],g[ATMO_NUM_CHANNELS],b[ATMO_NUM_CHANNELS];
                    for(int i=0;i<ATMO_NUM_CHANNELS;i++) {
                        r[i] = (atmoConfig->getStaticColor_Red()   * atmoConfig->getWhiteAdjustment_Red())/255;
                        g[i] = (atmoConfig->getStaticColor_Green() * atmoConfig->getWhiteAdjustment_Green())/255;
                        b[i] = (atmoConfig->getStaticColor_Blue()  * atmoConfig->getWhiteAdjustment_Blue())/255;
                    }
                    atmoConnection->SendData(ATMO_NUM_CHANNELS,r,g,b);
                break;

            case emLivePicture:
                currentEffect = new CAtmoLiveView(pDynData);
                break;

#if !defined(_ATMO_VLC_PLUGIN_)
            case emColorChange:
                currentEffect = new CAtmoColorChanger(atmoConnection, atmoConfig);
                break;
#endif

#if !defined(_ATMO_VLC_PLUGIN_)
            case emLrColorChange:
                currentEffect = new CAtmoLeftRightColorChanger(atmoConnection, atmoConfig);
                break;
#endif
        }

    }

    atmoConfig->setEffectMode(newEffectMode);

    pDynData->setEffectThread(currentEffect);

    if(currentEffect!=NULL)
       currentEffect->Run();

    pDynData->UnLockCriticalSection();
    return oldEffectMode;
}

ATMO_BOOL CAtmoTools::RecreateConnection(CAtmoDynData *pDynData)
{
    pDynData->LockCriticalSection();

    CAtmoConnection *current = pDynData->getAtmoConnection();
    AtmoConnectionType act = pDynData->getAtmoConfig()->getConnectionType();
    pDynData->setAtmoConnection(NULL);
    if(current != NULL) {
       current->CloseConnection();
       delete current;
    }

    switch(act) {
           case actSerialPort: {
               CAtmoSerialConnection *tempConnection = new CAtmoSerialConnection(pDynData->getAtmoConfig());
               if(tempConnection->OpenConnection() == ATMO_FALSE) {
#if !defined(_ATMO_VLC_PLUGIN_)
                  char errorMsgBuf[200];
                  sprintf(errorMsgBuf,"Failed to open serial port com%d with errorcode: %d (0x%x)",
                            pDynData->getAtmoConfig()->getComport(),
                            tempConnection->getLastError(),
                            tempConnection->getLastError()
                        );
                  MessageBox(0,errorMsgBuf,"Error",MB_ICONERROR | MB_OK);
#endif
                  delete tempConnection;

                  pDynData->UnLockCriticalSection();
                  return ATMO_FALSE;
               }
               pDynData->setAtmoConnection(tempConnection);

               CAtmoTools::SetChannelAssignment(pDynData,
                           pDynData->getAtmoConfig()->getCurrentChannelAssignment());

               pDynData->UnLockCriticalSection();
               return ATMO_TRUE;
           }

#if !defined(_ATMO_VLC_PLUGIN_)
           case actDummy: {
               CAtmoDummyConnection *tempConnection = new CAtmoDummyConnection(pDynData->getHinstance(),
                                                                               pDynData->getAtmoConfig());
               if(tempConnection->OpenConnection() == ATMO_FALSE) {
                  delete tempConnection;

                  pDynData->UnLockCriticalSection();
                  return ATMO_FALSE;
               }
               pDynData->setAtmoConnection(tempConnection);

               CAtmoTools::SetChannelAssignment(pDynData, pDynData->getAtmoConfig()->getCurrentChannelAssignment());

               pDynData->UnLockCriticalSection();
               return ATMO_TRUE;
           }

           case actDMX: {
               // create here your DMX connections... instead of the dummy....
               CAtmoDmxSerialConnection *tempConnection = new CAtmoDmxSerialConnection(pDynData->getAtmoConfig());
               if(tempConnection->OpenConnection() == ATMO_FALSE) {
                  delete tempConnection;

                  pDynData->UnLockCriticalSection();
                  return ATMO_FALSE;
               }
               pDynData->setAtmoConnection(tempConnection);

               CAtmoTools::SetChannelAssignment(pDynData, pDynData->getAtmoConfig()->getCurrentChannelAssignment());

               pDynData->UnLockCriticalSection();
               return ATMO_TRUE;
           }
#endif

           default: {
               pDynData->UnLockCriticalSection();
               return ATMO_FALSE;
           }
    }
}

tColorPacket CAtmoTools::WhiteCalibration(CAtmoConfig *pAtmoConfig, tColorPacket ColorPacket)
{
    int w_adj_red   = pAtmoConfig->getWhiteAdjustment_Red();
    int w_adj_green = pAtmoConfig->getWhiteAdjustment_Green();
    int w_adj_blue  = pAtmoConfig->getWhiteAdjustment_Blue();

    for (int i = 0; i < ATMO_NUM_CHANNELS; i++)  {
        ColorPacket.channel[i].r = (unsigned char)(((int)w_adj_red   * (int)ColorPacket.channel[i].r)   / 255);
        ColorPacket.channel[i].g = (unsigned char)(((int)w_adj_green * (int)ColorPacket.channel[i].g) / 255);
        ColorPacket.channel[i].b = (unsigned char)(((int)w_adj_blue  * (int)ColorPacket.channel[i].b)  / 255);
    }
    return ColorPacket;
}

tColorPacket CAtmoTools::ApplyGamma(CAtmoConfig *pAtmoConfig, tColorPacket ColorPacket)
{
  return ColorPacket;
}

int CAtmoTools::SetChannelAssignment(CAtmoDynData *pDynData, int index)
{
    CAtmoConfig *pAtmoConfig = pDynData->getAtmoConfig();
    CAtmoConnection *pAtmoConnection = pDynData->getAtmoConnection();
    int oldIndex = pAtmoConfig->getCurrentChannelAssignment();

    tChannelAssignment *ca = pAtmoConfig->getChannelAssignment(index);
    if((ca!=NULL) && (pAtmoConnection!=NULL)) {
        pAtmoConnection->SetChannelAssignment(ca);
        pAtmoConfig->setCurrentChannelAssignment(index);
    }
    return oldIndex;
}


#if !defined(_ATMO_VLC_PLUGIN_)

void CAtmoTools::SaveBitmap(HDC hdc,HBITMAP hBmp,char *fileName) {
     BITMAPINFO bmpInfo;
     BITMAPFILEHEADER  bmpFileHeader;
     ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
     bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);

     GetDIBits(hdc,hBmp,0,0,NULL,&bmpInfo,DIB_RGB_COLORS);
     if(bmpInfo.bmiHeader.biSizeImage<=0)
        bmpInfo.bmiHeader.biSizeImage=bmpInfo.bmiHeader.biWidth * abs(bmpInfo.bmiHeader.biHeight)*(bmpInfo.bmiHeader.biBitCount+7)/8;
     void *pBuf = malloc(bmpInfo.bmiHeader.biSizeImage);
     bmpInfo.bmiHeader.biCompression=BI_RGB;

     GetDIBits(hdc,hBmp,0,bmpInfo.bmiHeader.biHeight,pBuf, &bmpInfo, DIB_RGB_COLORS);


     bmpFileHeader.bfReserved1=0;
     bmpFileHeader.bfReserved2=0;
     bmpFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmpInfo.bmiHeader.biSizeImage;
#ifdef _ATMO_VLC_PLUGIN_
     bmpFileHeader.bfType = VLC_TWOCC('M','B');
#else
     bmpFileHeader.bfType = MakeWord('M','B');
#endif
     bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);


     FILE *fp = NULL;
     fp = fopen(fileName,"wb");
     fwrite(&bmpFileHeader,sizeof(BITMAPFILEHEADER),1,fp);
     fwrite(&bmpInfo.bmiHeader,sizeof(BITMAPINFOHEADER),1,fp);
     fwrite(pBuf,bmpInfo.bmiHeader.biSizeImage,1,fp);
     fclose(fp);
}

#endif


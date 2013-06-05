
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "AtmoDefs.h"

#if defined (_WIN32)
#  include <windows.h>
#else
#  include <vlc_codecs.h>
#endif

#include <math.h>
#include <stdio.h>
#include "AtmoZoneDefinition.h"

CAtmoZoneDefinition::CAtmoZoneDefinition(void)
{
}

CAtmoZoneDefinition::~CAtmoZoneDefinition(void)
{
}

void CAtmoZoneDefinition::Fill(unsigned char value)
{
  for(int i=0; i < IMAGE_SIZE; i++)
      m_BasicWeight[i] = value;
}


// max weight to left
void CAtmoZoneDefinition::FillGradientFromLeft(int start_row,int end_row)
{
   int index;
   unsigned char col_norm;
   index = start_row * CAP_WIDTH;
   for(int row=start_row; row < end_row; row++) {
       for(int col=0; col < CAP_WIDTH; col++) {
           // should be a value between 0 .. 255?
           col_norm = (255 * (CAP_WIDTH-col-1)) / (CAP_WIDTH-1);
           m_BasicWeight[index++] = col_norm;
       }
   }
}

// max weight to right
void CAtmoZoneDefinition::FillGradientFromRight(int start_row,int end_row)
{
   int index;
   unsigned char col_norm;
   index = start_row * CAP_WIDTH;
   for(int row=start_row; row < end_row; row++) {
      for(int col=0; col < CAP_WIDTH; col++) {
          col_norm = (255 * col) / (CAP_WIDTH-1); // should be a value between 0 .. 255?
          m_BasicWeight[index++] = col_norm;
       }
   }
}

// max weight from top
void CAtmoZoneDefinition::FillGradientFromTop(int start_col,int end_col)
{
   int index;
   unsigned char row_norm;

   for(int row=0; row < CAP_HEIGHT; row++) {
       index = row * CAP_WIDTH + start_col;

       row_norm = (255 * (CAP_HEIGHT-row-1)) / (CAP_HEIGHT-1); // should be a value between 0 .. 255?
       for(int col=start_col; col < end_col; col++) {
           m_BasicWeight[index++] = row_norm;
       }
   }
}

// max weight from bottom
void CAtmoZoneDefinition::FillGradientFromBottom(int start_col,int end_col)
{
   int index;
   unsigned char row_norm;
   for(int row=0; row < CAP_HEIGHT; row++) {
       index = row * CAP_WIDTH + start_col;
       row_norm = (255 * row) / (CAP_HEIGHT-1); // should be a value between 0 .. 255?
       for(int col=start_col; col < end_col; col++) {
           m_BasicWeight[index++] = row_norm;
       }
   }
}

#if !defined(_ATMO_VLC_PLUGIN_)

void CAtmoZoneDefinition::SaveZoneBitmap(char *fileName)
{
     if(!fileName) return;

     BITMAPINFO bmpInfo;
     // BITMAPINFOHEADER
     BITMAPFILEHEADER  bmpFileHeader;
     ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
     bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);


     bmpInfo.bmiHeader.biHeight = -CAP_HEIGHT;
     bmpInfo.bmiHeader.biWidth  = CAP_WIDTH;
     bmpInfo.bmiHeader.biSizeImage = abs(bmpInfo.bmiHeader.biHeight) * bmpInfo.bmiHeader.biWidth * 3;

     unsigned char *pBuf = (unsigned char *)malloc(bmpInfo.bmiHeader.biSizeImage);
     for(int y=0; y < CAP_HEIGHT; y++ )
     {
         for(int x=0; x < CAP_WIDTH; x++)
         {
             pBuf[y * CAP_WIDTH * 3 + x * 3 ] = 0;
             pBuf[y * CAP_WIDTH * 3 + x * 3 + 1 ] = m_BasicWeight[y * CAP_WIDTH + x];
             pBuf[y * CAP_WIDTH * 3 + x * 3 + 2] = 0;
         }
     }

     bmpInfo.bmiHeader.biCompression = BI_RGB;
     bmpInfo.bmiHeader.biPlanes = 1;
     bmpInfo.bmiHeader.biBitCount = 24;

     bmpFileHeader.bfReserved1=0;
     bmpFileHeader.bfReserved2=0;
     bmpFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmpInfo.bmiHeader.biSizeImage;
     bmpFileHeader.bfType = MakeIntelWord('M','B');
     bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);


     FILE *fp = NULL;
     fp = fopen(fileName,"wb");
     fwrite(&bmpFileHeader,sizeof(BITMAPFILEHEADER),1,fp);
     fwrite(&bmpInfo.bmiHeader,sizeof(BITMAPINFOHEADER),1,fp);
     fwrite(pBuf,bmpInfo.bmiHeader.biSizeImage,1,fp);
     fclose(fp);
     free(pBuf);
}

void CAtmoZoneDefinition::SaveWeightBitmap(char *fileName,int *weight)
{
     if(!fileName || !weight) return;

     BITMAPINFO bmpInfo;
     // BITMAPINFOHEADER
     BITMAPFILEHEADER  bmpFileHeader;
     ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
     bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);


     bmpInfo.bmiHeader.biHeight = -CAP_HEIGHT;
     bmpInfo.bmiHeader.biWidth  = CAP_WIDTH;
     bmpInfo.bmiHeader.biSizeImage = abs(bmpInfo.bmiHeader.biHeight) * bmpInfo.bmiHeader.biWidth * 3;

     unsigned char *pBuf = (unsigned char *)malloc(bmpInfo.bmiHeader.biSizeImage);
     for(int y=0; y < CAP_HEIGHT; y++ )
     {
         for(int x=0; x < CAP_WIDTH; x++)
         {
             pBuf[y * CAP_WIDTH * 3 + x * 3 ] = 0;
             pBuf[y * CAP_WIDTH * 3 + x * 3 + 1 ] = (unsigned char)weight[y * CAP_WIDTH + x];
             pBuf[y * CAP_WIDTH * 3 + x * 3 + 2] = 0;
         }
     }

     bmpInfo.bmiHeader.biCompression = BI_RGB;
     bmpInfo.bmiHeader.biPlanes = 1;
     bmpInfo.bmiHeader.biBitCount = 24;

     bmpFileHeader.bfReserved1=0;
     bmpFileHeader.bfReserved2=0;
     bmpFileHeader.bfSize=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+bmpInfo.bmiHeader.biSizeImage;
     bmpFileHeader.bfType = MakeIntelWord('M','B');
     bmpFileHeader.bfOffBits=sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER);


     FILE *fp = NULL;
     fp = fopen(fileName,"wb");
     fwrite(&bmpFileHeader,sizeof(BITMAPFILEHEADER),1,fp);
     fwrite(&bmpInfo.bmiHeader,sizeof(BITMAPINFOHEADER),1,fp);
     fwrite(pBuf,bmpInfo.bmiHeader.biSizeImage,1,fp);
     fclose(fp);
     free(pBuf);
}


#endif



int CAtmoZoneDefinition::LoadGradientFromBitmap(char *pszBitmap)
{
  // transform 256 color image (gray scale!)
  // into m_basicWeight or use the GREEN value of a 24bit image!
  // channel of a true color bitmap!
  VLC_BITMAPINFO bmpInfo;
  BITMAPFILEHEADER  bmpFileHeader;

  /*
  ATMO_LOAD_GRADIENT_FILENOTFOND
#define ATMO_LOAD_GRADIENT_OK  0
#define ATMO_LOAD_GRADIENT_FAILED_SIZE    1
#define ATMO_LOAD_GRADIENT_FAILED_HEADER  2
  */


   FILE *bmp = fopen(pszBitmap, "rb");
   if(!bmp)
    return ATMO_LOAD_GRADIENT_FILENOTFOND;

    if(fread(&bmpFileHeader, sizeof(BITMAPFILEHEADER), 1, bmp) != 1)
    {
        fclose(bmp);
        return ATMO_LOAD_GRADIENT_FAILED_SIZE;
    }

    if(bmpFileHeader.bfType != MakeIntelWord('M','B'))
    {
        fclose(bmp);
        return ATMO_LOAD_GRADIENT_FAILED_HEADER;
    }

    if(fread(&bmpInfo, sizeof(VLC_BITMAPINFO), 1, bmp) != 1)
    {
        fclose(bmp);
        return ATMO_LOAD_GRADIENT_FAILED_SIZE;
    }

    if(bmpInfo.bmiHeader.biCompression != BI_RGB)
    {
        fclose(bmp);
        return ATMO_LOAD_GRADIENT_FAILED_FORMAT;
    }
    if((bmpInfo.bmiHeader.biBitCount != 8) && (bmpInfo.bmiHeader.biBitCount != 24))
    {
        fclose(bmp);
        return ATMO_LOAD_GRADIENT_FAILED_FORMAT;
    }

    int width = bmpInfo.bmiHeader.biWidth;
    int height = bmpInfo.bmiHeader.biHeight;
    ATMO_BOOL invertDirection = (height > 0);
    height = abs(height);
    if((width != CAP_WIDTH) || (height != CAP_HEIGHT))
    {
        fclose(bmp);
        return ATMO_LOAD_GRADIENT_FAILED_SIZE;
    }

    fseek(bmp, bmpFileHeader.bfOffBits, SEEK_SET);

    int imageSize = width * height * bmpInfo.bmiHeader.biBitCount/8;

    unsigned char *pixelBuffer = (unsigned char *)malloc(imageSize);
    if(fread(pixelBuffer,imageSize,1,bmp) != 1)
    {
        free(pixelBuffer);
        fclose(bmp);
        return ATMO_LOAD_GRADIENT_FAILED_SIZE;
    }

    if(bmpInfo.bmiHeader.biBitCount == 8)
    {
        int ydest;
        for(int y=0;y < CAP_HEIGHT; y++) {
            if(invertDirection) {
                ydest = (CAP_HEIGHT - y - 1);
            } else {
                ydest = y;
            }
            for(int x=0;x < CAP_WIDTH; x++) {
                // palette should be grey scale - so that index 0 is black and
                // index 255 means white!
                // everything else would produce funny results!
                m_BasicWeight[ydest * CAP_WIDTH + x] =
                    pixelBuffer[y * CAP_WIDTH + x];
            }
        }
    }

    if(bmpInfo.bmiHeader.biBitCount == 24)
    {
        int ydest;
        for(int y=0;y < CAP_HEIGHT; y++) {
            if(invertDirection) {
                ydest = (CAP_HEIGHT - y - 1);
            } else {
                ydest = y;
            }
            for(int x=0;x < CAP_WIDTH; x++) {
                // use the green value as reference...
                m_BasicWeight[ydest * CAP_WIDTH + x] =
                    pixelBuffer[y * CAP_WIDTH * 3 + (x*3) + 1 ];
            }
        }
    }
    free(pixelBuffer);
    fclose(bmp);

    return ATMO_LOAD_GRADIENT_OK;
}


void CAtmoZoneDefinition::UpdateWeighting(int *destWeight,
                                          int WidescreenMode,
                                          int newEdgeWeightning)
{
  /*
    use the values in m_BasicWeight and newWeightning to
    update the direct control array for the output thread!
  */

  int index = 0;
  for(int row=0; row < CAP_HEIGHT; row++) {
      for(int col=0; col < CAP_WIDTH; col++) {
          if ((WidescreenMode == 1) && ((row <= CAP_HEIGHT/8) || (row >= (7*CAP_HEIGHT)/8)))
          {
             destWeight[index] = 0;
          } else {
   		     destWeight[index] = (int)(255.0 * (float)pow( ((float)m_BasicWeight[index])/255.0 , newEdgeWeightning));
          }
          index++;
      }
  }
}

void CAtmoZoneDefinition::setZoneNumber(int num)
{
    m_zonenumber = num;
}

int CAtmoZoneDefinition::getZoneNumber()
{
    return m_zonenumber;
}



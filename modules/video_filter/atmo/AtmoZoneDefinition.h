#ifndef _AtmoZoneDefinition_h_
#define _AtmoZoneDefinition_h_

#include "AtmoDefs.h"

#define ATMO_LOAD_GRADIENT_OK  0
#define ATMO_LOAD_GRADIENT_FILENOTFOND    1
#define ATMO_LOAD_GRADIENT_FAILED_SIZE    2
#define ATMO_LOAD_GRADIENT_FAILED_HEADER  3
#define ATMO_LOAD_GRADIENT_FAILED_FORMAT  4


class CAtmoZoneDefinition
{
private:
    int m_zonenumber; // just for identification and channel assignment!
    unsigned char m_BasicWeight[IMAGE_SIZE];

public:
    CAtmoZoneDefinition(void);
    ~CAtmoZoneDefinition(void);

    void Fill(unsigned char value);
    void FillGradientFromLeft();
    void FillGradientFromRight();
    void FillGradientFromTop();
    void FillGradientFromBottom();

    int LoadGradientFromBitmap(char *pszBitmap);

    void UpdateWeighting(int *destWeight,
                         int WidescreenMode,
                         int newEdgeWeightning);

    void setZoneNumber(int num);
    int getZoneNumber();
};

#endif

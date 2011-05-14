/*
 * AtmoConfig.cpp: Class for holding all configuration values of AtmoWin - stores
 * the values and retrieves its values from registry
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "AtmoConfig.h"

/* Import Hint

   if somebody Adds new config option this has to be done in the following
   files and includes!

   AtmoConfig.h  -- extend class definition!, and add get... and set... Methods!
                    so that the real variables are still hidden inside the class!
   AtmoConfigRegistry.cpp --> SaveToRegistry();
   AtmoConfigRegistry.cpp --> LoadFromRegistry();
   AtmoConfig.cpp --> Assign( ... );

*/

CAtmoConfig::CAtmoConfig()
{
  // setup basic configruation structures...
  m_IsShowConfigDialog = 0;
  m_eAtmoConnectionType = actClassicAtmo;
  for(int i=0;i<10;i++)
      m_ChannelAssignments[i] = NULL;

#if defined (_ATMO_VLC_PLUGIN_)
  m_devicename = NULL;
  m_devicenames[0] = NULL;
  m_devicenames[1] = NULL;
  m_devicenames[2] = NULL;
#endif
  // load all config values with there defaults
  m_ZoneDefinitions  = NULL;
  m_AtmoZoneDefCount = -1;
  m_DMX_BaseChannels = NULL;

  m_chWhiteAdj_Red   = NULL;
  m_chWhiteAdj_Green = NULL;
  m_chWhiteAdj_Blue  = NULL;

  LoadDefaults();
}

CAtmoConfig::~CAtmoConfig() {
   // and finally cleanup...
   clearAllChannelMappings();

   if(m_ZoneDefinitions)
   {
     for(int zone=0; zone<m_AtmoZoneDefCount; zone++)
         delete m_ZoneDefinitions[zone];
     delete m_ZoneDefinitions;
     m_ZoneDefinitions = NULL;
   }

   delete []m_chWhiteAdj_Red;
   delete []m_chWhiteAdj_Green;
   delete []m_chWhiteAdj_Blue;

   free( m_DMX_BaseChannels );

#if defined (_ATMO_VLC_PLUGIN_)
    free( m_devicename );
    free( m_devicenames[0] );
    free( m_devicenames[1] );
    free( m_devicenames[2] );
#endif
}

void CAtmoConfig::LoadDefaults() {
    //    m_eAtmoConnectionType = actSerialPort;
    //    m_Comport
#if defined (_ATMO_VLC_PLUGIN_)

    free( m_devicename );
    free( m_devicenames[0] );
    free( m_devicenames[1] );
    free( m_devicenames[2] );

    m_devicename = NULL;
    m_devicenames[0] = NULL;
    m_devicenames[1] = NULL;
    m_devicenames[2] = NULL;

#else

    m_Comport     = -1;
    m_Comports[0] = -1;
    m_Comports[1] = -1;
    m_Comports[2] = -1;

#endif

    m_eEffectMode = emDisabled;

    m_IgnoreConnectionErrorOnStartup = ATMO_FALSE;

    m_UpdateEdgeWeightningFlag = 0;

    m_Software_gamma_mode = agcNone;
    m_Software_gamma_red    = 10;
    m_Software_gamma_green  = 10;
    m_Software_gamma_blue   = 10;
    m_Software_gamma_global = 10;

    m_WhiteAdjustment_Red    = 255;
    m_WhiteAdjustment_Green  = 255;
    m_WhiteAdjustment_Blue   = 255;
    m_UseSoftwareWhiteAdj    = 1;

    m_WhiteAdjPerChannel = ATMO_FALSE;
    m_chWhiteAdj_Count = 0;

    delete []m_chWhiteAdj_Red;
    delete []m_chWhiteAdj_Green;
    delete []m_chWhiteAdj_Blue;

    m_chWhiteAdj_Red   = NULL;
    m_chWhiteAdj_Green = NULL;
    m_chWhiteAdj_Blue  = NULL;

    m_ColorChanger_iSteps    = 50;
    m_ColorChanger_iDelay    = 25;

    m_LrColorChanger_iSteps  = 50;
    m_LrColorChanger_iDelay  = 25;

    m_IsSetShutdownColor     = 1;
    m_ShutdownColor_Red      = 0;
    m_ShutdownColor_Green    = 0;
    m_ShutdownColor_Blue     = 0;

    m_StaticColor_Red        = 127; // ??
    m_StaticColor_Green      = 192;
    m_StaticColor_Blue       = 255;

    m_LiveViewFilterMode         = afmCombined;
    m_LiveViewFilter_PercentNew  = 50;
    m_LiveViewFilter_MeanLength  = 300;
    m_LiveViewFilter_MeanThreshold   = 40;
    m_show_statistics = ATMO_FALSE;

    m_LiveView_EdgeWeighting  = 8;
    m_LiveView_BrightCorrect  = 100;
    m_LiveView_DarknessLimit  = 5;
    m_LiveView_HueWinSize     = 3;
    m_LiveView_SatWinSize     = 3;
    m_LiveView_WidescreenMode = 0;

    m_LiveView_HOverscanBorder  = 0;
    m_LiveView_VOverscanBorder  = 0;
    m_LiveView_DisplayNr        = 0;
    m_LiveView_FrameDelay       = 30;
    m_LiveView_GDI_FrameRate    = 25;
    m_LiveView_RowsPerFrame     = 0;


    m_Hardware_global_gamma    = 128;
    m_Hardware_global_contrast = 100;
    m_Hardware_contrast_red    = 100;
    m_Hardware_contrast_green  = 100;
    m_Hardware_contrast_blue   = 100;

    m_Hardware_gamma_red       = 22;
    m_Hardware_gamma_green     = 22;
    m_Hardware_gamma_blue      = 22;

    m_DMX_BaseChannels         = strdup("0");
    m_DMX_RGB_Channels        = 5; // so wie atmolight

    m_MoMo_Channels           = 3; // default momo, there exists also a 4 ch version!
    m_Fnordlicht_Amount       = 2; // default fnordlicht, there are 2 fnordlicht's!

    m_ZonesTopCount            = 1;
    m_ZonesBottomCount         = 1;
    m_ZonesLRCount             = 1;
    m_ZoneSummary              = ATMO_FALSE;
    UpdateZoneCount();

    clearAllChannelMappings();
    m_CurrentChannelAssignment = 0;
    CAtmoChannelAssignment *temp = new CAtmoChannelAssignment();
    temp->system = true;
    temp->setName( "Standard" );
    this->m_ChannelAssignments[0] =  temp;

    UpdateZoneDefinitionCount();
}

void CAtmoConfig::Assign(CAtmoConfig *pAtmoConfigSrc) {

#if defined(_ATMO_VLC_PLUGIN_)
    this->setSerialDevice(0, pAtmoConfigSrc->getSerialDevice(0));
    this->setSerialDevice(1, pAtmoConfigSrc->getSerialDevice(1));
    this->setSerialDevice(2, pAtmoConfigSrc->getSerialDevice(2));
    this->setSerialDevice(3, pAtmoConfigSrc->getSerialDevice(3));
#else
    this->m_Comport                  = pAtmoConfigSrc->m_Comport;
    this->m_Comports[0]              = pAtmoConfigSrc->m_Comports[0];
    this->m_Comports[1]              = pAtmoConfigSrc->m_Comports[1];
    this->m_Comports[2]              = pAtmoConfigSrc->m_Comports[2];
#endif

    this->m_eAtmoConnectionType      = pAtmoConfigSrc->m_eAtmoConnectionType;
    this->m_eEffectMode              = pAtmoConfigSrc->m_eEffectMode;

    this->m_WhiteAdjustment_Red      = pAtmoConfigSrc->m_WhiteAdjustment_Red;
    this->m_WhiteAdjustment_Green    = pAtmoConfigSrc->m_WhiteAdjustment_Green;
    this->m_WhiteAdjustment_Blue     = pAtmoConfigSrc->m_WhiteAdjustment_Blue;
    this->m_UseSoftwareWhiteAdj      = pAtmoConfigSrc->m_UseSoftwareWhiteAdj;

    this->m_WhiteAdjPerChannel       = pAtmoConfigSrc->m_WhiteAdjPerChannel;
    this->m_chWhiteAdj_Count         = pAtmoConfigSrc->m_chWhiteAdj_Count;
    delete []m_chWhiteAdj_Red;
    delete []m_chWhiteAdj_Green;
    delete []m_chWhiteAdj_Blue;
    if(m_chWhiteAdj_Count > 0)
    {
       m_chWhiteAdj_Red   = new int[ m_chWhiteAdj_Count ];
       m_chWhiteAdj_Green = new int[ m_chWhiteAdj_Count ];
       m_chWhiteAdj_Blue  = new int[ m_chWhiteAdj_Count ];
       memcpy(m_chWhiteAdj_Red, pAtmoConfigSrc->m_chWhiteAdj_Red, sizeof(int) * m_chWhiteAdj_Count);
       memcpy(m_chWhiteAdj_Green, pAtmoConfigSrc->m_chWhiteAdj_Green, sizeof(int) * m_chWhiteAdj_Count);
       memcpy(m_chWhiteAdj_Blue, pAtmoConfigSrc->m_chWhiteAdj_Blue, sizeof(int) * m_chWhiteAdj_Count);
    } else {
       m_chWhiteAdj_Red   = NULL;
       m_chWhiteAdj_Green = NULL;
       m_chWhiteAdj_Blue  = NULL;
    }

    this->m_IsSetShutdownColor       = pAtmoConfigSrc->m_IsSetShutdownColor;
    this->m_ShutdownColor_Red        = pAtmoConfigSrc->m_ShutdownColor_Red;
    this->m_ShutdownColor_Green      = pAtmoConfigSrc->m_ShutdownColor_Green;
    this->m_ShutdownColor_Blue       = pAtmoConfigSrc->m_ShutdownColor_Blue;

    this->m_ColorChanger_iSteps      = pAtmoConfigSrc->m_ColorChanger_iSteps;
    this->m_ColorChanger_iDelay      = pAtmoConfigSrc->m_ColorChanger_iDelay;

    this->m_LrColorChanger_iSteps    = pAtmoConfigSrc->m_LrColorChanger_iSteps;
    this->m_LrColorChanger_iDelay    = pAtmoConfigSrc->m_LrColorChanger_iDelay;

    this->m_StaticColor_Red          = pAtmoConfigSrc->m_StaticColor_Red;
    this->m_StaticColor_Green        = pAtmoConfigSrc->m_StaticColor_Green;
    this->m_StaticColor_Blue         = pAtmoConfigSrc->m_StaticColor_Blue;

    this->m_LiveViewFilterMode             = pAtmoConfigSrc->m_LiveViewFilterMode;
    this->m_LiveViewFilter_PercentNew      = pAtmoConfigSrc->m_LiveViewFilter_PercentNew;
    this->m_LiveViewFilter_MeanLength      = pAtmoConfigSrc->m_LiveViewFilter_MeanLength;
    this->m_LiveViewFilter_MeanThreshold   = pAtmoConfigSrc->m_LiveViewFilter_MeanThreshold;

    this->m_show_statistics               = pAtmoConfigSrc->m_show_statistics;

    this->m_LiveView_EdgeWeighting  =  pAtmoConfigSrc->m_LiveView_EdgeWeighting;
    this->m_LiveView_BrightCorrect  =  pAtmoConfigSrc->m_LiveView_BrightCorrect;
    this->m_LiveView_DarknessLimit  =  pAtmoConfigSrc->m_LiveView_DarknessLimit;
    this->m_LiveView_HueWinSize     =  pAtmoConfigSrc->m_LiveView_HueWinSize;
    this->m_LiveView_SatWinSize     =  pAtmoConfigSrc->m_LiveView_SatWinSize;
    this->m_LiveView_WidescreenMode =  pAtmoConfigSrc->m_LiveView_WidescreenMode;

    this->m_LiveView_HOverscanBorder  = pAtmoConfigSrc->m_LiveView_HOverscanBorder;
    this->m_LiveView_VOverscanBorder  = pAtmoConfigSrc->m_LiveView_VOverscanBorder;
    this->m_LiveView_DisplayNr        = pAtmoConfigSrc->m_LiveView_DisplayNr;
    this->m_LiveView_FrameDelay       = pAtmoConfigSrc->m_LiveView_FrameDelay;
    this->m_LiveView_GDI_FrameRate    = pAtmoConfigSrc->m_LiveView_GDI_FrameRate;
    this->m_LiveView_RowsPerFrame   =  pAtmoConfigSrc->m_LiveView_RowsPerFrame;

    this->m_ZonesTopCount             = pAtmoConfigSrc->m_ZonesTopCount;
    this->m_ZonesBottomCount          = pAtmoConfigSrc->m_ZonesBottomCount;
    this->m_ZonesLRCount              = pAtmoConfigSrc->m_ZonesLRCount;
    this->m_ZoneSummary               = pAtmoConfigSrc->m_ZoneSummary;
    UpdateZoneCount();

    this->m_Software_gamma_mode      =  pAtmoConfigSrc->m_Software_gamma_mode;
    this->m_Software_gamma_red       =  pAtmoConfigSrc->m_Software_gamma_red;
    this->m_Software_gamma_green     =  pAtmoConfigSrc->m_Software_gamma_green;
    this->m_Software_gamma_blue      =  pAtmoConfigSrc->m_Software_gamma_blue;
    this->m_Software_gamma_global    =  pAtmoConfigSrc->m_Software_gamma_global;

    this->setDMX_BaseChannels( pAtmoConfigSrc->getDMX_BaseChannels() );

    this->m_DMX_RGB_Channels         = pAtmoConfigSrc->m_DMX_RGB_Channels;

    this->m_MoMo_Channels            = pAtmoConfigSrc->m_MoMo_Channels;

    this->m_Fnordlicht_Amount        = pAtmoConfigSrc->m_Fnordlicht_Amount;

    this->m_CurrentChannelAssignment = pAtmoConfigSrc->m_CurrentChannelAssignment;

    clearChannelMappings();
    for(int i=1;i<pAtmoConfigSrc->getNumChannelAssignments();i++) {
        CAtmoChannelAssignment *ta = pAtmoConfigSrc->m_ChannelAssignments[i];
        if(ta!=NULL) {
            CAtmoChannelAssignment *dest = this->m_ChannelAssignments[i];
            if(dest == NULL) {
               dest = new CAtmoChannelAssignment();
               this->m_ChannelAssignments[i] = dest;
            }
            // memcpy(dest, ta, sizeof(tChannelAssignment));
            dest->setSize(ta->getSize());
            dest->setName(ta->getName());
            dest->system = ta->system;
            for(int c=0;c<dest->getSize();c++)
                dest->setZoneIndex(c, ta->getZoneIndex(c));
        }
    }

    UpdateZoneDefinitionCount();
}

int CAtmoConfig::getNumChannelAssignments() {
    int z=0;
    for(int i=0;i<10;i++)
        if(this->m_ChannelAssignments[i]!=NULL) z++;
    return z;
}

void CAtmoConfig::clearChannelMappings() {
    for(int i=1;i<10;i++) {
        CAtmoChannelAssignment *ca = m_ChannelAssignments[i];
        if(ca!=NULL)
           delete ca;
        m_ChannelAssignments[i] = NULL;
    }
}

void CAtmoConfig::clearAllChannelMappings() {
    for(int i=0;i<10;i++) {
        CAtmoChannelAssignment *ca = m_ChannelAssignments[i];
        if(ca!=NULL)
           delete ca;
        m_ChannelAssignments[i] = NULL;
    }
}

void CAtmoConfig::AddChannelAssignment(CAtmoChannelAssignment *ta) {
    for(int i=0;i<10;i++) {
        if(m_ChannelAssignments[i] == NULL) {
           m_ChannelAssignments[i] = ta;
           break;
        }
    }
}

void CAtmoConfig::SetChannelAssignment(int index, CAtmoChannelAssignment *ta) {
     if(m_ChannelAssignments[index]!=NULL)
        delete m_ChannelAssignments[index];
     m_ChannelAssignments[index] = ta;
}

CAtmoZoneDefinition *CAtmoConfig::getZoneDefinition(int zoneIndex) {
    if(zoneIndex < 0)
       return NULL;
    if(zoneIndex >= m_AtmoZoneDefCount)
       return NULL;
    return m_ZoneDefinitions[zoneIndex];

}

void CAtmoConfig::UpdateZoneCount()
{
  m_computed_zones_count = m_ZonesTopCount + m_ZonesBottomCount + 2 * m_ZonesLRCount;
  if(m_ZoneSummary)
     m_computed_zones_count++;
}

int CAtmoConfig::getZoneCount()
{
    return(m_computed_zones_count);
}

void CAtmoConfig::UpdateZoneDefinitionCount()
{
   if( getZoneCount() != m_AtmoZoneDefCount)
   {
      // okay zonen anzahl hat sich geändert - wir müssen neu rechnen
      // und allokieren!
      if(m_ZoneDefinitions)
      {
        for(int zone=0; zone<m_AtmoZoneDefCount; zone++)
            delete m_ZoneDefinitions[zone];
        delete m_ZoneDefinitions;
        m_ZoneDefinitions = NULL;
      }
      m_AtmoZoneDefCount = getZoneCount();
      if(m_AtmoZoneDefCount > 0)
      {
         m_ZoneDefinitions = new CAtmoZoneDefinition*[m_AtmoZoneDefCount];
         for(int zone=0; zone< m_AtmoZoneDefCount; zone++) {
             m_ZoneDefinitions[zone] = new CAtmoZoneDefinition();
             m_ZoneDefinitions[zone]->Fill(255);
         }
      }
   }
}

#if defined(_ATMO_VLC_PLUGIN_)

char *CAtmoConfig::getSerialDevice(int i)
{
   if(i == 0)
      return m_devicename;
   else  {
       i--;
       return m_devicenames[i];
   }
}

void CAtmoConfig::setSerialDevice(int i,const char *pszNewDevice)
{
    if(i == 0)
       setSerialDevice(pszNewDevice);
    else {
       i--;
       free( m_devicenames[i] );
       if(pszNewDevice)
          m_devicenames[i] = strdup(pszNewDevice);
       else
          m_devicenames[i] = NULL;
    }
}

#else

int CAtmoConfig::getComport(int i)
{
  if(i == 0)
     return this->m_Comport;
  else {
     i--;
     return this->m_Comports[i];
  }
}

void CAtmoConfig::setComport(int i, int nr)
{
  if(i == 0)
      this->m_Comport = nr;
  else {
    this->m_Comports[i-1] = nr;
  }
}

#endif

void CAtmoConfig::setDMX_BaseChannels(char *channels)
{
     free(m_DMX_BaseChannels);
     m_DMX_BaseChannels = strdup(channels);
}

void CAtmoConfig::getChannelWhiteAdj(int channel,int &red,int &green,int &blue)
{
  if(channel >= m_chWhiteAdj_Count)
  {
     red = 256;
     green = 256;
     blue = 256;

  } else {

    red   = m_chWhiteAdj_Red[ channel ];
    green = m_chWhiteAdj_Green[ channel ];
    blue  = m_chWhiteAdj_Blue[ channel ];

  }
}

void CAtmoConfig::setChannelWhiteAdj(int channel,int red,int green,int blue)
{
    if(channel >= m_chWhiteAdj_Count)
    {
       int *tmp = new int[channel+1];
       if(m_chWhiteAdj_Red)
          memcpy( tmp, m_chWhiteAdj_Red, m_chWhiteAdj_Count * sizeof(int) );
       delete []m_chWhiteAdj_Red;
       m_chWhiteAdj_Red = tmp;

       tmp = new int[channel + 1];
       if(m_chWhiteAdj_Green)
          memcpy( tmp, m_chWhiteAdj_Green, m_chWhiteAdj_Count * sizeof(int) );
       delete []m_chWhiteAdj_Green;
       m_chWhiteAdj_Green = tmp;

       tmp = new int[channel + 1];
       if(m_chWhiteAdj_Blue)
          memcpy( tmp, m_chWhiteAdj_Blue, m_chWhiteAdj_Count * sizeof(int) );
       delete []m_chWhiteAdj_Blue;
       m_chWhiteAdj_Blue = tmp;

       m_chWhiteAdj_Count = channel + 1;
   }

   m_chWhiteAdj_Red[channel]   = red;
   m_chWhiteAdj_Green[channel] = green;
   m_chWhiteAdj_Blue[channel]  = blue;

}

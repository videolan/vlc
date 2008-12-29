/*
 * AtmoConfig.cpp: Class for holding all configuration values of AtmoWin - stores
 * the values and retrieves its values from registry
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

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
  m_eAtmoConnectionType = actSerialPort;
  for(int i=0;i<10;i++)
      m_ChannelAssignments[i] = NULL;
#if defined (_ATMO_VLC_PLUGIN_)
  m_devicename = NULL;
#endif
  // load all config values with there defaults
  LoadDefaults();

  //   CAtmoZoneDefinition *m_ZoneDefinitions[ATMO_NUM_CHANNELS];
  // generate default channel parameters which may be loaded later from .bmp files
  for(int i=0;i<ATMO_NUM_CHANNELS;i++) {
      m_ZoneDefinitions[i] = new CAtmoZoneDefinition();
      m_ZoneDefinitions[i]->setZoneNumber(i);
      switch(i) {
          case 0:  // summary channel
              m_ZoneDefinitions[i]->Fill(255);
              break;
          case 1: // left channel
              m_ZoneDefinitions[i]->FillGradientFromLeft();
              break;
          case 2: // right channel
              m_ZoneDefinitions[i]->FillGradientFromRight();
              break;
          case 3: // top channel
              m_ZoneDefinitions[i]->FillGradientFromTop();
              break;
          case 4: // bottom channel
              m_ZoneDefinitions[i]->FillGradientFromBottom();
              break;
      }
  }
}

CAtmoConfig::~CAtmoConfig() {
   // and finally cleanup...
   clearAllChannelMappings();
#if !defined (WIN32)
   if(m_devicename)
      free(m_devicename);
#endif
}

void CAtmoConfig::LoadDefaults() {
    //    m_eAtmoConnectionType = actSerialPort;
    //    m_Comport

    m_eEffectMode = emDisabled;

    m_WhiteAdjustment_Red    = 255;
    m_WhiteAdjustment_Green  = 255;
    m_WhiteAdjustment_Blue   = 255;
	m_UseSoftwareWhiteAdj    = 1;

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

    m_LiveView_EdgeWeighting  = 8;
    m_LiveView_BrightCorrect  = 100;
    m_LiveView_DarknessLimit  = 5;
    m_LiveView_HueWinSize     = 3;
    m_LiveView_SatWinSize     = 3;
    m_LiveView_WidescreenMode = 0;

    m_LiveView_HOverscanBorder  = 0;
    m_LiveView_VOverscanBorder  = 0;
    m_LiveView_DisplayNr        = 0;
    m_LiveView_FrameDelay       = 0;


    m_Hardware_global_gamma    = 128;
    m_Hardware_global_contrast = 100;
    m_Hardware_contrast_red    = 100;
    m_Hardware_contrast_green  = 100;
    m_Hardware_contrast_blue   = 100;

    m_Hardware_gamma_red       = 22;
    m_Hardware_gamma_green     = 22;
    m_Hardware_gamma_blue      = 22;

    clearAllChannelMappings();
    m_CurrentChannelAssignment = 0;
    tChannelAssignment* temp = new tChannelAssignment;
    temp->system = true;
    for(int i=0;i<ATMO_NUM_CHANNELS;i++)
        temp->mappings[i] = i;
    strcpy(temp->name,"Standard");
    this->m_ChannelAssignments[0] =  temp;
}

void CAtmoConfig::Assign(CAtmoConfig *pAtmoConfigSrc) {

#if defined(_ATMO_VLC_PLUGIN_)
    this->setSerialDevice(pAtmoConfigSrc->getSerialDevice());
#else
    this->m_Comport                  = pAtmoConfigSrc->m_Comport;
#endif

    this->m_eAtmoConnectionType      = pAtmoConfigSrc->m_eAtmoConnectionType;
    this->m_eEffectMode              = pAtmoConfigSrc->m_eEffectMode;

    this->m_WhiteAdjustment_Red      = pAtmoConfigSrc->m_WhiteAdjustment_Red;
    this->m_WhiteAdjustment_Green    = pAtmoConfigSrc->m_WhiteAdjustment_Green;
    this->m_WhiteAdjustment_Blue     = pAtmoConfigSrc->m_WhiteAdjustment_Blue;
    this->m_UseSoftwareWhiteAdj      = pAtmoConfigSrc->m_UseSoftwareWhiteAdj;

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

    clearChannelMappings();
    for(int i=1;i<pAtmoConfigSrc->getNumChannelAssignments();i++) {
        tChannelAssignment *ta = pAtmoConfigSrc->m_ChannelAssignments[i];
        if(ta!=NULL) {
            tChannelAssignment *dest = this->m_ChannelAssignments[i];
            if(dest == NULL) {
               dest = new tChannelAssignment;
               this->m_ChannelAssignments[i] = dest;
            }
            memcpy(dest, ta, sizeof(tChannelAssignment));
        }
    }
}



int CAtmoConfig::getNumChannelAssignments() {
    int z=0;
    for(int i=0;i<10;i++)
        if(this->m_ChannelAssignments[i]!=NULL) z++;
    return z;
}

void CAtmoConfig::clearChannelMappings() {
    for(int i=1;i<10;i++) {
        tChannelAssignment *ca = m_ChannelAssignments[i];
        if(ca!=NULL)
           delete ca;
        m_ChannelAssignments[i] = NULL;
    }
}

void CAtmoConfig::clearAllChannelMappings() {
    for(int i=0;i<10;i++) {
        tChannelAssignment *ca = m_ChannelAssignments[i];
        if(ca!=NULL)
           delete ca;
        m_ChannelAssignments[i] = NULL;
    }
}

void CAtmoConfig::AddChannelAssignment(tChannelAssignment *ta) {
    for(int i=0;i<10;i++) {
        if(m_ChannelAssignments[i] == NULL) {
           m_ChannelAssignments[i] = ta;
           break;
        }
    }
}

void CAtmoConfig::SetChannelAssignment(int index, tChannelAssignment *ta) {
     if(m_ChannelAssignments[index]!=NULL)
        delete m_ChannelAssignments[index];
     m_ChannelAssignments[index] = ta;
}

CAtmoZoneDefinition *CAtmoConfig::getZoneDefinition(int zoneIndex) {
    if(zoneIndex < 0)
       return NULL;
    if(zoneIndex >= ATMO_NUM_CHANNELS)
       return NULL;
    return m_ZoneDefinitions[zoneIndex];

}


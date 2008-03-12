/*
 * AtmoConfig.h: Class for holding all configuration values of AtmoWin
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifndef _AtmoConfig_h_
#define _AtmoConfig_h_

#include "AtmoDefs.h"
#include "AtmoZoneDefinition.h"

#if defined(_ATMO_VLC_PLUGIN_)
#   include <stdlib.h>
#   include <string.h>
#endif


class CAtmoConfig {

    protected:
	   int m_IsShowConfigDialog;	
#if defined(_ATMO_VLC_PLUGIN_)
       char *m_devicename;
#else
	   int m_Comport;
#endif
       enum AtmoConnectionType m_eAtmoConnectionType;
       enum EffectMode m_eEffectMode;

    protected:
       ATMO_BOOL m_UseSoftwareWhiteAdj;
       int m_WhiteAdjustment_Red;
       int m_WhiteAdjustment_Green;
       int m_WhiteAdjustment_Blue;

    protected:
       int m_IsSetShutdownColor;
	   int m_ShutdownColor_Red;
	   int m_ShutdownColor_Green;
	   int m_ShutdownColor_Blue;

    protected:
       /* Config Values for Color Changer */
       int m_ColorChanger_iSteps;
       int m_ColorChanger_iDelay;

    protected:
        /* Config  values for the primitive Left Right Color Changer */
       int m_LrColorChanger_iSteps;
       int m_LrColorChanger_iDelay;

    protected:
       /* the static background color */
       int m_StaticColor_Red;
       int m_StaticColor_Green;
       int m_StaticColor_Blue;

    protected:
        /*
           one for System + 9 for userdefined channel
           assignments (will it be enough?)
        */
        tChannelAssignment *m_ChannelAssignments[10];
        int m_CurrentChannelAssignment;

    protected:
        CAtmoZoneDefinition *m_ZoneDefinitions[ATMO_NUM_CHANNELS];


    protected:
        /* Live View Parameters (most interesting) */
        AtmoFilterMode m_LiveViewFilterMode;
        int m_LiveViewFilter_PercentNew;
        int m_LiveViewFilter_MeanLength;
        int m_LiveViewFilter_MeanThreshold;

        // weighting of distance to edge
        int m_LiveView_EdgeWeighting; //  = 8;
        // brightness correction
        int m_LiveView_BrightCorrect; //  = 100;
        // darkness limit (pixels below this value will be ignored)
        int m_LiveView_DarknessLimit; //  = 5;
        // Windowing size for hue histogram building
        int m_LiveView_HueWinSize;    //  = 3;
        // Windowing size for sat histogram building
        int m_LiveView_SatWinSize;    //  = 3;
        /*
          special (hack) for ignorning black borders durring
          playback of letterboxed material on a 16:9 output device
        */
        int m_LiveView_WidescreenMode; // = 0

        // border from source image which should be ignored
        // the values are only used by the Win32 GDI Screen capture
        int m_LiveView_HOverscanBorder;
        int m_LiveView_VOverscanBorder;
        int m_LiveView_DisplayNr;

        /*
           a special delay to get the light in sync with the video
           was required because the frames will pass my VLC filter some [ms]
           before they become visible on screen with this delay - screenoutput
           and light timing could be "synchronized"
        */
        int m_LiveView_FrameDelay;

    protected:
         /* values of the last hardware white adjustment (only for hardware with new firmware) */
         int m_Hardware_global_gamma;
         int m_Hardware_global_contrast;
         int m_Hardware_contrast_red;
         int m_Hardware_contrast_green;
         int m_Hardware_contrast_blue;
         int m_Hardware_gamma_red;
         int m_Hardware_gamma_green;
         int m_Hardware_gamma_blue;

    public:
       CAtmoConfig();
       virtual ~CAtmoConfig();
       virtual void SaveSettings() {}
       virtual void LoadSettings() {};
       void LoadDefaults();

       /*
         function to copy  the values of one configuration object to another
         will be used in windows settings dialog as backup if the user
         presses cancel
       */
       void Assign(CAtmoConfig *pAtmoConfigSrc);

    public:
        int isShowConfigDialog()            { return m_IsShowConfigDialog; }
        void setShowConfigDialog(int value) { m_IsShowConfigDialog = value; }

#if defined(_ATMO_VLC_PLUGIN_)
        char *getSerialDevice()               { return m_devicename; }
        void setSerialDevice(char *newdevice) { free( m_devicename ); if(newdevice) m_devicename = strdup(newdevice); else m_devicename = NULL; }
#else
        int getComport()                    { return m_Comport; }
        void setComport(int value)          { m_Comport = value; }
#endif

        int getWhiteAdjustment_Red() { return m_WhiteAdjustment_Red;  }
        void setWhiteAdjustment_Red(int value) { m_WhiteAdjustment_Red = value; }
        int getWhiteAdjustment_Green() { return m_WhiteAdjustment_Green;  }
        void setWhiteAdjustment_Green(int value) { m_WhiteAdjustment_Green = value; }
        int getWhiteAdjustment_Blue() { return m_WhiteAdjustment_Blue;  }
        void setWhiteAdjustment_Blue(int value) { m_WhiteAdjustment_Blue = value; }
        ATMO_BOOL isUseSoftwareWhiteAdj() { return m_UseSoftwareWhiteAdj; }
        void setUseSoftwareWhiteAdj(ATMO_BOOL value) { m_UseSoftwareWhiteAdj = value; }

        int isSetShutdownColor()     { return m_IsSetShutdownColor; }
        void SetSetShutdownColor(int value) { m_IsSetShutdownColor = value; }
        int getShutdownColor_Red()   { return m_ShutdownColor_Red; }
        void setShutdownColor_Red(int value) { m_ShutdownColor_Red = value; }
        int getShutdownColor_Green() { return m_ShutdownColor_Green; }
        void setShutdownColor_Green(int value) { m_ShutdownColor_Green = value; }
        int getShutdownColor_Blue()  { return m_ShutdownColor_Blue; }
        void setShutdownColor_Blue(int value) { m_ShutdownColor_Blue=value; }

        int getColorChanger_iSteps() { return m_ColorChanger_iSteps; }
        void setColorChanger_iSteps(int value) { m_ColorChanger_iSteps = value; }
        int getColorChanger_iDelay() { return m_ColorChanger_iDelay; }
        void setColorChanger_iDelay(int value) { m_ColorChanger_iDelay = value; }

        int getLrColorChanger_iSteps() { return m_LrColorChanger_iSteps; }
        void setLrColorChanger_iSteps(int value) { m_LrColorChanger_iSteps = value; }
        int getLrColorChanger_iDelay() { return m_LrColorChanger_iDelay; }
        void setLrColorChanger_iDelay(int value) { m_LrColorChanger_iDelay = value; }

        int getStaticColor_Red()   { return m_StaticColor_Red;   }
        void setStaticColor_Red(int value)  { m_StaticColor_Red=value; }
        int getStaticColor_Green() { return m_StaticColor_Green; }
        void setStaticColor_Green(int value) { m_StaticColor_Green=value; }
        int getStaticColor_Blue()  { return m_StaticColor_Blue;  }
        void  setStaticColor_Blue(int value) { m_StaticColor_Blue=value; }


        AtmoConnectionType getConnectionType() { return m_eAtmoConnectionType; }
        void setConnectionType(AtmoConnectionType value) { m_eAtmoConnectionType = value; }

        EffectMode getEffectMode() { return m_eEffectMode; }
        void setEffectMode(EffectMode value) { m_eEffectMode = value; }

        AtmoFilterMode getLiveViewFilterMode() { return m_LiveViewFilterMode; }
        void setLiveViewFilterMode(AtmoFilterMode value) { m_LiveViewFilterMode = value; }

        int getLiveViewFilter_PercentNew() { return m_LiveViewFilter_PercentNew; }
        void setLiveViewFilter_PercentNew(int value) { m_LiveViewFilter_PercentNew=value; }
        int getLiveViewFilter_MeanLength() { return m_LiveViewFilter_MeanLength; }
        void setLiveViewFilter_MeanLength(int value) { m_LiveViewFilter_MeanLength = value; }
        int getLiveViewFilter_MeanThreshold() { return m_LiveViewFilter_MeanThreshold; }
        void setLiveViewFilter_MeanThreshold(int value) { m_LiveViewFilter_MeanThreshold = value; }

        int getLiveView_EdgeWeighting() { return m_LiveView_EdgeWeighting; }
        void setLiveView_EdgeWeighting(int value) { m_LiveView_EdgeWeighting=value; }

        int getLiveView_BrightCorrect() { return m_LiveView_BrightCorrect; }
        void setLiveView_BrightCorrect(int value) { m_LiveView_BrightCorrect=value; }

        int getLiveView_DarknessLimit() { return m_LiveView_DarknessLimit; }
        void setLiveView_DarknessLimit(int value) { m_LiveView_DarknessLimit=value; }

        int getLiveView_HueWinSize() { return m_LiveView_HueWinSize; }
        void setLiveView_HueWinSize(int value) { m_LiveView_HueWinSize=value; }

        int getLiveView_SatWinSize() { return m_LiveView_SatWinSize; }
        void setLiveView_SatWinSize(int value) { m_LiveView_SatWinSize=value; }

        int getLiveView_WidescreenMode() { return m_LiveView_WidescreenMode; }
        void setLiveView_WidescreenMode(int value) { m_LiveView_WidescreenMode=value; }

        int getLiveView_HOverscanBorder() { return m_LiveView_HOverscanBorder; }
        void setLiveView_HOverscanBorder(int value) { m_LiveView_HOverscanBorder = value; }

        int getLiveView_VOverscanBorder() { return m_LiveView_VOverscanBorder; }
        void setLiveView_VOverscanBorder(int value) { m_LiveView_VOverscanBorder = value; }

        int getLiveView_DisplayNr() { return m_LiveView_DisplayNr; }
        void setLiveView_DisplayNr(int value) { m_LiveView_DisplayNr = value; }

        int getLiveView_FrameDelay() { return m_LiveView_FrameDelay; }
        void setLiveView_FrameDelay(int delay) { m_LiveView_FrameDelay = delay; }

        int getHardware_global_gamma() { return m_Hardware_global_gamma ; }
        void setHardware_global_gamma(int value) { m_Hardware_global_gamma=value; }

        int getHardware_global_contrast() { return m_Hardware_global_contrast; }
        void setHardware_global_contrast(int value) { m_Hardware_global_contrast=value; }

        int getHardware_contrast_red() { return m_Hardware_contrast_red; }
        void setHardware_contrast_red(int value) { m_Hardware_contrast_red=value; }

        int getHardware_contrast_green() { return m_Hardware_contrast_green; }
        void setHardware_contrast_green(int value) { m_Hardware_contrast_green=value; }

        int getHardware_contrast_blue() { return m_Hardware_contrast_blue; }
        void setHardware_contrast_blue(int value) { m_Hardware_contrast_blue=value; }

        int getHardware_gamma_red() { return m_Hardware_gamma_red; }
        void setHardware_gamma_red(int value) { m_Hardware_gamma_red=value; }

        int getHardware_gamma_green() { return m_Hardware_gamma_green; }
        void setHardware_gamma_green(int value) { m_Hardware_gamma_green=value; }

        int getHardware_gamma_blue() { return m_Hardware_gamma_blue; }
        void setHardware_gamma_blue(int value) { m_Hardware_gamma_blue=value; }

        tChannelAssignment *getChannelAssignment(int nummer) {
            return this->m_ChannelAssignments[nummer];
        }
        int getCurrentChannelAssignment() { return m_CurrentChannelAssignment; }
        void setCurrentChannelAssignment(int index) { m_CurrentChannelAssignment = index; }

        int getNumChannelAssignments();
        void clearChannelMappings();
        void clearAllChannelMappings();
        void AddChannelAssignment(tChannelAssignment *ta);
        void SetChannelAssignment(int index, tChannelAssignment *ta);

        CAtmoZoneDefinition *getZoneDefinition(int zoneIndex);

};

#endif

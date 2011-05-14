/*
 * AtmoConnection.h: generic/abstract class defining all methods for the
 * communication with the hardware
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoConnection_h_
#define _AtmoConnection_h_

#include <stdlib.h>

#include "AtmoDefs.h"
#include "AtmoConfig.h"
#include "AtmoChannelAssignment.h"

#if defined(_ATMO_VLC_PLUGIN_)
#   include <vlc_common.h>
#   include <vlc_threads.h>
#else
#   include <windows.h>
#endif

class CAtmoConnection
{
protected:
	CAtmoConfig *m_pAtmoConfig;

#if defined(_ATMO_VLC_PLUGIN_)
    vlc_mutex_t m_AccessConnection;
#else
    CRITICAL_SECTION m_AccessConnection;
#endif

    int *m_ChannelAssignment;
    int m_NumAssignedChannels;

protected:
    void Lock();
    void Unlock();

public:
	CAtmoConnection(CAtmoConfig *cfg);
	virtual ~CAtmoConnection(void);
	virtual ATMO_BOOL OpenConnection() = 0;
	virtual void CloseConnection() {};
	virtual ATMO_BOOL isOpen(void) { return false; }

    virtual ATMO_BOOL SendData(pColorPacket data) = 0;

    virtual ATMO_BOOL setChannelColor(int /*channel*/, tRGBColor /*color*/)
    { return false; }
    virtual ATMO_BOOL setChannelValues(int /*num*/,unsigned char * /*values*/)
    { return false; }

    virtual ATMO_BOOL HardwareWhiteAdjust(int /*global_gamma*/,
                                          int /*global_contrast*/,
                                          int /*contrast_red*/,
                                          int /*contrast_green*/,
                                          int /*contrast_blue*/,
                                          int /*gamma_red*/,
                                          int /*gamma_green*/,
                                          int /*gamma_blue*/,
                                          ATMO_BOOL /*storeToEeprom*/)
    { return false; }

#if !defined(_ATMO_VLC_PLUGIN_)
    virtual ATMO_BOOL ShowConfigDialog(HINSTANCE hInst, HWND parent, CAtmoConfig *cfg);
#endif

    virtual void SetChannelAssignment(CAtmoChannelAssignment *ca);

    virtual int getNumChannels() { return 0; }
    virtual char *getChannelName(int /*ch*/) { return NULL; }

    virtual const char *getDevicePath() { return "none"; }

    virtual ATMO_BOOL CreateDefaultMapping(CAtmoChannelAssignment * /*ca*/)
    { return false; }

};

#endif

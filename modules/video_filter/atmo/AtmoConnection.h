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

#include "AtmoDefs.h"
#include "AtmoConfig.h"

class CAtmoConnection
{
protected:
	CAtmoConfig *m_pAtmoConfig;
    int m_ChannelAssignment[ATMO_NUM_CHANNELS];

public:
	CAtmoConnection(CAtmoConfig *cfg);
	virtual ~CAtmoConnection(void);
	virtual ATMO_BOOL OpenConnection() { return false; }
	virtual void CloseConnection() {};
	virtual ATMO_BOOL isOpen(void) { return false; }

    virtual ATMO_BOOL SendData(unsigned char numChannels,
                               int red[],
                               int green[],
                               int blue[]) { return false; }

    virtual ATMO_BOOL SendData(tColorPacket data) { return false; }

    virtual ATMO_BOOL setChannelColor(int channel, tRGBColor color) { return false; }
    virtual ATMO_BOOL setChannelValues(int numValues,unsigned char *channel_values) { return false; }

    virtual ATMO_BOOL HardwareWhiteAdjust(int global_gamma,
                                          int global_contrast,
                                          int contrast_red,
                                          int contrast_green,
                                          int contrast_blue,
                                          int gamma_red,
                                          int gamma_green,
                                          int gamma_blue,
                                          ATMO_BOOL storeToEeprom) { return false; }

    virtual void SetChannelAssignment(tChannelAssignment *ca);

};

#endif

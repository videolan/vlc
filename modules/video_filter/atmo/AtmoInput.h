/*
 * AtmoInput.h:  abstract class for retrieving precalculated image data from
 * different sources in the live view mode
 *
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoInput_h_
#define _AtmoInput_h_

#include "AtmoDefs.h"
#include "AtmoDynData.h"

/*
  basic definition of an AtmoLight data/image source ...
*/
class CAtmoInput {

protected:
    tColorPacket m_ColorPacket;
    volatile ATMO_BOOL m_FrameArrived;
    CAtmoDynData *m_pAtmoDynData;

public:
    CAtmoInput(CAtmoDynData *pAtmoDynData);
    virtual ~CAtmoInput(void);

    // Opens the input-device.
    // Returns true if the input-device was opened successfully.
    virtual ATMO_BOOL Open(void) { return ATMO_FALSE; }

    // Closes the input-device.
    // Returns true if the input-device was closed successfully.
    virtual ATMO_BOOL Close(void) { return ATMO_FALSE; }

    // Returns the calculated tColorPacket for further processing (e.g. filtering).
    virtual tColorPacket GetColorPacket(void) { return m_ColorPacket; }

    // wait for the arrival of the next frame...(to come in sync again)
    virtual void WaitForNextFrame(DWORD timeout);
};

#endif

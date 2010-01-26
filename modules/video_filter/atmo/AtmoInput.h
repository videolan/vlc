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
#include "AtmoCalculations.h"
#include "AtmoPacketQueue.h"
#include "AtmoThread.h"
#include "AtmoDynData.h"

class CAtmoDynData;

/*
  basic definition of an AtmoLight data/image source ...
*/
class CAtmoInput : public CThread {

protected:
    CAtmoDynData         *m_pAtmoDynData;
    CAtmoColorCalculator *m_pAtmoColorCalculator;

public:
    CAtmoInput(CAtmoDynData *pAtmoDynData);
    virtual ~CAtmoInput(void);

    // Opens the input-device.
    // Returns true if the input-device was opened successfully.
    virtual ATMO_BOOL Open(void) { return ATMO_FALSE; }

    // Closes the input-device.
    // Returns true if the input-device was closed successfully.
    virtual ATMO_BOOL Close(void) { return ATMO_FALSE; }

};

#endif

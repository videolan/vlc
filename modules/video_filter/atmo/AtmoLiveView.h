/*
 * AtmoLiveView.h:  this effect outputs colors as result of a picture content
 * (most complex effect) see thread.c of the linux version - to fully understand
 * what happes here..
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoLiveView_h_
#define _AtmoLiveView_h_

#include "AtmoDefs.h"

#if !defined(_ATMO_VLC_PLUGIN_)
#   include <windows.h>
#endif

#include "AtmoThread.h"
#include "AtmoConfig.h"
#include "AtmoConnection.h"
#include "AtmoInput.h"

class CAtmoLiveView :  public CThread
{
protected:
	virtual DWORD Execute(void);

protected:
    CAtmoDynData *m_pAtmoDynData;

public:
    CAtmoLiveView(CAtmoDynData *pAtmoDynData);
    virtual ~CAtmoLiveView(void);
};

#endif

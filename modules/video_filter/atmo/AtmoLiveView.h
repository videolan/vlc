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
#   include <comdef.h>		
#   include "AtmoWin_h.h"
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

#if !defined(_ATMO_VLC_PLUGIN_)
public:
    STDMETHODIMP setLiveViewSource(enum ComLiveViewSource dwModus);
    STDMETHODIMP getCurrentLiveViewSource(enum ComLiveViewSource *modus);
#endif

protected:
    CAtmoDynData *m_pAtmoDynData;
    CAtmoInput *m_pAtmoInput;

#if !defined(_ATMO_VLC_PLUGIN_)
    ComLiveViewSource m_LiveViewSource;
    ComLiveViewSource m_CurrentLiveViewSource;
    CRITICAL_SECTION m_InputChangeCriticalSection;
    HANDLE m_InputChangedEvent;
#endif

public:
    CAtmoLiveView(CAtmoDynData *pAtmoDynData);
    virtual ~CAtmoLiveView(void);

    CAtmoInput *getAtmoInput() { return m_pAtmoInput; }

#if !defined(_ATMO_VLC_PLUGIN_)
    ComLiveViewSource getLiveViewSource() { return m_CurrentLiveViewSource; }
#endif
};

#endif

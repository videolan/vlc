/*
 * AtmoThread.h: Base thread class for all threads inside AtmoWin
 *
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#ifndef _AtmoThread_h_
#define _AtmoThread_h_

#include "AtmoDefs.h"

#if defined(_ATMO_VLC_PLUGIN_)
// use threading stuff from videolan!
#   include <vlc_common.h>
#   include <vlc_threads.h>
#else
#   include <windows.h>
#endif

class CThread
{
protected:

#if defined(_ATMO_VLC_PLUGIN_)

    vlc_mutex_t  m_TerminateLock;
    vlc_cond_t   m_TerminateCond;
    vlc_object_t *m_pOwner;
    ATMO_BOOL    m_HasThread;
    vlc_thread_t m_Thread;

#else

    HANDLE m_hThread;
	DWORD m_dwThreadID;
	HANDLE m_hTerminateEvent;

#endif

    volatile ATMO_BOOL m_bTerminated;

private:

#if defined(_ATMO_VLC_PLUGIN_)
    static void *ThreadProc(void *);
#else
	static DWORD WINAPI ThreadProc(LPVOID lpParameter);
#endif

protected:
	virtual DWORD Execute(void);
	ATMO_BOOL ThreadSleep(DWORD millisekunden);

public:
#if defined(_ATMO_VLC_PLUGIN_)
	CThread(vlc_object_t *pOwner);
#else
	CThread(void);
#endif

    virtual ~CThread(void);

    void Terminate(void);
    void Run();

};

#endif


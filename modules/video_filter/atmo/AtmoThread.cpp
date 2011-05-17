/*
 * AtmoThread.cpp: Base thread class for all threads inside AtmoWin
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <assert.h>

#include "AtmoThread.h"

#if defined(_ATMO_VLC_PLUGIN_)

CThread::CThread(vlc_object_t *pOwner)
{
    m_bTerminated  = ATMO_FALSE;
    vlc_mutex_init( &m_TerminateLock );
    vlc_cond_init( &m_TerminateCond );
    m_pOwner = pOwner;
    m_HasThread = ATMO_FALSE;
}

#else

CThread::CThread(void)
{
  m_bTerminated  = ATMO_FALSE;

  m_hThread = CreateThread(NULL, 0, CThread::ThreadProc ,
                           this, CREATE_SUSPENDED, &m_dwThreadID);

  m_hTerminateEvent = CreateEvent(NULL,0,0,NULL);
}

#endif



#if defined(_ATMO_VLC_PLUGIN_)

CThread::~CThread(void)
{
  assert(m_HasThread == ATMO_FALSE);
  vlc_mutex_destroy( &m_TerminateLock );
  vlc_cond_destroy( &m_TerminateCond );
}

#else

CThread::~CThread(void)
{
  CloseHandle(m_hThread);	
  CloseHandle(m_hTerminateEvent);
}

#endif

#if defined(_ATMO_VLC_PLUGIN_)

void *CThread::ThreadProc(void *obj)
{
      CThread *pThread = static_cast<CThread*>(obj);

      int canc = vlc_savecancel ();
      pThread->Execute();
      vlc_restorecancel (canc);

      return NULL;
}

#else

DWORD WINAPI CThread::ThreadProc(LPVOID lpParameter)
{
	   CThread *pThread = (CThread *)lpParameter;
	   if(pThread)
	      return pThread->Execute();
	   else
		  return (DWORD)-1;
}

#endif


DWORD CThread::Execute(void)
{
  /*
    to do implement! override!

	while(!bTerminated) {
	 ...
	}
  */	
 return 0;
}

void CThread::Terminate(void)
{
   // Set Termination Flag and EventObject!
   // and wait for Termination

#if defined(_ATMO_VLC_PLUGIN_)
   if(m_HasThread != ATMO_FALSE)
   {
      vlc_mutex_lock( &m_TerminateLock );
      m_bTerminated = ATMO_TRUE;
      vlc_cond_signal( &m_TerminateCond  );
      vlc_mutex_unlock( &m_TerminateLock );

      vlc_cancel( m_Thread );
      vlc_join( m_Thread, NULL );
   }
#else
   m_bTerminated = ATMO_TRUE;
   SetEvent(m_hTerminateEvent);
   WaitForSingleObject(m_hThread,INFINITE);
#endif
}

void CThread::Run()
{
   m_bTerminated = ATMO_FALSE;

#if defined(_ATMO_VLC_PLUGIN_)
   if (vlc_clone( &m_Thread, CThread::ThreadProc, this, VLC_THREAD_PRIORITY_LOW))
   {
       m_HasThread = ATMO_FALSE;
       msg_Err( m_pOwner, "cannot launch one of the AtmoLight threads");
   }
   else
   {
       m_HasThread = ATMO_TRUE;;
   }

#else

   ResetEvent(m_hTerminateEvent);
   ResumeThread(m_hThread);

#endif
}

/*
   does a sleep if the sleep was interrupted through
   the thread kill event return false...
*/
ATMO_BOOL CThread::ThreadSleep(DWORD millisekunden)
{
#if defined(_ATMO_VLC_PLUGIN_)
     ATMO_BOOL temp;
     vlc_mutex_lock( &m_TerminateLock );
     vlc_cond_timedwait(&m_TerminateCond,
                        &m_TerminateLock,
                        mdate() + (mtime_t)(millisekunden * 1000));
     temp = m_bTerminated;
     vlc_mutex_unlock( &m_TerminateLock );
     return !temp;

#else
     DWORD res = WaitForSingleObject(m_hTerminateEvent,millisekunden);
	 return (res == WAIT_TIMEOUT);
#endif
}


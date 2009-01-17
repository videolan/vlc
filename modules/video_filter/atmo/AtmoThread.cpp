/*
 * AtmoThread.cpp: Base thread class for all threads inside AtmoWin
 *
 * See the README.txt file for copyright information and how to reach the author(s).
 *
 * $Id$
 */
#include "AtmoThread.h"

#if defined(_ATMO_VLC_PLUGIN_)

CThread::CThread(vlc_object_t *pOwner)
{
    int err;
    m_pAtmoThread = (atmo_thread_t *)vlc_object_create( pOwner,
                                                        sizeof(atmo_thread_t) );
    if(m_pAtmoThread)
    {
        m_pAtmoThread->p_thread = this;
        this->m_pOwner = pOwner;

        vlc_object_attach( m_pAtmoThread, m_pOwner);

        vlc_mutex_init( &m_TerminateLock );
        err = vlc_cond_init( &m_TerminateCond );
        if(err) {
           msg_Err( m_pAtmoThread, "vlc_cond_init failed %d",err);
        }
    }
}

#else

CThread::CThread(void)
{
  m_hThread = CreateThread(NULL, 0, CThread::ThreadProc ,
                           this, CREATE_SUSPENDED, &m_dwThreadID);
  m_hTerminateEvent = CreateEvent(NULL,ATMO_FALSE,ATMO_FALSE,NULL);
}

#endif



#if defined(_ATMO_VLC_PLUGIN_)

CThread::~CThread(void)
{
  if(m_pAtmoThread)
  {
      vlc_mutex_destroy( &m_TerminateLock );
      vlc_cond_destroy( &m_TerminateCond );
      vlc_object_detach(m_pAtmoThread);
      vlc_object_release(m_pAtmoThread);
  }
}

#else

CThread::~CThread(void)
{
  CloseHandle(m_hThread);	
  CloseHandle(m_hTerminateEvent);
}

#endif

#if defined(_ATMO_VLC_PLUGIN_)

void *CThread::ThreadProc(vlc_object_t *obj)
{
      atmo_thread_t *pAtmoThread = (atmo_thread_t *)obj;
      CThread *pThread = (CThread *)pAtmoThread->p_thread;
      if(pThread) {
         int canc;

         canc = vlc_savecancel ();
         pThread->Execute();
         vlc_restorecancel (canc);
      }
      return NULL;
}

#else

DWORD WINAPI CThread::ThreadProc(LPVOID lpParameter)
{
	   CThread *aThread = (CThread *)lpParameter;
	   if(aThread)
	      return aThread->Execute();
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
   m_bTerminated = ATMO_TRUE;

#if defined(_ATMO_VLC_PLUGIN_)
   if(m_pAtmoThread)
   {
      vlc_mutex_lock( &m_TerminateLock );
      vlc_cond_signal( &m_TerminateCond  );
      vlc_mutex_unlock( &m_TerminateLock );
      vlc_object_kill( m_pAtmoThread );

      vlc_thread_join( m_pAtmoThread );
   }
#else
   SetEvent(m_hTerminateEvent);
   WaitForSingleObject(m_hThread,INFINITE);
#endif
}

void CThread::Run()
{
   m_bTerminated = ATMO_FALSE;

#if defined(_ATMO_VLC_PLUGIN_)
   m_pAtmoThread->b_die = false;
   if(vlc_thread_create( m_pAtmoThread,
                         "Atmo-CThread-Class",
                         CThread::ThreadProc,
                         VLC_THREAD_PRIORITY_LOW ))
   {
      msg_Err( m_pOwner, "cannot launch one of the AtmoLight threads");
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
     vlc_mutex_lock( &m_TerminateLock );
     int value = vlc_cond_timedwait(&m_TerminateCond,
                                    &m_TerminateLock,
                                    mdate() + (mtime_t)(millisekunden * 1000));
     vlc_mutex_unlock( &m_TerminateLock );
     return (value != 0);

#else
     DWORD res = WaitForSingleObject(m_hTerminateEvent,millisekunden);
	 return (res == WAIT_TIMEOUT);
#endif
}


/*******************************************************************************
 * video_sys.h: system dependant video output display method API
 * (c)1999 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
#if defined(VIDEO_X11)
int          vout_SysCreate  ( p_vout_thread_t p_vout, Display *p_display, Window root_window );
#elif defined(VIDEO_FB)
int          vout_SysCreate  ( p_vout_thread_t p_vout );
#endif
int          vout_SysInit    ( p_vout_thread_t p_vout );
void         vout_SysEnd     ( p_vout_thread_t p_vout );
void         vout_SysDestroy ( p_vout_thread_t p_vout );
int          vout_SysManage  ( p_vout_thread_t p_vout );
void         vout_SysDisplay ( p_vout_thread_t p_vout );



/*****************************************************************************
 * intf_sys.h: system dependant interface API
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int     intf_DummySysCreate  ( p_intf_thread_t p_intf );
void    intf_DummySysDestroy ( p_intf_thread_t p_intf );
void    intf_DummySysManage  ( p_intf_thread_t p_intf );
#ifdef VIDEO_X11
int     intf_X11SysCreate    ( p_intf_thread_t p_intf );
void    intf_X11SysDestroy   ( p_intf_thread_t p_intf );
void    intf_X11SysManage    ( p_intf_thread_t p_intf );
#endif
#ifdef VIDEO_FB
int     intf_FBSysCreate     ( p_intf_thread_t p_intf );
void    intf_FBSysDestroy    ( p_intf_thread_t p_intf );
void    intf_FBSysManage     ( p_intf_thread_t p_intf );
#endif
#ifdef VIDEO_GLIDE
int     intf_GlideSysCreate  ( p_intf_thread_t p_intf );
void    intf_GlideSysDestroy ( p_intf_thread_t p_intf );
void    intf_GlideSysManage  ( p_intf_thread_t p_intf );
#endif
#ifdef VIDEO_DGA
int     intf_DGASysCreate    ( p_intf_thread_t p_intf );
void    intf_DGASysDestroy   ( p_intf_thread_t p_intf );
void    intf_DGASysManage    ( p_intf_thread_t p_intf );
#endif
#ifdef VIDEO_GGI
int     intf_GGISysCreate    ( p_intf_thread_t p_intf );
void    intf_GGISysDestroy   ( p_intf_thread_t p_intf );
void    intf_GGISysManage    ( p_intf_thread_t p_intf );
#endif
#ifdef VIDEO_BEOS
int     intf_BeSysCreate     ( p_intf_thread_t p_intf );
void    intf_BeSysDestroy    ( p_intf_thread_t p_intf );
void    intf_BeSysManage     ( p_intf_thread_t p_intf );
#endif


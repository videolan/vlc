/*****************************************************************************
 * video_sys.h: system dependant video output display method API
 * (c)1999 VideoLAN
 *****************************************************************************/

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
int     vout_DummySysCreate   ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_DummySysInit     ( p_vout_thread_t p_vout );
void    vout_DummySysEnd      ( p_vout_thread_t p_vout );
void    vout_DummySysDestroy  ( p_vout_thread_t p_vout );
int     vout_DummySysManage   ( p_vout_thread_t p_vout );
void    vout_DummySysDisplay  ( p_vout_thread_t p_vout );
#ifdef VIDEO_X11
int     vout_X11SysCreate     ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_X11SysInit       ( p_vout_thread_t p_vout );
void    vout_X11SysEnd        ( p_vout_thread_t p_vout );
void    vout_X11SysDestroy    ( p_vout_thread_t p_vout );
int     vout_X11SysManage     ( p_vout_thread_t p_vout );
void    vout_X11SysDisplay    ( p_vout_thread_t p_vout );
#endif
#ifdef VIDEO_FB
int     vout_FBSysCreate      ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_FBSysInit        ( p_vout_thread_t p_vout );
void    vout_FBSysEnd         ( p_vout_thread_t p_vout );
void    vout_FBSysDestroy     ( p_vout_thread_t p_vout );
int     vout_FBSysManage      ( p_vout_thread_t p_vout );
void    vout_FBSysDisplay     ( p_vout_thread_t p_vout );
#endif
#ifdef VIDEO_GLIDE
int     vout_GlideSysCreate   ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_GlideSysInit     ( p_vout_thread_t p_vout );
void    vout_GlideSysEnd      ( p_vout_thread_t p_vout );
void    vout_GlideSysDestroy  ( p_vout_thread_t p_vout );
int     vout_GlideSysManage   ( p_vout_thread_t p_vout );
void    vout_GlideSysDisplay  ( p_vout_thread_t p_vout );
#endif
#ifdef VIDEO_DGA
int     vout_DGASysCreate     ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_DGASysInit       ( p_vout_thread_t p_vout );
void    vout_DGASysEnd        ( p_vout_thread_t p_vout );
void    vout_DGASysDestroy    ( p_vout_thread_t p_vout );
int     vout_DGASysManage     ( p_vout_thread_t p_vout );
void    vout_DGASysDisplay    ( p_vout_thread_t p_vout );
#endif
#ifdef VIDEO_GGI
int     vout_GGISysCreate     ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_GGISysInit       ( p_vout_thread_t p_vout );
void    vout_GGISysEnd        ( p_vout_thread_t p_vout );
void    vout_GGISysDestroy    ( p_vout_thread_t p_vout );
int     vout_GGISysManage     ( p_vout_thread_t p_vout );
void    vout_GGISysDisplay    ( p_vout_thread_t p_vout );
#endif
#ifdef VIDEO_BEOS
int     vout_BeSysCreate      ( p_vout_thread_t p_vout, char *psz_display, int i_root_window );
int     vout_BeSysInit        ( p_vout_thread_t p_vout );
void    vout_BeSysEnd         ( p_vout_thread_t p_vout );
void    vout_BeSysDestroy     ( p_vout_thread_t p_vout );
int     vout_BeSysManage      ( p_vout_thread_t p_vout );
void    vout_BeSysDisplay     ( p_vout_thread_t p_vout );
#endif


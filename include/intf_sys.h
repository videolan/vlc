/*******************************************************************************
 * intf_sys.h: system dependant interface API
 * (c)1999 VideoLAN
 *******************************************************************************/

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
int     intf_SysCreate ( p_intf_thread_t p_intf );
void    intf_SysDestroy( p_intf_thread_t p_intf );
void    intf_SysManage ( p_intf_thread_t p_intf );

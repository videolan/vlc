/*******************************************************************************
 * input_vlan.h: vlan input method
 * (c)1999 VideoLAN
 *******************************************************************************
 * ?? 
 *******************************************************************************
 * Required headers:
 * <netinet/in.h>
 * "vlc_thread.h"
 *******************************************************************************/

/*******************************************************************************
 * Vlan server related constants
 *******************************************************************************/

#define VLAN_SERVER_MSG_LENGTH  256             /* maximum length of a message */
#define VLAN_CLIENT_VERSION     "1.3.0"                 /* vlan client version */

/* Messages codes */
#define VLAN_LOGIN_REQUEST      98        /* login: <version> <login> <passwd> */
#define VLAN_LOGIN_ANSWER       97                    /* login accepted: [msg] */
#define VLAN_LOGIN_REJECTED     96                    /* login rejected: [msg] */

#define VLAN_LOGOUT             99                                   /* logout */

#define VLAN_INFO_REQUEST       31                        /* info: no argument */
#define VLAN_INFO_ANSWER        32/* info ok: <switch> <port> <vlan> <sharers> */
#define VLAN_INFO_REJECTED      33                     /* info rejected: [msg] */

#define VLAN_CHANGE_REQUEST     21/* change: <mac> [ip] <vlan dest> [vlan src] */
#define VLAN_CHANGE_ANSWER      22                         /* change ok: [msg] */
#define VLAN_CHANGE_REJECTED    23                     /* change failed: [msg] */

/*******************************************************************************
 * Macros to build/extract vlan_ids
 *******************************************************************************/
#define VLAN_ID_IFACE( vlan_id )    ( ((vlan_id) >> 8) & 0xff ) 
#define VLAN_ID_VLAN( vlan_id )     ( (vlan_id) & 0xff )
#define VLAN_ID( iface, vlan )      ( ((iface) << 8) | (vlan) )

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
int     input_VlanCreate  ( void );
void    input_VlanDestroy ( void );

int     input_VlanId            ( char *psz_iface, int i_vlan );
int     input_VlanJoin          ( int i_vlan_id );
void    input_VlanLeave         ( int i_vlan_id );
int     input_VlanRequest       ( char *psz_iface );
int     input_VlanSynchronize   ( void );




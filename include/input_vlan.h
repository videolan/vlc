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
 * input_vlan_server_t: vlan server
 *******************************************************************************
 * This structure describes a vlan server.
 *******************************************************************************/
typedef struct
{
    struct sockaddr_in  sa_in;                               /* server address */
    int                 i_socket;                         /* socket descriptor */

    /* Login informations */
    char *              psz_login;                             /* server login */
    char *              psz_passwd;                         /* server password */
} input_vlan_server_t;

/*******************************************************************************
 * input_vlan_iface_t: vlan-capable network interface
 *******************************************************************************
 * This structure describes the abilities of a network interface capable of
 * vlan management. Note that an interface could have several IP adresses, but
 * since only the MAC address is used to change vlan, only one needs to be
 * retrieved.
 * ?? it could be interesting to send a port id on vlan request, to know if two
 * interfaces are dependant regarding vlan changes.
 *******************************************************************************/
typedef struct
{
    char *                  psz_name;                        /* interface name */
    struct sockaddr_in      sa_in;                             /* interface IP */
    char                    psz_mac[20];                      /* interface MAC */

    /* Hardware properties */
    int                     i_master;                /* master interface index */
    int                     i_switch;                         /* switch number */
    int                     i_port;                             /* port number */
    int                     i_sharers;          /* number of MACs on this port */
    
    /* Vlan properties - these are only used if i_master is negative */
    int                     i_refcount;                       /* locks counter */
    int                     i_vlan;                            /* current vlan */
    int                     i_default_vlan;                    /* default vlan */
} input_vlan_iface_t;

/*******************************************************************************
 * vlan_method_data_t
 *******************************************************************************
 * Store global vlan library data.
 *******************************************************************************/
typedef struct
{    
    vlc_mutex_t             lock;                              /* library lock */

    /* Server */
    input_vlan_server_t     server;                             /* vlan server */
 
    /* Network interfaces */
    int                     i_ifaces;   /* number of vlan-compliant interfaces */
    input_vlan_iface_t *    p_iface;                             /* interfaces */
} input_vlan_method_t;

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
int     input_VlanMethodInit    ( input_vlan_method_t *p_method,
                                  char *psz_server, int i_port);
void    input_VlanMethodFree    ( input_vlan_method_t *p_method );

int     input_VlanId            ( char *psz_iface, int i_vlan );
int     input_VlanJoin          ( int i_vlan_id );
void    input_VlanLeave         ( int i_vlan_id );
int     input_VlanRequest       ( char *psz_iface );
int     input_VlanSynchronize   ( void );




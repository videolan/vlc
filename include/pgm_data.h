/*******************************************************************************
 * pgm_data.h: access to all program variables
 * (c)1999 VideoLAN
 *******************************************************************************
 * This header provides structures to access to all program variables. It should
 * only be used by interface.
 *******************************************************************************
 * Required headers:
 *  <netinet/in.h>
 *  <sys/soundcard.h>
 *  <sys/uio.h>
 *  <X11/Xlib.h>
 *  <X11/extensions/XShm.h>
 *  "config.h"
 *  "common.h"
 *  "mtime.h"
 *  "vlc_thread.h"
 *  "input.h"
 *  "input_vlan.h"
 *  "audio_output.h"
 *  "video.h"
 *  "video_output.h"
 *  "xconsole.h"
 *  "interface.h"
 *  "intf_msg.h"
 *******************************************************************************/

/*******************************************************************************
 * main_config_t
 *******************************************************************************
 * Store the main configuration (non thread-dependant configuration), such as
 * parameters read from command line and name of configuration file
 *******************************************************************************/
typedef struct
{
    boolean_t               b_audio;              /* is audio output allowed ? */
    boolean_t               b_video;              /* is video output allowed ? */
    boolean_t               b_vlans;                  /* are vlans supported ? */
    
    /* Vlan input method configuration */
    char *                  psz_input_vlan_server;              /* vlan server */
    int                     i_input_vlan_server_port;      /* vlan server port */    
} main_config_t;

/*******************************************************************************
 * program_data_t, p_program_data (global variable)
 *******************************************************************************
 * This structure has an unique instance, declared in main and pointed by the
 * only global variable of the program. It should allow access to any variable
 * of the program, for user-interface purposes or more easier call of interface
 * and common functions (example: the intf_*Msg functions). Please avoid using
 * it when you can access the members you need in an other way. In fact, it
 * should only be used by interface thread.
 *******************************************************************************/
typedef struct
{
    /* Global properties */
    int                     i_argc;            /* command line arguments count */
    char **                 ppsz_argv;               /* command line arguments */
    char **                 ppsz_env;                 /* environment variables */
    
    /* Configurations */
    main_config_t           cfg;                      /* general configuration */
    video_cfg_t             vout_cfg;            /* video output configuration */

    /* Threads */
    aout_thread_t           aout_thread;                /* audio output thread */
    intf_thread_t           intf_thread;                   /* interface thread */    

    /* Shared data - these structures are accessed directly from p_program_data
     * by several libraries */
    interface_msg_t         intf_msg;               /* messages interface data */
    input_vlan_method_t     input_vlan_method;            /* vlan input method */    
} program_data_t;

extern program_data_t *p_program_data;


/*******************************************************************************
 * main.h: access to all program variables
 * (c)1999 VideoLAN
 *******************************************************************************
 * Declaration and extern access to global program object.
 *******************************************************************************/

/*******************************************************************************
 * main_t, p_main (global variable)
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
    int                    i_argc;             /* command line arguments count */
    char **                ppsz_argv;                /* command line arguments */
    char **                ppsz_env;                  /* environment variables */

    /* Generic settings */
    boolean_t              b_audio;               /* is audio output allowed ? */
    boolean_t              b_video;               /* is video output allowed ? */
    boolean_t              b_vlans;                   /* are vlans supported ? */
    
    /* Unique threads */
    p_aout_thread_t        p_aout;                      /* audio output thread */
    p_intf_thread_t        p_intf;                    /* main interface thread */

    /* Shared data - these structures are accessed directly from p_main by
     * several modules */
    p_intf_msg_t           p_msg;                   /* messages interface data */
    p_input_vlan_method_t  p_input_vlan;                  /* vlan input method */
} main_t;

extern main_t *p_main;

/*******************************************************************************
 * Prototypes - these methods are used to get default values for some threads
 * and modules.
 *******************************************************************************/
int    main_GetIntVariable( char *psz_name, int i_default );
char * main_GetPszVariable( char *psz_name, char *psz_default );
void   main_PutIntVariable( char *psz_name, int i_value );
void   main_PutPszVariable( char *psz_name, char *psz_value );

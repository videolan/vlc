/*******************************************************************************
 * config.h: limits and configuration
 * (c)1999 VideoLAN
 *******************************************************************************
 * Defines all compilation-time configuration constants and size limits
 *******************************************************************************
 * required headers:
 *  none
 *******************************************************************************/

/*******************************************************************************
 * Program information
 *******************************************************************************/

/* Program version and copyright message */
#define PROGRAM_VERSION		"0.0.x"
#define COPYRIGHT_MESSAGE	"VideoLAN Client v" PROGRAM_VERSION " (" __DATE__ ") - (c)1999 VideoLAN\n"

/*******************************************************************************
 * General compilation options
 *******************************************************************************/

/* Define for DVB support - Note that some extensions or restrictions may be
 * incompatible with native MPEG2 streams */
//#define DVB_EXTENSIONS
//#define DVB_RESTRICTIONS

/* Define for profiling support */
//#define STATS

/* Define for unthreaded version of the program - ?? not yet implemented */
//#define NO_THREAD

/*******************************************************************************
 * Debugging options - define or undefine symbols
 *******************************************************************************/

/* General debugging support */
#define DEBUG

/* Modules specific debugging */
#define DEBUG_INTF
#define DEBUG_INPUT
#define DEBUG_AUDIO
#define DEBUG_VIDEO

/* Debugging log file - if defined, a file can be used to store all messages. If
 * DEBUG_LOG_ONLY is defined, debug messages will only be printed to the log and
 * will not appear on the screen */
//#define DEBUG_LOG       "vlc-debug.log"
//#define DEBUG_LOG_ONLY  

/* ?? VOUT_DEBUG and co have changed ! */

/*******************************************************************************
 * Common settings
 *******************************************************************************/

/* Automagically spawn input, audio and video threads ? */
#define AUTO_SPAWN

/* Startup script */
#define INIT_SCRIPT	"vlc.init"

/* ?? */
#define THREAD_SLEEP    100000

/*
 * X11/XLib settings
 */

/* Default font used when a wished font could not be loaded - note that this
 * font should be universal, else the program will exit when it can't find
 * a font */
#define X11_DEFAULT_FONT                "fixed"

/*
 * Decoders FIFO configuration
 */

/* Size of the FIFO. FIFO_SIZE+1 must be a multiple of 2 */
#define FIFO_SIZE                       511

/*******************************************************************************
 * Input thread configuration
 *******************************************************************************/

/* ?? */
#define INPUT_IDLE_SLEEP                100000

/*
 * General limitations
 */

/* Broadcast address, in case of a broadcasted stream */
#define INPUT_BCAST_ADDR                "138.195.143.255"

/* Maximum number of input threads - this value is used exclusively by
 * interface, and is in fact an interface limitation */
#define INPUT_MAX_THREADS               10

/* Maximum number of programs definitions in a TS stream */
#define INPUT_MAX_PGRM                  10

/* Maximum number of ES definitions in a TS stream */
#define INPUT_MAX_ES                    10

/* Maximum number of ES in a single program */
#define INPUT_MAX_PROGRAM_ES            10

/* Maximum number of selected ES in an input thread */
#define INPUT_MAX_SELECTED_ES           10

/* Maximum number of TS packets in the client at any time
 * INPUT_MAX_TS + 1 must be a power of 2, to optimize the %(INPUT_MAX_TS+1)
 * operation with a &INPUT_MAX_TS in the case of a fifo netlist.
 * It should be > number of fifos * FIFO_SIZE to avoid input deadlock. */
#define INPUT_MAX_TS                    16383      /* INPUT_MAX_TS + 1 = 2^14 */

/* Same thing with PES packets */
#define INPUT_MAX_PES                   16383

/* Maximum number of TS packets we read from the socket in one readv().
 * Since you can't put more than 7 TS packets in an Ethernet frame,
 * the maximum value is 7. This number should also limit the stream server,
 * otherwise any supplementary packet is lost. */
#define INPUT_TS_READ_ONCE              7

/* Use a LIFO or FIFO for TS netlist ? */
#undef INPUT_LIFO_TS_NETLIST

/* Use a LIFO or FIFO for PES netlist ? */
#undef INPUT_LIFO_PES_NETLIST

/* Maximum length of a hostname */
#define INPUT_MAX_HOSTNAME_LENGTH       100


/* Default input method */
#define INPUT_DEFAULT_METHOD            20 /* unicast (debug) */

/* Default remote server */
#define VIDEOLAN_DEFAULT_SERVER         "vod.via.ecp.fr"

/* Default videolan port */
#define VIDEOLAN_DEFAULT_PORT           1234

/* Default videolan VLAN */
#define VIDEOLAN_DEFAULT_VLAN           3

/*
 * Vlan method 
 */ 

/* Default VLAN server */
#define VLAN_DEFAULT_SERVER             "vlanserver.via.ecp.fr"
#define VLAN_DEFAULT_SERVER_PORT        6010

/*******************************************************************************
 * Audio output thread configuration
 *******************************************************************************/

/*******************************************************************************
 * Video output thread configuration
 *******************************************************************************/

/*
 * Default settings for video output threads
 */

/* Title of the window */
#define VOUT_TITLE                      "VideoLAN Client: output"

/* Default use of XShm extension */
#define VOUT_SHM_EXT                    1

/* Dimensions for display window */
#define VOUT_WIDTH                      544
#define VOUT_HEIGHT                     576

/* Default heap size */
#define VOUT_HEAP_SIZE                  100

/*
 * Limitations
 */

/* Maximum number of video output threads - this value is used exclusively by
 * interface, and is in fact an interface limitation */
#define VOUT_MAX_THREADS                10

/* Maximum number of video streams per video output thread */
#define VOUT_MAX_STREAMS                10

/* Maximum number of pictures which can be rendered in one loop, plus one */
#define VOUT_MAX_PICTURES               10

/*
 * Other settings
 */

/* Time during which the thread will sleep if it has nothing to 
 * display (in micro-seconds) */
/* ?? this constant will probably evolve to a calculated value */
#define VOUT_IDLE_SLEEP                 50000

/* Maximum lap of time allowed between the beginning of rendering and
 * display. If, compared to the current date, the next image is too
 * late, the thread will perform an idle loop. This time should be
 * at least VOUT_IDLE_SLEEP plus the time required to render a few
 * images, to avoid trashing of decoded images */
/* ?? this constant will probably evolve to a calculated value */
#define VOUT_DISPLAY_DELAY              150000

/* Maximum lap of time during which images are rendered in the same 
 * time. It should be greater than the maximum time between two succesive
 * images to avoid useless renderings and calls to the display driver,
 * but not to high to avoid desynchronization */
/* ?? this constant will probably evolve to a calculated value */
#define VOUT_DISPLAY_TOLERANCE          150000

/*******************************************************************************
 * Video decoder configuration
 *******************************************************************************/

#define VDEC_IDLE_SLEEP                 100000

/*******************************************************************************
 * Generic decoder configuration
 *******************************************************************************/

#define GDEC_IDLE_SLEEP                 100000

/*******************************************************************************
 * Interface (main) thread configuration
 *******************************************************************************/

/*
 * Interface configuration
 */

/* Base delay in micro second for interface sleeps ?? */
#define INTF_IDLE_SLEEP                 100000

/* Maximal number of arguments on a command line, including the function name */
#define INTF_MAX_ARGS                   20

/* Maximal size of a command line in a script */
#define INTF_MAX_CMD_SIZE               240

/*
 * Messages functions
 */

/* Maximal size of the message queue - in case of overflow, all messages in the
 * queue are printed by the calling thread */
#define INTF_MSG_QSIZE                  32

/* Define to enable messages queues - disabling messages queue can be usefull
 * when debugging, since it allows messages which would not otherwise be printed,
 * due to a crash, to be printed anyway */
/*#define INTF_MSG_QUEUE*/

/* Format of the header for debug messages. The arguments following this header
 * are the file (char *), the function (char *) and the line (int) in which the
 * message function was called */
#define INTF_MSG_DBG_FORMAT "## %s:%s(),%i: "

/* Filename to log message
 * Note that messages are only logged when debugging */
//#define INTF_MSG_LOGFILE "vlc.log"

/*
 * X11 console properties
 */

/* Title of the X11 console interface window */
#define INTF_XCONSOLE_TITLE             "VideoLAN Client: console"

/* Welcome message: this message is always displayed when a new console is
 * openned */
#define INTF_XCONSOLE_WELCOME_MSG       COPYRIGHT_MESSAGE "try `help' to have a list of available commands"

/* Background pixmap - if not defined, no pixmap is used */
#define INTF_XCONSOLE_BACKGROUND_PIXMAP "Resources/background.xpm"

/* Default X11 console interface window geometry. It should at least give a
 * default size */
#define INTF_XCONSOLE_GEOMETRY          "400x100"

/* Font used in console. If first font is not found, the fallback font is
 * used. Therefore, the fallback font should be a universal one. */
#define INTF_XCONSOLE_FONT              "-*-helvetica-medium-r-normal-*-18-*-*-*-*-*-iso8859-1"

/* Number of memorized lines in X11 console window text zone */
#define INTF_XCONSOLE_MAX_LINES         100

/* Maximal number of commands which can be saved in history list */
#define INTF_XCONSOLE_HISTORY_SIZE      20

/* Maximum width of a line in an X11 console window. If a larger line is
 * printed, it will be wrapped. */
#define INTF_XCONSOLE_MAX_LINE_WIDTH    120


/*******************************************************************************
 * Network and VLAN management
 *******************************************************************************/
/* Default network interface to use */
#define NET_DFLT_IF			"eth0"

/* Default VLANserver address */
#define VLAN_DFLT_VLANSRV		"vlanserver"

/* Default VLANserver port */
#define VLAN_DFLT_VLANPORT		"6010"

/* Client identification */
#define VLAN_LOGIN			"guest"
#define VLAN_PASSWD			"none"

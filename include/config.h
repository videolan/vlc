/*******************************************************************************
 * config.h: limits and configuration
 * (c)1999 VideoLAN
 *******************************************************************************
 * Defines all compilation-time configuration constants and size limits
 *******************************************************************************/

/* Conventions regarding names of symbols and variables
 * ----------------------------------------------------
 *
 * - Symbols should begin with a prefix indicating in which module they are
 *   used, such as INTF_, VOUT_ or ADEC_.
 *
 * - Regarding environment variables, which are used as initialization parameters 
 *   for threads :
 *   + variable names should end with '_VAR'
 *   + environment variable default value should end with '_DEFAULT'
 *   + values having a special meaning with '_VAL' 
 *   
 */

/*******************************************************************************
 * Program information
 *******************************************************************************/

/* Program options - this part will produce a copyright message with compilation
 * options informations, based on some definines set in the Makefile - do not
 * edit */
#if defined(VIDEO_X11)
#define VIDEO_OPTIONS                   "X11"
#elif defined(VIDEO_FB)
#define VIDEO_OPTIONS                   "Framebuffer"
#elif defined(VIDEO_GGI)
#define VIDEO_OPTIONS                   "GGI"
#else
#define VIDEO_OPTIONS                   ""
#endif
#if defined(HAVE_MMX)
#define ARCH_OPTIONS                    "MMX"
#else
#define ARCH_OPTIONS                    ""
#endif
#define PROGRAM_OPTIONS                 VIDEO_OPTIONS " " ARCH_OPTIONS

/* Program version and copyright message */
#define PROGRAM_VERSION		        "DR 2.1"
#define COPYRIGHT_MESSAGE	        "VideoLAN Client v" PROGRAM_VERSION " (" __DATE__ ") - " \
                                        PROGRAM_OPTIONS " - (c)1999 VideoLAN"

/*******************************************************************************
 * General compilation options
 *******************************************************************************/

/* Define for DVB support - Note that some extensions or restrictions may be
 * incompatible with native MPEG2 streams */
//#define DVB_EXTENSIONS
//#define DVB_RESTRICTIONS

/* Define to disable some obscure heuristics behind the video_parser and the
 * video_decoder that improve performance but are not fully MPEG2 compliant
 * and might cause problems with some very weird streams. */
//#define MPEG2_COMPLIANT

/* Define for profiling and statistics support - such informations, like FPS
 * or pictures count won't be available if it not set */
#define STATS

/* Define for unthreaded version of the program - ?? not yet implemented */
//#define NO_THREAD

/*******************************************************************************
 * Debugging options - define or undefine symbols
 *******************************************************************************/

/* General debugging support */
#define DEBUG

/* Modules specific debugging - this will produce a lot of output, but can be
 * usefull to track a bug */
#define DEBUG_INTF
#define DEBUG_INPUT
#define DEBUG_AUDIO
#define DEBUG_VIDEO

/* Debugging log file - if defined, a file can be used to store all messages. If
 * DEBUG_LOG_ONLY is defined, debug messages will only be printed to the log and
 * will not appear on the screen */
#define DEBUG_LOG                       "vlc-debug.log"
#define DEBUG_LOG_ONLY

/*******************************************************************************
 * General configuration
 *******************************************************************************/

/* Automagically spawn input, audio and video threads ? */
// ?? used ?
#define AUTO_SPAWN

/* When creating or destroying threads in blocking mode, delay to poll thread
 * status */
#define THREAD_SLEEP                    10000

/*
 * Decoders FIFO configuration
 */

/* Size of the FIFO. FIFO_SIZE+1 must be a multiple of 2 */
#define FIFO_SIZE                       1023


/*******************************************************************************
 * Interface configuration
 *******************************************************************************/

/* Environment variable used to store startup script name and default value */
#define INTF_INIT_SCRIPT_VAR	        "vlc_init"
#define INTF_INIT_SCRIPT_DEFAULT        "vlc.init"

/* Base delay in micro second for interface sleeps */
#define INTF_IDLE_SLEEP                 100000

/*
 * X11 settings
 */

/* Title of the X11 window */
#define VOUT_TITLE                      "VideoLAN Client"

/* Environment variable used in place of DISPLAY if available */
#define ENV_VLC_DISPLAY                 "vlc_DISPLAY"

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
#define INPUT_MAX_TS                    32767      /* INPUT_MAX_TS + 1 = 2^15 */

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
#define INPUT_DEFAULT_METHOD            INPUT_METHOD_TS_UCAST

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
 * Audio configuration
 *******************************************************************************/

/* Environment variable used to store dsp device name, and default value */
#define AOUT_DSP_VAR                    "vlc_dsp"
#define AOUT_DSP_DEFAULT                "/dev/dsp"

/* Environment variable for stereo, and default value */
#define AOUT_STEREO_VAR                 "vlc_stereo"
#define AOUT_STEREO_DEFAULT             1

/* Environment variable for output rate, and default value */
#define AOUT_RATE_VAR                   "vlc_audio_rate"
#define AOUT_RATE_DEFAULT               44100 

/*******************************************************************************
 * Video configuration
 *******************************************************************************/

/*
 * Default settings for video output threads
 */

/* Default dimensions for display window - these dimensions are the standard 
 * width and height for broadcasted MPEG-2 */
#define VOUT_WIDTH                      544
#define VOUT_HEIGHT                     576

/* Default video heap size - remember that a decompressed picture is big 
 * (~1 Mbyte) before using huge values */
#define VOUT_MAX_PICTURES               10

/* Environment variable for grayscale output mode, and default value */
#define VOUT_GRAYSCALE_VAR              "vlc_grayscale"
#define VOUT_GRAYSCALE_DEFAULT          0

/*
 * Time settings
 */

/* Time during which the thread will sleep if it has nothing to 
 * display (in micro-seconds) */
/* ?? this constant will probably evolve to a calculated value */
#define VOUT_IDLE_SLEEP                 20000

/* Maximum lap of time allowed between the beginning of rendering and
 * display. If, compared to the current date, the next image is too
 * late, the thread will perform an idle loop. This time should be
 * at least VOUT_IDLE_SLEEP plus the time required to render a few
 * images, to avoid trashing of decoded images */
/* ?? this constant will probably evolve to a calculated value */
#define VOUT_DISPLAY_DELAY              100000

/* Delay (in microseconds) between increments in idle levels */
#define VOUT_IDLE_DELAY                 5000000

/* Number of pictures required to computes the FPS rate */
#define VOUT_FPS_SAMPLES                5

/*
 * Framebuffer settings
 */

/* Environment variable for framebuffer device, and default value */
#define VOUT_FB_DEV_VAR                 "vlc_fb_dev"
#define VOUT_FB_DEV_DEFAULT             "/dev/fb0"

/*
 * X11 settings 
 */

/* Allow use of X11 XShm (shared memory) extension if possible */
#define VOUT_XSHM                       1

/* Font maximum and minimum characters - characters outside this range are not
 * printed - maximum range is 1-256 */
#define VOUT_MIN_CHAR                   1
#define VOUT_MAX_CHAR                   128

/*******************************************************************************
 * Video parser configuration
 *******************************************************************************/

#define VPAR_IDLE_SLEEP                 100000

/* Number of macroblock buffers available. It should be always greater than
 * twice the number of macroblocks in a picture. VFIFO_SIZE + 1 should also
 * be a power of two. */
#define VFIFO_SIZE                      4095

/* Maximum number of macroblocks in a picture. */
#define MAX_MB                          2048

/*******************************************************************************
 * Video decoder configuration
 *******************************************************************************/

#define VDEC_IDLE_SLEEP                 100000

/* Number of video_decoder threads to launch on startup of the video_parser.
 * It should always be less than half the number of macroblocks of a
 * picture. */
#define NB_VDEC                         1

/* Maximum range of values out of the IDCT + motion compensation. Only
 * used if you define MPEG2_COMPLIANT above. */
#define VDEC_CROPRANGE                  2048

/*******************************************************************************
 * Generic decoder configuration
 *******************************************************************************/

#define GDEC_IDLE_SLEEP                 100000

/*******************************************************************************
 * Messages and console interfaces configuration
 *******************************************************************************/

/* Maximal size of the message queue - in case of overflow, all messages in the
 * queue are printed by the calling thread */
#define INTF_MSG_QSIZE                  64

/* Define to enable messages queues - disabling messages queue can be usefull
 * when debugging, since it allows messages which would not otherwise be printed,
 * due to a crash, to be printed anyway */
#define INTF_MSG_QUEUE

/* Format of the header for debug messages. The arguments following this header
 * are the file (char *), the function (char *) and the line (int) in which the
 * message function was called */
#define INTF_MSG_DBG_FORMAT             "## %s:%s(),%i: "

/* Maximal number of arguments on a command line, including the function name */
#define INTF_MAX_ARGS                   20

/* Maximal size of a command line in a script */
#define INTF_MAX_CMD_SIZE               240

/* Number of memorized lines in console window text zone */
#define INTF_CONSOLE_MAX_TEXT           100

/* Maximal number of commands which can be saved in history list */
#define INTF_CONSOLE_MAX_HISTORY        20

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

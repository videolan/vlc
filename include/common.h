/*****************************************************************************
 * common.h: common definitions
 * Collection of useful common types and macros definitions
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 * $Id: common.h,v 1.73 2002/02/15 13:32:52 sam Exp $
 *
 * Authors: Samuel Hocevar <sam@via.ecp.fr>
 *          Vincent Seguin <seguin@via.ecp.fr>
 *          Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Required system headers
 *****************************************************************************/
#include <string.h>                                            /* strerror() */

/*****************************************************************************
 * Basic types definitions
 *****************************************************************************/

typedef u8                  byte_t;

/* Boolean type */
#ifdef BOOLEAN_T_IN_SYS_TYPES_H
#   include <sys/types.h>
#elif defined(BOOLEAN_T_IN_PTHREAD_H)
#   include <pthread.h>
#elif defined(BOOLEAN_T_IN_CTHREADS_H)
#   include <cthreads.h>
#else
typedef int                 boolean_t;
#endif
#ifdef SYS_GNU
#   define _MACH_I386_BOOLEAN_H_
#endif

/* ptrdiff_t definition */
#ifdef HAVE_STDDEF_H
#   include <stddef.h>
#else
#   include <malloc.h>
#   ifndef _PTRDIFF_T
#       define _PTRDIFF_T
/* Not portable in a 64-bit environment. */
typedef int                 ptrdiff_t;
#   endif
#endif

/* Counter for statistics and profiling */
typedef unsigned long       count_t;

/* DCT elements types */
typedef s16                 dctelem_t;

/* Video buffer types */
typedef u8                  yuv_data_t;

/*****************************************************************************
 * mtime_t: high precision date or time interval
 *****************************************************************************
 * Store an high precision date or time interval. The maximum precision is the
 * micro-second, and a 64 bits integer is used to avoid any overflow (maximum
 * time interval is then 292271 years, which should be long enough for any
 * video). Date are stored as a time interval since a common date.
 * Note that date and time intervals can be manipulated using regular
 * arithmetic operators, and that no special functions are required.
 *****************************************************************************/
typedef s64 mtime_t;

/*****************************************************************************
 * Classes declaration
 *****************************************************************************/

/* Plugins */
struct plugin_bank_s;
struct plugin_info_s;

typedef struct plugin_bank_s *          p_plugin_bank_t;
typedef struct plugin_info_s *          p_plugin_info_t;

/* Plugins */
struct playlist_s;
struct playlist_item_s;
struct module_s;

typedef struct playlist_s *             p_playlist_t;
typedef struct playlist_item_s *        p_playlist_item_t;

/* Interface */
struct intf_thread_s;
struct intf_sys_s;
struct intf_console_s;
struct intf_msg_s;
struct intf_channel_s;

typedef struct intf_thread_s *          p_intf_thread_t;
typedef struct intf_sys_s *             p_intf_sys_t;
typedef struct intf_console_s *         p_intf_console_t;
typedef struct intf_msg_s *             p_intf_msg_t;
typedef struct intf_channel_s *         p_intf_channel_t;

/* Input */
struct input_thread_s;
struct input_channel_s;
struct input_cfg_s;
struct input_area_s;

typedef struct input_thread_s *         p_input_thread_t;
typedef struct input_channel_s *        p_input_channel_t;
typedef struct input_cfg_s *            p_input_cfg_t;
typedef struct input_area_s *           p_input_area_t;

/* Audio */
struct aout_thread_s;
struct aout_sys_s;

typedef struct aout_thread_s *          p_aout_thread_t;
typedef struct aout_sys_s *             p_aout_sys_t;

/* Video */
struct vout_thread_s;
struct vout_font_s;
struct vout_sys_s;
struct chroma_sys_s;
struct vdec_thread_s;
struct vpar_thread_s;
struct video_parser_s;

typedef struct vout_thread_s *          p_vout_thread_t;
typedef struct vout_font_s *            p_vout_font_t;
typedef struct vout_sys_s *             p_vout_sys_t;
typedef struct chroma_sys_s *           p_chroma_sys_t;
typedef struct vdec_thread_s *          p_vdec_thread_t;
typedef struct vpar_thread_s *          p_vpar_thread_t;
typedef struct video_parser_s *         p_video_parser_t;

/* Decoders */
struct decoder_config_s;
struct decoder_fifo_s;

/* Misc */
struct macroblock_s;
struct data_packet_s;
struct imdct_s;
struct complex_s;
struct dm_par_s;
struct picture_s;
struct picture_sys_s;
struct picture_heap_s;
struct es_descriptor_s;
struct pgrm_descriptor_s;
struct pes_packet_s;
struct input_area_s;
struct bit_stream_s;

/*****************************************************************************
 * Macros and inline functions
 *****************************************************************************/
#ifdef NTOHL_IN_SYS_PARAM_H
#   include <sys/param.h>

#elif defined(WIN32)
/* Swap bytes in 16 bit value.  */
#   define __bswap_constant_16(x) \
     ((((x) >> 8) & 0xff) | (((x) & 0xff) << 8))

#   if defined __GNUC__ && __GNUC__ >= 2
#       define __bswap_16(x) \
            (__extension__                                                    \
             ({ register unsigned short int __v;                              \
                if (__builtin_constant_p (x))                                 \
                  __v = __bswap_constant_16 (x);                              \
                else                                                          \
                  __asm__ __volatile__ ("rorw $8, %w0"                        \
                                        : "=r" (__v)                          \
                                        : "0" ((unsigned short int) (x))      \
                                        : "cc");                              \
                __v; }))
#   else
/* This is better than nothing.  */
#       define __bswap_16(x) __bswap_constant_16 (x)
#   endif

/* Swap bytes in 32 bit value.  */
#   define __bswap_constant_32(x) \
        ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) |            \
         (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

#   if defined __GNUC__ && __GNUC__ >= 2
/* To swap the bytes in a word the i486 processors and up provide the
   `bswap' opcode.  On i386 we have to use three instructions.  */
#       if !defined __i486__ && !defined __pentium__ && !defined __pentiumpro__
#           define __bswap_32(x) \
                (__extension__                                                \
                 ({ register unsigned int __v;                                \
                    if (__builtin_constant_p (x))                             \
                      __v = __bswap_constant_32 (x);                          \
                    else                                                      \
                      __asm__ __volatile__ ("rorw $8, %w0;"                   \
                                            "rorl $16, %0;"                   \
                                            "rorw $8, %w0"                    \
                                            : "=r" (__v)                      \
                                            : "0" ((unsigned int) (x))        \
                                            : "cc");                          \
                    __v; }))
#       else
#           define __bswap_32(x) \
                (__extension__                                                \
                 ({ register unsigned int __v;                                \
                    if (__builtin_constant_p (x))                             \
                      __v = __bswap_constant_32 (x);                          \
                    else                                                      \
                      __asm__ __volatile__ ("bswap %0"                        \
                                            : "=r" (__v)                      \
                                            : "0" ((unsigned int) (x)));      \
                    __v; }))
#       endif
#   else
#       define __bswap_32(x) __bswap_constant_32 (x)
#   endif

#   if defined __GNUC__ && __GNUC__ >= 2
/* Swap bytes in 64 bit value.  */
#       define __bswap_constant_64(x) \
            ((((x) & 0xff00000000000000ull) >> 56)                            \
             | (((x) & 0x00ff000000000000ull) >> 40)                          \
             | (((x) & 0x0000ff0000000000ull) >> 24)                          \
             | (((x) & 0x000000ff00000000ull) >> 8)                           \
             | (((x) & 0x00000000ff000000ull) << 8)                           \
             | (((x) & 0x0000000000ff0000ull) << 24)                          \
             | (((x) & 0x000000000000ff00ull) << 40)                          \
             | (((x) & 0x00000000000000ffull) << 56))

#       define __bswap_64(x) \
            (__extension__                                                    \
             ({ union { __extension__ unsigned long long int __ll;            \
                        unsigned long int __l[2]; } __w, __r;                 \
                if (__builtin_constant_p (x))                                 \
                  __r.__ll = __bswap_constant_64 (x);                         \
                else                                                          \
                  {                                                           \
                    __w.__ll = (x);                                           \
                    __r.__l[0] = __bswap_32 (__w.__l[1]);                     \
                    __r.__l[1] = __bswap_32 (__w.__l[0]);                     \
                  }                                                           \
                __r.__ll; }))
#   endif

#else /* NTOHL_IN_SYS_PARAM_H || WIN32 */
#   include <netinet/in.h>

#endif /* NTOHL_IN_SYS_PARAM_H || WIN32 */

/* CEIL: division with round to nearest greater integer */
#define CEIL(n, d)  ( ((n) / (d)) + ( ((n) % (d)) ? 1 : 0) )

/* PAD: PAD(n, d) = CEIL(n ,d) * d */
#define PAD(n, d)   ( ((n) % (d)) ? ((((n) / (d)) + 1) * (d)) : (n) )

/* MAX and MIN: self explanatory */
#ifndef MAX
#   define MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#endif
#ifndef MIN
#   define MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )
#endif

/* MSB (big endian)/LSB (little endian) conversions - network order is always
 * MSB, and should be used for both network communications and files. Note that
 * byte orders other than little and big endians are not supported, but only
 * the VAX seems to have such exotic properties - note that these 'functions'
 * needs <netinet/in.h> or the local equivalent. */
#if !defined( WIN32 )
#if WORDS_BIGENDIAN
#   define hton16      htons
#   define hton32      htonl
#   define hton64(i)   ( i )
#   define ntoh16      ntohs
#   define ntoh32      ntohl
#   define ntoh64(i)   ( i )
#else
#   define hton16      htons
#   define hton32      htonl
    static __inline__ u64 __hton64( u64 i )
    {
        return ((u64)(htonl((i) & 0xffffffff)) << 32)
                | htonl(((i) >> 32) & 0xffffffff );
    }
#   define hton64(i)   __hton64( i )
#   define ntoh16      ntohs
#   define ntoh32      ntohl
#   define ntoh64      hton64
#endif
#endif /* !defined( WIN32 ) */

/* Macros with automatic casts */
#define U64_AT(p)   ( ntoh64 ( *( (u64 *)(p) ) ) )
#define U32_AT(p)   ( ntoh32 ( *( (u32 *)(p) ) ) )
#define U16_AT(p)   ( ntoh16 ( *( (u16 *)(p) ) ) )

/* Alignment of critical static data structures */
#ifdef ATTRIBUTE_ALIGNED_MAX
#   define ATTR_ALIGN(align) __attribute__ ((__aligned__ ((ATTRIBUTE_ALIGNED_MAX < align) ? ATTRIBUTE_ALIGNED_MAX : align)))
#else
#   define ATTR_ALIGN(align)
#endif

/* Alignment of critical dynamic data structure */
#ifdef HAVE_MEMALIGN
    /* Some systems have memalign() but no declaration for it */
    void * memalign( size_t align, size_t size );
#else
#   ifdef HAVE_VALLOC
        /* That's like using a hammer to kill a fly, but eh... */
#       include <unistd.h>
#       define memalign(align,size) valloc(size)
#   else
        /* Assume malloc alignment is sufficient */
#       define memalign(align,size) malloc(size)
#   endif    
#endif

#define I64C(x)         x##LL


#if defined( WIN32 )
/* The ntoh* and hton* bytes swapping functions are provided by winsock
 * but for conveniency and speed reasons it is better to implement them
 * ourselves. ( several plugins use them and it is too much hassle to link
 * winsock with each of them ;-)
 */
#   ifdef WORDS_BIGENDIAN
#       define ntoh32(x)       (x)
#       define ntoh16(x)       (x)
#       define ntoh64(x)       (x)
#       define hton32(x)       (x)
#       define hton16(x)       (x)
#       define hton64(x)       (x)
#   else
#       define ntoh32(x)     __bswap_32 (x)
#       define ntoh16(x)     __bswap_16 (x)
#       define ntoh64(x)     __bswap_32 (x)
#       define hton32(x)     __bswap_32 (x)
#       define hton16(x)     __bswap_16 (x)
#       define hton64(x)     __bswap_64 (x)
#   endif

/* win32, cl and icl support */
#   if defined( _MSC_VER )
#       define __attribute__(x)
#       define __inline__      __inline
#       define strncasecmp     strnicmp
#       define strcasecmp      stricmp
#       define S_ISBLK(m)      (0)
#       define S_ISCHR(m)      (0)
#       define S_ISFIFO(m)     (((m)&_S_IFMT) == _S_IFIFO)
#       define S_ISREG(m)      (((m)&_S_IFMT) == _S_IFREG)
#       undef I64C(x)
#       define I64C(x)         x##i64
#   endif

/* several type definitions */
#   if defined( __MINGW32__ )
#       if !defined( _OFF_T_ )
typedef long long _off_t;
typedef _off_t off_t;
#           define _OFF_T_
#       else
#           define off_t long long
#       endif
#   endif

#   if defined( _MSC_VER )
#       if !defined( _OFF_T_DEFINED )
typedef __int64 off_t;
#           define _OFF_T_DEFINED
#       else
#           define off_t __int64
#       endif
#       define stat _stati64
#   endif

#   ifndef O_NONBLOCK
#       define O_NONBLOCK 0
#   endif

#   ifndef snprintf
#       define snprintf _snprintf  /* snprintf not defined in mingw32 (bug?) */
#   endif

#endif

/*****************************************************************************
 * CPU capabilities
 *****************************************************************************/
#define CPU_CAPABILITY_NONE    0
#define CPU_CAPABILITY_486     (1<<0)
#define CPU_CAPABILITY_586     (1<<1)
#define CPU_CAPABILITY_PPRO    (1<<2)
#define CPU_CAPABILITY_MMX     (1<<3)
#define CPU_CAPABILITY_3DNOW   (1<<4)
#define CPU_CAPABILITY_MMXEXT  (1<<5)
#define CPU_CAPABILITY_SSE     (1<<6)
#define CPU_CAPABILITY_ALTIVEC (1<<16)
#define CPU_CAPABILITY_FPU     (1<<31)

/*****************************************************************************
 * I18n stuff
 *****************************************************************************/
#if defined( ENABLE_NLS ) && defined ( HAVE_GETTEXT )
#   include <libintl.h>
#else
#   define _(String) (String)
#   define N_(String) (String)
#endif

/*****************************************************************************
 * Plug-in stuff
 *****************************************************************************/
typedef struct module_symbols_s
{
    struct main_s* p_main;
    struct input_bank_s* p_input_bank;
    struct aout_bank_s*  p_aout_bank;
    struct vout_bank_s*  p_vout_bank;

    int    ( * main_GetIntVariable ) ( char *, int );
    char * ( * main_GetPszVariable ) ( char *, char * );
    void   ( * main_PutIntVariable ) ( char *, int );
    void   ( * main_PutPszVariable ) ( char *, char * );

    int  ( * intf_ProcessKey ) ( struct intf_thread_s *, int );
    void ( * intf_AssignKey )  ( struct intf_thread_s *, int, int, int );

    void ( * intf_Msg )        ( char *, ... );
    void ( * intf_ErrMsg )     ( char *, ... );
    void ( * intf_StatMsg )    ( char *, ... );
    void ( * intf_WarnMsg )    ( int, char *, ... );
    void ( * intf_WarnMsgImm ) ( int, char *, ... );
#ifdef TRACE
    void ( * intf_DbgMsg )     ( char *, char *, int, char *, ... );
    void ( * intf_DbgMsgImm )  ( char *, char *, int, char *, ... );
#endif

    int  ( * intf_PlaylistAdd )     ( struct playlist_s *, int, const char* );
    int  ( * intf_PlaylistDelete )  ( struct playlist_s *, int );
    void ( * intf_PlaylistNext )    ( struct playlist_s * );
    void ( * intf_PlaylistPrev )    ( struct playlist_s * );
    void ( * intf_PlaylistDestroy ) ( struct playlist_s * );
    void ( * intf_PlaylistJumpto )  ( struct playlist_s *, int );
    void ( * intf_UrlDecode )       ( char * );
    int  ( * intf_Eject )           ( const char * );

    void    ( * msleep )         ( mtime_t );
    mtime_t ( * mdate )          ( void );
    char  * ( * mstrtime )        ( char *, mtime_t );

    int  ( * network_ChannelCreate )( void );
    int  ( * network_ChannelJoin )  ( int );

    int  ( * input_SetProgram )     ( struct input_thread_s *,
                                      struct pgrm_descriptor_s * );
    void ( * input_SetStatus )      ( struct input_thread_s *, int );
    void ( * input_Seek )           ( struct input_thread_s *, off_t );
    void ( * input_DumpStream )     ( struct input_thread_s * );
    char * ( * input_OffsetToTime ) ( struct input_thread_s *, char *, off_t );
    int  ( * input_ChangeES )       ( struct input_thread_s *,
                                      struct es_descriptor_s *, u8 );
    int  ( * input_ToggleES )       ( struct input_thread_s *,
                                      struct es_descriptor_s *, boolean_t );
    int  ( * input_ChangeArea )     ( struct input_thread_s *,
                                      struct input_area_s * );
    struct es_descriptor_s * ( * input_FindES ) ( struct input_thread_s *,
                                                  u16 );
    struct es_descriptor_s * ( * input_AddES ) ( struct input_thread_s *,
                                      struct pgrm_descriptor_s *, u16, size_t );
    void ( * input_DelES )          ( struct input_thread_s *,
                                      struct es_descriptor_s * );
    int  ( * input_SelectES )       ( struct input_thread_s *,
                                      struct es_descriptor_s * );
    int  ( * input_UnselectES )     ( struct input_thread_s *,
                                      struct es_descriptor_s * );
    struct pgrm_descriptor_s* ( * input_AddProgram ) ( struct input_thread_s *,
                                                       u16, size_t );
    void ( * input_DelProgram )     ( struct input_thread_s *,
                                      struct pgrm_descriptor_s * );
    struct input_area_s * ( * input_AddArea ) ( struct input_thread_s * );
    void ( * input_DelArea )        ( struct input_thread_s *,
                                      struct input_area_s * );

    void ( * InitBitstream )        ( struct bit_stream_s *,
                                      struct decoder_fifo_s *,
                                      void ( * ) ( struct bit_stream_s *,
                                                   boolean_t ),
                                      void * );
    void ( * BitstreamNextDataPacket )( struct bit_stream_s * );
    boolean_t ( * NextDataPacket )  ( struct decoder_fifo_s *,
                                      struct data_packet_s ** );
    void ( * DecoderError )         ( struct decoder_fifo_s * p_fifo );
    int  ( * input_InitStream )     ( struct input_thread_s *, size_t );
    void ( * input_EndStream )      ( struct input_thread_s * );

    void ( * input_ParsePES )       ( struct input_thread_s *,
                                      struct es_descriptor_s * );
    void ( * input_GatherPES )      ( struct input_thread_s *,
                                      struct data_packet_s *,
                                      struct es_descriptor_s *,
                                      boolean_t, boolean_t );
    void ( * input_DecodePES )      ( struct decoder_fifo_s *,
                                      struct pes_packet_s * );
    struct es_descriptor_s * ( * input_ParsePS ) ( struct input_thread_s *,
                                                   struct data_packet_s * );
    void ( * input_DemuxPS )        ( struct input_thread_s *,
                                      struct data_packet_s * );
    void ( * input_DemuxTS )        ( struct input_thread_s *,
                                      struct data_packet_s * );
    void ( * input_DemuxPSI )       ( struct input_thread_s *,
                                      struct data_packet_s *,
                                      struct es_descriptor_s *, 
                                      boolean_t, boolean_t );

    int ( * input_ClockManageControl )   ( struct input_thread_s *,
                                           struct pgrm_descriptor_s *,
                                           mtime_t );

    struct aout_fifo_s * ( * aout_CreateFifo ) 
                                       ( int, int, long, long, long, void * );
    void ( * aout_DestroyFifo )     ( struct aout_fifo_s * );

    struct vout_thread_s * (* vout_CreateThread) ( int *, int, int, u32, int );
    void  ( * vout_DestroyThread )  ( struct vout_thread_s *, int * );

    struct picture_s * ( * vout_CreatePicture )
                                    ( struct vout_thread_s *,
                                      boolean_t, boolean_t, boolean_t ); 
    void  ( * vout_AllocatePicture )( struct picture_s *, int, int, u32 );
    void  ( * vout_DisplayPicture ) ( struct vout_thread_s *, 
                                      struct picture_s * );
    void  ( * vout_DestroyPicture ) ( struct vout_thread_s *,
                                      struct picture_s * );
    void  ( * vout_LinkPicture )    ( struct vout_thread_s *,
                                      struct picture_s * );
    void  ( * vout_UnlinkPicture )  ( struct vout_thread_s *,
                                      struct picture_s * );
    void  ( * vout_DatePicture )    ( struct vout_thread_s *, 
                                      struct picture_s *, mtime_t );
    void  ( * vout_PlacePicture )   ( struct vout_thread_s *, int, int,
                                      int *, int *, int *, int * );

    struct subpicture_s * (* vout_CreateSubPicture)
                                        ( struct vout_thread_s *, int, int );
    void  ( * vout_DestroySubPicture )  ( struct vout_thread_s *, 
                                          struct subpicture_s * );
    void  ( * vout_DisplaySubPicture )  ( struct vout_thread_s *, 
                                          struct subpicture_s * );
    
    u32  ( * UnalignedShowBits )    ( struct bit_stream_s *, unsigned int );
    void ( * UnalignedRemoveBits )  ( struct bit_stream_s * );
    u32  ( * UnalignedGetBits )     ( struct bit_stream_s *, unsigned int );
    void ( * CurrentPTS )           ( struct bit_stream_s *, mtime_t *,
                                      mtime_t * );

    char * ( * DecodeLanguage ) ( u16 );

    struct module_s * ( * module_Need ) ( int, char *, void * );
    void ( * module_Unneed )            ( struct module_s * );

} module_symbols_t;

#ifdef PLUGIN
extern module_symbols_t* p_symbols;
#endif


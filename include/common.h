/*******************************************************************************
 * common.h: common definitions
 * (c)1998 VideoLAN
 *******************************************************************************
 * Collection of usefull common types and macros definitions
 *******************************************************************************
 * required headers:
 *  config.h
 *******************************************************************************/

/*******************************************************************************
 * Types definitions
 *******************************************************************************/

/* Basic types definitions */
typedef signed char         s8;
typedef signed short        s16;
typedef signed int          s32;
typedef signed long long    s64;
typedef unsigned char       u8;
typedef unsigned short      u16;
typedef unsigned int        u32;
typedef unsigned long long  u64;

typedef u8                  byte_t;
 
/* Boolean type */
typedef int                 boolean_t;

/* Counter for statistics and profiling */
typedef unsigned long       count_t;

/*******************************************************************************
 * Macros and inline functions
 *******************************************************************************/

/* CEIL: division with round to nearest greater integer */
#define CEIL(n, d)  ( ((n) / (d)) + ( ((n) % (d)) ? 1 : 0) )

/* PAD: PAD(n, d) = CEIL(n ,d) * d */
#define PAD(n, d)   ( ((n) % (d)) ? ((((n) / (d)) + 1) * (d)) : (n) )

/* MAX and MIN: self explanatory */
#define MAX(a, b)   ( ((a) > (b)) ? (a) : (b) )
#define MIN(a, b)   ( ((a) < (b)) ? (a) : (b) )

/* MSB (big endian)/LSB (little endian) convertions - network order is always 
 * MSB, and should be used for both network communications and files. Note that
 * byte orders other than little and big endians are not supported, but only
 * the VAX seems to have such exotic properties - note that these 'functions'
 * needs <netinet/in.h> or the local equivalent. */
/* ?? hton64 should be declared as an extern inline function to avoid border
 * effects (see byteorder.h) */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define hton16      htons
#define hton32      htonl
#define hton64(i)   ( ((u64)(htonl((i) & 0xffffffff)) << 32) | htonl(((i) >> 32) & 0xffffffff ) )
#define ntoh16      ntohs                              
#define ntoh32      ntohl
#define ntoh64      hton64
#elif __BYTE_ORDER == __BIG_ENDIAN
#define hton16      htons
#define hton32      htonl
#define hton64(i)   ( i )                            
#define ntoh16      ntohs
#define ntoh32      ntohl
#define ntoh64(i)   ( i )
#else
/* ?? cause a compilation error */
#endif

/* Macros used by input to access the TS buffer */
#define U32_AT(p)   ( ntohl ( *( (u32 *)(p) ) ) )
#define U16_AT(p)   ( ntohs ( *( (u16 *)(p) ) ) )

/*******************************************************************************
 * mtime.h: high rezolution time management functions
 * (c)1999 VideoLAN
 *******************************************************************************
 * This header provides portable high precision time management functions,
 * which should be the only ones used in other segments of the program, since
 * functions like gettimeofday() and ftime() are not always supported.
 * Most functions are declared as inline or as macros since they are only
 * interfaces to system calls and have to be called frequently.
 * 'm' stands for 'micro', since maximum resolution is the microsecond.
 * Functions prototyped are implemented in interface/mtime.c.
 *******************************************************************************
 * Required headers:
 *  none
 * this header includes inline functions
 *******************************************************************************/

/*******************************************************************************
 * mtime_t: high precision date or time interval
 *******************************************************************************
 * Store an high precision date or time interval. The maximum precision is the
 * micro-second, and a 64 bits integer is used to avoid any overflow (maximum
 * time interval is then 292271 years, which should be length enough for any
 * video). Date are stored as a time interval since a common date.
 * Note that date and time intervals can be manipulated using regular arithmetic
 * operators, and that no special functions are required.
 *******************************************************************************/
typedef s64 mtime_t;

/*******************************************************************************
 * LAST_MDATE: date which will never happen
 *******************************************************************************
 * This date can be used as a 'never' date, to mark missing events in a function
 * supposed to return a date, such as nothing to display in a function
 * returning the date of the first image to be displayed. It can be used in
 * comparaison with other values: all existing dates will be earlier.
 *******************************************************************************/
#define LAST_MDATE ((mtime_t)((u64)(-1)/2))

/*******************************************************************************
 * MSTRTIME_MAX_SIZE: maximum possible size of mstrtime
 *******************************************************************************
 * This values is the maximal possible size of the string returned by the
 * mstrtime() function, including '-' and the final '\0'. It should be used to
 * allocate the buffer.
 *******************************************************************************/
#define MSTRTIME_MAX_SIZE 22

/*******************************************************************************
 * Prototypes
 *******************************************************************************/
char *  mstrtime ( char *psz_buffer, mtime_t date );
mtime_t mdate    ( void );
void    mwait    ( mtime_t date );
void    msleep   ( mtime_t delay );

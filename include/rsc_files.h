/*****************************************************************************
 * rsc_files.h: resources files manipulation functions
 * (c)1999 VideoLAN
 *****************************************************************************
 * This library describe a general format used to store 'resources'. Resources
 * can be anything, including pictures, audio streams, and so on.
 *****************************************************************************
 * Requires:
 *  config.h
 *  common.h
 *****************************************************************************/

/*****************************************************************************
 * Constants
 *****************************************************************************/

/* Maximum length of a resource name (not including the final '\0') - this
 * constant should not be changed without extreme care */
#define RESOURCE_MAX_NAME   32

/*****************************************************************************
 * resource_descriptor_t: resource descriptor
 *****************************************************************************
 * This type describe an entry in the resource table.
 *****************************************************************************/
typedef struct
{
    char    psz_name[RESOURCE_MAX_NAME + 1];                         /* name */
    u16     i_type;                                                  /* type */
    u64     i_offset;                                         /* data offset */
    u64     i_size;                                             /* data size */
} resource_descriptor_t;

/* Resources types */
#define EMPTY_RESOURCE      0                        /* empty place in table */
#define PICTURE_RESOURCE    10                       /* native video picture */

/*****************************************************************************
 * resource_file_t: resource file descriptor
 *****************************************************************************
 * This type describes a resource file and store it's resources table. It can
 * be used through the *Resource functions, or directly with the i_file field.
 *****************************************************************************/
typedef struct
{
    /* File informations */
    int                     i_file;                       /* file descriptor */
    int                     i_type;                             /* file type */
    boolean_t               b_up_to_date;            /* is file up to date ? */
    boolean_t               b_read_only;                   /* read-only mode */

    /* Resources table */
    int                     i_size;                            /* table size */
    resource_descriptor_t * p_resource;                   /* resources table */
} resource_file_t;

/* Resources files types */
#define VLC_RESOURCE_FILE   0               /* VideoLAN Client resource file */

/*****************************************************************************
 * Prototypes
 *****************************************************************************/
resource_file_t *   CreateResourceFile  ( char *psz_filename, int i_type, int i_size, int i_mode );
resource_file_t *   OpenResourceFile    ( char *psz_filename, int i_type, int i_flags );
int                 UpdateResourceFile  ( resource_file_t *p_file );
int                 CloseResourceFile   ( resource_file_t *p_file );

int                 SeekResource        ( resource_file_t *p_file, char *psz_name, int i_type );
int                 ReadResource        ( resource_file_t *p_file, char *psz_name, int i_type,
                                          size_t max_size, byte_t *p_data );
int                 WriteResource       ( resource_file_t *p_file, char *psz_name, int i_type,
                                          size_t size, byte_t *p_data );


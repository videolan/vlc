/*******************************************************************************
 * rsc_files.c: resources files manipulation functions
 * (c)1999 VideoLAN
 *******************************************************************************
 * This library describe a general format used to store 'resources'. Resources
 * can be anything, including pictures, audio streams, and so on.
 *******************************************************************************/

/*******************************************************************************
 * Format of a resource file:
 *  offset      type        meaning
 *  0           char[2]     "RF" (magic number)
 *  2           char[2]     "VL" (minor magic number, ignored)
 *  4           u16         i_type: resource file type. This is to allow
 *                          different versions of the resources types constants.
 *  6           u16         i_size: number of entries in the resources table.
 * {
 *  +0          char[32]    resource name (ASCIIZ or ASCII[32])
 *  +32         u16         resource type
 *  +34         u64         data offset in bytes, from beginning of file
 *  +42         u64         data size in bytes
 * } * i_size
 *******************************************************************************/

/*******************************************************************************
 * Preamble
 *******************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "common.h"
#include "mtime.h"

#include "rsc_files.h"

#include "intf_msg.h"

/*******************************************************************************
 * CreateResourceFile: create a new resource file
 *******************************************************************************
 * Creates a new resource file. The file is opened read/write and erased if
 * it already exists.
 *******************************************************************************
 * Messages type: rsc, major code 101
 *******************************************************************************/
resource_file_t *CreateResourceFile( char *psz_filename, int i_type, int i_size, 
                                     int i_mode )
{
    resource_file_t *   p_file;                              /* new descriptor */
    int                 i_index;                             /* resource index */

    /* Create descriptor and tables */
    p_file = malloc( sizeof(resource_file_t) );
    if( p_file == NULL )
    {
        intf_ErrMsg("rsc error 101-1: %s\n", strerror(errno));
        return( NULL );
    }
    p_file->p_resource = malloc( sizeof(resource_descriptor_t) * i_size );
    if( p_file->p_resource == NULL )
    {
        intf_ErrMsg("rsc error 101-2: %s\n", strerror(errno));
        free( p_file );
        return( NULL );
    } 
                
    /* Open file */
    p_file->i_file = open( psz_filename, O_CREAT | O_RDWR, i_mode ); 
    if( p_file->i_file == -1 )                                        /* error */
    {
        intf_ErrMsg("rsc error 101-3: %s: %s\n", psz_filename, strerror(errno) );
        free( p_file->p_resource );
        free( p_file );        
    }

    /* Initialize tables */
    p_file->i_type = i_type;
    p_file->i_size = i_size;
    p_file->b_read_only = 0;
    for( i_index = 0; i_index < i_size; i_index++ )
    {
        p_file->p_resource[i_index].i_type = EMPTY_RESOURCE;
    }

    return( p_file );
}

/*******************************************************************************
 * OpenResourceFile: open an existing resource file read-only
 *******************************************************************************
 * Open an existing resource file. i_flags should be O_RDONLY or O_RDWR. An
 * error will occurs if the file does not exists.
 *******************************************************************************
 * Messages type: rsc, major code 102
 *******************************************************************************/
resource_file_t *OpenResourceFile( char *psz_filename, int i_type, int i_flags )
{
    resource_file_t *   p_file;                              /* new descriptor */
    int                 i_index;                             /* resource index */
    byte_t              p_buffer[50];                                /* buffer */

    /* Create descriptor and tables */
    p_file = malloc( sizeof(resource_file_t) );
    if( p_file == NULL )
    {
        intf_ErrMsg("rsc error 102-1: %s\n", strerror(errno));
        return( NULL );
    }   
                
    /* Open file */
    p_file->i_file = open( psz_filename, i_flags ); 
    if( p_file->i_file == -1 )                                        /* error */
    {
        intf_ErrMsg("rsc error 102-2: %s: %s\n", psz_filename, strerror(errno) );
        free( p_file );         
        return( NULL );
    }   
    
    /* Read header */
    if( read( p_file->i_file, p_buffer, 8 ) != 8)
    {
        intf_ErrMsg("rsc error 102-3: %s: unexpected end of file (not a resource file ?)\n");
        close( p_file->i_file );
        free( p_file);
        return( NULL );
    }
    if( (p_buffer[0] != 'R') || (p_buffer[0] != 'F') || (*(u16 *)(p_buffer + 4) != i_type) )
    {
        intf_ErrMsg("rsc error 102-4: %s is not a valid resource file or has incorrect type\n", psz_filename);
        close( p_file->i_file );
        free( p_file );
        return( NULL );
    }
    p_file->i_type = i_type;
    p_file->i_size = *(u16 *)(p_buffer + 6);
    intf_DbgMsg("rsc debug 102-1: %s opened, %d resources\n", psz_filename, p_file->i_size);

    /* Allocate tables */
    p_file->p_resource = malloc( sizeof(resource_descriptor_t) * p_file->i_size );
    if( p_file->p_resource == NULL )
    {
        intf_ErrMsg("rsc error 102-5: %s\n", strerror(errno));
        close( p_file->i_file );
        free( p_file );
        return( NULL );
    } 
    
    /* Initialize table */
    p_file->b_up_to_date = 1;
    p_file->b_read_only = ( i_flags & O_RDWR ) ? 0 : 1;
    for( i_index = 0; i_index < p_file->i_size; i_index++ )
    {
        if( read( p_file->i_file, p_buffer, 50 ) != 50 )
        {
            intf_ErrMsg("rsc error 102-6: %s: unexpected end of file\n", psz_filename);
            close( p_file->i_file );
            free( p_file->p_resource );
            free( p_file );
            return( NULL );
        }
        memcpy( p_file->p_resource[i_index].psz_name, p_buffer, 32 );
        p_file->p_resource[i_index].psz_name[RESOURCE_MAX_NAME] = '\0';
        p_file->p_resource[i_index].i_type =    *(u16 *)(p_buffer + 32 );
        p_file->p_resource[i_index].i_offset =  *(u64 *)(p_buffer + 34 );
        p_file->p_resource[i_index].i_size =    *(u64 *)(p_buffer + 42 );
    }

    return( p_file );
}

/*******************************************************************************
 * UpdateResourceFile: write the resources table in a resource file
 *******************************************************************************
 * This function writes resources table in the resource file. This is 
 * automatically done when the file is closed, but can also be done manually.
 *******************************************************************************
 * Messages type: rsc, major code 103
 *******************************************************************************/
int UpdateResourceFile( resource_file_t *p_file )
{
    byte_t      p_buffer[50];                                        /* buffer */
    int         i_index;                                     /* resource index */
                             
#ifdef DEBUG
    if( p_file->b_read_only )
    {
        intf_DbgMsg("rsc debug 103-1: can't update a read-only file\n");
        return( -1 );
    }
#endif

    /* Seek beginning of file */
    if( lseek( p_file->i_file, 0, SEEK_SET ) )
    {
        intf_ErrMsg("rsc error 103-1: %s\n", strerror(errno));
        return( -2 );
    }

    /* Write header */
    p_buffer[0] =               'R';
    p_buffer[1] =               'F';
    p_buffer[2] =               'V';
    p_buffer[3] =               'L';
    *(u16 *)(p_buffer + 4) =    p_file->i_type;
    *(u16 *)(p_buffer + 6) =    p_file->i_size;
    if( write( p_file->i_file, p_buffer, 8 ) != 8 )
    {
        intf_ErrMsg("rsc error 103-2: %s\n", strerror(errno)); 
        return( -3 );
    }

    /* Write resources table */
    for( i_index = 0; i_index < p_file->i_size; i_index++ )
    {
        memcpy( p_buffer, p_file->p_resource[i_index].psz_name, 32 );
        *(u16 *)(p_buffer + 32) =   p_file->p_resource[i_index].i_type;
        *(u64 *)(p_buffer + 34) =   p_file->p_resource[i_index].i_offset;
        *(u64 *)(p_buffer + 42) =   p_file->p_resource[i_index].i_size;            
        if( write( p_file->i_file, p_buffer, 8 ) != 8 )
        {
            intf_ErrMsg("rsc error 103-3: %s\n", strerror(errno)); 
            return( -4 );
        }           
    }

    /* Mark file as up to date */
    p_file->b_up_to_date = 1;

    return( 0 );
}

/*******************************************************************************
 * CloseResourceFile: close a resource file
 *******************************************************************************
 * Updates the resources table if required, and close the file. It returns non
 * 0 in case of error.
 *******************************************************************************
 * Messages type: rsc, major code 104
 *******************************************************************************/
int CloseResourceFile( resource_file_t *p_file )
{
    /* Update table */
    if( !p_file->b_up_to_date && ( UpdateResourceFile( p_file ) < 0 ) )
    {
        return( -1 );
    }

    /* Close and destroy descriptor */
    if( close( p_file->i_file ) )
    {
        intf_ErrMsg("rsc error 104-1: %s\n", strerror(errno));
        return( -2 );
    }
    free( p_file->p_resource );
    free( p_file );
    return( 0 );
}

/*******************************************************************************
 * SeekResource: seek a resource in a resource file
 *******************************************************************************
 * Look for a resource in file and place the "reading head" at the beginning of
 * the resource data.
 * In case of success, the resource number is returned. Else, a negative number
 * is returned.
 *******************************************************************************
 * Messages type: rsc, major code 105
 *******************************************************************************/
int SeekResource( resource_file_t *p_file, char *psz_name, int i_type )
{
    int     i_index;                                         /* resource index */

    /* Look for resource in table */
    for( i_index = 0; 
         (i_index < p_file->i_size) 
             && ((i_type != p_file->p_resource[i_index].i_type )
                 || strcmp(psz_name, p_file->p_resource[i_index].psz_name));
         i_index++ )
    {
        ;
    }
    if( i_index == p_file->i_size )
    {
        intf_ErrMsg("rsc error 105-1: unknown resource %s.%d\n", psz_name, i_type);
        return( -1 );
    }

    /* Seek */
    if( lseek( p_file->i_file, p_file->p_resource[i_index].i_offset, SEEK_SET ) )
    {
        intf_ErrMsg("rsc error 105-2: can not reach %s.%d: %s\n", psz_name, 
                    i_type, strerror(errno));
        return( -2 );
    }

    return( i_index );
}

/*******************************************************************************
 * ReadResource: read a resource
 *******************************************************************************
 * Read a resource from a file. The function returns a negative value in case
 * of error, and 0 in case of success.
 *******************************************************************************
 * Messages type: rsc, major code 106
 *******************************************************************************/
int ReadResource( resource_file_t *p_file, char *psz_name, int i_type,
                  size_t max_size, byte_t *p_data )
{
    int i_index;                                             /* resource index */

    /* Seek resource */
    i_index = SeekResource( p_file, psz_name, i_type );
    if( i_index < 0 )
    {
        return( -1 );
    }

    /* Check if buffer is large enough */
    if( max_size < p_file->p_resource[i_index].i_size )
    {
        intf_ErrMsg("rsc error 106-1: buffer is too small\n");
        return( -2 );
    }

    /* Read data */
    if( read( p_file->i_file, p_data, p_file->p_resource[i_index].i_size ) 
        != p_file->p_resource[i_index].i_size )
    {
        intf_ErrMsg("rsc error 106-2: can not read %s.%d: %s\n",
                    p_file->p_resource[i_index].psz_name, 
                    p_file->p_resource[i_index].i_type,
                    strerror(errno));
        return( -3 );
    }

    return( 0 );
}

/*******************************************************************************
 * WriteResource: write a resource
 *******************************************************************************
 * Append a resource at the end of the file. It returns non 0 on error.
 *******************************************************************************
 * Messages type: rsc, major code 107
 *******************************************************************************/
int WriteResource( resource_file_t *p_file, char *psz_name, int i_type, 
                   size_t size, byte_t *p_data )
{
    int i_index;                                             /* resource index */
    int i_tmp_index;                               /* temporary resource index */
    u64 i_offset;                                                    /* offset */

#ifdef DEBUG
    if( p_file->b_read_only )
    {
        intf_DbgMsg("rsc debug 107-1: can not write to a read-only resource file\n");
        return( -1 );
    }
#endif

    /* Look for an empty place in the resources table */
    i_index = -1;
    i_offset = p_file->i_size * 50 + 8;
    for( i_tmp_index = 0; i_tmp_index < p_file->i_size; i_tmp_index++ )
    {
        if( p_file->p_resource[i_tmp_index].i_type != EMPTY_RESOURCE)
        {
            i_offset = MAX( i_offset, p_file->p_resource[i_tmp_index].i_offset 
                            + p_file->p_resource[i_tmp_index].i_size );
        }
        else if( i_index == -1 )
        {
            i_index = i_tmp_index;
        }
    }
    if( i_index == -1 )
    {
        intf_ErrMsg("rsc error 107-1: resources table is full\n");
        return( -1 );
    }

    /* Seek end of file */
    if( lseek( p_file->i_file, i_offset, SEEK_SET ) )
    {
        intf_ErrMsg("rsc error 107-2: %s\n", strerror(errno));
        return( -2 );
    }

    /* Write data */
    if( write( p_file->i_file, p_data, size ) != size )
    {
        intf_ErrMsg("rsc error 107-3: %s\n", strerror(errno));
        return( -3 );
    }

    /* Update table */
    strncpy( p_file->p_resource[i_index].psz_name, psz_name, RESOURCE_MAX_NAME );
    p_file->p_resource[i_index].psz_name[RESOURCE_MAX_NAME] = '\0';        
    p_file->p_resource[i_index].i_type = i_type;
    p_file->p_resource[i_index].i_offset = i_offset;
    p_file->p_resource[i_index].i_size = size;
    p_file->b_up_to_date = 0;

    return( 0 );
}

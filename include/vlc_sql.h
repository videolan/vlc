/*****************************************************************************
 * vlc_sql.h: SQL abstraction layer
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Srikanth Raju <srikiraju@gmail.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#if !defined( __LIBVLC__ )
# error You are not libvlc or one of its plugins. You cannot include this file
#endif

#ifndef VLC_SQL_H
# define VLC_SQL_H

# ifdef __cplusplus
extern "C" {
# endif


/*****************************************************************************
 * General structure: SQL object.
 *****************************************************************************/

typedef struct sql_t sql_t;
typedef struct sql_sys_t sql_sys_t;

typedef int ( *sql_query_callback_t ) ( void*, int, char**, char** );

struct sql_t
{
    VLC_COMMON_MEMBERS

    /** Module properties */
    module_t  *p_module;

    /** Connection Data */
    char *psz_host;         /**< Location or host of the database */
    char *psz_user;         /**< Username used to connect to database */
    char *psz_pass;         /**< Password used to connect to database */
    int i_port;             /**< Port on which database is running */

    /** Internal data */
    sql_sys_t *p_sys;

    /** Perform a query with a row-by-row callback function */
    int (*pf_query_callback) ( sql_t *, const char *, sql_query_callback_t, void * );

    /** Perform a query and return result directly */
    int (*pf_query) ( sql_t *, const char *, char ***, int *, int * );

    /** Get database tables */
    int (*pf_get_tables) ( sql_t *, char *** );

    /** Free result of a call to sql_Query or sql_GetTables */
    void (*pf_free) ( sql_t *, char ** );

    /** vmprintf replacement for SQL */
    char* (*pf_vmprintf) ( const char*, va_list args );

    /** Begin transaction */
    int (*pf_begin) ( sql_t* );

    /** Commit transaction */
    void (*pf_commit) ( sql_t* );

    /** Rollback transaction */
    void (*pf_rollback) ( sql_t* );
};


/*****************************************************************************
 * SQL Function headers
 *****************************************************************************/

/**
 * @brief Create a new SQL object.
 * @param p_this Parent object to attach the SQL object to.
 * @param psz_host URL to the database
 * @param i_port Port on which the database is running
 * @param psz_user Username to access the database
 * @param psz_pass Password for the database
 * @return The VLC SQL object, type sql_t.
 **/
VLC_EXPORT( sql_t*, sql_Create, ( vlc_object_t *p_this, const char *psz_name,
            const char* psz_host, int i_port,
            const char* psz_user, const char* psz_pass ) );
#define sql_Create( a, b, c, d, e, f ) sql_Create( VLC_OBJECT(a), b, c, d, e, f )


/**
 * @brief Destructor for p_sql object
 * @param obj This p_sql object
 * @return Nothing
 */
VLC_EXPORT( void, sql_Destroy, ( vlc_object_t *obj ) );
#define sql_Destroy( a ) sql_Destroy( VLC_OBJECT( a ) )


/**
 * @brief Perform a query using a callback function
 * @param p_sql This SQL object.
 * @param psz_query The SQL query string.
 * @param pf_callback A callback function that will be called for each row of
 * the result: 1st argument is be p_opaque,
 *             2nd argument is the number of columns,
 *             3rd is the result columns (array of strings),
 *             4th is the columns names (array of strings).
 * @param p_opaque Any pointer to an object you may need in the callback.
 * @return VLC_SUCCESS or VLC_EGENERIC.
 * @note The query will not necessarily be processed in a separate thread!
 **/
static inline int sql_QueryCallback( sql_t *p_sql, const char *psz_query,
                                     sql_query_callback_t pf_callback,
                                     void *p_opaque )
{
    return p_sql->pf_query_callback( p_sql, psz_query, pf_callback, p_opaque );
}

/**
 * @brief Perform a query directly
 * @param p_sql This SQL object.
 * @param psz_query The SQL query string.
 * @param pppsz_result A pointer to a array of strings: result of the query.
 * Dynamically allocated.
 * @param pi_rows Pointer to an integer that will receive the number of result
 * rows written.
 * @param pi_cols Pointer to an integer that will receive the number of result
 * columns written.
 * @return VLC_SUCCESS or VLC_EGENERIC.
 * @note pppsz_result will point to an array of strings, ppsz_result.
 * This array of strings contains actually a 2D-matrix of strings where the
 * first row (row 0) contains the SQL table header names.
 * *pi_rows will be the number of result rows, so that the number of text rows
 * in ppsz_result will be (*pi_rows + 1) (because of row 0).
 * To get result[row,col] use (*pppsz_result)[ (row+1) * (*pi_cols) + col ].
 **/
static inline int sql_Query( sql_t *p_sql, const char *psz_query,
                             char ***pppsz_result, int *pi_rows, int *pi_cols )
{
    return p_sql->pf_query( p_sql, psz_query, pppsz_result, pi_rows, pi_cols );
}

/**
 * @brief Get database table name list
 * @param p_sql This SQL object.
 * @param pppsz_tables Pointer to an array of strings. Dynamically allocated.
 * Similar to pppsz_result of sql_Query but with only one row.
 * @return Number of tables or <0 in case of error.
 **/
static inline int sql_GetTables( sql_t *p_sql, char ***pppsz_tables )
{
    return p_sql->pf_get_tables( p_sql, pppsz_tables );
}

/**
 * @brief Free the result of a query.
 * @param p_sql This SQL object.
 * @param ppsz_result The result of sql_Query or sql_GetTables. See above.
 * @return Nothing.
 **/
static inline void sql_Free( sql_t *p_sql, char **ppsz_result )
{
    p_sql->pf_free( p_sql, ppsz_result );
}

/**
 * @brief printf-like function that can escape forbidden/reserved characters.
 * @param p_sql This SQL object.
 * @param psz_fmt Format of the string (with %q, %Q and %z enabled).
 * @param ... Printf arguments
 * @return Dynamically allocated string or NULL in case of error.
 * @note Refer to SQLite documentation for more details about %q, %Q and %z.
 **/
static inline char* sql_Printf( sql_t *p_sql, const char *psz_fmt, ... )
{
    va_list args;
    va_start( args, psz_fmt );
    char *r = p_sql->pf_vmprintf( psz_fmt, args );
    va_end( args );
    return r;
}

/**
 * @brief vprintf replacement for SQL queries, escaping forbidden characters
 * @param p_sql This SQL object
 * @param psz_fmt Format of the string
 * @param arg Variable list of arguments
 * @return Dynamically allocated string or NULL in case of error.
 **/
static inline char* sql_VPrintf( sql_t *p_sql, const char *psz_fmt,
                                 va_list arg )
{
    return p_sql->pf_vmprintf( psz_fmt, arg );
}

/**
 * @brief Begin a SQL transaction
 * @param p_sql The SQL object
 * @return VLC error code or success
 **/
static inline int sql_BeginTransaction( sql_t *p_sql )
{
    return p_sql->pf_begin( p_sql );
}

/**
 * @brief Commit a SQL transaction
 * @param p_sql The SQL object
 * @return VLC error code or success
 **/
static inline void sql_CommitTransaction( sql_t *p_sql )
{
    p_sql->pf_commit( p_sql );
}

/**
 * @brief Rollback a SQL transaction
 * @param p_sql The SQL object
 * @return VLC error code or success
 **/
static inline void sql_RollbackTransaction( sql_t *p_sql )
{
    p_sql->pf_rollback( p_sql );
}

# ifdef __cplusplus
}
# endif /* C++ extern "C" */

#endif /* VLC_SQL_H */

/*****************************************************************************
 * vlc_sql.h: SQL abstraction layer
 *****************************************************************************
 * Copyright (C) 2009 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Srikanth Raju <srikiraju@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_SQL_H
# define VLC_SQL_H

# ifdef __cplusplus
extern "C" {
# endif


/*****************************************************************************
 * General structure: SQL object.
 *****************************************************************************/

/**
 * Return values for the function @see sql_Run()
 */
#define VLC_SQL_ROW 1
#define VLC_SQL_DONE 2

typedef struct sql_t sql_t;
typedef struct sql_sys_t sql_sys_t;
typedef struct sql_stmt_t sql_stmt_t;

typedef int ( *sql_query_callback_t ) ( void*, int, char**, char** );

typedef enum {
    SQL_NULL,
    SQL_INT,
    SQL_DOUBLE,
    SQL_TEXT,
    SQL_BLOB
} sql_type_e;

typedef struct
{
    int length;
    union
    {
        int i;
        double dbl;
        char* psz;
        void* ptr;
    } value;
} sql_value_t;

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

    /** All the functions are implemented as threadsafe functions */
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
    int (*pf_commit) ( sql_t* );

    /** Rollback transaction */
    void (*pf_rollback) ( sql_t* );

    /** Create a statement object */
    sql_stmt_t* (*pf_prepare) ( sql_t* p_sql, const char* p_fmt,
                                int i_length );

    /** Bind parameters to a statement */
    int (*pf_bind) ( sql_t* p_sql, sql_stmt_t* p_stmt, int i_pos,
                    unsigned int type, const sql_value_t* p_value );

    /** Run the prepared statement */
    int (*pf_run) ( sql_t* p_sql, sql_stmt_t* p_stmt );

    /** Reset the prepared statement */
    int (*pf_reset) ( sql_t* p_sql, sql_stmt_t* p_stmt );

    /** Destroy the statement object */
    int (*pf_finalize) ( sql_t* p_sql, sql_stmt_t* p_stmt );

    /** Get the datatype for a specified column */
    int (*pf_gettype) ( sql_t* p_sql, sql_stmt_t* p_stmt, int i_col,
                        int* type );

    /** Get the data from a specified column */
    int (*pf_getcolumn) ( sql_t* p_sql, sql_stmt_t* p_stmt, int i_col,
                          int type, sql_value_t *p_res );

    /** Get column size of a specified column */
    int (*pf_getcolumnsize) ( sql_t* p_sql, sql_stmt_t* p_stmt, int i_col );
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
VLC_API sql_t *sql_Create( vlc_object_t *p_this, const char *psz_name,
            const char* psz_host, int i_port,
            const char* psz_user, const char* psz_pass );
#define sql_Create( a, b, c, d, e, f ) sql_Create( VLC_OBJECT(a), b, c, d, e, f )


/**
 * @brief Destructor for p_sql object
 * @param obj This p_sql object
 * @return Nothing
 */
VLC_API void sql_Destroy( vlc_object_t *obj );
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
 * @note The query will not necessarily be processed in a separate thread, but
 * it is threadsafe
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
 * This function is threadsafe
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
 * @note This function is threadsafe
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
 * @note This function is threadsafe
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
 * @note This function is threadsafe
 **/
static inline int sql_BeginTransaction( sql_t *p_sql )
{
    return p_sql->pf_begin( p_sql );
}

/**
 * @brief Commit a SQL transaction
 * @param p_sql The SQL object
 * @return VLC error code or success
 * @note This function is threadsafe
 **/
static inline int sql_CommitTransaction( sql_t *p_sql )
{
    return p_sql->pf_commit( p_sql );
}

/**
 * @brief Rollback a SQL transaction
 * @param p_sql The SQL object
 * @return VLC error code or success
 * @note This function is threadsafe
 **/
static inline void sql_RollbackTransaction( sql_t *p_sql )
{
    p_sql->pf_rollback( p_sql );
}

/**
 * @brief Prepare an sql statement
 * @param p_sql The SQL object
 * @param p_fmt SQL query string
 * @param i_length length of the string. If negative, length will be
 * considered upto the first \0 character equivalent to strlen(p_fmt).
 * Otherwise the first i_length bytes will be used
 * @return a sql_stmt_t pointer or NULL on failure
 */
static inline sql_stmt_t* sql_Prepare( sql_t* p_sql, const char* p_fmt,
        int i_length )
{
    return p_sql->pf_prepare( p_sql, p_fmt, i_length );
}

/**
 * @brief Bind arguments to a sql_stmt_t object
 * @param p_sql The SQL object
 * @param p_stmt Statement Object
 * @param type Data type of the value
 * @param p_value Value to be bound
 * @param i_pos Position at which the parameter should be bound
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_BindGeneric( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_pos, int type, const sql_value_t* p_value )
{
    return p_sql->pf_bind( p_sql, p_stmt, i_pos, type, p_value );
}

/**
 * @brief Bind a NULL value to a position
 * @param p_sql The SQL object
 * @param p_stmt Statement Object
 * @param i_pos Position at which the parameter should be bound
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_BindNull( sql_t *p_sql, sql_stmt_t* p_stmt, int i_pos )
{
    int i_ret = sql_BindGeneric( p_sql, p_stmt, i_pos, SQL_NULL, NULL );
    return i_ret;
}

/**
 * @brief Bind an integer to the statement object at some position
 * @param p_sql The SQL object
 * @param p_stmt Statement Object
 * @param i_pos Position at which the parameter should be bound
 * @param i_int Value to be bound
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_BindInteger( sql_t *p_sql, sql_stmt_t* p_stmt,
                                   int i_pos, int i_int )
{
    sql_value_t value;
    value.length = 0;
    value.value.i = i_int;
    int i_ret = sql_BindGeneric( p_sql, p_stmt, i_pos, SQL_INT, &value );
    return i_ret;
}

/**
 * @brief Bind a double to the statement object at some position
 * @param p_sql The SQL object
 * @param p_stmt Statement Object
 * @param i_pos Position at which the parameter should be bound
 * @param d_dbl Value to be bound
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_BindDouble( sql_t *p_sql, sql_stmt_t* p_stmt,
                                  int i_pos, double d_dbl )
{
    sql_value_t value;
    value.length = 0;
    value.value.dbl = d_dbl;
    int i_ret = sql_BindGeneric( p_sql, p_stmt, i_pos, SQL_INT, &value );
    return i_ret;
}

/**
 * @brief Bind Text to the statement
 * @param p_sql The SQL object
 * @param p_stmt Statement Object
 * @param i_pos Position at which the parameter should be bound
 * @param p_fmt Value to be bound
 * @param i_length Length of text. If -ve text upto the first null char
 * will be selected.
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_BindText( sql_t *p_sql, sql_stmt_t* p_stmt, int i_pos,
                                   char* p_fmt, int i_length )
{
    sql_value_t value;
    value.length = i_length;
    value.value.psz = p_fmt;
    int i_ret = sql_BindGeneric( p_sql, p_stmt, i_pos, SQL_TEXT, &value );
    return i_ret;
}

/**
 * @brief Bind a binary object to the statement
 * @param p_sql The SQL object
 * @param p_stmt Statement Object
 * @param i_pos Position at which the parameter should be bound
 * @param p_ptr Value to be bound
 * @param i_length Size of the blob to read
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_BindBlob( sql_t *p_sql, sql_stmt_t* p_stmt, int i_pos,
                                   void* p_ptr, int i_length )
{
    sql_value_t value;
    value.length = i_length;
    value.value.ptr = p_ptr;
    int i_ret = sql_BindGeneric( p_sql, p_stmt, i_pos, SQL_INT, &value );
    return i_ret;
}

/**
 * @brief Run the SQL statement. If the statement fetches data, then only
 * one row of the data is fetched at a time. Run this function again to
 * fetch the next row.
 * @param p_sql The SQL object
 * @param p_stmt The statement
 * @return VLC_SQL_DONE if done fetching all rows or there are no rows to fetch
 * VLC_SQL_ROW if a row was fetched for this statement.
 * VLC_EGENERIC if this function failed
 */
static inline int sql_Run( sql_t* p_sql, sql_stmt_t* p_stmt )
{
    return p_sql->pf_run( p_sql, p_stmt );
}

/**
 * @brief Reset the SQL statement. Resetting the statement will unbind all
 * the values that were bound on this statement
 * @param p_sql The SQL object
 * @param p_stmt The sql statement object
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_Reset( sql_t* p_sql, sql_stmt_t* p_stmt )
{
    return p_sql->pf_reset( p_sql, p_stmt );
}

/**
 * @brief Destroy the sql statement object. This will free memory.
 * @param p_sql The SQL object
 * @param p_stmt The statement object
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_Finalize( sql_t* p_sql, sql_stmt_t* p_stmt )
{
    return p_sql->pf_finalize( p_sql, p_stmt );
}

/**
 * @brief Get the datatype of the result of the column
 * @param p_sql The SQL object
 * @param p_stmt The sql statement object
 * @param i_col The column
 * @param type pointer to datatype of the given column
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_GetColumnType( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_col, int* type )
{
    return p_sql->pf_gettype( p_sql, p_stmt, i_col, type );
}

/**
 * @brief Get the column data
 * @param p_sql The SQL object
 * @param p_stmt The statement object
 * @param i_col The column number
 * @param type Datatype of result
 * @param p_res The structure which contains the value of the result
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_GetColumn( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_col, int type, sql_value_t *p_res )
{
    return p_sql->pf_getcolumn( p_sql, p_stmt, i_col, type, p_res );
}

/**
 * @brief Get an integer from the results of a statement
 * @param p_sql The SQL object
 * @param p_stmt The statement object
 * @param i_col The column number
 * @param i_res Pointer of the location for result to be stored
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_GetColumnInteger( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_col, int* pi_res )
{
    sql_value_t tmp;
    int i_ret = p_sql->pf_getcolumn( p_sql, p_stmt, i_col, SQL_INT, &tmp );
    if( i_ret == VLC_SUCCESS )
        *pi_res = tmp.value.i;
    return i_ret;
}

/**
 * @brief Get a double from the results of a statement
 * @param p_sql The SQL object
 * @param p_stmt The statement object
 * @param i_col The column number
 * @param d_res Pointer of the location for result to be stored
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_GetColumnDouble( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_col, double* pd_res )
{
    sql_value_t tmp;
    int i_ret = p_sql->pf_getcolumn( p_sql, p_stmt, i_col, SQL_DOUBLE, &tmp );
    if( i_ret == VLC_SUCCESS )
        *pd_res = tmp.value.dbl;
    return i_ret;
}

/**
 * @brief Get some text from the results of a statement
 * @param p_sql The SQL object
 * @param p_stmt The statement object
 * @param i_col The column number
 * @param pp_res Pointer of the location for result to be stored
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_GetColumnText( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_col, char** pp_res )
{
    sql_value_t tmp;
    int i_ret = p_sql->pf_getcolumn( p_sql, p_stmt, i_col, SQL_TEXT, &tmp );
    if( i_ret == VLC_SUCCESS )
        *pp_res = tmp.value.psz;
    return i_ret;
}

/**
 * @brief Get a blob from the results of a statement
 * @param p_sql The SQL object
 * @param p_stmt The statement object
 * @param i_col The column number
 * @param pp_res Pointer of the location for result to be stored
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static inline int sql_GetColumnBlob( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_col, void** pp_res )
{
    sql_value_t tmp;
    int i_ret = p_sql->pf_getcolumn( p_sql, p_stmt, i_col, SQL_BLOB, &tmp );
    if( i_ret == VLC_SUCCESS )
        *pp_res = tmp.value.ptr;
    return i_ret;
}

/**
 * @brief Get the size of the column in bytes
 * @param p_sql The SQL object
 * @param p_stmt The sql statement object
 * @param i_col The column
 * @return Size of the column in bytes, excluding the zero terminator
 */
static inline int sql_GetColumnSize( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_col )
{
    return p_sql->pf_getcolumnsize( p_sql, p_stmt, i_col );
}

# ifdef __cplusplus
}
# endif /* C++ extern "C" */

#endif /* VLC_SQL_H */

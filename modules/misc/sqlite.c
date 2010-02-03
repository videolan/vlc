/*****************************************************************************
 * sqlite.c: An SQLite3 wrapper for VLC
 *****************************************************************************
 * Copyright (C) 2008-2009 the VideoLAN team
 * $Id$
 *
 * Authors: Antoine Lejeune <phytos@videolan.org>
 *          Jean-Philippe André <jpeg@videolan.org>
 *          Rémi Duraffort <ivoire@videolan.org>
 *          Adrien Maglo <magsoft@videolan.org>
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


#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_sql.h>
#include <vlc_plugin.h>

#include <sqlite3.h>
#include <assert.h>


/*****************************************************************************
 * Private structures
 *****************************************************************************/
struct sql_sys_t
{
    sqlite3 *db;              /**< Database connection. */
    vlc_mutex_t lock;         /**< SQLite mutex. Threads are evil here. */
    vlc_mutex_t trans_lock;   /**< Mutex for running transactions */
};

struct sql_stmt_t
{
    sqlite3_stmt* p_sqlitestmt;
};


/*****************************************************************************
 * Headers
 *****************************************************************************/
static int load ( vlc_object_t * );
static void unload ( vlc_object_t * );

static int OpenDatabase( sql_t * );
static int CloseDatabase (sql_t * );
static int QueryCallback( sql_t * p_sql,
                          const char * query,
                          sql_query_callback_t callback,
                          void *arg ); // 1st argument to callback
static int Query( sql_t * p_sql,
                  const char * query,
                  char *** result,
                  int * nrow,
                  int * ncol );
static int GetTables( sql_t * p_sql,
                      char *** result );
static void FreeResult( sql_t * p_sql,
                        char **pp_result );
static char* VMSprintf( const char* psz_fmt,
                        va_list args );
static int BeginTransaction( sql_t* p_sql );
static int CommitTransaction( sql_t* p_sql );
static void RollbackTransaction( sql_t* p_sql );
static sql_stmt_t* PrepareStatement( sql_t* p_sql,
                                     const char* psz_fmt,
                                     int i_length );
static int BindValues( sql_t* p_sql,
                       sql_stmt_t* p_stmt,
                       int i_pos,
                       unsigned int i_type,
                       const sql_value_t* p_value );
static int StatementStep( sql_t* p_sql,
                          sql_stmt_t* p_stmt );
static int StatementReset( sql_t* p_sql,
                           sql_stmt_t* p_stmt );
static int StatementFinalize( sql_t* p_sql,
                              sql_stmt_t* p_stmt );
static int GetColumnFromStatement( sql_t* p_sql,
                                   sql_stmt_t* p_stmt,
                                   int i_col,
                                   int type,
                                   sql_value_t *p_res );
static int GetColumnTypeFromStatement( sql_t* p_sql,
                                       sql_stmt_t* p_stmt,
                                       int i_col,
                                       int* pi_type );
static int GetColumnSize( sql_t* p_sql,
                          sql_stmt_t* p_stmt,
                          int i_col );

/*****************************************************************************
 * Module description
 *****************************************************************************/
vlc_module_begin()
    set_shortname( "SQLite" )
    set_description( _("SQLite database module") )
    set_capability( "sql", 1 )
    set_callbacks( load, unload )
    set_category( CAT_ADVANCED )
vlc_module_end()


/**
 * @brief Load module
 * @param obj Parent object
 * @return VLC_SUCCESS or VLC_ENOMEM
 */
static int load( vlc_object_t *p_this )
{
    sql_t *p_sql = (sql_t *) p_this;

    /* Initialize sys_t */
    p_sql->p_sys = calloc( 1, sizeof( *p_sql->p_sys ) );
    if( !p_sql->p_sys )
        return VLC_ENOMEM;

    vlc_mutex_init( &p_sql->p_sys->lock );
    vlc_mutex_init( &p_sql->p_sys->trans_lock );

    /* Open Database */
    if( OpenDatabase( p_sql ) == VLC_SUCCESS )
        msg_Dbg( p_sql, "sqlite module loaded" );
    else
    {
        free( p_sql->p_sys );
        vlc_mutex_destroy( &p_sql->p_sys->lock );
        vlc_mutex_destroy( &p_sql->p_sys->trans_lock );
        return VLC_EGENERIC;
    }

    p_sql->pf_query_callback = QueryCallback;
    p_sql->pf_get_tables = GetTables;
    p_sql->pf_query = Query;
    p_sql->pf_free = FreeResult;
    p_sql->pf_vmprintf = VMSprintf;
    p_sql->pf_begin = BeginTransaction;
    p_sql->pf_commit = CommitTransaction;
    p_sql->pf_rollback = RollbackTransaction;
    p_sql->pf_prepare = PrepareStatement;
    p_sql->pf_bind = BindValues;
    p_sql->pf_run = StatementStep;
    p_sql->pf_reset = StatementReset;
    p_sql->pf_finalize = StatementFinalize;
    p_sql->pf_gettype = GetColumnTypeFromStatement;
    p_sql->pf_getcolumn = GetColumnFromStatement;
    p_sql->pf_getcolumnsize = GetColumnSize;

    return VLC_SUCCESS;
}

/**
 * @brief Unload module
 * @param obj This sql_t object
 * @return Nothing
 */
static void unload( vlc_object_t *p_this )
{
    sql_t *p_sql = (sql_t *)p_this;

    CloseDatabase( p_sql );
    vlc_mutex_destroy( &p_sql->p_sys->lock );
    vlc_mutex_destroy( &p_sql->p_sys->trans_lock );
    free( p_sql->p_sys );
}

/**
 * @brief Sqlite Busy handler
 * @param p_data sql_t object
 * @param i_times Number of times busy handler has been invoked
 */
static int vlc_sqlite_busy_handler( void* p_data, int i_times )
{
    if( i_times >= 10 )
    {
        msg_Warn( (sql_t*) p_data, "Wait limit exceeded in SQLITE_BUSY handler" );
        return 0;
    }
    msleep( 2000000 );
    return 1;
}

/**
 * @brief Open current database
 * @param p_sql This sql_t object
 * @return VLC_SUCCESS or VLC_EGENERIC
 * @note p_sql->psz_host is required
 */
static int OpenDatabase( sql_t *p_sql )
{
    assert( p_sql->psz_host && *p_sql->psz_host );

    if( sqlite3_threadsafe() == 0 )
    {
        msg_Err( p_sql, "Sqlite library on your system is not threadsafe" );
        return VLC_EGENERIC;
    }
    if( sqlite3_open( p_sql->psz_host, &p_sql->p_sys->db ) != SQLITE_OK )
    {
        msg_Err( p_sql, "Can't open database : %s", p_sql->psz_host );
        msg_Err( p_sql, "sqlite3 error: %d: %s",
                      sqlite3_errcode( p_sql->p_sys->db ),
                      sqlite3_errmsg( p_sql->p_sys->db ) );
        return VLC_EGENERIC;
    }
    if( sqlite3_busy_timeout( p_sql->p_sys->db, 30000 ) != SQLITE_OK )
    {
        msg_Err( p_sql, "sqlite3 error: %d: %s",
                      sqlite3_errcode( p_sql->p_sys->db ),
                      sqlite3_errmsg( p_sql->p_sys->db ) );
        return VLC_EGENERIC;
    }
    if( sqlite3_busy_handler( p_sql->p_sys->db, vlc_sqlite_busy_handler, p_sql )
            != SQLITE_OK )
    {
        msg_Err( p_sql, "sqlite3 error: %d: %s",
                      sqlite3_errcode( p_sql->p_sys->db ),
                      sqlite3_errmsg( p_sql->p_sys->db ) );
        return VLC_EGENERIC;
    }

    return VLC_SUCCESS;
}

/**
 * @brief Close current database
 * @param p_sql This sql_t object
 * @return VLC_SUCCESS
 * You have to set and open current database first
 */
static int CloseDatabase( sql_t *p_sql )
{
    assert( p_sql->p_sys->db );

    /* Close all prepared statements */
    sqlite3_stmt* p_stmt;
    while( ( p_stmt = sqlite3_next_stmt( p_sql->p_sys->db, NULL ) ) != NULL )
    {
        if( sqlite3_finalize( p_stmt ) != SQLITE_OK )
        {
            msg_Warn( p_sql, "sqlite3 error: %d: %s",
                      sqlite3_errcode( p_sql->p_sys->db ),
                      sqlite3_errmsg( p_sql->p_sys->db ) );
        }
    }

    /* Close database */
    /* TODO: We've closed all open prepared statements
     * Perhaps sqlite3_close can still fail? */
    sqlite3_close( p_sql->p_sys->db );
    p_sql->p_sys->db = NULL;

    return VLC_SUCCESS;
}

/**
 * @brief SQL Query with callback
 * @param p_sql This sql_t object
 * @param query SQL query
 * @param callback Callback function to receive results row by row
 * @param arg Argument to pass to callback
 * @return VLC_SUCCESS or an error code
 * You have to set and open current database first
 */
static int QueryCallback( sql_t * p_sql,
                          const char * query,
                          sql_query_callback_t callback,
                          void *arg )
{
    int i_ret = VLC_SUCCESS;
    vlc_mutex_lock( &p_sql->p_sys->lock );
    assert( p_sql->p_sys->db );
#ifndef NDEBUG
    msg_Dbg( p_sql, "QueryCallback: %s", query );
#endif
    sqlite3_exec( p_sql->p_sys->db, query, callback, arg, NULL );
    if( sqlite3_errcode( p_sql->p_sys->db ) != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        i_ret = VLC_EGENERIC;
    }

    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Direct SQL Query
 * @param p_sql This sql_t object
 * @param query SQL query
 * @param result Return value : Array of results
 * @param nrow Return value : Row number
 * @param ncol Return value : Column number
 * @return VLC_SUCCESS or an error code
 * You have to set and open current database first
 * @todo Handle transaction closing due to errors in sql query
 */
static int Query( sql_t * p_sql,
                  const char * query,
                  char *** result,
                  int * nrow,
                  int * ncol )
{
    assert( p_sql->p_sys->db );
    int i_ret = VLC_SUCCESS;
    vlc_mutex_lock( &p_sql->p_sys->lock );

#ifndef NDEBUG
    msg_Dbg( p_sql, "Query: %s", query );
#endif
    sqlite3_get_table( p_sql->p_sys->db, query, result, nrow, ncol, NULL );
    if( sqlite3_errcode( p_sql->p_sys->db ) != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        i_ret = VLC_EGENERIC;
    }

    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Get tables in database
 * @param p_sql This sql_t object
 * @param result SQL query result
 * @return Number of elements
 * You have to set and open current database first
 */
static int GetTables( sql_t * p_sql,
                      char *** result )
{
    int nrow, i_num = -1;

    vlc_mutex_lock( &p_sql->p_sys->lock );

    assert( p_sql->p_sys->db );

    sqlite3_get_table( p_sql->p_sys->db, "SELECT * FROM sqlite_master;",
                       result, &nrow, &i_num, NULL );
    if( sqlite3_errcode( p_sql->p_sys->db ) != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
    }
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_num;
}

/**
 * @brief Free SQL request's result
 * @param p_sql This SQL object.
 * @param ppsz_result SQL result to free
 */
static void FreeResult( sql_t * p_sql, char **ppsz_result )
{
    VLC_UNUSED( p_sql );
    if( ppsz_result != NULL )
        sqlite3_free_table( ppsz_result );
}

/**
 * @brief vmprintf replacement for SQLite.
 * @param psz_fmt Format string
 * @param args va_list of arguments
 * This function implements the formats %q, %Q and %z.
 */
static char* VMSprintf( const char* psz_fmt, va_list args )
{
    char *psz = sqlite3_vmprintf( psz_fmt, args );
    char *ret = strdup( psz );
    sqlite3_free( psz );
    return ret;
}

/**
 * @brief Starts a Transaction and waits if necessary
 * @param p_sql The SQL object
 * @note This function locks the transactions on the database.
 * Within the period of the transaction, only the calling thread may
 * execute sql statements provided all threads use these transaction fns.
 */
static int BeginTransaction( sql_t* p_sql )
{
    int i_ret = VLC_SUCCESS;
    vlc_mutex_lock( &p_sql->p_sys->trans_lock );
    vlc_mutex_lock( &p_sql->p_sys->lock );
    assert( p_sql->p_sys->db );

    sqlite3_exec( p_sql->p_sys->db, "BEGIN;", NULL, NULL, NULL );
#ifndef NDEBUG
    msg_Dbg( p_sql, "Transaction Query: BEGIN;" );
#endif
    if( sqlite3_errcode( p_sql->p_sys->db ) != SQLITE_OK )
    {
        vlc_mutex_unlock( &p_sql->p_sys->trans_lock );
        vlc_mutex_unlock( &p_sql->p_sys->lock );
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        i_ret = VLC_EGENERIC;
    }
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Commit a transaction
 * @param p_sql The SQL object
 * @note This function unlocks the transactions on the database
 * Only the calling thread of "BeginTransaction" is allowed to call this method
 * If the commit fails, the transaction lock is still held by the thread
 * and this function may be retried or RollbackTransaction can be called
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static int CommitTransaction( sql_t* p_sql )
{
    int i_ret = VLC_SUCCESS;
    assert( p_sql->p_sys->db );
    vlc_mutex_lock( &p_sql->p_sys->lock );

    /** This turns the auto commit on. */
    sqlite3_exec( p_sql->p_sys->db, "COMMIT;", NULL, NULL, NULL );
#ifndef NDEBUG
    msg_Dbg( p_sql, "Transaction Query: COMMIT;" );
#endif
    if( sqlite3_errcode( p_sql->p_sys->db ) != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        i_ret = VLC_EGENERIC;
    }
    else
        vlc_mutex_unlock( &p_sql->p_sys->trans_lock );
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Rollback a transaction, in case of failure
 * @param p_sql The SQL object
 * @return VLC_SUCCESS or VLC_EGENERIC
 * @note This function unlocks the transactions on the database
 * Only the calling thread of "BeginTransaction" is allowed to call this method
 * If failed, if a statement in the transaction failed, it means that
 * the transaction was automatically rolled back
 * If failed otherwise, the engine is busy executing some queries and you must
 * try again
 */
static void RollbackTransaction( sql_t* p_sql )
{
    assert( p_sql->p_sys->db );
    vlc_mutex_lock( &p_sql->p_sys->lock );

    sqlite3_exec( p_sql->p_sys->db, "ROLLBACK;", NULL, NULL, NULL );
#ifndef NDEBUG
    msg_Dbg( p_sql, "Transaction Query: ROLLBACK;" );
#endif
    if( sqlite3_errcode( p_sql->p_sys->db ) != SQLITE_OK )
    {
        msg_Err( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
    }
    vlc_mutex_unlock( &p_sql->p_sys->trans_lock );
    vlc_mutex_unlock( &p_sql->p_sys->lock );
}

/**
 * Prepare an sqlite statement
 * @return statement object or NULL in case of failure
 */
static sql_stmt_t* PrepareStatement( sql_t* p_sql, const char* psz_fmt, int i_length )
{
    assert( p_sql->p_sys->db );
    sql_stmt_t* p_stmt;
    p_stmt = calloc( 1, sizeof( *p_stmt ) );
    if( p_stmt == NULL )
        return NULL;
    vlc_mutex_lock( &p_sql->p_sys->lock );
    if( sqlite3_prepare_v2( p_sql->p_sys->db, psz_fmt, i_length,
                            &p_stmt->p_sqlitestmt, NULL ) != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        vlc_mutex_unlock( &p_sql->p_sys->lock );
        free( p_stmt );
        return NULL;
    }

    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return p_stmt;
}

/**
 * @brief Bind arguments to a sql_stmt_t object
 * @param p_sql The SQL object
 * @param p_stmt Statement Object
 * @param i_pos Position at which the parameter should be bound
 * @param i_type Data type of the value
 * @param p_value Value to be bound
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static int BindValues( sql_t* p_sql, sql_stmt_t* p_stmt,
        int i_pos, unsigned int i_type, const sql_value_t* p_value )
{
    assert( p_sql->p_sys->db );
    assert( p_stmt->p_sqlitestmt );
    vlc_mutex_lock( &p_sql->p_sys->lock );
    int i_ret, i_vlc_ret = VLC_SUCCESS;
    switch( i_type )
    {
        case SQL_INT:
            i_ret = sqlite3_bind_int( p_stmt->p_sqlitestmt, i_pos, p_value->value.i );
            break;
        case SQL_DOUBLE:
            i_ret = sqlite3_bind_double( p_stmt->p_sqlitestmt, i_pos, p_value->value.dbl );
            break;
        case SQL_TEXT:
            i_ret = sqlite3_bind_text( p_stmt->p_sqlitestmt, i_pos, p_value->value.psz, p_value->length, NULL );
            break;
        case SQL_BLOB:
            i_ret = sqlite3_bind_blob( p_stmt->p_sqlitestmt, i_pos, p_value->value.ptr, p_value->length, NULL );
            break;
        case SQL_NULL:
            i_ret = sqlite3_bind_null( p_stmt->p_sqlitestmt, i_pos );
            break;
        default:
            msg_Warn( p_sql, "Trying to bind invalid type of value %d", i_type );
            vlc_mutex_unlock( &p_sql->p_sys->lock );
            return VLC_EGENERIC;
    }
    if( i_ret != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        i_vlc_ret = VLC_EGENERIC;
    }
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_vlc_ret;
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
static int StatementStep( sql_t* p_sql, sql_stmt_t* p_stmt )
{
    assert( p_sql->p_sys->db );
    assert( p_stmt->p_sqlitestmt );
    vlc_mutex_lock( &p_sql->p_sys->lock );
    int i_sqlret = sqlite3_step( p_stmt->p_sqlitestmt );
    int i_ret = VLC_EGENERIC;
    if( i_sqlret == SQLITE_ROW )
        i_ret = VLC_SQL_ROW;
    else if( i_ret == SQLITE_DONE )
        i_ret = VLC_SQL_DONE;
    else
    {
       msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
       i_ret = VLC_EGENERIC;
    }
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Reset the SQL statement. Resetting the statement will unbind all
 * the values that were bound on this statement
 * @param p_sql The SQL object
 * @param p_stmt The sql statement object
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static int StatementReset( sql_t* p_sql, sql_stmt_t* p_stmt )
{
    assert( p_sql->p_sys->db );
    assert( p_stmt->p_sqlitestmt );
    int i_ret = VLC_SUCCESS;
    vlc_mutex_lock( &p_sql->p_sys->lock );
    if( sqlite3_reset( p_stmt->p_sqlitestmt ) != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        i_ret = VLC_EGENERIC;
    }
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Destroy the sql statement object. This will free memory.
 * @param p_sql The SQL object
 * @param p_stmt The statement object
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static int StatementFinalize( sql_t* p_sql, sql_stmt_t* p_stmt )
{
    assert( p_sql->p_sys->db );
    assert( p_stmt->p_sqlitestmt );
    int i_ret = VLC_SUCCESS;
    vlc_mutex_lock( &p_sql->p_sys->lock );
    if( sqlite3_finalize( p_stmt->p_sqlitestmt ) != SQLITE_OK )
    {
        msg_Warn( p_sql, "sqlite3 error: %d: %s",
                  sqlite3_errcode( p_sql->p_sys->db ),
                  sqlite3_errmsg( p_sql->p_sys->db ) );
        i_ret = VLC_EGENERIC;
    }
    free( p_stmt );
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
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
static int GetColumnFromStatement( sql_t* p_sql, sql_stmt_t* p_stmt, int i_col,
                          int type, sql_value_t *p_res )
{
    assert( p_sql->p_sys->db );
    assert( p_stmt->p_sqlitestmt );
    int i_ret = VLC_SUCCESS;
    vlc_mutex_lock( &p_sql->p_sys->lock );
    const unsigned char* psz;
    const void* ptr;
    int size;
    switch( type )
    {
        case SQL_INT:
            p_res->value.i = sqlite3_column_int( p_stmt->p_sqlitestmt, i_col );
            break;
        case SQL_DOUBLE:
            p_res->value.dbl = sqlite3_column_double( p_stmt->p_sqlitestmt, i_col );
            break;
        case SQL_TEXT:
            psz = sqlite3_column_text( p_stmt->p_sqlitestmt, i_col );
            if( psz )
                p_res->value.psz = strdup( (const char* ) psz );
            break;
        case SQL_BLOB:
            ptr = sqlite3_column_blob( p_stmt->p_sqlitestmt, i_col );
            size = sqlite3_column_bytes( p_stmt->p_sqlitestmt, i_col );
            if( ptr )
            {
                p_res->value.ptr = malloc( size );
                p_res->length = size;
                if( p_res->value.ptr )
                    memcpy( p_res->value.ptr, ptr, size );
                else
                    i_ret = VLC_ENOMEM;
            }
            break;
        case SQL_NULL:
        default:
            msg_Warn( p_sql, "Trying to bind invalid type of value %d", type );
            i_ret = VLC_EGENERIC;
    }
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Get the datatype of the result of the column
 * @param p_sql The SQL object
 * @param p_stmt The sql statement object
 * @param i_col The column
 * @param pi_type pointer to datatype of the given column
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
static int GetColumnTypeFromStatement( sql_t* p_sql, sql_stmt_t* p_stmt, int i_col,
                        int* pi_type )
{
    assert( p_sql->p_sys->db );
    assert( p_stmt->p_sqlitestmt );
    assert( pi_type );
    vlc_mutex_lock( &p_sql->p_sys->lock );
    int i_ret = VLC_SUCCESS;
    int i_sqlret = sqlite3_column_type( p_stmt->p_sqlitestmt, i_col );
    switch( i_sqlret )
    {
        case SQLITE_INTEGER:
            *pi_type = SQL_INT;
            break;
        case SQLITE_FLOAT:
            *pi_type= SQL_DOUBLE;
            break;
        case SQLITE_TEXT:
            *pi_type = SQL_TEXT;
            break;
        case SQLITE_BLOB:
            *pi_type = SQL_BLOB;
            break;
        case SQLITE_NULL:
            *pi_type = SQL_NULL;
            break;
        default:
            i_ret = VLC_EGENERIC;
    }
    vlc_mutex_unlock( &p_sql->p_sys->lock );
    return i_ret;
}

/**
 * @brief Get the size of the column in bytes
 * @param p_sql The SQL object
 * @param p_stmt The sql statement object
 * @param i_col The column
 * @return Size of the column in bytes, undefined for invalid columns
 */
static int GetColumnSize( sql_t* p_sql, sql_stmt_t* p_stmt, int i_col )
{
    assert( p_sql->p_sys->db );
    assert( p_stmt->p_sqlitestmt );
    return sqlite3_column_bytes( p_stmt->p_sqlitestmt, i_col );
}

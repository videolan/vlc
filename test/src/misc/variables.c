/*****************************************************************************
 * variables.c: test for variables
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
 * $Id$
 *
 * Authors: Rémi Duraffort <ivoire@videolan.org>
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

#include <limits.h>

#include "../../libvlc/test.h"
#include "../lib/libvlc_internal.h"

const char *psz_var_name[] = { "a", "abcdef", "abcdefg", "abc123", "abc-123", "é€!!" };
const int i_var_count = 6;
vlc_value_t var_value[6];


static void test_integer( libvlc_int_t *p_libvlc )
{
    int i;
    for( i = 0; i < i_var_count; i++ )
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_INTEGER );

    for( i = 0; i < i_var_count; i++ )
    {
        var_value[i].i_int = rand();
        var_SetInteger( p_libvlc, psz_var_name[i], var_value[i].i_int );
    }

    for( i = 0; i < i_var_count; i++ )
    {
        assert( var_GetInteger( p_libvlc, psz_var_name[i] ) == var_value[i].i_int );
        var_IncInteger( p_libvlc, psz_var_name[i] );
        assert( var_GetInteger( p_libvlc, psz_var_name[i] ) == var_value[i].i_int + 1 );
        var_DecInteger( p_libvlc, psz_var_name[i] );
        assert( var_GetInteger( p_libvlc, psz_var_name[i] ) == var_value[i].i_int );
    }

    for( i = 0; i < i_var_count; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_booleans( libvlc_int_t *p_libvlc )
{
    int i;
    for( i = 0; i < i_var_count; i++ )
         var_Create( p_libvlc, psz_var_name[i], VLC_VAR_BOOL );

    for( i = 0; i < i_var_count; i++ )
    {
        var_value[i].b_bool = (rand() > RAND_MAX/2);
        var_SetBool( p_libvlc, psz_var_name[i], var_value[i].b_bool );
    }

    for( i = 0; i < i_var_count; i++ )
        assert( var_GetBool( p_libvlc, psz_var_name[i] ) == var_value[i].b_bool );

    for( i = 0; i < i_var_count; i++ )
        var_ToggleBool( p_libvlc, psz_var_name[i] );

    for( i = 0; i < i_var_count; i++ )
        assert( var_GetBool( p_libvlc, psz_var_name[i] ) != var_value[i].b_bool );

    for( i = 0; i < i_var_count; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_times( libvlc_int_t *p_libvlc )
{
    int i;
    for( i = 0; i < i_var_count; i++ )
         var_Create( p_libvlc, psz_var_name[i], VLC_VAR_TIME );

    for( i = 0; i < i_var_count; i++ )
    {
        var_value[i].i_time = rand();
        var_SetTime( p_libvlc, psz_var_name[i], var_value[i].i_time );
    }

    for( i = 0; i < i_var_count; i++ )
        assert( var_GetTime( p_libvlc, psz_var_name[i] ) == var_value[i].i_time );

    for( i = 0; i < i_var_count; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_floats( libvlc_int_t *p_libvlc )
{
    int i;
    for( i = 0; i < i_var_count; i++ )
         var_Create( p_libvlc, psz_var_name[i], VLC_VAR_FLOAT );

    for( i = 0; i < i_var_count; i++ )
    {
        var_value[i].f_float = rand();
        var_SetFloat( p_libvlc, psz_var_name[i], var_value[i].f_float );
    }

    for( i = 0; i < i_var_count; i++ )
        assert( var_GetFloat( p_libvlc, psz_var_name[i] ) == var_value[i].f_float );

    for( i = 0; i < i_var_count; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_strings( libvlc_int_t *p_libvlc )
{
    int i;
    char *psz_tmp;
    for( i = 0; i < i_var_count; i++ )
         var_Create( p_libvlc, psz_var_name[i], VLC_VAR_STRING );

    for( i = 0; i < i_var_count; i++ )
        var_SetString( p_libvlc, psz_var_name[i], psz_var_name[i] );

    for( i = 0; i < i_var_count; i++ )
    {
        psz_tmp = var_GetString( p_libvlc, psz_var_name[i] );
        assert( !strcmp( psz_tmp, psz_var_name[i] ) );
        free( psz_tmp );
    }

    for( i = 0; i < i_var_count; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );


    /* Some more test for strings */
    var_Create( p_libvlc, "bla", VLC_VAR_STRING );
    assert( var_GetNonEmptyString( p_libvlc, "bla" ) == NULL );
    var_SetString( p_libvlc, "bla", "" );
    assert( var_GetNonEmptyString( p_libvlc, "bla" ) == NULL );
    var_SetString( p_libvlc, "bla", "test" );
    psz_tmp = var_GetNonEmptyString( p_libvlc, "bla" );
    assert( !strcmp( psz_tmp, "test" ) );
    free( psz_tmp );
    var_Destroy( p_libvlc, "bla" );
}

static void test_address( libvlc_int_t *p_libvlc )
{
    char dummy[i_var_count];

    int i;
    for( i = 0; i < i_var_count; i++ )
         var_Create( p_libvlc, psz_var_name[i], VLC_VAR_ADDRESS );

    for( i = 0; i < i_var_count; i++ )
    {
        var_value[i].p_address = dummy + i;
        var_SetAddress( p_libvlc, psz_var_name[i], var_value[i].p_address );
    }

    for( i = 0; i < i_var_count; i++ )
    {
        vlc_value_t val;
        var_Get( p_libvlc, psz_var_name[i], &val );
        assert( val.p_address == var_value[i].p_address );
    }

    for( i = 0; i < i_var_count; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static int callback( vlc_object_t* p_this, char const *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    (void)p_this;    (void)oldval;
    int i;

    // Check the parameters
    assert( p_data == psz_var_name );

    // Find the variable
    for( i = 0; i < i_var_count; i++ )
    {
        if( !strcmp( psz_var_name[i], psz_var ) )
            break;
    }
    // Check the variable is known
    assert( i < i_var_count );

    var_value[i].i_int = newval.i_int;
    return VLC_SUCCESS;
}

static void test_callbacks( libvlc_int_t *p_libvlc )
{
    /* add the callbacks */
    int i;
    for( i = 0; i < i_var_count; i++ )
    {
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_INTEGER );
        var_AddCallback( p_libvlc, psz_var_name[i], callback, psz_var_name );
    }

    /* Set the variables and trigger the callbacks */
    for( i = 0; i < i_var_count; i++ )
    {
        int i_temp = rand();
        var_SetInteger( p_libvlc, psz_var_name[i], i_temp );
        assert( i_temp == var_value[i].i_int );
        var_SetInteger( p_libvlc, psz_var_name[i], 0 );
        assert( var_value[i].i_int == 0 );
        var_value[i].i_int = 1;
    }

    /* Only trigger the callback: the value will be 0 again */
    for( i = 0; i < i_var_count; i++ )
    {
        var_TriggerCallback( p_libvlc, psz_var_name[i] );
        assert( var_value[i].i_int == 0 );
    }

    for( i = 0; i < i_var_count; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_limits( libvlc_int_t *p_libvlc )
{
    vlc_value_t val;
    val.i_int = 0;
    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );

    var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val, NULL );
    assert( val.i_int == 0 );

    val.i_int = -1234;
    var_Change( p_libvlc, "bla", VLC_VAR_SETMIN, &val, NULL );
    val.i_int = 12345;
    var_Change( p_libvlc, "bla", VLC_VAR_SETMAX, &val, NULL );

    var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val, NULL );
    assert( val.i_int == -1234 );
    var_Change( p_libvlc, "bla", VLC_VAR_GETMAX, &val, NULL );
    assert( val.i_int == 12345 );

    var_SetInteger( p_libvlc, "bla", -123456 );
    assert( var_GetInteger( p_libvlc, "bla" ) == -1234 );
    var_SetInteger( p_libvlc, "bla", 1234 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 1234 );
    var_SetInteger( p_libvlc, "bla", 12346 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 12345 );

    val.i_int = 42;
    var_Change( p_libvlc, "bla", VLC_VAR_SETSTEP, &val, NULL );
    var_SetInteger( p_libvlc, "bla", 20 );
    val.i_int = 0;
    var_Change( p_libvlc, "bla", VLC_VAR_GETSTEP, &val, NULL );
    assert( val.i_int == 42 );

    var_SetInteger( p_libvlc, "bla", 20 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 0 );

    var_SetInteger( p_libvlc, "bla", 21 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 42 );

    var_Destroy( p_libvlc, "bla" );
}

static void test_choices( libvlc_int_t *p_libvlc )
{
    vlc_value_t val, val2;
    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE |
                                 VLC_VAR_ISCOMMAND );
    val.i_int = 1;
    val2.psz_string = (char*)"one";
    var_Change( p_libvlc, "bla", VLC_VAR_ADDCHOICE, &val, &val2 );

    val.i_int = 2;
    val2.psz_string = (char*)"two";
    var_Change( p_libvlc, "bla", VLC_VAR_ADDCHOICE, &val, &val2 );

    assert( var_CountChoices( p_libvlc, "bla" ) == 2 );

    var_Change( p_libvlc, "bla", VLC_VAR_DELCHOICE, &val, &val2 );
    assert( var_CountChoices( p_libvlc, "bla" ) == 1 );

    var_Change( p_libvlc, "bla", VLC_VAR_GETCHOICES, &val, &val2 );
    assert( val.p_list->i_count == 1 && val.p_list->p_values[0].i_int == 1 &&
            val2.p_list->i_count == 1 &&
            !strcmp( val2.p_list->p_values[0].psz_string, "one" ) );
    var_FreeList( &val, &val2 );

    var_Change( p_libvlc, "bla", VLC_VAR_CLEARCHOICES, NULL, NULL );
    assert( var_CountChoices( p_libvlc, "bla" ) == 0 );

    var_Destroy( p_libvlc, "bla" );
}

static void test_change( libvlc_int_t *p_libvlc )
{
    /* Add min, max and step
       Yes we can have min > max but we don't really care */
    vlc_value_t val;
    int i_min, i_max, i_step;

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );
    val.i_int = i_min = rand();
    var_Change( p_libvlc, "bla", VLC_VAR_SETMIN, &val, NULL );
    val.i_int = i_max = rand();
    var_Change( p_libvlc, "bla", VLC_VAR_SETMAX, &val, NULL );
    val.i_int = i_step = rand();
    var_Change( p_libvlc, "bla", VLC_VAR_SETSTEP, &val, NULL );

    /* Do something */
    var_SetInteger( p_libvlc, "bla", rand() );
    val.i_int = var_GetInteger( p_libvlc, "bla" ); /* dummy read */

    /* Test everything is right */
    var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val, NULL );
    assert( val.i_int == i_min );
    var_Change( p_libvlc, "bla", VLC_VAR_GETMAX, &val, NULL );
    assert( val.i_int == i_max );
    var_Change( p_libvlc, "bla", VLC_VAR_GETSTEP, &val, NULL );
    assert( val.i_int == i_step );

    var_Destroy( p_libvlc, "bla" );
}

static void test_creation_and_type( libvlc_int_t *p_libvlc )
{
    vlc_value_t val;
    val.i_int = 4212;

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER) );

    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER) );

    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND) );

    var_Change( p_libvlc, "bla", VLC_VAR_SETMIN, &val, NULL );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASMIN) );

    var_Change( p_libvlc, "bla", VLC_VAR_SETMAX, &val, NULL );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASMIN | VLC_VAR_HASMAX) );

    var_Change( p_libvlc, "bla", VLC_VAR_SETSTEP, &val, NULL );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASMIN | VLC_VAR_HASMAX | VLC_VAR_HASSTEP) );

    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    assert( var_Get( p_libvlc, "bla", &val ) == VLC_ENOVAR );

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER) );

    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND) );

    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASCHOICE ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASCHOICE) );

    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    assert( var_Get( p_libvlc, "bla", &val ) == VLC_ENOVAR );

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );
    var_Change( p_libvlc, "bla", VLC_VAR_SETMIN, &val, NULL );
    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASMIN) );
    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASMIN | VLC_VAR_HASCHOICE) );

    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    assert( var_Get( p_libvlc, "bla", &val ) == VLC_ENOVAR );

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );
    var_Change( p_libvlc, "bla", VLC_VAR_SETMAX, &val, NULL );
    var_Change( p_libvlc, "bla", VLC_VAR_SETSTEP, &val, NULL );
    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASMAX | VLC_VAR_HASSTEP) );
    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_HASCHOICE ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND | VLC_VAR_HASMAX | VLC_VAR_HASSTEP | VLC_VAR_HASCHOICE) );

    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    assert( var_Get( p_libvlc, "bla", &val ) == VLC_ENOVAR );
}

static void test_variables( libvlc_instance_t *p_vlc )
{
    libvlc_int_t *p_libvlc = p_vlc->p_libvlc_int;
    srand( time( NULL ) );

    log( "Testing for integers\n" );
    test_integer( p_libvlc );

    log( "Testing for booleans\n" );
    test_booleans( p_libvlc );

    log( "Testing for times\n" );
    test_times( p_libvlc );

    log( "Testing for floats\n" );
    test_floats( p_libvlc );

    log( "Testing for strings\n" );
    test_strings( p_libvlc );

    log( "Testing for addresses\n" );
    test_address( p_libvlc );

    log( "Testing the callbacks\n" );
    test_callbacks( p_libvlc );

    log( "Testing the limits\n" );
    test_limits( p_libvlc );

    log( "Testing choices\n" );
    test_choices( p_libvlc );

    log( "Testing var_Change()\n" );
    test_change( p_libvlc );

    log( "Testing type at creation\n" );
    test_creation_and_type( p_libvlc );
}


int main( void )
{
    libvlc_instance_t *p_vlc;

    test_init();

    log( "Testing the core variables\n" );
    p_vlc = libvlc_new( test_defaults_nargs, test_defaults_args );
    assert( p_vlc != NULL );

    test_variables( p_vlc );

    libvlc_release( p_vlc );

    return 0;
}


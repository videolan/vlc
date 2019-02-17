/*****************************************************************************
 * variables.c: test for variables
 *****************************************************************************
 * Copyright (C) 2009 the VideoLAN team
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

static const char *psz_var_name[] = {
    "a", "abcdef", "abcdefg", "abc123", "abc-123", "é€!!"
};
#define VAR_COUNT (ARRAY_SIZE(psz_var_name))
static vlc_value_t var_value[VAR_COUNT];

static void test_integer( libvlc_int_t *p_libvlc )
{
    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_INTEGER );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        var_value[i].i_int = rand();
        var_SetInteger( p_libvlc, psz_var_name[i], var_value[i].i_int );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        assert( var_GetInteger( p_libvlc, psz_var_name[i] ) == var_value[i].i_int );
        var_IncInteger( p_libvlc, psz_var_name[i] );
        assert( var_GetInteger( p_libvlc, psz_var_name[i] ) == var_value[i].i_int + 1 );
        var_DecInteger( p_libvlc, psz_var_name[i] );
        assert( var_GetInteger( p_libvlc, psz_var_name[i] ) == var_value[i].i_int );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_booleans( libvlc_int_t *p_libvlc )
{
    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_BOOL );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        var_value[i].b_bool = (rand() > RAND_MAX/2);
        var_SetBool( p_libvlc, psz_var_name[i], var_value[i].b_bool );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        assert( var_GetBool( p_libvlc, psz_var_name[i] ) == var_value[i].b_bool );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_ToggleBool( p_libvlc, psz_var_name[i] );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        assert( var_GetBool( p_libvlc, psz_var_name[i] ) != var_value[i].b_bool );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_floats( libvlc_int_t *p_libvlc )
{
    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_FLOAT );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        var_value[i].f_float = rand();
        var_SetFloat( p_libvlc, psz_var_name[i], var_value[i].f_float );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        assert( var_GetFloat( p_libvlc, psz_var_name[i] ) == var_value[i].f_float );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_fracts( libvlc_int_t *p_libvlc )
{
    const char *name = psz_var_name[0];
    unsigned num, den;

    var_Create( p_libvlc, name, VLC_VAR_STRING );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) != 0 );

    var_SetString( p_libvlc, name, "123garbage" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) != 0 );

    var_SetString( p_libvlc, name, "4/5garbage" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) != 0 );

    var_SetString( p_libvlc, name, "6.7garbage" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) != 0 );

    var_SetString( p_libvlc, name, "." );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 0 && den == 1 );

    var_SetString( p_libvlc, name, "010" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 10 && den == 1 );

    var_SetString( p_libvlc, name, "30" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 30 && den == 1 );

    var_SetString( p_libvlc, name, "30.0" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 30 && den == 1 );

    var_SetString( p_libvlc, name, "030.030" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 3003 && den == 100 );

    var_SetString( p_libvlc, name, "60/2" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 30 && den == 1 );

    var_SetString( p_libvlc, name, "29.97" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 2997 && den == 100 );

    var_SetString( p_libvlc, name, "30000/1001" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 30000 && den == 1001 );

    var_SetString( p_libvlc, name, ".125" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 1 && den == 8 );

    var_SetString( p_libvlc, name, "12:9" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 4 && den == 3 );

    var_SetString( p_libvlc, name, "000000/00000000" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 0 && den == 0 );

    var_SetString( p_libvlc, name, "12345/0" );
    assert( var_InheritURational( p_libvlc, &num, &den, name ) == 0 );
    assert( num == 1 && den == 0 );

    var_Destroy( p_libvlc, name );
}

static void test_strings( libvlc_int_t *p_libvlc )
{
    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_STRING );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_SetString( p_libvlc, psz_var_name[i], psz_var_name[i] );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        char *psz_tmp = var_GetString( p_libvlc, psz_var_name[i] );

        assert( psz_tmp != NULL );
        assert( !strcmp( psz_tmp, psz_var_name[i] ) );
        free( psz_tmp );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );

    /* Some more test for strings */
    var_Create( p_libvlc, "bla", VLC_VAR_STRING );
    assert( var_GetNonEmptyString( p_libvlc, "bla" ) == NULL );
    var_SetString( p_libvlc, "bla", "" );
    assert( var_GetNonEmptyString( p_libvlc, "bla" ) == NULL );
    var_SetString( p_libvlc, "bla", "test" );

    char *psz_tmp = var_GetNonEmptyString( p_libvlc, "bla" );
    assert( psz_tmp != NULL );
    assert( !strcmp( psz_tmp, "test" ) );
    free( psz_tmp );
    var_Destroy( p_libvlc, "bla" );
}

static void test_address( libvlc_int_t *p_libvlc )
{
    char dummy[VAR_COUNT];

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_ADDRESS );

    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        var_value[i].p_address = &dummy[i];
        var_SetAddress( p_libvlc, psz_var_name[i], var_value[i].p_address );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        vlc_value_t val;
        var_Get( p_libvlc, psz_var_name[i], &val );
        assert( val.p_address == var_value[i].p_address );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static int callback( vlc_object_t* p_this, char const *psz_var,
                     vlc_value_t oldval, vlc_value_t newval, void *p_data)
{
    unsigned i;

    (void)p_this;    (void)oldval;

    // Check the parameters
    assert( p_data == psz_var_name );

    // Find the variable
    for( i = 0; i < VAR_COUNT; i++ )
    {
        if( !strcmp( psz_var_name[i], psz_var ) )
            break;
    }
    // Check the variable is known
    assert( i < VAR_COUNT );

    var_value[i].i_int = newval.i_int;
    return VLC_SUCCESS;
}

static void test_callbacks( libvlc_int_t *p_libvlc )
{
    /* add the callbacks */
    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        var_Create( p_libvlc, psz_var_name[i], VLC_VAR_INTEGER );
        var_AddCallback( p_libvlc, psz_var_name[i], callback, psz_var_name );
    }

    /* Set the variables and trigger the callbacks */
    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        int i_temp = rand();
        var_SetInteger( p_libvlc, psz_var_name[i], i_temp );
        assert( i_temp == var_value[i].i_int );
        var_SetInteger( p_libvlc, psz_var_name[i], 0 );
        assert( var_value[i].i_int == 0 );
        var_value[i].i_int = 1;
    }

    /* Only trigger the callback: the value will be 0 again */
    for( unsigned i = 0; i < VAR_COUNT; i++ )
    {
        var_TriggerCallback( p_libvlc, psz_var_name[i] );
        assert( var_value[i].i_int == 0 );
    }

    for( unsigned i = 0; i < VAR_COUNT; i++ )
        var_Destroy( p_libvlc, psz_var_name[i] );
}

static void test_limits( libvlc_int_t *p_libvlc )
{
    vlc_value_t val;
    val.i_int = 0;
    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );

    var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val );
    assert( val.i_int == INT64_MIN );
    var_Change( p_libvlc, "bla", VLC_VAR_GETMAX, &val );
    assert( val.i_int == INT64_MAX );

    var_Change( p_libvlc, "bla", VLC_VAR_SETMINMAX,
                (vlc_value_t){ .i_int = -1234 },
                (vlc_value_t){ .i_int = 12345 } );

    var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val );
    assert( val.i_int == -1234 );
    var_Change( p_libvlc, "bla", VLC_VAR_GETMAX, &val );
    assert( val.i_int == 12345 );

    var_SetInteger( p_libvlc, "bla", -123456 );
    assert( var_GetInteger( p_libvlc, "bla" ) == -1234 );
    var_SetInteger( p_libvlc, "bla", 1234 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 1234 );
    var_SetInteger( p_libvlc, "bla", 12346 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 12345 );

    val.i_int = 42;
    var_Change( p_libvlc, "bla", VLC_VAR_SETSTEP, val );
    var_SetInteger( p_libvlc, "bla", 20 );
    val.i_int = 0;
    var_Change( p_libvlc, "bla", VLC_VAR_GETSTEP, &val );
    assert( val.i_int == 42 );

    var_SetInteger( p_libvlc, "bla", 20 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 0 );

    var_SetInteger( p_libvlc, "bla", 21 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 42 );

    var_Destroy( p_libvlc, "bla" );
}

static void test_choices( libvlc_int_t *p_libvlc )
{
    vlc_value_t val;
    vlc_value_t *vals;
    char **texts;
    size_t count;

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND );
    val.i_int = 1;
    var_Change( p_libvlc, "bla", VLC_VAR_ADDCHOICE, val, "one" );

    val.i_int = 2;
    var_Change( p_libvlc, "bla", VLC_VAR_ADDCHOICE, val, "two" );

    assert( var_CountChoices( p_libvlc, "bla" ) == 2 );

    var_Change( p_libvlc, "bla", VLC_VAR_DELCHOICE, val );
    assert( var_CountChoices( p_libvlc, "bla" ) == 1 );

    var_Change( p_libvlc, "bla", VLC_VAR_GETCHOICES, &count, &vals, &texts );
    assert( count == 1 && vals[0].i_int == 1 && !strcmp( texts[0], "one" ) );
    free(texts[0]);
    free(texts);
    free(vals);

    var_Change( p_libvlc, "bla", VLC_VAR_CLEARCHOICES );
    assert( var_CountChoices( p_libvlc, "bla" ) == 0 );

    var_Destroy( p_libvlc, "bla" );
}

static void test_change( libvlc_int_t *p_libvlc )
{
    vlc_value_t val, min, max, step;

    min.i_int = -1242;
    max.i_int = +42;
    step.i_int = 13;

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );
    var_Change( p_libvlc, "bla", VLC_VAR_SETMINMAX, min, max );
    var_Change( p_libvlc, "bla", VLC_VAR_SETSTEP, step );

    var_SetInteger( p_libvlc, "bla", 13 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 13 );
    var_SetInteger( p_libvlc, "bla", 27 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 26 );
    var_SetInteger( p_libvlc, "bla", 35 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 39 );
    var_SetInteger( p_libvlc, "bla", -2 );
    assert( var_GetInteger( p_libvlc, "bla" ) == 0 );
    var_SetInteger( p_libvlc, "bla", -9 );
    assert( var_GetInteger( p_libvlc, "bla" ) == -13 );
    var_SetInteger( p_libvlc, "bla", -27 );
    assert( var_GetInteger( p_libvlc, "bla" ) == -26 );

    /* Test everything is right */
    var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val );
    assert( val.i_int == min.i_int );
    var_Change( p_libvlc, "bla", VLC_VAR_GETMAX, &val );
    assert( val.i_int == max.i_int );
    var_Change( p_libvlc, "bla", VLC_VAR_GETSTEP, &val );
    assert( val.i_int == step.i_int );

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

    assert( var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val ) != 0
         || val.i_int == INT64_MIN );
    assert( var_Change( p_libvlc, "bla", VLC_VAR_GETMAX, &val ) != 0
         || val.i_int == INT64_MAX );
    val.i_int = 4212;
    var_Change( p_libvlc, "bla", VLC_VAR_SETMINMAX, val, val );
    assert( var_Change( p_libvlc, "bla", VLC_VAR_GETMIN, &val ) == 0
         && val.i_int == 4212 );
    assert( var_Change( p_libvlc, "bla", VLC_VAR_GETMAX, &val ) == 0
         && val.i_int == 4212 );

    assert( var_Change( p_libvlc, "bla" , VLC_VAR_GETSTEP, &val ) != 0 );
    val.i_int = 4212;
    var_Change( p_libvlc, "bla", VLC_VAR_SETSTEP, val );
    assert( var_Change( p_libvlc, "bla" , VLC_VAR_GETSTEP, &val ) == 0
         && val.i_int == 4212 );

    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    assert( var_Get( p_libvlc, "bla", &val ) == VLC_ENOVAR );

    var_Create( p_libvlc, "program", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    assert( var_Type( p_libvlc, "program" ) == (VLC_VAR_INTEGER) );

    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND) );

    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    assert( var_Get( p_libvlc, "bla", &val ) == VLC_ENOVAR );

    var_Create( p_libvlc, "bla", VLC_VAR_INTEGER );
    assert( var_Create( p_libvlc, "bla", VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND ) == VLC_SUCCESS );
    assert( var_Type( p_libvlc, "bla" ) == (VLC_VAR_INTEGER | VLC_VAR_ISCOMMAND) );

    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    var_Destroy( p_libvlc, "bla" );
    assert( var_Get( p_libvlc, "bla", &val ) == VLC_ENOVAR );
}

static void test_variables( libvlc_instance_t *p_vlc )
{
    libvlc_int_t *p_libvlc = p_vlc->p_libvlc_int;
    srand( time( NULL ) );

    test_log( "Testing for integers\n" );
    test_integer( p_libvlc );

    test_log( "Testing for booleans\n" );
    test_booleans( p_libvlc );

    test_log( "Testing for floats\n" );
    test_floats( p_libvlc );

    test_log( "Testing for rationals\n" );
    test_fracts( p_libvlc );

    test_log( "Testing for strings\n" );
    test_strings( p_libvlc );

    test_log( "Testing for addresses\n" );
    test_address( p_libvlc );

    test_log( "Testing the callbacks\n" );
    test_callbacks( p_libvlc );

    test_log( "Testing the limits\n" );
    test_limits( p_libvlc );

    test_log( "Testing choices\n" );
    test_choices( p_libvlc );

    test_log( "Testing var_Change()\n" );
    test_change( p_libvlc );

    test_log( "Testing type at creation\n" );
    test_creation_and_type( p_libvlc );
}


int main( void )
{
    libvlc_instance_t *p_vlc;

    test_init();

    test_log( "Testing the core variables\n" );
    p_vlc = libvlc_new( test_defaults_nargs, test_defaults_args );
    assert( p_vlc != NULL );

    test_variables( p_vlc );

    libvlc_release( p_vlc );

    return 0;
}

/*****************************************************************************
 * css_parser.c : CSS parser
 *****************************************************************************
 * Copyright (C) 2017 VideoLabs, VLC authors and VideoLAN
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>

#include "css_bridge.h"
#include "css_parser.h"
#include "CSSGrammar.h"

#include <ctype.h>

static void vlc_css_term_Debug( const vlc_css_term_t a, int depth );
static void vlc_css_expression_Debug( const vlc_css_expr_t *p_expr, int depth );
static void vlc_css_declarations_Debug( const vlc_css_declaration_t *p_decl, int depth );
static void vlc_css_selectors_Debug( const vlc_css_selector_t *p_sel, int depth );
static void vlc_css_rules_Debug( const vlc_css_rule_t *p_rule, int depth );

#define CHAIN_APPEND_IMPL(n, t) CHAIN_APPEND_DECL(n ,t)\
{\
    t ** insert = &p_a->p_next;\
    while( *insert ) insert = &((*insert)->p_next);\
    *insert = p_b;\
}

void vlc_css_term_Clean( vlc_css_term_t a )
{
    if( a.type >= TYPE_STRING )
        free( a.psz );

    if( a.type == TYPE_FUNCTION )
    {
        if( a.function )
            vlc_css_expression_Delete( a.function );
    }
}

static void vlc_css_term_Debug( const vlc_css_term_t a, int depth )
{
    for(int i=0;i<depth;i++) printf(" ");
    printf("term: ");
    if( a.type >= TYPE_STRING )
    {
        printf("%x %s\n", a.type, a.psz);
        if( a.type == TYPE_FUNCTION && a.function )
            vlc_css_expression_Debug( a.function, depth + 1 );
    }
    else printf("%x %f\n", a.type, a.val);
}

bool vlc_css_expression_AddTerm( vlc_css_expr_t *p_expr,
                                        char op, vlc_css_term_t a )
{
    if( p_expr->i_count >= p_expr->i_alloc )
    {
        size_t i_realloc = (p_expr->i_alloc == 0) ? 1 : p_expr->i_alloc + 4;
        void *reac = realloc( p_expr->seq, i_realloc * sizeof(p_expr->seq[0]) );
        if( reac )
        {
            p_expr->seq = reac;
            p_expr->i_alloc = i_realloc;
        }
    }

    if( p_expr->i_count >= p_expr->i_alloc )
        return false;

    p_expr->seq[p_expr->i_count].op = op;
    p_expr->seq[p_expr->i_count++].term = a;
    return true;
}

void vlc_css_expression_Delete( vlc_css_expr_t *p_expr )
{
    if( p_expr )
    {
        for(size_t i=0; i<p_expr->i_count; i++)
            vlc_css_term_Clean( p_expr->seq[i].term );
        free( p_expr->seq );
    }
    free( p_expr );
}

static void vlc_css_expression_Debug( const vlc_css_expr_t *p_expr, int depth )
{
    if( p_expr )
    {
        for(int i=0;i<depth;i++) printf(" ");
        printf("expression: \n");
        for(size_t i=0; i<p_expr->i_count; i++)
            vlc_css_term_Debug( p_expr->seq[i].term, depth + 1 );
    }
}

vlc_css_expr_t * vlc_css_expression_New( vlc_css_term_t term )
{
    vlc_css_expr_t *p_expr = calloc(1, sizeof(*p_expr));
    if(!vlc_css_expression_AddTerm( p_expr, 0, term ))
    {
        free(p_expr);
        p_expr = NULL;
    }
    return p_expr;
}

CHAIN_APPEND_IMPL(vlc_css_declarations_Append, vlc_css_declaration_t)

void vlc_css_declarations_Delete( vlc_css_declaration_t *p_decl )
{
    while( p_decl )
    {
        vlc_css_declaration_t *p_next = p_decl->p_next;
        vlc_css_expression_Delete( p_decl->expr );
        free( p_decl->psz_property );
        free( p_decl );
        p_decl = p_next;
    }
}

static void vlc_css_declarations_Debug( const vlc_css_declaration_t *p_decl, int depth )
{
    while( p_decl )
    {
        for(int i=0;i<depth;i++) printf(" ");
        printf("declaration: %s\n", p_decl->psz_property );
        vlc_css_expression_Debug( p_decl->expr, depth + 1 );
        p_decl = p_decl->p_next;
    }
}

vlc_css_declaration_t * vlc_css_declaration_New( const char *psz )
{
    vlc_css_declaration_t *p_decl = calloc(1, sizeof(*p_decl));
    p_decl->psz_property = strdup(psz);
    return p_decl;
}

CHAIN_APPEND_IMPL(vlc_css_selector_Append, vlc_css_selector_t)

void
vlc_css_selector_AddSpecifier( vlc_css_selector_t *p_sel, vlc_css_selector_t *p_spec )
{
    *p_sel->specifiers.pp_append = p_spec;
    while(p_spec)
    {
        p_sel->specifiers.pp_append = &p_spec->p_next;
        p_spec = p_spec->p_next;
    }
}

void vlc_css_selectors_Delete( vlc_css_selector_t *p_sel )
{
    while( p_sel )
    {
        vlc_css_selector_t *p_next = p_sel->p_next;
        free( p_sel->psz_name );
        vlc_css_selectors_Delete( p_sel->specifiers.p_first );
        vlc_css_selectors_Delete( p_sel->p_matchsel );
        free( p_sel );
        p_sel = p_next;
    }
}

static void vlc_css_selectors_Debug( const vlc_css_selector_t *p_sel, int depth )
{
    while( p_sel )
    {
        for(int i=0;i<depth;i++) printf(" "); printf("selector %c%s:\n", p_sel->combinator, p_sel->psz_name );
        vlc_css_selectors_Debug( p_sel->p_matchsel, depth + 1 );
        vlc_css_selectors_Debug( p_sel->specifiers.p_first, depth + 1 );
        p_sel = p_sel->p_next;
    }
}

vlc_css_selector_t * vlc_css_selector_New( int type, const char *psz )
{
    vlc_css_selector_t *p_sel = calloc(1, sizeof(*p_sel));
    p_sel->psz_name = strdup(psz);
    p_sel->type = type;
    p_sel->combinator = RELATION_SELF;
    p_sel->specifiers.pp_append = &p_sel->specifiers.p_first;
    return p_sel;
}

void vlc_css_rules_Delete( vlc_css_rule_t *p_rule )
{
    while(p_rule)
    {
        vlc_css_rule_t *p_next = p_rule->p_next;
        vlc_css_selectors_Delete( p_rule->p_selectors );
        vlc_css_declarations_Delete( p_rule->p_declarations );
        free(p_rule);
        p_rule = p_next;
    }
}

static void vlc_css_rules_Debug( const vlc_css_rule_t *p_rule, int depth )
{
    int j = 0;
    while(p_rule)
    {
        for(int i=0;i<depth;i++) printf(" "); printf("rule %d:\n", j++);
        vlc_css_selectors_Debug( p_rule->p_selectors, depth + 1 );
        vlc_css_declarations_Debug( p_rule->p_declarations, depth + 1 );
        p_rule = p_rule->p_next;
    }
}

vlc_css_rule_t * vlc_css_rule_New( void )
{
    vlc_css_rule_t *p_rule = calloc(1, sizeof(*p_rule));
    return p_rule;
}

void vlc_css_parser_AddRule( vlc_css_parser_t *p_parser,
                                           vlc_css_rule_t *p_rule )
{
    (*p_parser->rules.pp_append) = p_rule;
    p_parser->rules.pp_append = &p_rule->p_next;
}

void vlc_css_parser_Debug( const vlc_css_parser_t *p_parser )
{
    vlc_css_rules_Debug( p_parser->rules.p_first, 0 );
}

void vlc_css_parser_Clean( vlc_css_parser_t *p_parser )
{
    vlc_css_rules_Delete( p_parser->rules.p_first );
}

void vlc_css_parser_Init( vlc_css_parser_t *p_parser )
{
    memset(p_parser, 0, sizeof(vlc_css_parser_t));
    p_parser->rules.pp_append = &p_parser->rules.p_first;
}

bool vlc_css_parser_ParseBytes( vlc_css_parser_t *p_parser, const uint8_t *p_data, size_t i_data )
{
    yyscan_t yy;
    yylex_init(&yy);

    YY_BUFFER_STATE buf = yy_scan_bytes( (const char*) p_data, i_data, yy );

    bool b_ret = !yyparse( yy, p_parser );

    yy_delete_buffer( buf, yy );
    yylex_destroy( yy );

    return b_ret;
}

bool vlc_css_parser_ParseString( vlc_css_parser_t *p_parser, const char *psz_css )
{
    yyscan_t yy;
    yylex_init(&yy);

    YY_BUFFER_STATE buf = yy_scan_string( psz_css, yy );

    bool b_ret = !yyparse( yy, p_parser );

    yy_delete_buffer( buf, yy );
    yylex_destroy( yy );

    return b_ret;
}

static int CodePointToUTF8( uint32_t ucs4, char *p )
{
    /* adapted from codepoint conversion from strings.h */
    if( ucs4 <= 0x7F )
    {
        p[0] = ucs4;
        return 1;
    }
    else if( ucs4 <= 0x7FF )
    {
        p[0] = 0xC0 |  (ucs4 >>  6);
        p[1] = 0x80 |  (ucs4        & 0x3F);
        return 2;
    }
    else if( ucs4 <= 0xFFFF )
    {
        p[0] = 0xE0 |  (ucs4 >> 12);
        p[1] = 0x80 | ((ucs4 >>  6) & 0x3F);
        p[2] = 0x80 |  (ucs4        & 0x3F);
        return 3;
    }
    else if( ucs4 <= 0x1FFFFF )
    {
        p[0] = 0xF0 |  (ucs4 >> 18);
        p[1] = 0x80 | ((ucs4 >> 12) & 0x3F);
        p[2] = 0x80 | ((ucs4 >>  6) & 0x3F);
        p[3] = 0x80 |  (ucs4        & 0x3F);
        return 4;
    }
    else if( ucs4 <= 0x3FFFFFF )
    {
        p[0] = 0xF8 |  (ucs4 >> 24);
        p[1] = 0x80 | ((ucs4 >> 18) & 0x3F);
        p[2] = 0x80 | ((ucs4 >> 12) & 0x3F);
        p[3] = 0x80 | ((ucs4 >>  6) & 0x3F);
        p[4] = 0x80 |  (ucs4        & 0x3F);
        return 5;
    }
    else
    {
        p[0] = 0xFC |  (ucs4 >> 30);
        p[1] = 0x80 | ((ucs4 >> 24) & 0x3F);
        p[2] = 0x80 | ((ucs4 >> 18) & 0x3F);
        p[3] = 0x80 | ((ucs4 >> 12) & 0x3F);
        p[4] = 0x80 | ((ucs4 >>  6) & 0x3F);
        p[5] = 0x80 |  (ucs4        & 0x3F);
        return 6;
    }
}

void vlc_css_unescape( char *psz )
{
    if( !psz )
        return;
    char *r = psz;
    char *w = psz;

    while( *r )
    {
        if( *r == '\\' )
        {
            r++;
            /* newlines */
            if( *r == 0 )
            {
                break;
            }
            else if( strchr( "nfr", *r ) )
            {
                switch( r[0] )
                {
                    case 'n':
                        *w++ = '\n';
                        r++;
                        break;
                    case 'r':
                        *w++ = '\r';
                        if( r[1] && r[1] == 'n' )
                        {
                            *w++ = '\n';
                            r++;
                        }
                        r++;
                        break;
                    case 'f':
                        *w++ = '\f';
                        r++;
                        break;
                }
            }
            else if( isxdigit( *r ) )
            {
                const char *p_start = r;
                int i;
                for( i=0; i<6 && *r && isxdigit( *r ); i++ )
                    r++;
                const char backup = *r;
                *r = 0;
                unsigned i_value = strtoul( p_start, NULL, 16 );
                *r = backup;
                if( i < 6 && *r && *r == ' ' )
                    r++;
                w += CodePointToUTF8( i_value, w );
            }
        }
        else
        {
            *w++ = *r++;
        }
    }

    *w = 0;
}

char * vlc_css_unescaped( const char *psz )
{
    char *psz_ret = strdup( psz );
    vlc_css_unescape( psz_ret );
    return psz_ret;
}

char * vlc_css_unquoted( const char *psz )
{
    char *psz_ret;
    if( *psz == '\'' || *psz == '\"' )
    {
        size_t i_len = strlen(psz);
        if( psz[i_len - 1] == psz[0] )
            psz_ret = strndup( psz + 1, i_len - 2 );
        else
            psz_ret = strdup( psz );
    }
    else
    {
        psz_ret = strdup( psz );
    }
    return psz_ret;
}


char * vlc_css_unquotedunescaped( const char *psz )
{
    char *psz_ret = vlc_css_unquoted( psz );
    if( psz_ret )
        vlc_css_unescape( psz_ret );
    return psz_ret;
}

#ifdef CSS_PARSER_DEBUG


static void css_properties_Debug( const vlc_css_declaration_t *p_decl )
{
    printf("set %s to ", p_decl->psz_property);
    for( size_t i=0; i<p_decl->expr->i_count; i++ )
    {
        printf("term %s ", p_decl->expr->seq[i].term.psz);
    }
    printf("\n");
}

void css_selector_Debug( const vlc_css_selector_t *p_sel )
{
    printf("select its ");
    switch( p_sel->combinator )
    {
        case RELATION_DESCENDENT:
            printf("descendent");
            break;
        case RELATION_DIRECTADJACENT:
            printf("adjacent");
            break;
        case RELATION_INDIRECTADJACENT:
            printf("indirect adjacent");
            break;
        case RELATION_CHILD:
            printf("child");
            break;
        case RELATION_SELF:
            break;
    }

    printf(" nodes matching filter: ");
    switch( p_sel->type )
    {
        case SELECTOR_SIMPLE:
            printf("<%s>\n", p_sel->psz_name);
            break;
        case SELECTOR_PSEUDOCLASS:
            printf(":%s\n", p_sel->psz_name);
            break;
        case SELECTOR_PSEUDOELEMENT:
            printf("::%s\n", p_sel->psz_name);
            break;
        case SPECIFIER_ID:
            printf("%s\n", p_sel->psz_name);
            break;
        case SPECIFIER_CLASS:
            printf(".%s\n", p_sel->psz_name);
            break;
        case SPECIFIER_ATTRIB:
            printf("[%s]\n", p_sel->psz_name);
            break;
    }
}

void css_rule_Debug( const vlc_css_rule_t *p_rule )
{
    if( p_rule == NULL )
    return;
    printf("add for rule nodes:\n");
    for( const vlc_css_selector_t *p_sel = p_rule->p_selectors;
                                   p_sel; p_sel = p_sel->p_next )
    {
        css_selector_Debug( p_sel );
        for( const vlc_css_selector_t *p_spec = p_sel->specifiers.p_first;
                                       p_spec; p_spec = p_spec->p_next )
            css_selector_Debug( p_spec );

        if( p_sel->p_next )
            printf("add nodes\n");
    }

    for( const vlc_css_declaration_t *p_decl = p_rule->p_declarations;
                                      p_decl; p_decl = p_decl->p_next )
    {
        css_properties_Debug( p_decl );
    }
}

#endif

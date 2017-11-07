/*****************************************************************************
 * CSSGrammar.y: bison production rules for simplified css parsing
 *****************************************************************************
 *  Copyright ©   2017 VideoLabs, VideoLAN and VLC Authors
 *
 *  Adapted from webkit's CSSGrammar.y:
 *
 *  Copyright (C) 2002-2003 Lars Knoll (knoll@kde.org)
 *  Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *  Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 *  Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */
%pure-parser

%parse-param { yyscan_t scanner }
%parse-param { vlc_css_parser_t *css_parser }
%lex-param   { yyscan_t scanner }
%lex-param   { vlc_css_parser_t *css_parser }

%{
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include "css_parser.h"

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif
%}

%union {
    bool boolean;
    char character;
    int integer;
    char *string;
    enum vlc_css_relation_e relation;

    vlc_css_term_t term;
    vlc_css_expr_t *expr;
    vlc_css_rule_t  *rule;
    vlc_css_declaration_t *declaration;
    vlc_css_declaration_t *declarationList;
    vlc_css_selector_t *selector;
    vlc_css_selector_t *selectorList;
}

%{
/* See bison pure calling */
int yylex(union YYSTYPE *, yyscan_t, vlc_css_parser_t *);

static int yyerror(yyscan_t scanner, vlc_css_parser_t *p, const char *msg)
{
    VLC_UNUSED(scanner);VLC_UNUSED(p);VLC_UNUSED(msg);
    return 1;
}

%}

%expect 10

%nonassoc LOWEST_PREC

%left UNIMPORTANT_TOK

%token WHITESPACE SGML_CD
%token TOKEN_EOF 0

%token INCLUDES
%token DASHMATCH
%token BEGINSWITH
%token ENDSWITH
%token CONTAINS

%token <string> STRING
%right <string> IDENT

%nonassoc <string> IDSEL
%nonassoc <string> HASH
%nonassoc ':'
%nonassoc '.'
%nonassoc '['
%nonassoc <character> '*'
%nonassoc error
%left '|'

%token FONT_FACE_SYM
%token CHARSET_SYM

%token IMPORTANT_SYM

%token CDO
%token CDC
%token <term> LENGTH
%token <term> ANGLE
%token <term> TIME
%token <term> FREQ
%token <term> DIMEN
%token <term> PERCENTAGE
%token <term> NUMBER

%destructor { vlc_css_term_Clean($$); } <term>

%token <string> URI
%token <string> FUNCTION
%token <string> UNICODERANGE

%type <relation> combinator

%type <rule> charset
%type <rule> ignored_charset
%type <rule> ruleset
%type <rule> font_face
%type <rule> invalid_rule
%type <rule> rule
%type <rule> valid_rule
%destructor { vlc_css_rules_Delete($$); } <rule>

%type <string> ident_or_string
%type <string> property

%type <selector> specifier
%type <selector> specifier_list
%type <selector> simple_selector
%type <selector> selector
%type <selectorList> selector_list
%type <selector> selector_with_trailing_whitespace
%type <selector> class
%type <selector> attrib
%type <selector> pseudo
%destructor { vlc_css_selectors_Delete($$); } <selector> <selectorList>

%type <declarationList> declaration_list
%type <declarationList> decl_list
%type <declaration> declaration
%destructor { vlc_css_declarations_Delete($$); } <declaration> <declarationList>

%type <boolean> prio

%type <integer> match
%type <integer> unary_operator
%type <integer> maybe_unary_operator
%type <character> operator

%type <expr> expr
%type <term> term
%type <term> unary_term
%type <term> function
%destructor { vlc_css_expression_Delete($$); } <expr>

%type <string> element_name
%type <string> attr_name

%destructor { free($$); } <string>

%%

stylesheet:
    maybe_space maybe_charset maybe_sgml rule_list
  ;

maybe_space:
    /* empty */ %prec UNIMPORTANT_TOK
  | maybe_space WHITESPACE
  ;

maybe_sgml:
    /* empty */
  | maybe_sgml SGML_CD
  | maybe_sgml WHITESPACE
  ;

maybe_charset:
   /* empty */
  | charset {
    vlc_css_rules_Delete($1);
  }
  ;

closing_brace:
    '}'
  | %prec LOWEST_PREC TOKEN_EOF
  ;

charset:
  CHARSET_SYM maybe_space STRING maybe_space ';' {
      free( $3 );
      $$ = 0;
  }
  | CHARSET_SYM error invalid_block {
      $$ = 0;
  }
  | CHARSET_SYM error ';' {
      $$ = 0;
  }
;

ignored_charset:
    CHARSET_SYM maybe_space STRING maybe_space ';' {
        // Ignore any @charset rule not at the beginning of the style sheet.
        free( $3 );
        $$ = 0;
    }
    | CHARSET_SYM maybe_space ';' {
        $$ = 0;
    }
;

rule_list:
   /* empty */
 | rule_list rule maybe_sgml {
     if( $2 )
         vlc_css_parser_AddRule( css_parser, $2 );
 }
 ;

valid_rule:
    ruleset
  | font_face
  ;

rule:
    valid_rule {
        $$ = $1;
        if($$)
            $$->b_valid = true;
    }
  | ignored_charset
  | invalid_rule
  ;

font_face:
    FONT_FACE_SYM maybe_space
    '{' maybe_space declaration_list closing_brace {
        vlc_css_declarations_Delete( $5 );
        $$ = NULL;
    }
    | FONT_FACE_SYM error invalid_block {
        $$ = NULL;
    }
    | FONT_FACE_SYM error ';' {
        $$ = NULL;
    }
;

combinator:
    '+' maybe_space { $$ = RELATION_DIRECTADJACENT; }
  | '~' maybe_space { $$ = RELATION_INDIRECTADJACENT; }
  | '>' maybe_space { $$ = RELATION_CHILD; }
  ;

maybe_unary_operator:
    unary_operator { $$ = $1; }
    | { $$ = 1; }
    ;

unary_operator:
    '-' { $$ = -1; }
  | '+' { $$ = 1; }
  ;

ruleset:
    selector_list '{' maybe_space declaration_list closing_brace {
        $$ = vlc_css_rule_New();
        if($$)
        {
            $$->p_selectors = $1;
            $$->p_declarations = $4;
        }
    }
  ;

selector_list:
    selector %prec UNIMPORTANT_TOK {
        if ($1) {
            $$ = $1;
        }
    }
    | selector_list ',' maybe_space selector %prec UNIMPORTANT_TOK {
        if ($1 && $4 )
        {
            $$ = $1;
            vlc_css_selector_Append( $$, $4 );
        }
        else
        {
            vlc_css_selectors_Delete( $1 );
            vlc_css_selectors_Delete( $4 );
            $$ = NULL;
        }
    }
  | selector_list error {
        vlc_css_selectors_Delete( $1 );
        $$ = NULL;
    }
   ;

selector_with_trailing_whitespace:
    selector WHITESPACE {
        $$ = $1;
    }
    ;

selector:
    simple_selector {
        $$ = $1;
    }
    | selector_with_trailing_whitespace
    {
        $$ = $1;
    }
    | selector_with_trailing_whitespace simple_selector
    {
        $$ = $1;
        if ($$)
        {
            vlc_css_selector_AddSpecifier( $$, $2 );
            $2->combinator = RELATION_DESCENDENT;
        }
        else $$ = $2;
    }
    | selector combinator simple_selector {
        $$ = $1;
        if ($$)
        {
            vlc_css_selector_AddSpecifier( $$, $3 );
            $3->combinator = $2;
        }
        else $$ = $3;
    }
    | selector error {
        vlc_css_selectors_Delete( $1 );
        $$ = NULL;
    }
    ;

simple_selector:
    element_name {
        $$ = vlc_css_selector_New( SELECTOR_SIMPLE, $1 );
        free( $1 );
    }
    | element_name specifier_list {
        $$ = vlc_css_selector_New( SELECTOR_SIMPLE, $1 );
        if( $$ && $2 )
        {
            vlc_css_selector_AddSpecifier( $$, $2 );
        }
        else
        {
            vlc_css_selectors_Delete( $2 );
        }
        free( $1 );
    }
    | specifier_list {
        $$ = $1;
    }
  ;

element_name:
    IDENT
    | '*' { $$ = strdup("*"); }
  ;

specifier_list:
    specifier {
        $$ = $1;
    }
    | specifier_list specifier {
        if( $1 )
        {
            $$ = $1;
            while( $1->specifiers.p_first )
                $1 = $1->specifiers.p_first;
            vlc_css_selector_AddSpecifier( $1, $2 );
        }
        else $$ = $2;
    }
    | specifier_list error {
        vlc_css_selectors_Delete( $1 );
        $$ = NULL;
    }
;

specifier:
    IDSEL {
        $$ = vlc_css_selector_New( SPECIFIER_ID, $1 );
        free( $1 );
    }
    /* Case when #fffaaa like token is lexed as HEX instead of IDSEL */
  | HASH {
        if ($1[0] >= '0' && $1[0] <= '9') {
            $$ = NULL;
        } else {
            $$ = vlc_css_selector_New( SPECIFIER_ID, $1 );
        }
        free( $1 );
    }
  | class
  | attrib
  | pseudo
    ;

class:
    '.' IDENT {
        $$ = vlc_css_selector_New( SPECIFIER_CLASS, $2 );
        free( $2 );
    }
  ;

attr_name:
    IDENT maybe_space {
        $$ = $1;
    }
    ;

attrib:
    '[' maybe_space attr_name ']' {
        $$ = vlc_css_selector_New( SPECIFIER_ATTRIB, $3 );
        free( $3 );
    }
    | '[' maybe_space attr_name match maybe_space ident_or_string maybe_space ']' {
        $$ = vlc_css_selector_New( SPECIFIER_ATTRIB, $3 );
        if( $$ )
        {
            $$->match = $4;
            $$->p_matchsel = vlc_css_selector_New( SPECIFIER_ID, $6 );
        }
        free( $3 );
        free( $6 );
    }
  ;

match:
    '=' {
        $$ = MATCH_EQUALS;
    }
    | INCLUDES {
        $$ = MATCH_INCLUDES;
    }
    | DASHMATCH {
        $$ = MATCH_DASHMATCH;
    }
    | BEGINSWITH {
        $$ = MATCH_BEGINSWITH;
    }
    | ENDSWITH {
        $$ = MATCH_ENDSWITH;
    }
    | CONTAINS {
        $$ = MATCH_CONTAINS;
    }
    ;

ident_or_string:
    IDENT
  | STRING
    ;

pseudo:
    ':' IDENT {
        $$ = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, $2 );
        free( $2 );
    }
    | ':' ':' IDENT {
        $$ = vlc_css_selector_New( SELECTOR_PSEUDOELEMENT, $3 );
        free( $3 );
    }
    // used by :nth-*
    | ':' FUNCTION maybe_space maybe_unary_operator NUMBER maybe_space ')' {
        if(*$2 != 0)
            $2[strlen($2) - 1] = 0;
        $$ = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, $2 );
        $5.val *= $4;
        free( $2 );
        vlc_css_term_Clean( $5 );
    }
    // required for WEBVTT weirdos cue::(::past)
    | ':' ':' FUNCTION maybe_space selector maybe_space ')' {
        if(*$3 != 0)
            $3[strlen($3) - 1] = 0;
        $$ = vlc_css_selector_New( SELECTOR_PSEUDOELEMENT, $3 );
        free( $3 );
        if( $$ && $5 )
        {
            vlc_css_selector_AddSpecifier( $$, $5 );
            $5->combinator = RELATION_SELF;
        }
        else
            vlc_css_selectors_Delete( $5 );
    }
    // used by :nth-*(odd/even) and :lang
    | ':' FUNCTION maybe_space IDENT maybe_space ')' {
        if(*$2 != 0)
            $2[strlen($2) - 1] = 0;
        $$ = vlc_css_selector_New( SELECTOR_PSEUDOCLASS, $2 );
        free( $2 );
        free( $4 );
    }
  ;

declaration_list:
    declaration {
        $$ = $1;
    }
    | decl_list declaration {
        $$ = $1;
        if( $$ )
            vlc_css_declarations_Append( $$, $2 );
    }
    | decl_list {
        $$ = $1;
    }
    | error invalid_block_list error {
        $$ = NULL;
    }
    | error {
        $$ = NULL;
    }
    | decl_list error {
        $$ = $1;
    }
    | decl_list invalid_block_list {
        $$ = $1;
    }
    ;

decl_list:
    declaration ';' maybe_space {
        $$ = $1;
    }
    | declaration invalid_block_list maybe_space {
        vlc_css_declarations_Delete( $1 );
        $$ = NULL;
    }
    | declaration invalid_block_list ';' maybe_space {
        vlc_css_declarations_Delete( $1 );
        $$ = NULL;
    }
    | error ';' maybe_space {
        $$ = NULL;
    }
    | error invalid_block_list error ';' maybe_space {
        $$ = NULL;
    }
    | decl_list declaration ';' maybe_space {
        if( $1 )
        {
            $$ = $1;
            vlc_css_declarations_Append( $$, $2 );
        }
        else $$ = $2;
    }
    | decl_list error ';' maybe_space {
        $$ = $1;
    }
    | decl_list error invalid_block_list error ';' maybe_space {
        $$ = $1;
    }
    ;

declaration:
    property ':' maybe_space expr prio {
        $$ = vlc_css_declaration_New( $1 );
        if( $$ )
            $$->expr = $4;
        else
            vlc_css_expression_Delete( $4 );
        free( $1 );
    }
    |
    property error {
        free( $1 );
        $$ = NULL;
    }
    |
    property ':' maybe_space error expr prio {
        free( $1 );
        vlc_css_expression_Delete( $5 );
        /* The default movable type template has letter-spacing: .none;  Handle this by looking for
        error tokens at the start of an expr, recover the expr and then treat as an error, cleaning
        up and deleting the shifted expr.  */
        $$ = NULL;
    }
    |
    property ':' maybe_space expr prio error {
        free( $1 );
        vlc_css_expression_Delete( $4 );
        /* When we encounter something like p {color: red !important fail;} we should drop the declaration */
        $$ = NULL;
    }
    |
    IMPORTANT_SYM maybe_space {
        /* Handle this case: div { text-align: center; !important } Just reduce away the stray !important. */
        $$ = NULL;
    }
    |
    property ':' maybe_space {
        free( $1 );
        /* div { font-family: } Just reduce away this property with no value. */
        $$ = NULL;
    }
    |
    property ':' maybe_space error {
        free( $1 );
        /* if we come across rules with invalid values like this case: p { weight: *; }, just discard the rule */
        $$ = NULL;
    }
    |
    property invalid_block {
        /* if we come across: div { color{;color:maroon} }, ignore everything within curly brackets */
        free( $1 );
        $$ = NULL;
    }
  ;

property:
    IDENT maybe_space {
        $$ = $1;
    }
  ;

prio:
    IMPORTANT_SYM maybe_space { $$ = true; }
    | /* empty */ { $$ = false; }
  ;

expr:
    term {
        $$ = vlc_css_expression_New( $1 );
        if( !$$ )
            vlc_css_term_Clean( $1 );
    }
    | expr operator term {
        $$ = $1;
        if( !$1 || !vlc_css_expression_AddTerm($1, $2, $3) )
            vlc_css_term_Clean( $3 );
    }
    | expr invalid_block_list {
        vlc_css_expression_Delete( $1 );
        $$ = NULL;
    }
    | expr invalid_block_list error {
        vlc_css_expression_Delete( $1 );
        $$ = NULL;
    }
    | expr error {
        vlc_css_expression_Delete( $1 );
        $$ = NULL;
    }
  ;

operator:
    '/' maybe_space {
        $$ = '/';
    }
  | ',' maybe_space {
        $$ = ',';
    }
  | /* empty */ {
        $$ = 0;
  }
  ;

term:
  unary_term { $$ = $1; }
  | unary_operator unary_term {
      $$ = $2;
      $$.val *= $1;
  }
  | STRING maybe_space { $$.type = TYPE_STRING; $$.psz = $1; }
  | IDENT maybe_space { $$.type = TYPE_IDENTIFIER; $$.psz = $1; }
  /* We might need to actually parse the number from a dimension, but we can't just put something that uses $$.string into unary_term. */
  | DIMEN maybe_space { $$ = $1; }
  | unary_operator DIMEN maybe_space { $$ = $2; }
  | URI maybe_space { $$.type = TYPE_URI; $$.psz = $1; }
  | UNICODERANGE maybe_space { $$.type = TYPE_UNICODERANGE; $$.psz = $1; }
  | IDSEL maybe_space { $$.type = TYPE_HEXCOLOR; $$.psz = $1; }
  | HASH maybe_space { $$.type = TYPE_HEXCOLOR; $$.psz = $1; }
  | '#' maybe_space { $$.type = TYPE_HEXCOLOR; $$.psz = NULL; } /* Handle error case: "color: #;" */
  /* FIXME: according to the specs a function can have a unary_operator in front. I know no case where this makes sense */
  | function {
      $$ = $1;
  }
  | '%' maybe_space { /* Handle width: %; */
      $$.type = TYPE_PERCENT; $$.val = 0;
  }
  ;

unary_term:
  NUMBER maybe_space
  | PERCENTAGE maybe_space
  | LENGTH maybe_space
  | ANGLE maybe_space
  | TIME maybe_space
  | FREQ maybe_space
  ;

function:
    FUNCTION maybe_space expr ')' maybe_space {
        $$.type = TYPE_FUNCTION; $$.function = $3;
        $$.psz = $1;
        if(*$$.psz != 0)
            $$.psz[strlen($$.psz) - 1] = 0;
    } |
    FUNCTION maybe_space expr TOKEN_EOF {
        $$.type = TYPE_FUNCTION; $$.function = $3; $$.psz = $1;
        if(*$$.psz != 0)
            $$.psz[strlen($$.psz) - 1] = 0;
    } |
    FUNCTION maybe_space ')' maybe_space {
        $$.type = TYPE_FUNCTION; $$.function = NULL; $$.psz = $1;
        if(*$$.psz != 0)
            $$.psz[strlen($$.psz) - 1] = 0;
    } |
    FUNCTION maybe_space error {
        $$.type = TYPE_FUNCTION; $$.function = NULL; $$.psz = $1;
        if(*$$.psz != 0)
            $$.psz[strlen($$.psz) - 1] = 0;
  }
  ;

/* error handling rules */

invalid_rule:
    error invalid_block {
        $$ = NULL;
    }

/*
  Seems like the two rules below are trying too much and violating
  http://www.hixie.ch/tests/evil/mixed/csserrorhandling.html

  | error ';' {
        $$ = 0;
    }
  | error '}' {
        $$ = 0;
    }
*/
    ;

invalid_block:
    '{' error invalid_block_list error closing_brace
  | '{' error closing_brace
    ;

invalid_block_list:
    invalid_block
  | invalid_block_list error invalid_block
;

%%

#ifdef YYDEBUG
    int yydebug=1;
#else
    int yydebug=0;
#endif

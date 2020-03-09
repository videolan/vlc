/*****************************************************************************
 * CSSLexer.l : lexer for simplified CSS, based on W3C spec
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
%option case-insensitive
%option reentrant
%option bison-bridge
%option noyywrap
%option nounput
%option noinput
%option never-interactive
%option nostdinit

%{
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <vlc_common.h>
#include "css_parser.h"
#include "CSSGrammar.h"
#define VAL(a, b) { yylval->term.val = (a); yylval->term.type = TYPE_ ## b; }
#define us_strtof strtof
char *d;
%}

h		[0-9a-f]
nonascii	[\200-\377]
unicode		\\{h}{1,6}[ \t\r\n\f]?
escape		{unicode}|\\[ -~\200-\377]
nmstart		[a-z]|{nonascii}|{escape}
nmchar		[a-z0-9-]|{nonascii}|{escape}
string1		\"([\t !#$%&(-~]|\\{nl}|\'|{nonascii}|{escape})*\"
string2		\'([\t !#$%&(-~]|\\{nl}|\"|{nonascii}|{escape})*\'

ident		[-]?{nmstart}{nmchar}*
name		{nmchar}+
num		[0-9]+|[0-9]*"."[0-9]+
string		{string1}|{string2}
url		([!#$%&*-~]|{nonascii}|{escape})*
w		[ \t\r\n\f]*
nl		\n|\r\n|\r|\f
range		\?{1,6}|{h}(\?{0,5}|{h}(\?{0,4}|{h}(\?{0,3}|{h}(\?{0,2}|{h}(\??|{h})))))

%%

[ \t\r\n\f]+		{return WHITESPACE;}

\/\*[^*]*\*+([^/][^*]*\*+)*\/	/* ignore comments */

"<!--"			{return CDO;}
"-->"			{return CDC;}
"~="			{return INCLUDES;}
"|="			{return DASHMATCH;}
"^="			{return BEGINSWITH;}
"$="			{return ENDSWITH;}
"*="			{return CONTAINS;}

{string}		{ yylval->string = vlc_css_unquotedunescaped(yytext); return STRING;}

{ident}			{ yylval->string = vlc_css_unescaped(yytext); return IDENT;}

"@font-face"		{return FONT_FACE_SYM;}

"!{w}important"		{return IMPORTANT_SYM;}

{num}em			{ VAL( us_strtof(yytext, &d), EMS ); return LENGTH;}
{num}ex			{ VAL( atoi(yytext), EXS ); return LENGTH;}
{num}px			{ VAL( atoi(yytext), PIXELS ); return LENGTH;}
{num}cm			{ VAL( us_strtof(yytext, &d) * 10, MILLIMETERS ); return LENGTH;}
{num}mm			{ VAL( atoi(yytext), MILLIMETERS ); return LENGTH;}
{num}in			{ VAL( us_strtof(yytext, &d) * 25.4, MILLIMETERS ); return LENGTH;}
{num}pt			{ VAL( us_strtof(yytext, &d), POINTS ); return LENGTH;}
{num}pc			{ VAL( us_strtof(yytext, &d), POINTS ); return LENGTH;}
{num}deg		{ VAL( us_strtof(yytext, &d), DEGREES ); return ANGLE;}
{num}rad		{ VAL( us_strtof(yytext, &d) * 0.0174533, DEGREES ); return ANGLE;}
{num}grad		{ VAL( us_strtof(yytext, &d) * 1.1111111, DEGREES ); return ANGLE;}
{num}ms			{ VAL( atoi(yytext), MILLISECONDS ); return TIME;}
{num}s			{ VAL( atoi(yytext) * 1000, MILLISECONDS ); return TIME;}
{num}Hz			{ VAL( atoi(yytext), HERTZ ); return FREQ;}
{num}kHz		{ VAL( atoi(yytext) * 1000, HERTZ ); return FREQ;}
{num}{ident}	{ VAL( 0, DIMENSION ); return DIMEN;}
{num}%			{ VAL( atoi(yytext), PERCENT ); return PERCENTAGE;}
{num}			{ VAL( us_strtof(yytext, &d), NONE ); return NUMBER;}

"url("{w}{string}{w}")"	{ yylval->string = vlc_css_unquotedunescaped(yytext); return URI;}
"url("{w}{url}{w}")"	{ yylval->string = vlc_css_unquotedunescaped(yytext); return URI;}
{ident}"("		{ yylval->string = vlc_css_unescaped(yytext); return FUNCTION;}
"#"{ident}      {yylval->string = vlc_css_unescaped(yytext); return IDSEL;}
"#"{name}       {yylval->string = vlc_css_unescaped(yytext); return HASH;}

U\+{range}		{ yylval->string = strdup(yytext); return UNICODERANGE;}
U\+{h}{1,6}-{h}{1,6}	{ yylval->string = strdup(yytext); return UNICODERANGE;}

.			{return *yytext;}

/*****************************************************************************
 * json/grammar.y: JSON parser
 *****************************************************************************
 * Copyright © 2020 Rémi Denis-Courmont
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

%define api.pure full
%lex-param { void *scanner }
%parse-param { void *log }
%parse-param { void *scanner }
%parse-param { struct json_object *result }

%{

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include "json.h"

static void json_array_free(struct json_array *);

static void json_value_free(struct json_value *v)
{
	switch (v->type) {
		case JSON_NULL:
		case JSON_BOOLEAN:
		case JSON_NUMBER:
			break;

		case JSON_STRING:
			free(v->string);
			break;

		case JSON_ARRAY:
			json_array_free(&v->array);
			break;

		case JSON_OBJECT:
			json_free(&v->object);
			break;
	}
}

static void json_array_free(struct json_array *a)
{
	for (size_t i = 0; i < a->size; i++)
		json_value_free(&a->entries[i]);

	free(a->entries);
}

static void json_member_free(struct json_member *m)
{
	free(m->name);
	json_value_free(&m->value);
}

void json_free(struct json_object *o)
{
	for (size_t i = 0; i < o->count; i++)
		json_member_free(&o->members[i]);

	free(o->members);
}

static void json_array_init(struct json_array *a)
{
	a->size = 0;
	a->entries = NULL;
}

static void json_array_append(struct json_array *a, struct json_value v)
{
	size_t size = a->size;
	struct json_value *ne = realloc(a->entries, (size + 1) * sizeof (v));

	if (ne != NULL) {
		ne[size] = v;
		a->entries = ne;
		a->size = size + 1;
	} else
		json_value_free(&v);

}

static void json_init(struct json_object *o)
{
	o->count = 0;
	o->members = NULL;
}

static void json_append(struct json_object *o, struct json_member m)
{
	size_t count = o->count;
	struct json_member *nm = realloc(o->members, (count + 1) * sizeof (m));

	if (nm != NULL) {
		nm[count] = m;
		o->members = nm;
		o->count = count + 1;
	} else
		json_member_free(&m);
}

%}

%union {
	bool boolean;
	double number;
	char *string;
	struct json_value value;
	struct json_array array;
	struct json_member member;
	struct json_object object;
}

%token YYEOF 0
%token VALUE_NULL
%token <boolean> BOOLEAN
%token <number> NUMBER
%token <string> STRING

%type <value> value
%type <array> values
%type <array> array
%type <member> member
%type <object> members
%type <object> object

%{

static void yyerror(void *log, void *scanner, struct json_object *result,
			const char *msg)
{
	json_parse_error(log, msg);
	(void) scanner; (void) result;
}

extern int yylex_init(void **);
extern void yyset_in(FILE *, void *);
extern int yylex(YYSTYPE *value, void *scanner);
extern int yylex_destroy(void *);

%}

%destructor { free($$); } <string>
%destructor { json_value_free(&$$); } <value>
%destructor { json_array_free(&$$); } <array>
%destructor { json_member_free(&$$); } <member>
%destructor { json_free(&$$); } <object>

%%

parse:
	object YYEOF	{ *result = $1; }
;

object:
	'{' '}'			{ json_init(&$$); }
|	'{' members '}'		{ $$ = $2; }
;

array:
	'[' ']'			{ json_array_init(&$$); }
|	'[' values ']'		{ $$ = $2; }
;

members:
	member			{ json_init(&$$); json_append(&$$, $1); }
|	members ',' member	{ $$ = $1; json_append(&$$, $3); }
;

member:
	STRING ':' value	{ $$.name = $1; $$.value = $3; }
;

values:
	value			{ json_array_init(&$$);
					json_array_append(&$$, $1); }
|	values ',' value	{ $$ = $1;
					json_array_append(&$$, $3); }
;

value:
	VALUE_NULL		{ $$.type = JSON_NULL; }
|	BOOLEAN			{ $$.type = JSON_BOOLEAN;
					$$.boolean = $1; }
|	NUMBER			{ $$.type = JSON_NUMBER;
					$$.number = $1; }
|	STRING			{ $$.type = JSON_STRING;
					$$.string = $1; }
|	array			{ $$.type = JSON_ARRAY;
					$$.array = $1; }
|	object			{ $$.type = JSON_OBJECT;
					$$.object = $1; }
;

%%

int json_parse(void *log, FILE *in, struct json_object *result)
{
	void *scanner;
	int ret = yylex_init(&scanner);

	if (ret)
		return ret;

	yyset_in(in, scanner);
	ret = yyparse(log, scanner, result);
	yylex_destroy(scanner);
	return ret;
}

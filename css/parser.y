/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file

CSS parser using the lemon parser generator.

see CSS2.1 Specification, chapter 4
http://www.w3.org/TR/CSS21/syndata.html

stylesheet  : [ CDO | CDC | S | statement ]*;
statement   : ruleset | at-rule;
at-rule     : ATKEYWORD S* any* [ block | ';' S* ];
block       : '{' S* [ any | block | ATKEYWORD S* | ';' S* ]* '}' S*;
ruleset     : selector? '{' S* declaration? [ ';' S* declaration? ]* '}' S*;
selector    : any+;
declaration : DELIM? property S* ':' S* value S*;
property    : IDENT;
value       : [ any | block | ATKEYWORD S* ]+;
any         : [ IDENT | NUMBER | PERCENTAGE | DIMENSION | STRING
              | DELIM | URI | HASH | UNICODE-RANGE | INCLUDES
              | DASHMATCH | FUNCTION S* any* ')'
              | '(' S* any* ')' | '[' S* any* ']' ] S*;

Note: CDO, CDC will be stripped out by the scanner
*/

stylesheet ::= ws statement_list.

ws ::= .
ws ::= ws_1.

ws_1 ::= S.
ws_1 ::= ws_1 S.

statement_list ::= .
statement_list ::= statement_list statement.

statement ::= ws_1.
statement ::= ruleset.
statement ::= at_rule.

at_rule ::= ATKEYWORD ws any_list block.
at_rule ::= ATKEYWORD(A) ws any_list(B) SEMI ws.
		{ if ((A.length == 7) && (strncasecmp(A.text, "@import", 7) == 0)
				&& B)
			css_atimport(param->stylesheet, B);
		css_free_node(B); }

block ::= LBRACE ws block_body RBRACE ws.
block_body ::= .
block_body ::= block_body any ws.
block_body ::= block_body block.
block_body ::= block_body ATKEYWORD ws.
block_body ::= block_body SEMI ws.

ruleset ::= selector_list(A) LBRACE ws declaration_list(B) RBRACE ws.
		{ if (A && B)
			css_add_ruleset(param->stylesheet, A, B);
		else
			css_free_selector(A);
		css_free_node(B); }
ruleset ::= LBRACE declaration_list(A) RBRACE.
		/* this form of ruleset not used in CSS2
		   used to parse style attributes (ruleset_only = 1) */
		{ if (param->ruleset_only) param->declaration = A;
		  else css_free_node(A); }
ruleset ::= any_list_1(A) LBRACE declaration_list(B) RBRACE.
		{ css_free_node(A); css_free_node(B); } /* not CSS2 */

selector_list(A) ::= selector(B) ws.
		{ A = B; }
selector_list(A) ::= selector_list(B) COMMA ws selector(C) ws.
		{ if (B && C) {
			C->next = B;
			A = C;
		} else {
			css_free_selector(B);
			css_free_selector(C);
			A = 0;
		} }

selector(A) ::= simple_selector(B).
		{ A = B; }
selector(A) ::= selector(B) css_combinator(C) simple_selector(D).
		{ if (B && D) {
			D->combiner = B;
			D->comb = C;
			D->specificity += B->specificity;
			A = D;
		} else {
			css_free_selector(B);
			css_free_selector(D);
			A = 0;
		} }

css_combinator(A) ::= ws PLUS ws.
		{ A = CSS_COMB_PRECEDED; }
css_combinator(A) ::= ws GT ws.
		{ A = CSS_COMB_PARENT; }
css_combinator(A) ::= ws_1.
		{ A = CSS_COMB_ANCESTOR; }

simple_selector(A) ::= element_name(B) detail_list(C).
		{ if (C && (A = css_new_selector(CSS_SELECTOR_ELEMENT,
				B.text, B.length))) {
			A->detail = C;
			A->specificity = 1 + C->specificity;
		} else {
			param->memory_error = true;
			css_free_selector(C);
			A = 0;
		} }
simple_selector(A) ::= element_name(B).
		{ if ((A = css_new_selector(CSS_SELECTOR_ELEMENT,
				B.text, B.length)))
			A->specificity = 1;
		else
			param->memory_error = true;
		}
simple_selector(A) ::= detail_list(C).
		{ if (C && (A = css_new_selector(CSS_SELECTOR_ELEMENT, 0, 0))) {
			A->detail = C;
			A->specificity = C->specificity;
		} else {
			param->memory_error = true;
			css_free_selector(C);
			A = 0;
                } }

element_name(A) ::= IDENT(B).
		{ A = B; }
element_name(A) ::= ASTERISK.
		{ A.text = 0; }

detail_list(A) ::= detail(B).
		{ A = B; }
detail_list(A) ::= detail(B) detail_list(C).
		{ if (B && C) {
			B->specificity += C->specificity;
			B->next = C;
			A = B;
		} else {
			css_free_selector(B);
			css_free_selector(C);
			A = 0;
		} }

detail(A) ::= HASH(B).
		{ A = css_new_selector(CSS_SELECTOR_ID, B.text+1, B.length-1);
		if (A) A->specificity = 0x10000;
		else param->memory_error = true; }
detail(A) ::= DOT IDENT(B).
		{ A = css_new_selector(CSS_SELECTOR_CLASS, B.text, B.length);
		if (A) A->specificity = 0x100;
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB, B.text, B.length);
		if (A) A->specificity = 0x100;
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws EQUALS ws IDENT(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_EQ, B.text, B.length);
		if (A) { A->data2 = C.text; A->data2_length = C.length;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws EQUALS ws STRING(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_EQ, B.text, B.length);
		if (A) { A->data2 = C.text + 1; A->data2_length = C.length - 2;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws INCLUDES ws IDENT(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_INC, B.text, B.length);
		if (A) { A->data2 = C.text; A->data2_length = C.length;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws INCLUDES ws STRING(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_INC, B.text, B.length);
		if (A) { A->data2 = C.text + 1; A->data2_length = C.length - 2;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws DASHMATCH ws IDENT(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_DM, B.text, B.length);
		if (A) { A->data2 = C.text; A->data2_length = C.length;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws DASHMATCH ws STRING(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_DM, B.text, B.length);
		if (A) { A->data2 = C.text + 1; A->data2_length = C.length - 2;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws PREFIX ws IDENT(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_PRE, B.text, B.length);
		if (A) { A->data2 = C.text; A->data2_length = C.length;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws PREFIX ws STRING(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_PRE, B.text, B.length);
		if (A) { A->data2 = C.text + 1; A->data2_length = C.length - 2;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws SUFFIX ws IDENT(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_SUF, B.text, B.length);
		if (A) { A->data2 = C.text; A->data2_length = C.length;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws SUFFIX ws STRING(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_SUF, B.text, B.length);
		if (A) { A->data2 = C.text + 1; A->data2_length = C.length - 2;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws SUBSTR ws IDENT(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_SUB, B.text, B.length);
		if (A) { A->data2 = C.text; A->data2_length = C.length;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= LBRAC ws IDENT(B) ws SUBSTR ws STRING(C) ws RBRAC.
		{ A = css_new_selector(CSS_SELECTOR_ATTRIB_SUB, B.text, B.length);
		if (A) { A->data2 = C.text + 1; A->data2_length = C.length - 2;
			A->specificity = 0x100; }
		else param->memory_error = true; }
detail(A) ::= COLON IDENT(B).
		{ if (B.length == 4 && strncasecmp(B.text, "link", 4) == 0) {
			A = css_new_selector(CSS_SELECTOR_ATTRIB, "href", 4);
			if (A) A->specificity = 0x100;
			else param->memory_error = true;
		} else {
			A = css_new_selector(CSS_SELECTOR_PSEUDO, B.text, B.length);
			if (A) A->specificity = 0x100;
			else param->memory_error = true;
		} }
detail(A) ::= COLON FUNCTION(B) ws IDENT ws RPAREN.
		{ A = css_new_selector(CSS_SELECTOR_PSEUDO, B.text, B.length);
		if (A) A->specificity = 0x100;
		else param->memory_error = true; }
detail(A) ::= COLON FUNCTION(B) ws RPAREN.
		{ A = css_new_selector(CSS_SELECTOR_PSEUDO, B.text, B.length);
		if (A) A->specificity = 0x100;
		else param->memory_error = true; }

declaration_list(A) ::= .
		{ A = 0; }
declaration_list(A) ::= declaration(B).
		{ A = B; }
declaration_list(A) ::= declaration_list(B) SEMI.
		{ A = B; }
declaration_list(A) ::= declaration(B) SEMI ws declaration_list(C).
		{ if (B) { B->next = C; A = B; } else { A = C; } }

declaration ::= DELIM property ws COLON ws value ws.
		/* ignore this as it has no meaning in CSS2 */

declaration(A) ::= property(B) ws COLON ws value(C) ws.
		{ if (C && (A = css_new_node(param->stylesheet,
		                CSS_NODE_DECLARATION,
				B.text, B.length))) {
			A->value = C;
		} else {
			param->memory_error = true;
			css_free_node(C);
			A = 0;
		} }
declaration(A) ::= any_list_1(B).  /* malformed declaration: ignore */
		{ A = 0; css_free_node(B); }

property(A) ::= IDENT(B).
		{ A = B; }

value(A) ::= any(B) ws.
		{ A = B; }
value(A) ::= any(B) ws value(C).
		{ if (B && C) { B->next = C; A = B; }
		else { css_free_node(B); css_free_node(C); A = 0; } }
value(A) ::= value(B) ws block.
		{ A = B; }
value(A) ::= value(B) ws ATKEYWORD ws.
		{ A = B; }


any_list(A) ::= .
		{ A = 0; }
any_list(A) ::= any(B) ws any_list(C).
		{ if (B) { B->next = C; A = B; }
		else { css_free_node(B); css_free_node(C); A = 0; } }
any_list_1(A) ::= any(B) ws any_list(C).
		{ if (B) { B->next = C; A = B; }
		else { css_free_node(B); css_free_node(C); A = 0; } }
any(A) ::= IDENT(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_IDENT,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= NUMBER(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_NUMBER,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= PERCENTAGE(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_PERCENTAGE,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= DIMENSION(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_DIMENSION,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= STRING(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_STRING,
		                   B.text + 1, B.length - 2);
		if (!A) param->memory_error = true; }
any(A) ::= DELIM(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_DELIM,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= URI(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_URI,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= HASH(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_HASH,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= UNICODE_RANGE(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_UNICODE_RANGE,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }
any(A) ::= INCLUDES.
		{ A = css_new_node(param->stylesheet, CSS_NODE_INCLUDES,
		                   0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= FUNCTION(B) ws any_list(C) RPAREN.
		{ if ((A = css_new_node(param->stylesheet, CSS_NODE_FUNCTION,
		                        B.text, B.length)))
			A->value = C;
		else {
			param->memory_error = true;
			css_free_node(C);
			A = 0;
		} }
any(A) ::= DASHMATCH.
		{ A = css_new_node(param->stylesheet, CSS_NODE_DASHMATCH,
		                   0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= PREFIX.
		{ A = css_new_node(param->stylesheet, CSS_NODE_PREFIX,
		                   0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= SUFFIX.
		{ A = css_new_node(param->stylesheet, CSS_NODE_SUFFIX,
		                   0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= SUBSTR.
		{ A = css_new_node(param->stylesheet, CSS_NODE_SUBSTR,
		                   0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= COLON.
		{ A = css_new_node(param->stylesheet, CSS_NODE_COLON, 0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= COMMA.
		{ A = css_new_node(param->stylesheet, CSS_NODE_COMMA, 0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= DOT.
		{ A = css_new_node(param->stylesheet, CSS_NODE_DOT, 0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= PLUS.
		{ A = css_new_node(param->stylesheet, CSS_NODE_PLUS, 0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= GT.
		{ A = css_new_node(param->stylesheet, CSS_NODE_GT, 0, 0);
		if (!A) param->memory_error = true; }
any(A) ::= LPAREN ws any_list(B) RPAREN.
		{ if ((A = css_new_node(param->stylesheet, CSS_NODE_PAREN,
		                        0, 0)))
			A->value = B;
		else {
			param->memory_error = true;
			css_free_node(B);
			A = 0;
		} }
any(A) ::= LBRAC ws any_list(B) RBRAC.
		{ if ((A = css_new_node(param->stylesheet, CSS_NODE_BRAC,
		                        0, 0)))
			A->value = B;
		else {
			param->memory_error = true;
			css_free_node(B);
			A = 0;
		} }
any(A) ::= ASTERISK(B).
		{ A = css_new_node(param->stylesheet, CSS_NODE_DELIM,
		                   B.text, B.length);
		if (!A) param->memory_error = true; }


/* lemon directives */

%extra_argument { struct css_parser_params *param }
%include {
#include <string.h>
#define CSS_INTERNALS
#include "netsurf/css/css.h"
#include "netsurf/utils/utils.h" }
%name css_parser_

%token_type { struct css_parser_token }

%type selector_list { struct css_selector * }
%type selector { struct css_selector * }
%type css_combinator { css_combinator }
%type simple_selector { struct css_selector * }
%type detail_list { struct css_selector * }
%type detail { struct css_selector * }
%type declaration_list { struct css_node * }
%type declaration { struct css_node * }
%type value { struct css_node * }
%type any_list { struct css_node * }
%type any_list_1 { struct css_node * }
%type any { struct css_node * }

%destructor selector_list { css_free_selector($$); }
%destructor selector { css_free_selector($$); }
%destructor simple_selector { css_free_selector($$); }
%destructor detail_list { css_free_selector($$); }
%destructor detail { css_free_selector($$); }
%destructor declaration_list { css_free_node($$); }
%destructor declaration { css_free_node($$); }
%destructor value { css_free_node($$); }
%destructor any_list { css_free_node($$); }
%destructor any_list_1 { css_free_node($$); }
%destructor any { css_free_node($$); }

%left COLON COMMA GT HASH LBRAC PLUS.
%left DOT.
%left IDENT.
%left LBRACE.

%syntax_error { param->syntax_error = true; }

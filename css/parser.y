/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

/*

CSS parser using the lemon parser generator

see CSS2 Specification, chapter 4
http://www.w3.org/TR/REC-CSS2/syndata.html,
and errata
http://www.w3.org/Style/css2-updates/REC-CSS2-19980512-errata

stylesheet  : [ CDO | CDC | S | statement ]*;
statement   : ruleset | at-rule;
at-rule     : ATKEYWORD S* any* [ block | ';' S* ];
block       : '{' S* [ any | block | ATKEYWORD S* | ';' ]* '}' S*;
ruleset     : selector? '{' S* declaration? [ ';' S* declaration? ]* '}' S*;
selector    : any+;
declaration : property ':' S* value;
property    : IDENT S*;
value       : [ any | block | ATKEYWORD S* ]+;
any         : [ IDENT | NUMBER | PERCENTAGE | DIMENSION | STRING
              | DELIM | URI | HASH | UNICODE-RANGE | INCLUDES
              | FUNCTION | DASHMATCH | '(' any* ')' | '[' any* ']' ] S*;

Note: S, CDO, CDC will be stripped out by the scanner
*/

stylesheet ::= statement_list.

statement_list ::= .
statement_list ::= statement_list statement.

statement ::= ruleset.
statement ::= at_rule.

at_rule ::= ATKEYWORD any_list block.
at_rule ::= ATKEYWORD(A) any_list(B) SEMI.
		{ if (strcasecmp(A, "@import") == 0)
			css_atimport(param->stylesheet, B);
		free(A); css_free_node(B); }

block ::= LBRACE block_body RBRACE.
block_body ::= .
block_body ::= block_body any.
block_body ::= block_body block.
block_body ::= block_body ATKEYWORD.
block_body ::= block_body SEMI.

ruleset ::= selector_list(A) LBRACE declaration_list(B) RBRACE.
		{ css_add_ruleset(param->stylesheet, A, B);
		css_free_node(B); }
ruleset ::= LBRACE declaration_list(A) RBRACE.
		/* this form of ruleset not used in CSS2
		   used to parse style attributes (ruleset_only = 1) */
		{ if (param->ruleset_only) param->declaration = A;
		  else css_free_node(A); }
ruleset ::= any_list_1(A) LBRACE declaration_list(B) RBRACE.
		{ css_free_node(A); css_free_node(B); } /* not CSS2 */

selector_list(A) ::= selector(B).
		{ A = B; }
selector_list(A) ::= selector_list(B) COMMA selector(C).
		{ C->next = B; A = C; }

selector(A) ::= simple_selector(B).
		{ A = B; }
selector(A) ::= selector(B) css_combinator(C) simple_selector(D).
		{ D->right = B; D->comb = C; A = D;
		A->specificity += B->specificity; }

css_combinator(A) ::= .
		{ A = CSS_COMB_ANCESTOR; }
css_combinator(A) ::= PLUS.
		{ A = CSS_COMB_PRECEDED; }
css_combinator(A) ::= GT.
		{ A = CSS_COMB_PARENT; }

simple_selector(A) ::= element_name(B) detail_list(C).
		{ A = css_new_node(CSS_NODE_SELECTOR, B, C, 0);
		A->specificity = 1 + C->specificity; }
simple_selector(A) ::= element_name(B).
		{ A = css_new_node(CSS_NODE_SELECTOR, B, 0, 0);
		A->specificity = 1; }
simple_selector(A) ::= detail_list(C).
		{ A = css_new_node(CSS_NODE_SELECTOR, 0, C, 0);
		A->specificity = C->specificity; }

element_name(A) ::= IDENT(B).
		{ A = B; }

detail_list(A) ::= detail(B).
		{ A = B; }
detail_list(A) ::= detail(B) detail_list(C).
		{ A = B; A->specificity += C->specificity; A->next = C; }

detail(A) ::= HASH(B).
		{ A = css_new_node(CSS_NODE_ID, B, 0, 0);
		A->specificity = 0x10000; }
detail(A) ::= DOT IDENT(B).
		{ A = css_new_node(CSS_NODE_CLASS, B, 0, 0);
		A->specificity = 0x100; }
detail(A) ::= LBRAC IDENT(B) RBRAC.
		{ A = css_new_node(CSS_NODE_ATTRIB, B, 0, 0);
		A->specificity = 0x100; }
detail(A) ::= LBRAC IDENT(B) EQUALS IDENT(C) RBRAC.
		{ A = css_new_node(CSS_NODE_ATTRIB_EQ, B, 0, 0); A->data2 = C;
		A->specificity = 0x100; }
detail(A) ::= LBRAC IDENT(B) EQUALS STRING(C) RBRAC.
		{ A = css_new_node(CSS_NODE_ATTRIB_EQ, B, 0, 0); A->data2 = css_unquote(C);
		A->specificity = 0x100; }
detail(A) ::= LBRAC IDENT(B) INCLUDES IDENT(C) RBRAC.
		{ A = css_new_node(CSS_NODE_ATTRIB_INC, B, 0, 0); A->data2 = C;
		A->specificity = 0x100; }
detail(A) ::= LBRAC IDENT(B) INCLUDES STRING(C) RBRAC.
		{ A = css_new_node(CSS_NODE_ATTRIB_INC, B, 0, 0); A->data2 = css_unquote(C);
		A->specificity = 0x100; }
detail(A) ::= LBRAC IDENT(B) DASHMATCH IDENT(C) RBRAC.
		{ A = css_new_node(CSS_NODE_ATTRIB_DM, B, 0, 0); A->data2 = C;
		A->specificity = 0x100; }
detail(A) ::= LBRAC IDENT(B) DASHMATCH STRING(C) RBRAC.
		{ A = css_new_node(CSS_NODE_ATTRIB_DM, B, 0, 0); A->data2 = css_unquote(C);
		A->specificity = 0x100; }
detail(A) ::= COLON IDENT(B).
		{ if (strcasecmp(B, "link") == 0) {
			A = css_new_node(CSS_NODE_ATTRIB, xstrdup("href"), 0, 0);
			A->specificity = 0x100;
			free(B);
		} else {
			A = css_new_node(CSS_NODE_PSEUDO, B, 0, 0);
			A->specificity = 0x100;
		} }
detail(A) ::= COLON FUNCTION(B) IDENT RPAREN.
		{ A = css_new_node(CSS_NODE_PSEUDO, B, 0, 0);
		A->specificity = 0x100; }

declaration_list(A) ::= .
		{ A = 0; }
declaration_list(A) ::= declaration(B).
		{ A = B; }
declaration_list(A) ::= declaration_list(B) SEMI.
		{ A = B; }
declaration_list(A) ::= declaration(B) SEMI declaration_list(C).
		{ if (B) { B->next = C; A = B; } else { A = C; } }

declaration(A) ::= property(B) COLON value(C).
		{ A = css_new_node(CSS_NODE_DECLARATION, B, C, 0); }
declaration(A) ::= any_list_1(B).  /* malformed declaration: ignore */
		{ A = 0; css_free_node(B); }

property(A) ::= IDENT(B).
		{ A = B; }

value(A) ::= any(B).
		{ A = B; }
value(A) ::= any(B) value(C).
		{ B->next = C; A = B; }
value(A) ::= value(B) block.
		{ A = B; }
value(A) ::= value(B) ATKEYWORD.
		{ A = B; }


any_list(A) ::= .
		{ A = 0; }
any_list(A) ::= any(B) any_list(C).
		{ B->next = C; A = B; }
any_list_1(A) ::= any(B) any_list(C).
		{ B->next = C; A = B; }
any(A) ::= IDENT(B).
		{ A = css_new_node(CSS_NODE_IDENT, B, 0, 0); }
any(A) ::= NUMBER(B).
		{ A = css_new_node(CSS_NODE_NUMBER, B, 0, 0); }
any(A) ::= PERCENTAGE(B).
		{ A = css_new_node(CSS_NODE_PERCENTAGE, B, 0, 0); }
any(A) ::= DIMENSION(B).
		{ A = css_new_node(CSS_NODE_DIMENSION, B, 0, 0); }
any(A) ::= STRING(B).
		{ A = css_new_node(CSS_NODE_STRING, css_unquote(B), 0, 0); }
any(A) ::= DELIM(B).
		{ A = css_new_node(CSS_NODE_DELIM, B, 0, 0); }
any(A) ::= URI(B).
		{ A = css_new_node(CSS_NODE_URI, B, 0, 0); }
any(A) ::= HASH(B).
		{ A = css_new_node(CSS_NODE_HASH, B, 0, 0); }
any(A) ::= UNICODE_RANGE(B).
		{ A = css_new_node(CSS_NODE_UNICODE_RANGE, B, 0, 0); }
any(A) ::= INCLUDES.
		{ A = css_new_node(CSS_NODE_INCLUDES, 0, 0, 0); }
any(A) ::= FUNCTION(B).
		{ A = css_new_node(CSS_NODE_FUNCTION, B, 0, 0); }
any(A) ::= DASHMATCH.
		{ A = css_new_node(CSS_NODE_DASHMATCH, 0, 0, 0); }
any(A) ::= COLON.
		{ A = css_new_node(CSS_NODE_COLON, 0, 0, 0); }
any(A) ::= COMMA.
		{ A = css_new_node(CSS_NODE_COMMA, 0, 0, 0); }
any(A) ::= DOT.
		{ A = css_new_node(CSS_NODE_DOT, 0, 0, 0); }
any(A) ::= PLUS.
		{ A = css_new_node(CSS_NODE_PLUS, 0, 0, 0); }
any(A) ::= GT.
		{ A = css_new_node(CSS_NODE_GT, 0, 0, 0); }
any(A) ::= LPAREN any_list(B) RPAREN.
		{ A = css_new_node(CSS_NODE_PAREN, 0, B, 0); }
any(A) ::= LBRAC any_list(B) RBRAC.
		{ A = css_new_node(CSS_NODE_BRAC, 0, B, 0); }


/* lemon directives */

%extra_argument { struct parse_params *param }
%include {
#define CSS_INTERNALS
#include "netsurf/css/scanner.h"
#include "netsurf/css/css.h"
#include "netsurf/utils/utils.h" }
%name css_parser_

%token_type { char* }
%token_destructor { xfree($$); }

%type selector_list { struct css_node * }
%type selector { struct css_node * }
%type css_combinator { css_combinator }
%type simple_selector { struct css_node * }
%type detail_list { struct css_node * }
%type detail { struct css_node * }
%type declaration_list { struct css_node * }
%type declaration { struct css_node * }
%type value { struct css_node * }
%type any_list { struct css_node * }
%type any_list_1 { struct css_node * }
%type any { struct css_node * }

%destructor selector_list { css_free_node($$); }
%destructor selector { css_free_node($$); }
%destructor simple_selector { css_free_node($$); }
%destructor detail_list { css_free_node($$); }
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

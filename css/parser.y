/**
 * $Id: parser.y,v 1.7 2003/04/13 12:50:10 bursa Exp $
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
/*ruleset ::= any_list_1(A) LBRACE declaration_list(B) RBRACE.
		{ css_free_node(A); css_free_node(B); } /* not CSS2 */
ruleset ::= LBRACE declaration_list(A) RBRACE.
		/* this form of ruleset not used in CSS2
		   used to parse style attributes (ruleset_only = 1) */
		{ if (param->ruleset_only) param->declaration = A;
		  else css_free_node(A); }

selector_list(A) ::= selector(B).
		{ A = B; }
selector_list(A) ::= selector_list(B) COMMA selector(C).
		{ C->next = B; A = C; }

selector(A) ::= simple_selector(B).
		{ A = B; }
selector(A) ::= selector(B) combinator(C) simple_selector(D).
		{ D->right = B; D->comb = C; A = D; }

combinator(A) ::= .
		{ A = COMB_ANCESTOR; }
combinator(A) ::= PLUS.
		{ A = COMB_PRECEDED; }
combinator(A) ::= GT.
		{ A = COMB_PARENT; }

simple_selector(A) ::= element_name(B) detail_list(C).
		{ A = css_new_node(NODE_SELECTOR, B, C, 0); }

element_name(A) ::= .
		{ A = 0; }
element_name(A) ::= IDENT(B).
		{ A = B; }

detail_list(A) ::= .
		{ A = 0; }
detail_list(A) ::= HASH(B) detail_list(C).
		{ A = css_new_node(NODE_ID, B, 0, 0); A->next = C; }
detail_list(A) ::= DOT IDENT(B) detail_list(C).
		{ A = css_new_node(NODE_CLASS, B, 0, 0); A->next = C; }
/* TODO: attrib, pseudo */

declaration_list(A) ::= .
		{ A = 0; }
declaration_list(A) ::= declaration(B).
		{ A = B; }
declaration_list(A) ::= declaration_list(B) SEMI.
		{ A = B; }
declaration_list(A) ::= declaration(B) SEMI declaration_list(C).
		{ B->next = C; A = B; }

declaration(A) ::= property(B) COLON value(C).
		{ A = css_new_node(NODE_DECLARATION, B, C, 0); }

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
/*any_list_1(A) ::= any(B) any_list(C).
		{ B->next = C; A = B; }*/
any(A) ::= IDENT(B).
		{ A = css_new_node(NODE_IDENT, B, 0, 0); }
any(A) ::= NUMBER(B).
		{ A = css_new_node(NODE_NUMBER, B, 0, 0); }
any(A) ::= PERCENTAGE(B).
		{ A = css_new_node(NODE_PERCENTAGE, B, 0, 0); }
any(A) ::= DIMENSION(B).
		{ A = css_new_node(NODE_DIMENSION, B, 0, 0); }
any(A) ::= STRING(B).
		{ A = css_new_node(NODE_STRING, B, 0, 0); }
any(A) ::= DELIM(B).
		{ A = css_new_node(NODE_DELIM, B, 0, 0); }
any(A) ::= URI(B).
		{ A = css_new_node(NODE_URI, B, 0, 0); }
any(A) ::= HASH(B).
		{ A = css_new_node(NODE_HASH, B, 0, 0); }
any(A) ::= UNICODE_RANGE(B).
		{ A = css_new_node(NODE_UNICODE_RANGE, B, 0, 0); }
any(A) ::= INCLUDES.
		{ A = css_new_node(NODE_INCLUDES, 0, 0, 0); }
any(A) ::= FUNCTION(B).
		{ A = css_new_node(NODE_FUNCTION, B, 0, 0); }
any(A) ::= DASHMATCH.
		{ A = css_new_node(NODE_DASHMATCH, 0, 0, 0); }
any(A) ::= COLON.
		{ A = css_new_node(NODE_COLON, 0, 0, 0); }
any(A) ::= COMMA.
		{ A = css_new_node(NODE_COMMA, 0, 0, 0); }
any(A) ::= PLUS.
		{ A = css_new_node(NODE_PLUS, 0, 0, 0); }
any(A) ::= GT.
		{ A = css_new_node(NODE_GT, 0, 0, 0); }
any(A) ::= LPAREN any_list(B) RPAREN.
		{ A = css_new_node(NODE_PAREN, 0, B, 0); }
any(A) ::= LBRAC any_list(B) RBRAC.
		{ A = css_new_node(NODE_BRAC, 0, B, 0); }


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

%type selector_list { struct node * }
%type selector { struct node * }
%type combinator { combinator }
%type simple_selector { struct node * }
%type detail_list { struct node * }
%type declaration_list { struct node * }
%type declaration { struct node * }
%type value { struct node * }
%type any_list { struct node * }
%type any_list_1 { struct node * }
%type any { struct node * }

%destructor selector_list { css_free_node($$); }
%destructor selector { css_free_node($$); }
%destructor simple_selector { css_free_node($$); }
%destructor declaration_list { css_free_node($$); }
%destructor declaration { css_free_node($$); }
%destructor value { css_free_node($$); }
%destructor any_list { css_free_node($$); }
%destructor any_list_1 { css_free_node($$); }
%destructor any { css_free_node($$); }



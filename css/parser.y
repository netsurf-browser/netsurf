/**
 * $Id: parser.y,v 1.1 2003/03/25 21:03:14 bursa Exp $
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

stylesheet ::= statement_list(A).
		{ *stylesheet = A; }

statement_list(A) ::= .
		{ A = 0; }
statement_list(A) ::= statement(B) statement_list(C).
		{ if (B == 0) { A = C; } else { B->next = C; A = B; } }

statement(A) ::= ruleset(B).
		{ A = B; }
statement(A) ::= at_rule(B).
		{ A = B; }

at_rule(A) ::= ATKEYWORD any_list block.
		{ A = css_new_node(CSS_NODE_AT_RULE, 0, 0, 0); }
at_rule(A) ::= ATKEYWORD any_list SEMI.
		{ A = css_new_node(CSS_NODE_AT_RULE, 0, 0, 0); }

block ::= LBRACE block_body RBRACE.
block_body ::= .
block_body ::= block_body any.
block_body ::= block_body block.
block_body ::= block_body ATKEYWORD.
block_body ::= block_body SEMI.

ruleset(A) ::= selector(B) LBRACE declaration_list(C) RBRACE.
		{ A = css_new_node(CSS_NODE_RULESET, 0, B, C); }
ruleset(A) ::= LBRACE declaration_list RBRACE.
		{ A = 0; } /* this form of ruleset not used in CSS2 */

selector(A) ::= any_list_1(B).
		{ A = B; }

declaration_list(A) ::= .
		{ A = 0; }
declaration_list(A) ::= declaration(B).
		{ A = B; }
declaration_list(A) ::= declaration_list(B) SEMI.
		{ A = B; }
declaration_list(A) ::= declaration(B) SEMI declaration_list(C).
		{ B->next = C; A = B; }

declaration(A) ::= property(B) COLON value(C).
		{ A = css_new_node(CSS_NODE_DECLARATION, B, C, 0); }

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
		{ A = css_new_node(CSS_NODE_STRING, B, 0, 0); }
any(A) ::= DELIM(B).
		{ A = css_new_node(CSS_NODE_DELIM, B, 0, 0); }
any(A) ::= URI(B).
		{ A = css_new_node(CSS_NODE_URI, B, 0, 0); }
any(A) ::= HASH(B).
		{ A = css_new_node(CSS_NODE_HASH, B, 0, 0); }
any(A) ::= UNICODE_RANGE(B).
		{ A = css_new_node(CSS_NODE_UNICODE_RANGE, B, 0, 0); }
any(A) ::= INCLUDES(B).
		{ A = css_new_node(CSS_NODE_INCLUDES, B, 0, 0); }
any(A) ::= FUNCTION(B).
		{ A = css_new_node(CSS_NODE_FUNCTION, B, 0, 0); }
any(A) ::= DASHMATCH(B).
		{ A = css_new_node(CSS_NODE_DASHMATCH, B, 0, 0); }
any(A) ::= COLON(B).
		{ A = css_new_node(CSS_NODE_COLON, B, 0, 0); }
any(A) ::= LPAREN any_list(B) RPAREN.
		{ A = css_new_node(CSS_NODE_PAREN, 0, B, 0); }
any(A) ::= LBRAC any_list(B) RBRAC.
		{ A = css_new_node(CSS_NODE_BRAC, 0, B, 0); }


/* lemon directives */

%extra_argument { struct css_node **stylesheet }
%include { #include "netsurf/css/css.h"
#include "netsurf/utils/utils.h" }
%name css_parser_

%token_type { char* }
%token_destructor { xfree($$); }

%type stylesheet { struct css_node * }
%type statement_list { struct css_node * }
%type statement { struct css_node * }
%type at_rule { struct css_node * }
%type ruleset { struct css_node * }
%type selector { struct css_node * }
%type declaration_list { struct css_node * }
%type declaration { struct css_node * }
%type value { struct css_node * }
%type any_list { struct css_node * }
%type any_list_1 { struct css_node * }
%type any { struct css_node * }

%destructor stylesheet { css_free_node($$); }
%destructor statement_list { css_free_node($$); }
%destructor statement { css_free_node($$); }
%destructor at_rule { css_free_node($$); }
%destructor ruleset { css_free_node($$); }
%destructor selector { css_free_node($$); }
%destructor declaration_list { css_free_node($$); }
%destructor declaration { css_free_node($$); }
%destructor value { css_free_node($$); }
%destructor any_list { css_free_node($$); }
%destructor any_list_1 { css_free_node($$); }
%destructor any { css_free_node($$); }

%parse_failure { *stylesheet = 0; }


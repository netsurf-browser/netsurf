/*
 * This file is part of NetSurf, http://netsurf.sourceforge.net/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2003 James Bursa <bursa@users.sourceforge.net>
 */

%{
#include "parser.h"
%}

%option 8bit
%option batch
%option case-insensitive
%option header-file="scanner.h"
%option outfile="scanner.c"
%option prefix="css_"
%option reentrant
%option never-interactive
%option noyywrap
%option yylineno

/* see CSS2 Specification, chapter 4
   http://www.w3.org/TR/REC-CSS2/syndata.html,
   and errata
   http://www.w3.org/Style/css2-updates/REC-CSS2-19980512-errata */

ident		{nmstart}{nmchar}*
name		{nmchar}+
nmstart		[a-zA-Z_]|{nonascii}|{escape}
nonascii	[\200-\377]
unicode		\\[0-9a-f]{1,6}[ \n\r\t\f]?
escape		{unicode}|\\[ -~\200-\377]
nmchar		[-a-zA-Z0-9_]|{nonascii}|{escape}
num		[+-]?[0-9]+|[0-9]*\.[0-9]+
string		{string1}|{string2}
string1		\"([\t !#$%&(-~]|\\{nl}|\'|{nonascii}|{escape})*\"
string2		\'([\t !#$%&(-~]|\\{nl}|\"|{nonascii}|{escape})*\'
nl		\n|\r\n|\r|\f
w		[ \t\r\n\f]*

%%

{ident}		{ return IDENT; }
@{ident}	{ return ATKEYWORD; }
{string}	{ return STRING; }
#{name}		{ return HASH; }
{num}		{ return NUMBER; }
{num}%		{ return PERCENTAGE; }
{num}{ident}	{ return DIMENSION; }
url\({w}{string}{w}\)|url\({w}([!#$%&*-~]|{nonascii}|{escape})*{w}\)	{
		  return URI; }
U\+[0-9A-F?]{1,6}(-[0-9A-F]{1,6})?	{
		  return UNICODE_RANGE; }
"<!--"		/* ignore CDO */
"-->"		/* ignore CDC */
;		{ return SEMI; }
\{		{ return LBRACE; }
\}		{ return RBRACE; }
\(		{ return LPAREN; }
\)		{ return RPAREN; }
\[		{ return LBRAC; }
\]		{ return RBRAC; }
[ \t\r\n\f]+	/* ignore whitespace */
\/\*[^*]*\*+([^/][^*]*\*+)*\/	/* ignore comments */
{ident}\(	{ return FUNCTION; }
=		{ return EQUALS; }
~=		{ return INCLUDES; }
"|="		{ return DASHMATCH; }
:		{ return COLON; }
,		{ return COMMA; }
"+"		{ return PLUS; }
>		{ return GT; }
"."		{ return DOT; }
.		{ return DELIM; }

%%


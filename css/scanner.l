/*
 * This file is part of NetSurf, http://netsurf-browser.org/
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2004 James Bursa <bursa@users.sourceforge.net>
 */

/** \file
 * CSS tokeniser using re2c.
 *
 * see CSS2 Specification, chapter 4
 * http://www.w3.org/TR/REC-CSS2/syndata.html,
 * and errata
 * http://www.w3.org/Style/css2-updates/REC-CSS2-19980512-errata
 */

#include <stdbool.h>
#define CSS_INTERNALS
#include "css/css.h"
#include "css/parser.h"

#define YYCTYPE unsigned char
#define YYCURSOR (*buffer)
#define YYLIMIT end
#define YYMARKER marker
#define YYFILL(n) { return 0; }


/**
 * Identify a CSS source token.
 *
 * \param  buffer      source to tokenise, updated to new position
 * \param  end         end of source
 * \param  token_text  updated to start of recognized token
 * \return  token number
 */

int css_tokenise(unsigned char **buffer, unsigned char *end,
		unsigned char **token_text)
{
	unsigned char *marker;

start:
	*token_text = YYCURSOR;

/*!re2c
nonascii	=	[\200-\377];
unicode		=	"\\" [0-9a-f]+ ("\r\n" | [ \n\r\t\f])?;
escape		=	unicode | "\\" [ -~\200-\377];
nmchar		=	[-a-zA-Z0-9_] | nonascii | escape;
nmstart		=	[a-zA-Z_] | nonascii | escape;
ident		=	[-]? nmstart nmchar*;
name		=	nmchar+;
num		=	[+-]? ([0-9]+ | [0-9]* "." [0-9]+);
nl		=	"\n" | "\r\n" | "\r" | "\f";
string1		=	"\"" ([\t !#$%&(-~] | "\\" nl | "'" | nonascii | escape)* "\"";
string2		=	"'"  ([\t !#$%&(-~] | "\\" nl | "\""| nonascii | escape)* "'";
string		=	string1 | string2;
s		=	[ \t\r\n\f];
w		=	s*;
any		=	[\000-\377];

ident		{ return IDENT; }
"@" ident	{ return ATKEYWORD; }
string		{ return STRING; }
"#" name	{ return HASH; }

num		{ return NUMBER; }
num "%"		{ return PERCENTAGE; }
num ident	{ return DIMENSION; }

"url(" w string w ")" | "url(" w ([!#$%&*-~]|nonascii|escape)* w ")"
		{ return URI; }
"U+" [0-9A-F?]+ ("-" [0-9A-F]+ )?
		{ return UNICODE_RANGE; }

"<!--"		{ goto start; /* ignore CDO */ }
"-->"		{ goto start; /* ignore CDC */ }

";"		{ return SEMI; }
"{"		{ return LBRACE; }
"}"		{ return RBRACE; }
"("		{ return LPAREN; }
")"		{ return RPAREN; }
"["		{ return LBRAC; }
"]"		{ return RBRAC; }

s+		{ return S; }

"/*" (any\[*])* "*"+ ((any\[/*]) (any\[*])* "*"+)* "/"
		{ goto start; /* ignore comments */ }

ident "("	{ return FUNCTION; }

"~="		{ return INCLUDES; }
"|="		{ return DASHMATCH; }
"^="		{ return PREFIX; }
"$="		{ return SUFFIX; }
"*="		{ return SUBSTR; }

"="		{ return EQUALS; }
":"		{ return COLON; }
","		{ return COMMA; }
"+"		{ return PLUS; }
">"		{ return GT; }
"."		{ return DOT; }
"*"		{ return ASTERISK; }

any		{ return DELIM; }
*/

}

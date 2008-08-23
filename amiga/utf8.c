/*
 * Copyright 2008 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/types.h>
#include "utils/utf8.h"
#include <proto/codesets.h>
#include <proto/exec.h>
#include <parserutils/charset/mibenum.h>

utf8_convert_ret utf8_to_local_encoding(const char *string, size_t len,
	char **result)
{
/*
	struct codeset *cs = CodesetsFind("ISO-8859-1",
							CSA_FallbackToDefault,FALSE,
							TAG_DONE);
*/

//	if(!len) return UTF8_CONVERT_OK;

	*result = CodesetsUTF8ToStr(CSA_Source,string,
						CSA_SourceLen,len,
//						CSA_MapForeignChars,TRUE,
//						CSA_DestCodeset,cs,
						TAG_DONE);

	return UTF8_CONVERT_OK;
}

ULONG ami_utf8_to_any(const char *string, size_t len, char **result)
{
	uint16 mibenum = 0;

	struct codeset *cs = CodesetsFindBest(CSA_Source,string,
							CSA_SourceLen,len,
							CSA_FallbackToDefault,TRUE,
							TAG_DONE);

	*result = CodesetsUTF8ToStr(CSA_Source,string,
						CSA_SourceLen,len,
//						CSA_MapForeignChars,TRUE,
						CSA_DestCodeset,cs,
						TAG_DONE);

	mibenum = parserutils_charset_mibenum_from_name(cs->name,strlen(cs->name));

	printf("%ld\n",mibenum);

	return mibenum; // mibenum
}

/*
char *ami_utf8_alloc(char *string)
{
	return (AllocVec(CodesetsUTF8Len(string)+1,MEMF_CLEAR));
}
*/

void ami_utf8_free(char *ptr)
{
	if(ptr) CodesetsFreeA(ptr,NULL);
}

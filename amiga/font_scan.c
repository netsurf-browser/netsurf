/*
 * Copyright 2012 Chris Young <chris@unsatisfactorysoftware.co.uk>
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

/* scan fonts
* gcc -o scan_font font_scan.c -lwapcaplet -lauto -D__USE_INLINE__
*/

#include <stdio.h>
#include <string.h>

#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>

#include <libwapcaplet/libwapcaplet.h>

ULONG scan_font(const char *fontname, lwc_string **glypharray)
{
	struct OutlineFont *ofont;
	struct MinList *widthlist;
	struct GlyphWidthEntry *gwnode;
	ULONG foundglyphs = 0;
	ULONG serif = 0;

	ofont = OpenOutlineFont(fontname, NULL, OFF_OPEN);

	if(!ofont) return;

	if(ESetInfo(&ofont->olf_EEngine,
		OT_PointHeight, 10 * (1 << 16),
		OT_GlyphCode, 0x0000,
		OT_GlyphCode2, 0xffff,
		TAG_END) == OTERR_Success)
	{
		if(EObtainInfo(&ofont->olf_EEngine,
			OT_WidthList, &widthlist, TAG_END) == 0)
		{
			gwnode = (struct GlyphWidthEntry *)GetHead((struct List *)widthlist);
			do {
				if(gwnode && (glypharray[gwnode->gwe_Code] == NULL)) {
					 lwc_intern_string(fontname, strlen(fontname), &glypharray[gwnode->gwe_Code]);
					//printf("%lx\n",gwnode->gwe_Code);
					foundglyphs++;
				}
			} while(gwnode = (struct GlyphWidthEntry *)GetSucc((struct Node *)gwnode));
		}
	}

	CloseOutlineFont(ofont, NULL);

	return foundglyphs;
}

ULONG scan_fonts(lwc_string **glypharray)
{
	int afShortage, afSize = 100, i;
	struct AvailFontsHeader *afh;
	struct AvailFonts *af;
	ULONG found, total = 0;

	printf("Scanning fonts...\n");
	do {
		if(afh = (struct AvailFontsHeader *)AllocVec(afSize, MEMF_PRIVATE)) {
			if(afShortage = AvailFonts(afh, afSize, AFF_DISK | AFF_OTAG | AFF_SCALED)) {
				FreeVec(afh);
				afSize += afShortage;
			}
		} else {
			/* out of memory, bail out */
			return 0;
		}
	} while (afShortage);

	if(afh) {
		af = (struct AvailFonts *)&(afh[1]);
printf("af = %lx entries = %ld\n", af, afh->afh_NumEntries);
/* bug somewhere, this only does 36 fonts as size 0 */
		for(i = 0; i < afh->afh_NumEntries; i++) {
			if((af[i].af_Attr.ta_YSize == 0) && (af[i].af_Attr.ta_Style == FS_NORMAL)) {
				printf("%s (%ld) %ld\n", af[i].af_Attr.ta_Name, af[i].af_Attr.ta_Style, af[i].af_Attr.ta_YSize);
				found = scan_font(af[i].af_Attr.ta_Name, glypharray);
				total += found;
				printf("Found %ld new glyphs (total = %ld)\n", found, total);
			}
			af++;
		}
		FreeVec(afh);
	} else {
		return 0;
	}
}

int main(int argc, char** argv)
{
	lwc_string *glypharray[0xffff + 1];
	ULONG i, found = 0;
	BPTR fh;

	if(argc < 2) return 5;

	printf("%s\n",argv[1]);

	/* Ensure array zeroed */
	for(i=0x0000; i<=0xffff; i++)
		glypharray[i] = NULL;

	scan_fonts(glypharray);

	if(fh = FOpen(argv[1], ACCESS_WRITE, 0)) {
		printf("Writing %s\n", argv[1]);
		for(i=0x0000; i<=0xffff; i++)
		{
			if(glypharray[i]) {
				FPrintf(fh, "%04lx \"%s\"\n", i, lwc_string_data(glypharray[i]));
				lwc_string_unref(glypharray[i]);
				found++;
			}
		}
		FClose(fh);
	}

	printf("Found %ld glyphs\n", found);

	return 0;
}

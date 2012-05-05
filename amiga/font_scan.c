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

/** \file
 * Font glyph scanner for Unicode substitutions.
*/

#include <stdio.h>
#include <string.h>

#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>

#include <libwapcaplet/libwapcaplet.h>

/**
 * Scan a font for glyphs not present in glypharray.
 *
 * \param  fontname       font to scan
 * \param  glypharray     an array of 0xffff lwc_string pointers
 * \return number of new glyphs found
 */
ULONG ami_font_scan_font(const char *fontname, lwc_string **glypharray)
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
					 lwc_intern_string(fontname, strlen(fontname) - 5, &glypharray[gwnode->gwe_Code]);
					//printf("%lx\n",gwnode->gwe_Code);
					foundglyphs++;
				}
			} while(gwnode = (struct GlyphWidthEntry *)GetSucc((struct Node *)gwnode));
		}
	}

	CloseOutlineFont(ofont, NULL);

	return foundglyphs;
}

/**
 * Scan all fonts for glyphs.
 *
 * \param  glypharray     an array of 0xffff lwc_string pointers
 * \return number of glyphs found
 */
ULONG ami_font_scan_fonts(lwc_string **glypharray)
{
/* TODO: this function should take a list of fonts and add to it, ignoring duplicates */
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
				found = ami_font_scan_font(af[i].af_Attr.ta_Name, glypharray);
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

/**
 * Load a font glyph cache
 *
 * \param  filename       name of cache file to load
 * \param  glypharray     an array of 0xffff lwc_string pointers
 * \return number of glyphs loaded
 */
ULONG ami_font_scan_load(const char *filename, lwc_string **glypharray)
{
	ULONG found = 0;
	BPTR fh = 0;

	if(fh = FOpen(filename, MODE_OLDFILE, 0)) {
		printf("Reading %s\n", filename);
		/* TODO: read lines using ReadArgs() */
		FClose(fh);
	}

	return found;
}

/**
 * Save a font glyph cache
 *
 * \param  filename       name of cache file to save
 * \param  glypharray     an array of 0xffff lwc_string pointers
 */
void ami_font_scan_save(const char *filename, lwc_string **glypharray)
{
	ULONG i;
	BPTR fh = 0;

	if(fh = FOpen(filename, MODE_NEWFILE, 0)) {
		printf("Writing %s\n", filename);
		FPrintf(fh, "; This file is auto-generated. To recreate the cache, delete this file.\n");
		FPrintf(fh, "; This file is parsed using ReadArgs() with the following template:\n");
		FPrintf(fh, "; CODE/A,FONT/A\n;\n");

		for(i=0x0000; i<=0xffff; i++)
		{
			if(glypharray[i]) {
				FPrintf(fh, "%04lx \"%s\"\n", i, lwc_string_data(glypharray[i]));
				lwc_string_unref(glypharray[i]);
			}
		}
		FClose(fh);
	}
}

/**
 * Finalise the font glyph cache.
 *
 * \param  glypharray     an array of 0xffff lwc_string pointers to free
 */
void ami_font_scan_fini(lwc_string **glypharray)
{
	ULONG i;

	for(i=0x0000; i<=0xffff; i++)
	{
		if(glypharray[i]) {
			lwc_string_unref(glypharray[i]);
			glypharray[i] = NULL;
		}
	}
}

/**
 * Initialise the font glyph cache.
 * Reads an existing file or, if not present, generates a new cache.
 *
 * \param  filename       cache file to attempt to read
 * \param  glypharray     an array of 0xffff lwc_string pointers
 */
void ami_font_scan_init(const char *filename, lwc_string **glypharray)
{
	ULONG i, found;

	/* Ensure array zeroed */
	for(i=0x0000; i<=0xffff; i++)
		glypharray[i] = NULL;

	found = ami_font_scan_load(filename, glypharray);

	if(found == 0) {
		found = ami_font_scan_fonts(glypharray);
		ami_font_scan_save(filename, glypharray);
	}

	printf("Initialised with %ld glyphs\n", found);
}

#ifdef AMI_FONT_SCAN_STANDALONE
/* This can be compiled as standalone using:
* gcc -o font_scan font_scan.c -lwapcaplet -lauto -D__USE_INLINE__ -DAMI_FONT_SCAN_STANDALONE
*/
int main(int argc, char** argv)
{
	lwc_string *glypharray[0xffff + 1];
	ULONG found = 0;
	BPTR fh;

	if(argc < 2) return 5;

	printf("%s\n",argv[1]);

	ami_font_scan_init(argv[1], glypharray);
	ami_font_scan_fini(glypharray);

	return 0;
}
#endif

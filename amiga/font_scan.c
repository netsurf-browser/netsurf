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
#include <stdlib.h>
#include <string.h>

#include <proto/diskfont.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <diskfont/diskfonttag.h>
#include <diskfont/oterrors.h>

#include "amiga/font_scan.h"
#include "amiga/object.h"

/**
 * Lookup a font that contains a UTF-16 codepoint
 *
 * \param  code           UTF-16 codepoint to lookup
 * \param  glypharray     an array of 0xffff lwc_string pointers
 * \return font name or NULL
 */
const char *ami_font_scan_lookup(uint16 code, lwc_string **glypharray)
{
	if(glypharray[code] == NULL) return NULL;
		else return lwc_string_data(glypharray[code]);
}

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
	lwc_error lerror;

	ofont = OpenOutlineFont(fontname, NULL, OFF_OPEN);

	if(!ofont) return 0;

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
					lerror = lwc_intern_string(fontname, strlen(fontname) - 5, &glypharray[gwnode->gwe_Code]);
					if(lerror != lwc_error_ok) continue;
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
ULONG ami_font_scan_fonts(struct MinList *list, lwc_string **glypharray)
{
	ULONG found, total = 0;
	struct nsObject *node;
	struct nsObject *nnode;

	if(IsMinListEmpty(list)) return 0;

	node = (struct nsObject *)GetHead((struct List *)list);

	do {
		nnode = (struct nsObject *)GetSucc((struct Node *)node);
		printf("Scanning %s...\n", node->dtz_Node.ln_Name);
		found = ami_font_scan_font(node->dtz_Node.ln_Name, glypharray);
		total += found;
		printf("Found %ld new glyphs (total = %ld)\n", found, total);
	} while(node = nnode);

	return total;
}


/**
 * Add OS fonts to a list.
 *
 * \param  list   list to add font names to
 * \return number of fonts found
 */
ULONG ami_font_scan_list(struct MinList *list)
{
	int afShortage, afSize = 100, i;
	struct AvailFontsHeader *afh;
	struct AvailFonts *af;
	ULONG found;
	struct nsObject *node;

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

		for(i = 0; i < afh->afh_NumEntries; i++) {
			if(af[i].af_Attr.ta_Style == FS_NORMAL) {
				node = (struct nsObject *)FindIName((struct List *)list,
							af[i].af_Attr.ta_Name);
				if(node == NULL) {
					node = AddObject(list, AMINS_UNKNOWN);
					if(node) {
						node->dtz_Node.ln_Name = strdup(af[i].af_Attr.ta_Name);
						found++;
						printf("Added %s\n", af[i].af_Attr.ta_Name);
					}
				}
			}
			af++;
		}
		FreeVec(afh);
	} else {
		return 0;
	}
	return found;
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
	lwc_error lerror;
	char buffer[256];
	struct RDArgs *rargs = NULL;
	STRPTR template = "CODE/A,FONT/A";
	long rarray[] = {0,0};

	enum {
		A_CODE = 0,
		A_FONT
	};

	rargs = AllocDosObjectTags(DOS_RDARGS, TAG_DONE);

	if(fh = FOpen(filename, MODE_OLDFILE, 0)) {
		printf("Reading %s\n", filename);

		while(FGets(fh, (UBYTE *)&buffer, 256) != 0)
		{
			rargs->RDA_Source.CS_Buffer = (char *)&buffer;
			rargs->RDA_Source.CS_Length = 256;
			rargs->RDA_Source.CS_CurChr = 0;

			rargs->RDA_DAList = NULL;
			rargs->RDA_Buffer = NULL;
			rargs->RDA_BufSiz = 0;
			rargs->RDA_ExtHelp = NULL;
			rargs->RDA_Flags = 0;

			if(ReadArgs(template, rarray, rargs))
			{
				lerror = lwc_intern_string((const char *)rarray[A_FONT],
							strlen((const char *)rarray[A_FONT]),
							&glypharray[strtoul((const char *)rarray[A_CODE], NULL, 0)]);
				if(lerror != lwc_error_ok) continue;
				found++;
			}
		}
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
		FPrintf(fh, "; This file is auto-generated. To re-create the cache, delete this file.\n");
		FPrintf(fh, "; This file is parsed using ReadArgs() with the following template:\n");
		FPrintf(fh, "; CODE/A,FONT/A\n;\n");

		for(i=0x0000; i<=0xffff; i++)
		{
			if(glypharray[i]) {
				FPrintf(fh, "0x%04lx \"%s\"\n", i, lwc_string_data(glypharray[i]));
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
void ami_font_scan_init(const char *filename, struct MinList *list, lwc_string **glypharray)
{
	ULONG i, found, ffound;

	/* Ensure array zeroed */
	for(i=0x0000; i<=0xffff; i++)
		glypharray[i] = NULL;

	found = ami_font_scan_load(filename, glypharray);

	if(found == 0) {
		ffound = ami_font_scan_list(list);
		printf("Found %ld system fonts\n", ffound);
		found = ami_font_scan_fonts(list, glypharray);
		ami_font_scan_save(filename, glypharray);
	}

	printf("Initialised with %ld glyphs\n", found);
}

#ifdef AMI_FONT_SCAN_STANDALONE
/* This can be compiled as standalone using:
* gcc -o font_scan font_scan.c object.c -lwapcaplet -lauto -I .. -D__USE_INLINE__ -DAMI_FONT_SCAN_STANDALONE
*/
int main(int argc, char** argv)
{
	lwc_string *glypharray[0xffff + 1];
	ULONG found = 0;
	BPTR fh;
	struct MinList *list;

	if(argc < 2) return 5;

	printf("%s\n",argv[1]);

	list = NewObjList();
	ami_font_scan_init(argv[1], list, glypharray);
	FreeObjList(list);

	ami_font_scan_fini(glypharray);

	return 0;
}

void ami_font_close(APTR discard) { }
void ami_mime_entry_free(APTR discard) { }

#endif

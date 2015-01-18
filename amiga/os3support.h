/*
 * Copyright 2010 John-Mark Bell <jmb@netsurf-browser.org>
 * Copyright 2014 Chris Young <chris@unsatisfactorsysoftware.co.uk>
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
 * Minimal compatibility header for AmigaOS 3
 */

#ifndef AMIGA_OS3SUPPORT_H_
#define AMIGA_OS3SUPPORT_H_

#ifndef __amigaos4__

#include <stdint.h>
#include <proto/exec.h>
#include <proto/dos.h>

/* Include prototypes for amigalib */
#include <clib/alib_protos.h>

#ifndef EXEC_MEMORY_H
#include <exec/memory.h>
#endif

/* Macros */
#define IsMinListEmpty(L) (L)->mlh_Head->mln_Succ == 0

#define LIB_IS_AT_LEAST(B,V,R) ((B)->lib_Version>(V)) || \
	((B)->lib_Version==(V) && (B)->lib_Revision>=(R))

/* Define extra memory type flags */
#define MEMF_PRIVATE	MEMF_ANY
#define MEMF_SHARED	MEMF_ANY

/* Ignore unsupported tags */
#define ASO_NoTrack				TAG_IGNORE
#define BITMAP_DisabledSourceFile	TAG_IGNORE
#define BLITA_UseSrcAlpha		TAG_IGNORE
#define BLITA_MaskPlane			TAG_IGNORE
#define CLICKTAB_CloseImage		TAG_IGNORE
#define CLICKTAB_FlagImage		TAG_IGNORE
#define CLICKTAB_LabelTruncate	TAG_IGNORE
#define CLICKTAB_NodeClosed		TAG_IGNORE
#define GETFONT_OTagOnly		TAG_IGNORE
#define GETFONT_ScalableOnly	TAG_IGNORE
#define PDTA_PromoteMask	TAG_IGNORE
#define RPTAG_APenColor		TAG_IGNORE
#define GA_HintInfo			TAG_IGNORE
#define GAUGEIA_Level		TAG_IGNORE
#define IA_InBorder			TAG_IGNORE
#define IA_Label			TAG_IGNORE
#define SA_Compositing		TAG_IGNORE
#define SBNA_Text			TAG_IGNORE
#define TNA_CloseGadget		TAG_IGNORE
#define TNA_HintInfo		TAG_IGNORE
#define WA_ToolBox			TAG_IGNORE
#define WINDOW_BuiltInScroll	TAG_IGNORE
#define WINDOW_NewMenu		TAG_IGNORE
#define WINDOW_NewPrefsHook	TAG_IGNORE

/* raw keycodes */
#define RAWKEY_BACKSPACE	0x41
#define RAWKEY_TAB	0x42
#define RAWKEY_ESC	0x45
#define RAWKEY_DEL	0x46
#define RAWKEY_PAGEUP	0x48
#define RAWKEY_PAGEDOWN	0x49
#define RAWKEY_CRSRUP	0x4C
#define RAWKEY_CRSRDOWN	0x4D
#define RAWKEY_CRSRRIGHT	0x4E
#define RAWKEY_CRSRLEFT	0x4F
#define RAWKEY_F5	0x54
#define RAWKEY_HELP	0x5F
#define RAWKEY_HOME	0x70
#define RAWKEY_END	0x71

/* Other constants */
#define IDCMP_EXTENDEDMOUSE 0
#define WINDOW_BACKMOST 0
#define DN_FULLPATH 0

/* Renamed structures */
#define AnchorPathOld AnchorPath

/* ReAction (ClassAct) macros */
#define GetFileEnd End
#define GetFontEnd End
#define GetScreenModeEnd End

/* Easy compat macros */
/* application */
#define Notify(...) (void)0

/* diskfont */
/* Only used in one place we haven't ifdeffed, where it returns the charset name */
#define ObtainCharsetInfo(A,B,C) (const char *)"ISO-8859-1"

/* DOS */
#define AllocSysObjectTags(A,B,C,D) CreateMsgPort() /* Assume ASOT_PORT for now */
#define FOpen(A,B,C) Open(A,B)
#define FClose(A) Close(A)
#define CreateDirTree(D) CreateDir(D) /*\todo This isn't quite right */
#define DevNameFromLock(A,B,C,D) NameFromLock(A,B,C)

/* Exec */
#define AllocVecTagList(SZ,TAG) AllocVec(SZ,MEMF_ANY) /* AllocVecTagList with no tags */
#define GetPred(N) (N)->ln_Pred
#define GetSucc(N) (N)->ln_Succ

/* Gfx */
#define SetRPAttrs(...) (void)0 /*\todo Probably need to emulate this */

/* Intuition */
#define IDoMethod DoMethod
#define IDoMethodA DoMethodA
#define IDoSuperMethodA DoSuperMethodA
#define RefreshSetGadgetAttrs SetGadgetAttrs /*\todo This isn't quite right */
#define ShowWindow(...) (void)0

/* Utility */
#define SetMem memset

/* Integral type definitions */
typedef int8_t int8;
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef int32_t int32;
typedef uint32_t uint32;
typedef int64_t int64;
typedef uint64_t uint64;

/* TimeVal */
struct TimeVal {
	uint32 Seconds;
	uint32 Microseconds;
};

/* Compositing */
#define COMPFLAG_IgnoreDestAlpha 0
#define COMPFLAG_SrcAlphaOverride 0
#define COMPFLAG_SrcFilter 0

#define COMPOSITE_Src 0

#define COMPTAG_ScaleX 0
#define COMPTAG_ScaleY 0
#define COMPTAG_DestX 0
#define COMPTAG_DestY 0
#define COMPTAG_DestWidth 0
#define COMPTAG_DestHeight 0
#define COMPTAG_OffsetX 0
#define COMPTAG_OffsetY 0

#define CompositeTags(a, ...) ((void) (a))
#define COMP_FLOAT_TO_FIX(f) (f)

/* icon.library v51 (ie. AfA_OS version) */
#define ICONCTRLA_SetImageDataFormat        (ICONA_Dummy + 0x67) /*103*/
#define ICONCTRLA_GetImageDataFormat        (ICONA_Dummy + 0x68) /*104*/

#define IDFMT_BITMAPPED     (0)  /* Bitmapped icon (planar, legacy) */
#define IDFMT_PALETTEMAPPED (1)  /* Palette mapped icon (chunky, V44+) */
#define IDFMT_DIRECTMAPPED  (2)  /* Direct mapped icon (truecolor 0xAARRGGBB, V51+) */ 

/* Object types */
enum {
	ASOT_PORT = 1
};

/* Functions */
/* DOS */
int64 GetFileSize(BPTR fh);
void FreeSysObject(ULONG type, APTR obj);

/* Exec */
struct Node *GetHead(struct List *list);

/* Intuition */
uint32 GetAttrs(Object *obj, Tag tag1, ...);

/* Utility */
char *ASPrintf(const char *fmt, ...);

/* C */
char *strlwr(char *str);
#endif
#endif


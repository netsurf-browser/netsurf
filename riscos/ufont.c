/* ufont.c
 * Licensed under the GNU General Public License,
 *                http://www.opensource.org/licenses/gpl-license
 * Copyright 2000 James Bursa <bursa@users.sourceforge.net>
 * Copyright 2004 John Tytgat <John.Tytgat@aaug.net>
 */

/** \file
 * UFont - Unicode wrapper for non-Unicode aware FontManager
 *
 * This code allows non-Unicode aware FontManager to be used for
 * displaying Unicode encoded text lines.  It needs the !UFont
 * resource (accessed via UFont$Path).
 */

#include <assert.h>
#include <limits.h>
#include <wchar.h>

#include "oslib/osfile.h"

#include "ufont.h"
#include "utils/utils.h" /* \todo: has to go ! */

// #define DEBUG_UFONT
// #define DEBUG_ACTIVATE_SANITY_CHECK

#ifdef DEBUG_UFONT
#  define dbg_fprintf fprintf
#else
#  define dbg_fprintf (1)?0:fprintf
#endif
#ifdef DEBUG_ACTIVATE_SANITY_CHECK
#  define do_sanity_check sanity_check
#else
#  define do_sanity_check (1)?0:sanity_check
#endif
#define MALLOC_CHUNK 256

typedef struct usage_chain_s usage_chain_t;
typedef struct virtual_fh_s virtual_fh_t;
/* Virtual font handle :
 */
struct virtual_fh_s
  {
  const char    *fontNameP;    /* => malloc'ed block holding RISC OS font name */
  int            xsize, ysize; /* font size */
  int            xres, yres;   /* requested or actual resolution */
  unsigned int   usage;        /* the higher, the more this font handle is used for setting its glyphs */
  unsigned int   refCount;     /* number of times this struct is refered from ufont_f element */
  usage_chain_t *usageP;       /* Ptr to element usage chain; if non NULL, we have a RISC OS font handle allocated. When refCount is 0, this is not necessary NULL. */
  };
#define kInitialFHArraySize 20
static virtual_fh_t *oVirtualFHArrayP;
static size_t oCurVirtualFHArrayElems;
static size_t oMaxVirtualFHArrayElems;

/* Usage chain (one element per open RISC OS font handle) :
 */
struct usage_chain_s
  {
  usage_chain_t *nextP;
  usage_chain_t *prevP;

  size_t         chainTimer;      /* When equal to oChainTimer, you can not throw this element out the chain. */
  font_f         ro_fhandle;      /* RISC OS font handle (garanteed non zero) */
  virtual_fh_t  *virFHP;
  };

typedef struct ufont_map_s ufont_map_t;

struct ufont_map_s
  {
  byte fontnr[65536];     /* Must be 1st (comes straight from 'Data' file). Each entry is an index in the virtual_font_index array. */
  byte character[65536];  /* Must be 2nd (comes straight from 'Data' file) */

  const char *uFontNameP; /* => malloc'ed block holding UFont name */
  unsigned int refCount;
  ufont_map_t *nextP;
  };
static const ufont_map_t *oMapCollectionP;

struct ufont_font
  {
  const ufont_map_t *mapP;
  int virtual_handles_used; /* Number of filled virtual_font_index[] elements */
  size_t virtual_font_index[256]; /* Index in the oVirtualFHArrayP */
  };

/* Walking the chain starting with oUsageChain->nextP and continuing via
 * ->nextP until reaching oUsageChain again, results in equal or
 * decreasing ->virFHP->usage values.
 * Also walking the chain starting with oUsageChain->prevP and continuing
 * via ->prevP until reaching oUsageChain again, results in equal or
 * increasing ->virFHP->usage values.
 */
static usage_chain_t oUsageChain;
static size_t oCurUsageChainElems;
/* Maximum number of RISC OS handles open by UFont :
 */
#define kMaxUsageChainElems 80
static size_t oChainTimer;

static os_error *create_map(const char *fontNameP, const ufont_map_t **mapPP);
static os_error *delete_map(ufont_map_t *mapP);
static int eat_utf8(wchar_t *pwc, const byte *s, int n);
static os_error *addref_virtual_fonthandle(const char *fontNameP, int xsize, int ysize, int xres, int yres, int *xresOutP, int *yresOutP, size_t *offsetP);
static os_error *deref_virtual_fonthandle(size_t offset);
static os_error *activate_virtual_fh(virtual_fh_t *virFHP);
static os_error *remove_usage_chain_elem(usage_chain_t *usageElemP);
static void repos_usage_chain_elem(usage_chain_t *usageElemP);
static const char *get_rofontname(font_f rofhandle);
static void dump_internals(void);
static int sanity_check(const char *testMsgP);

/* UFont error messages :
 */
static os_error error_badparams = { error_BAD_PARAMETERS, "Bad parameters" };
static os_error error_exists = { error_FONT_NOT_FOUND, "UFont Fonts/Data file not found" };
static os_error error_memory = { error_FONT_NO_ROOM, "Insufficient memory for font" };
static os_error error_size = { error_FONT_BAD_FONT_FILE, "Wrong size of font file" };
static os_error error_fnt_corrupt = { 1 /** \todo */, "UFont is corrupt" };
static os_error error_toomany_handles = { 2 /** \todo */, "Too many UFont handles are needed to fulfill current request" };
static os_error error_noufont = { 3 /** \todo */, "Unable to find UFont font" };
static os_error error_badrohandle = { 4 /** \todo */, "Invalid internal RISC OS font handle" };

/*
 *  UFont_FindFont
 *
 *  => as Font_FindFont, but
 *     font_name does not support '\' qualifiers
 *
 *  <= as Font_FindFont, but
 *     handle is 32-bit
 *     Results from xres_out and yres_out are questionable because we
 *     delay-loading the real font data.
 */
os_error *
xufont_find_font(char const *fontNameP,
                 int xsize,
                 int ysize,
                 int xres,
                 int yres,
                 ufont_f *font,
                 int *xresOutP,
                 int *yresOutP)
{
  ufont_f fontP;
  const char *old_font;
  char *fonts_file;
  char file_name[256]; // \todo: fixed size, i.e. not safe.
  fileswitch_object_type objType;
  int size;
  os_error *errorP;

if (font == NULL)
  return &error_badparams;
/* Be sure never to return garbage as result. */
*font = NULL;

/* Allocate memory for UFont font set */
if ((fontP = (ufont_f)calloc(1, sizeof(struct ufont_font))) == NULL)
  return &error_memory;

if ((errorP = create_map(fontNameP, &fontP->mapP)) != NULL)
  {
  (void)xufont_lose_font(fontP);
  return errorP;
  }
if (fontP->mapP == NULL)
  {
  (void)xufont_lose_font(fontP);
  return &error_noufont;
  }

/* Find size of Fonts file :
 */
strcpy(file_name, fontNameP);
strcat(file_name, ".Fonts");
if ((errorP = xosfile_read_stamped_path(file_name, "UFont:", &objType, NULL, NULL, &size, NULL, NULL)) != NULL)
  {
  (void)xufont_lose_font(fontP);
  return errorP;
  }
if (objType != fileswitch_IS_FILE)
  {
  (void)xufont_lose_font(fontP);
  return &error_exists;
  }

if ((fonts_file = (char *)malloc(size)) == NULL)
  {
  (void)xufont_lose_font(fontP);
  return &error_memory;
  }

/* Load Fonts :
 */
if ((errorP = xosfile_load_stamped_path(file_name, fonts_file, "UFont:", NULL, NULL, NULL, NULL, NULL)) != NULL)
  {
  (void)xufont_lose_font(fontP);
  free((void *)fonts_file);
  return errorP;
  }

/* Open all fonts listed in Fonts :
 */
for (old_font = fonts_file, fontP->virtual_handles_used = 0;
     old_font - fonts_file < size;
     old_font += strlen(old_font) + 1, ++fontP->virtual_handles_used)
  {
  /* UFont can maximum have 256 real RISC OS fonts :
   */
  if (fontP->virtual_handles_used < 256)
    {
    dbg_fprintf(stderr, "%i %s: ", fontP->virtual_handles_used, old_font);
    errorP = addref_virtual_fonthandle(old_font, xsize, ysize, xres, yres, xresOutP, yresOutP, &fontP->virtual_font_index[fontP->virtual_handles_used]);
    }
  else
    errorP = &error_fnt_corrupt;

  if (errorP != NULL)
    {
    (void)xufont_lose_font(fontP);
    free((void *)fonts_file);
    return errorP;
    }

  dbg_fprintf(stderr, "%i\n", fontP->virtual_font_index[fontP->virtual_handles_used]);
  }

/* free Fonts */
free((void *)fonts_file); fonts_file = NULL;

*font = fontP;
if (xresOutP != NULL)
  *xresOutP = 96;
if (yresOutP != NULL)
  *yresOutP = 96;
return NULL;
}


/*
 *  UFont_LoseFont
 *
 *  => font handle as returned by UFont_FindFont
 *  Even if there was an error returned, we tried to delete as much
 *  as possible.  The ufont_f is definately not reusable afterwards.
 */
os_error *
xufont_lose_font(ufont_f fontP)
{
  size_t index;
  os_error *theErrorP;

theErrorP = (fontP->mapP != NULL) ? delete_map(fontP->mapP) : NULL;

/* Close all fonts used :
 */
for (index = 0; index < fontP->virtual_handles_used; ++index)
  {
  os_error *errorP;
  dbg_fprintf(stderr, "About to deref virtual font handle %d\n", fontP->virtual_font_index[index]);
  if ((errorP = deref_virtual_fonthandle(fontP->virtual_font_index[index])) != NULL)
    theErrorP = errorP;
  }

/* Free ufont structure :
 */
free((void *)fontP);

return theErrorP;
}


/*
 *  UFont_Paint
 *
 *  => font handle as returned by UFont_FindFont
 *     string is Unicode UTF-8 encoded
 *     other parameters as Font_Paint
 */
os_error *
xufont_paint(ufont_f fontP,
             unsigned char const *string,
             font_string_flags flags,
             int xpos,
             int ypos,
             font_paint_block const *block,
             os_trfm const *trfm,
             int length)
{
  char *result;
  os_error *error;

  if ((flags & font_GIVEN_LENGTH) == 0)
    length = INT_MAX;

  dbg_fprintf(stderr, "xufont_paint() : size %d, consider len %d\n", strlen(string), length);
  if ((error = xufont_convert(fontP, string, length, &result, NULL)) != NULL)
    return error;
  if (result[0] == '\0')
    return NULL;

  assert(result[0] == font_COMMAND_FONT);
  error = xfont_paint(result[1], &result[2],
                      (flags & (~font_GIVEN_LENGTH)) | font_GIVEN_FONT,
                      xpos, ypos, block, trfm, 0);
  return error;
}


/*
 *  UFont_ScanString
 *
 *  => font handle as returned by UFont_FindFont
 *     string is Unicode UTF-8 encoded
 *     split length is index in string, not pointer
 *     other parameters as Font_ScanString
 *
 *  <= as Font_ScanString
 */
os_error *
xufont_scan_string(ufont_f fontP,
                   unsigned char const *string,
                   font_string_flags flags,
                   int x,
                   int y,
                   font_scan_block const *block,
                   os_trfm const *trfm,
                   int length,
                   unsigned char const **split_point,
                   int *x_out,
                   int *y_out,
                   int *length_out)
{
  char *result;
  char *split_point_i;
  unsigned int *table;
  os_error *error;

  if ((flags & font_GIVEN_LENGTH) == 0)
    length = INT_MAX;

  dbg_fprintf(stderr, "xufont_scan_string() : size %d, consider len %d\n", strlen(string), length);
  if ((error = xufont_convert(fontP, string, length, &result, &table)) != NULL)
    return error;
  if (result[0] == '\0')
    {
    if (split_point != NULL)
      *split_point = string;
    if (x_out != NULL)
      *x_out = 0;
    if (y_out != NULL)
      *y_out = 0;
    if (length_out != NULL)
      *length_out = 0;
    return NULL;
    }

  assert(result[0] == font_COMMAND_FONT);
  error = xfont_scan_string(result[1], &result[2],
                            (flags & (~font_GIVEN_LENGTH)) | font_GIVEN_FONT,
                            x, y, block, trfm, 0,
                            (split_point) ? &split_point_i : NULL,
                            x_out, y_out, length_out);
  if (error != NULL)
    return error;

  if (split_point != NULL)
    {
    dbg_fprintf(stderr, "RISC OS scan string split at offset %d (char %d)\n", split_point_i - result, *split_point_i);
    *split_point = &string[table[split_point_i - result]];
    dbg_fprintf(stderr, "UTF-8 Split offset at %d (char %d)\n", *split_point - string, **split_point);
    }

  return NULL;
}


/**
 * Given a text line, return the number of bytes which can be set using
 * one RISC OS font and the bounding box fitting that part of the text
 * only.
 *
 * \param font a ufont font handle, as returned by xufont_find_font().
 * \param string string text.  Does not have to be NUL terminated.
 * \param flags FontManger flags to be used internally
 * \param length length in bytes of the text to consider.
 * \param width returned width of the text which can be set with one RISC OS font. If 0, then error happened or initial text length was 0.
 * \param rofontname returned name of the RISC OS font which can be used to set the text. If NULL, then error happened or initial text length was 0.
 * \param rotext returned string containing the characters in returned RISC OS font. Not necessary NUL terminated. free() after use.  If NULL, then error happened or initial text length was 0.
 * \param rolength length of return rotext string. If 0, then error happened or initial text length was 0.
 * \param consumed number of bytes of the given text which can be set with one RISC OS font. If 0, then error happened or initial text length was 0.
 */
os_error *xufont_txtenum(ufont_f fontP,
		unsigned char const *string,
		font_string_flags flags,
		size_t length,
		int *widthP,
		unsigned char const **rofontnameP,
		unsigned char const **rotextP,
		size_t *rolengthP,
		size_t *consumedP)
{
	char *result, *end_result;
	unsigned int *table;
	os_error *errorP;

	int width;
	const char *rofontname;
	char *rotext;
	size_t rolength;

	*rotextP = *rofontnameP = NULL;
	*consumedP = *rolengthP = *widthP = 0;

	if ((flags & font_GIVEN_LENGTH) == 0)
		length = INT_MAX;

	if (length == 0)
		return NULL;

	if ((errorP = xufont_convert(fontP, string, length, &result, &table)) != NULL)
		return errorP;
	if (result[0] == '\0')
		return NULL;
	assert(result[0] == font_COMMAND_FONT);

	/* Find how many characters starting at <result + 2> onwards
	   are set using the RISC OS font with handle <result + 1> */
	for (end_result = result + 2; *end_result != '\0' && *end_result != font_COMMAND_FONT; ++end_result)
		;

	rolength = end_result - result - 2;
	if ((errorP = xfont_scan_string(result[1], &result[2],
			flags | font_GIVEN_LENGTH | font_GIVEN_FONT,
			0x7fffffff, 0x7fffffff,
			NULL, NULL,
			rolength,
			NULL,
			&width, NULL,
			NULL)) != NULL)
		return errorP;
	if ((rofontname = get_rofontname(result[1])) == NULL)
		return &error_badrohandle;
	if ((rotext = malloc(rolength)) == NULL)
		return &error_memory;
	memcpy(rotext, result + 2, rolength);

	*widthP = width;
	*rofontnameP = rofontname;
	*rotextP = rotext;
	*rolengthP = rolength;
	*consumedP = table[end_result - result];

	return NULL;
}


/*
 *  UFont_Convert
 *
 *  => initial font
 *     UTF-8 string to convert to RISC OS font numbers and codes.
 *     max length to convert (characters) or NUL char terminated.
 *
 *  <= string converted to Font_Paint format
 *     table of offsets in UTF-8 string
 */
os_error *
xufont_convert(ufont_f fontP,
               unsigned char const *string,
               size_t length,
               char **presult, /* may not be NULL ! */
               size_t **ptable /* may be NULL */)
{
  static char *resultP;
  static size_t *tableP;
  static size_t currentSize;

  size_t max_length;
  size_t string_index, new_string_index, result_index;
  virtual_fh_t *curVirFH = NULL;

assert(presult != NULL);

do_sanity_check("xufont_convert() : begin");

/* Find upfront if we're NUL char terminated or length terminated. */
for (max_length = 0; max_length < length && string[max_length] != '\0'; ++max_length)
  /* no body */;

/* Increase timer so we can enforce a set of usage elements to remain active.
 */
++oChainTimer;

/* Ensure memory block.
 */
if (resultP == NULL)
  {
  if ((resultP = (char *)malloc(MALLOC_CHUNK)) == NULL)
    return &error_memory;
  currentSize = MALLOC_CHUNK;
  }
if (tableP == NULL && (tableP = (size_t *)malloc(MALLOC_CHUNK * sizeof(size_t))) == NULL)
  return &error_memory;

dbg_fprintf(stderr, "xufont_convert() : ");
for (string_index = 0, result_index = 0;
     string_index < max_length;
     string_index = new_string_index)
  {
    wchar_t wchar;
    int result = eat_utf8(&wchar, &string[string_index], max_length - string_index);

  if (result == 0)
    {
    /* Too few input bytes : abort conversion */
    fprintf(stderr, "eat_utf8() : too few input bytes\n");
    break;
    }
  else if (result < 0)
    {
    /* Corrupt UTF-8 stream : skip <-result> input characters. */
    fprintf(stderr, "eat_utf8() : error %d\n", result);
fprintf(stderr, "String <%.*s> error pos %d\n", length, string, string_index);
    wchar = '?';
    new_string_index = string_index - result;
    }
  else
    {
    /* Normal case : one wchar_t produced, <result> bytes consumed. */
    if (wchar >= 0x10000)
      wchar = '?';
    new_string_index = string_index + result;
    }

  dbg_fprintf(stderr, "src offset 0x%x : 0x%x ", string_index, wchar);

  /* Reserve room for at least 32 more entries.
   */
  if (result_index + 32 > currentSize)
    {
    if ((resultP = realloc(resultP, currentSize*2)) == NULL
        || (tableP = realloc(tableP, currentSize*2 * sizeof(size_t))) == NULL)
      return &error_memory;

    currentSize *= 2;
    }

    {
      const byte fontnr = fontP->mapP->fontnr[wchar];
      virtual_fh_t *virFHP;
      usage_chain_t *usageP;

    assert(fontnr < fontP->virtual_handles_used);
    virFHP = &oVirtualFHArrayP[fontP->virtual_font_index[fontnr]];

    /* Check if current font is ok :
     */
    if (virFHP != curVirFH)
      {
        os_error *errorP;

      curVirFH = virFHP;

      /* Make sure we have a RISC OS font handle associated :
       */
      if ((errorP = activate_virtual_fh(virFHP)) != NULL)
        return errorP;
      usageP = virFHP->usageP;
      assert(usageP != NULL);
      assert(usageP->ro_fhandle != 0);
      tableP[result_index] = tableP[result_index + 1] = string_index;
      resultP[result_index++] = font_COMMAND_FONT;
      resultP[result_index++] = usageP->ro_fhandle;

      dbg_fprintf(stderr, "{%i} ", resultP[result_index - 1]);
      }
    else
      {
      usageP = virFHP->usageP;
      assert(usageP != NULL);
      }

    ++virFHP->usage;
    /* By increasing the usage counter, it might that the oUsageChain needs
     * reordering.
     */
    if (usageP != oUsageChain.nextP && virFHP->usage > usageP->prevP->virFHP->usage)
      repos_usage_chain_elem(usageP);

    tableP[result_index] = string_index;
    resultP[result_index++] = fontP->mapP->character[wchar];
    dbg_fprintf(stderr, "[0x%x] ", resultP[result_index - 1]);
    }
  }
resultP[result_index] = 0;
*presult = resultP;

tableP[result_index] = string_index;
if (ptable != NULL)
  *ptable = tableP;

#ifdef DEBUG_UFONT
fprintf(stderr, "\nRISC OS font string result:\n");

for (result_index = 0; resultP[result_index] != 0; ++result_index)
  fprintf(stderr, "  Dst offset %d : 0x%x (src offset %d)\n", result_index, resultP[result_index], tableP[result_index]);
#endif

do_sanity_check("xufont_convert() : end");

dbg_fprintf(stderr, "--- After convert()\n");
//dump_internals();

return NULL;
}


/* Creates or reuses an existing ufont_map_t
 */
static os_error *create_map(const char *uFontNameP, const ufont_map_t **mapPP)
{
  ufont_map_t *curMapP;
  size_t uFontNameLen = strlen(uFontNameP);
  char *fileNameP;
  os_error *errorP;

/* Make sure we never return garbage results.
 */
*mapPP = NULL;

if ((fileNameP = (char *)alloca(uFontNameLen + sizeof(".Data"))) == NULL)
  return &error_memory;
memcpy(fileNameP, uFontNameP, uFontNameLen);
do {
    fileswitch_object_type objType;
    int size;

  memcpy(&fileNameP[uFontNameLen], ".Data", sizeof(".Data"));
  if ((errorP = xosfile_read_stamped_path(fileNameP, "UFont:", &objType, NULL, NULL, &size, NULL, NULL)) != NULL)
    return errorP;

  if (objType == fileswitch_NOT_FOUND)
    {
    /* Look for the Data file one directory level up. */
    while (uFontNameLen != 0 && fileNameP[--uFontNameLen] != '.')
      /* no body */;
    if (uFontNameLen == 0)
      return &error_exists;
    }
  else if (objType == fileswitch_IS_FILE)
    {
    if (size != 2*65536)
      return &error_size;
    break;
    }
  else
     return &error_exists;
  } while (1);

/* Try to reuse an existing map :
 */
for (curMapP = oMapCollectionP; curMapP != NULL; curMapP = curMapP->nextP)
  {
    size_t curUFontNameLen = strlen(curMapP->uFontNameP);

  if (uFontNameLen != curUFontNameLen)
    continue;

  if (memcmp(fileNameP, curMapP->uFontNameP, curUFontNameLen) != 0)
    break;
  }
if (curMapP != NULL)
  {
  ++curMapP->refCount;
  *mapPP = curMapP;
  return NULL;
  }

/* We need to create & load new map into memory :
 */
if ((curMapP = (ufont_map_t *)malloc(sizeof(ufont_map_t))) == NULL)
  return &error_memory;

/* Load Data file :
 */
if ((errorP = xosfile_load_stamped_path(fileNameP, (byte *)&curMapP->fontnr[0], "UFont:", NULL, NULL, NULL, NULL, NULL)) != NULL)
  {
  free((void *)curMapP);
  return errorP;
  }
fileNameP[uFontNameLen] = '\0';
if ((curMapP->uFontNameP = strdup(fileNameP)) == NULL)
  {
  free((void *)curMapP);
  return &error_memory;
  }
curMapP->refCount = 1;
curMapP->nextP = oMapCollectionP;
oMapCollectionP = curMapP;

*mapPP = curMapP;
return NULL;
}


static os_error *delete_map(ufont_map_t *mapP)
{
assert(mapP->refCount > 0);
--mapP->refCount;
/* \todo: we don't remove the map from the oMapCollection list.  Should we ?
 */

return NULL;
}


/* Returns:
 *   x > 0: number of bytes consumed at s, valid wchar_t returned at pwc[0]
 *   x = 0: too few input bytes, pwc[0] is undefined
 *   x < 0: illegal UTF-8 stream, skip -x characters at s, pwc[0] is undefined
 */
static int eat_utf8(wchar_t *pwc, const byte *s, int n)
{
	byte c;
	int i;

#if 0
fputs("<", stderr);
for (i = 0; i < n; ++i)
  fputc(s[i], stderr);
fputs(">\n", stderr);
#endif
	if (n < 1)
		return 0; /* not enough input bytes */
	else if ((c = s[0]) < 0x80) {
		*pwc = c;
		return 1;
	} else if (c < 0xc2)
		goto do_sync;
	else if (c < 0xe0) {
		if (n < 2)
			return 0; /* not enough input bytes */
		if (!((s[1] ^ 0x80) < 0x40))
			goto do_sync;
		*pwc = ((wchar_t) (c & 0x1f) << 6) | (wchar_t) (s[1] ^ 0x80);
		return 2;
	} else if (c < 0xf0) {
		if (n < 3)
			return 0; /* not enough input bytes */
		if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (c >= 0xe1 || s[1] >= 0xa0)))
			goto do_sync;
		*pwc = ((wchar_t) (c & 0x0f) << 12) | ((wchar_t) (s[1] ^ 0x80) << 6) | (wchar_t) (s[2] ^ 0x80);
		return 3;
	} else if (c < 0xf8) {
		if (n < 4)
			return 0; /* not enough input bytes */
		if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (c >= 0xf1 || s[1] >= 0x90)))
			goto do_sync;
		*pwc = ((wchar_t) (c & 0x07) << 18) | ((wchar_t) (s[1] ^ 0x80) << 12) | ((wchar_t) (s[2] ^ 0x80) << 6) | (wchar_t) (s[3] ^ 0x80);
		return 4;
	} else if (c < 0xfc) {
		if (n < 5)
			return 0; /* not enough input bytes */
		if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40 && (c >= 0xf9 || s[1] >= 0x88)))
			goto do_sync;
		*pwc = ((wchar_t) (c & 0x03) << 24) | ((wchar_t) (s[1] ^ 0x80) << 18) | ((wchar_t) (s[2] ^ 0x80) << 12) | ((wchar_t) (s[3] ^ 0x80) << 6) | (wchar_t) (s[4] ^ 0x80);
		return 5;
	} else if (c < 0xfe) {
		if (n < 6)
			return 0; /* not enough input bytes */
		if (!((s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40 && (s[3] ^ 0x80) < 0x40 && (s[4] ^ 0x80) < 0x40 && (s[5] ^ 0x80) < 0x40 && (c >= 0xfd || s[1] >= 0x84)))
			goto do_sync;
		*pwc = ((wchar_t) (c & 0x01) << 30) | ((wchar_t) (s[1] ^ 0x80) << 24) | ((wchar_t) (s[2] ^ 0x80) << 18) | ((wchar_t) (s[3] ^ 0x80) << 12) | ((wchar_t) (s[4] ^ 0x80) << 6) | (wchar_t) (s[5] ^ 0x80);
		return 6;
	}

do_sync:
	/* The UTF-8 sequence at s is illegal, skipping from s onwards
	 * until first non top character is found or %11xxxxxx is found
	 * (both valid UTF-8 *starts* - not necessary valid sequences).
	 */
	for (i = 1; i < n && !((s[i] & 0x80) == 0x00 || (s[i] & 0xC0) == 0xC0); ++i)
	  /* no body */;
	return -i;
}


/* Adds the RISC OS font <fontNameP> to the oVirtualFHArrayP list and
 * returns the index in that array.
 * oVirtualFHArrayP can be reallocated (and all virFHP ptrs in oUsageChain).
 * Results in xresOutP and yresOutP are not always that meaningful because
 * of the delayed-loading of the real RISC OS font data.
 * xresOutP and/or yresOutP may be NULL.
 */
static os_error *addref_virtual_fonthandle(const char *fontNameP, int xsize, int ysize, int xres, int yres, int *xresOutP, int *yresOutP, size_t *offsetP)
{
  size_t curIndex;
  virtual_fh_t *unusedSlotP;

assert(offsetP != NULL);

do_sanity_check("addref_virtual_fonthandle() : begin");

if (oVirtualFHArrayP == NULL)
  {
  if ((oVirtualFHArrayP = (virtual_fh_t *)calloc(kInitialFHArraySize, sizeof(virtual_fh_t))) == NULL)
    return &error_memory;
  /* oCurVirtualFHArrayElems = 0; Isn't really necessary because of static */
  oMaxVirtualFHArrayElems = kInitialFHArraySize;
  }

/* Check for duplicate (and find first unused slot if any) :
 */
for (unusedSlotP = NULL, curIndex = 0;
     curIndex < oCurVirtualFHArrayElems;
     ++curIndex)
  {
    virtual_fh_t *virFHP = &oVirtualFHArrayP[curIndex];

  if (virFHP->fontNameP != NULL /* case strdup(fontNameP) failed */
      && stricmp(virFHP->fontNameP, fontNameP) == 0
      && virFHP->xsize == xsize && virFHP->ysize == ysize)
    {
    if (xresOutP != NULL)
      *xresOutP = virFHP->xres;
    if (yresOutP != NULL)
      *yresOutP = virFHP->yres;
    ++virFHP->refCount;
    *offsetP = curIndex;

    do_sanity_check("addref_virtual_fonthandle() : case 1");
    return NULL;
    }

  if (virFHP->refCount == 0 && unusedSlotP == NULL)
    unusedSlotP = virFHP;
  }

/* Can we reuse a slot ?
 * I.e. a virtual FH which refCount is zero.
 */
if (unusedSlotP != NULL)
  {
  if (unusedSlotP->usageP != NULL)
    {
      os_error *errorP;

    /* This slot is refered in the usage chain, we have to unlink it.
     */
    if ((errorP = remove_usage_chain_elem(unusedSlotP->usageP)) != NULL)
      return errorP;
    }

  unusedSlotP->usage = 0;
  if (unusedSlotP->fontNameP != NULL)
    free((void *)unusedSlotP->fontNameP);
  if ((unusedSlotP->fontNameP = strdup(fontNameP)) == NULL)
    return &error_memory;

  unusedSlotP->xsize = xsize;
  unusedSlotP->ysize = ysize;
  unusedSlotP->xres = (xres > 1) ? xres : 96;
  if (xresOutP != NULL)
    *xresOutP = unusedSlotP->xres;
  unusedSlotP->yres = (yres > 1) ? yres : 96;
  if (yresOutP != NULL)
    *yresOutP = unusedSlotP->yres;
  unusedSlotP->refCount = 1;
  *offsetP = unusedSlotP - oVirtualFHArrayP;
  do_sanity_check("addref_virtual_fonthandle() : case 2");
  return NULL;
  }

/* Add new entry :
 */
if (oCurVirtualFHArrayElems == oMaxVirtualFHArrayElems)
  {
    virtual_fh_t *newVirtualFHArrayP;
    size_t extraOffset;
    usage_chain_t *usageP;

  /* Don't use realloc() as when that fails, we don't even have the original
   * memory block anymore.
   */
  if ((newVirtualFHArrayP = (virtual_fh_t *)calloc(2*oMaxVirtualFHArrayElems, sizeof(virtual_fh_t))) == NULL)
    return &error_memory;
  memcpy(newVirtualFHArrayP, oVirtualFHArrayP, oMaxVirtualFHArrayElems * sizeof(virtual_fh_t));
  free((void *)oVirtualFHArrayP);
  extraOffset = (const char *)newVirtualFHArrayP - (const char *)oVirtualFHArrayP;
  oVirtualFHArrayP = newVirtualFHArrayP;
  oMaxVirtualFHArrayElems *= 2;

  /* Update the virFHP pointers in the usage chain :
   */
  if (oUsageChain.nextP != NULL)
    {
    for (usageP = oUsageChain.nextP; usageP != &oUsageChain; usageP = usageP->nextP)
      usageP->virFHP = (virtual_fh_t *)&((char *)usageP->virFHP)[extraOffset];
    }
  }

unusedSlotP = &oVirtualFHArrayP[oCurVirtualFHArrayElems];
if ((unusedSlotP->fontNameP = (const char *)strdup(fontNameP)) == NULL)
  return &error_memory;
unusedSlotP->xsize = xsize;
unusedSlotP->ysize = ysize;
unusedSlotP->xres = (xres > 1) ? xres : 96;
if (xresOutP != NULL)
  *xresOutP = unusedSlotP->xres;
unusedSlotP->yres = (yres > 1) ? yres : 96;
if (yresOutP != NULL)
  *yresOutP = unusedSlotP->yres;
unusedSlotP->refCount = 1;
*offsetP = oCurVirtualFHArrayElems++;

do_sanity_check("addref_virtual_fonthandle() : case 3");
return NULL;
}


/* Deref virtual_fh_t element in oVirtualFHArrayP array.
 */
static os_error *deref_virtual_fonthandle(size_t offset)
{
assert(offset >= 0 && offset < oCurVirtualFHArrayElems);
assert(oVirtualFHArrayP[offset].refCount > 0);

/* When the refCount reaches 0, it will be reused by preference when a
 * new usageChain element is needed in addref_virtual_fonthandle().
 */
--oVirtualFHArrayP[offset].refCount;

do_sanity_check("deref_virtual_fonthandle()");
return NULL;
}


/* Virtual font handle <virFHP> needs to have a RISC OS font handle
 * associated via virFHP->usageP.  For this we can throw out all other
 * usage chain elements which have a chainTimer different from oChainTimer.
 * However, we may not have more than kMaxUsageChainElems chain elements
 * active.
 * Afterwards: either an error, either virFHP->usageP points to an usage chain
 * element in the oUsageChain linked list and that usage chain element has
 * a valid RISC OS font handle associated.
 */
static os_error *activate_virtual_fh(virtual_fh_t *virFHP)
{
  usage_chain_t *usageP;

dbg_fprintf(stderr, "+++ activate_virtual_fh(virFHP %p, usageP ? %p)\n", virFHP, virFHP->usageP);
do_sanity_check("activate_virtual_fh() : begin");

/* The easiest case : we already have a RISC OS font handle :
 */
if ((usageP = virFHP->usageP) != NULL)
  {
  usageP->chainTimer = oChainTimer;
  assert(usageP->ro_fhandle != 0);
  assert(usageP->virFHP == virFHP);
  do_sanity_check("activate_virtual_fh() : case 1");
  dbg_fprintf(stderr, "--- done, activate_virtual_fh(), case 1\n");
  return NULL;
  }

/* The second easiest case : we're still allowed to create an extra
 * chain element :
 */
if (oCurUsageChainElems < kMaxUsageChainElems)
  {
    os_error *errorP;

  if ((usageP = (usage_chain_t *)malloc(sizeof(usage_chain_t))) == NULL)
    return &error_memory;
  usageP->chainTimer = oChainTimer;

  if ((errorP = xfont_find_font(virFHP->fontNameP,
                                virFHP->xsize, virFHP->ysize,
                                virFHP->xres, virFHP->yres,
                                &usageP->ro_fhandle,
                                &virFHP->xres, &virFHP->yres)) != NULL)
    {
    free((void *)usageP);
    return errorP;
    }
  usageP->virFHP = virFHP;
  virFHP->usageP = usageP;
  ++oCurUsageChainElems;

  /* Make sure oUsageChain nextP and prevP point to at least something (is
   * only executed once and should probably better be done in a global
   * init routine).
   */
  if (oUsageChain.nextP == NULL)
    oUsageChain.nextP = oUsageChain.prevP = &oUsageChain;
  }
else
  {
    os_error *errorP;

  /* The more difficult one : we need to reuse a usage chain element.
   * Take the last one because that is least used but skip the onces
   * with chainTimer equal to the current oChainTimer because those
   * RISC OS font handles are still used in the font string we're
   * currently processing.
   */
  for (usageP = oUsageChain.prevP;
       usageP != &oUsageChain && usageP->chainTimer == oChainTimer;
       usageP = usageP->prevP)
    /* no body */;
  if (usageP == &oUsageChain)
    {
    /* Painful : all usage chain elements are in use and we already have the
     * maximum of chain elements reached.
     */
    return &error_toomany_handles;
    }

  usageP->chainTimer = oChainTimer;
  /* The virtual font handle currently in usageP->virFHP no longer has a real
   * RISC OS font handle anymore.
   */
  usageP->virFHP->usageP = NULL;
  if ((errorP = xfont_lose_font(usageP->ro_fhandle)) != NULL)
    return errorP;
  if ((errorP = xfont_find_font(virFHP->fontNameP,
                                virFHP->xsize, virFHP->ysize,
                                virFHP->xres, virFHP->yres,
                                &usageP->ro_fhandle,
                                &virFHP->xres, &virFHP->yres)) != NULL)
    return errorP;
  usageP->virFHP = virFHP;
  virFHP->usageP = usageP;

  /* Delink :
   */
  usageP->prevP->nextP = usageP->nextP;
  usageP->nextP->prevP = usageP->prevP;
  }

/* Link usageP in the oUsageChain based on its current virFHP->usage value :
 */
  {
    const unsigned int usage = virFHP->usage;
    usage_chain_t *runUsageP;

  for (runUsageP = &oUsageChain;
       runUsageP != oUsageChain.nextP && runUsageP->prevP->virFHP->usage <= usage;
       runUsageP = runUsageP->prevP)
    /* no body */;

  /* We have to link usageP between runUsageP and runUsageP->prevP
   * because runUsageP->prevP has higher usage than the one we need to
   * link in -or- we're at the end/start.
   */
  usageP->nextP = runUsageP;
  usageP->prevP = runUsageP->prevP;
  runUsageP->prevP->nextP = usageP;
  runUsageP->prevP = usageP;
  }
do_sanity_check("activate_virtual_fh() : case 2");
dbg_fprintf(stderr, "--- done, activate_virtual_fh(), case 2\n");

return NULL;
}


/* Remove this element from the usage chain :
 */
static os_error *remove_usage_chain_elem(usage_chain_t *usageP)
{
  os_error *errorP;

assert(usageP != NULL);
assert(oCurUsageChainElems > 0);
assert(usageP->ro_fhandle != 0);
assert(usageP->virFHP != NULL);

if ((errorP = xfont_lose_font(usageP->ro_fhandle)) != NULL)
  return errorP;

usageP->virFHP->usageP = NULL;

/* Delink it :
 */
usageP->prevP->nextP = usageP->nextP;
usageP->nextP->prevP = usageP->prevP;

--oCurUsageChainElems;
free((void *)usageP);

do_sanity_check("remove_usage_chain_elem() : end");
return NULL;
}


/* usageP is no longer in the right place in the chain because its
 * ->virFHP->usage value increased.  Reposition it towards prev
 * direction.
 */
static void repos_usage_chain_elem(usage_chain_t *usageP)
{
  usage_chain_t *prev1P, *prev2P;
  const unsigned int curUsage = usageP->virFHP->usage;

dbg_fprintf(stderr, "+++ repos_usage_chain_elem(%p)\n", usageP);

/* If this assert goes off, then it means that this routine shouldn't
 * have been called.
 */
assert(curUsage > usageP->prevP->virFHP->usage);

/* Delink :
 */
usageP->prevP->nextP = usageP->nextP;
usageP->nextP->prevP = usageP->prevP;

/* Place usageElemP between prev1P and prev2P.
 */
for (prev1P = usageP->prevP, prev2P = prev1P->prevP;
     prev2P != &oUsageChain && curUsage > prev2P->virFHP->usage;
     prev1P = prev2P, prev2P = prev2P->prevP)
  {
  dbg_fprintf(stderr, "> prev1P %p (%d), usageElemP %p (%d), prev2P %p (%d), dummy %p\n", prev1P, prev1P->virFHP->usage, usageP, usageP->virFHP->usage, prev2P, prev2P->virFHP->usage, &oUsageChain);
  assert(prev1P->virFHP->usage <= prev2P->virFHP->usage);
  }

dbg_fprintf(stderr, "prev1P %p (%d), usageElemP %p (%d), prev2P %p (%d), dummy %p\n", prev1P, prev1P->virFHP->usage, usageP, usageP->virFHP->usage, prev2P, prev2P->virFHP->usage, &oUsageChain);

/* Relink between prev1P and prev2P :
 */
prev1P->prevP = usageP;
usageP->prevP = prev2P;
prev2P->nextP = usageP;
usageP->nextP = prev1P;

do_sanity_check("repos_usage_chain_elem() : end");
dbg_fprintf(stderr, "--- done, repos_usage_chain_elem()\n");
}


/**
 * Retrieves the RISC OS font name of given RISC OS fonthandle.
 *
 * \param rofhandle RISC OS font handle
 */
static const char *get_rofontname(font_f rofhandle)
{
	usage_chain_t *usageP;

	if (oUsageChain.nextP == NULL)
		return NULL;

	for (usageP = oUsageChain.nextP; usageP != &oUsageChain; usageP = usageP->nextP)
		if (usageP->ro_fhandle == rofhandle)
			return usageP->virFHP->fontNameP;

	return NULL;
}


static void dump_internals(void)
{
fprintf(stderr, "Dump UFont internals:\n  - Virtual font handle array at %p (length %d, max length %d)\n  - Usage chain elements %d\n  - Chain timer is %d\n  Dump usage chain (first dummy at %p):\n", oVirtualFHArrayP, oCurVirtualFHArrayElems, oMaxVirtualFHArrayElems, oCurUsageChainElems, oChainTimer, &oUsageChain);
if (oUsageChain.prevP == NULL || oUsageChain.nextP == NULL)
  {
  fprintf(stderr, "  Empty usage chain\n");
  if (oUsageChain.prevP != oUsageChain.nextP)
    fprintf(stderr, "  *** Corrupted empty usage chain: next %p, prev %p\n", oUsageChain.nextP, oUsageChain.prevP);
  if (oCurUsageChainElems != 0)
    fprintf(stderr, "  *** Current usage chain length is wrong\n");
  }
else
  {
    size_t usageCount;
    const usage_chain_t *usageP;

  for (usageCount = 0, usageP = oUsageChain.nextP; usageP != &oUsageChain; ++usageCount, usageP = usageP->nextP)
    {
    fprintf(stderr, "  -%d- : cur %p, next %p, prev %p, timer %d, RISC OS font handle %d, virtual font %p (%d, %s), usage %d\n", usageCount, usageP, usageP->nextP, usageP->prevP, usageP->chainTimer, usageP->ro_fhandle, usageP->virFHP, usageP->virFHP - oVirtualFHArrayP, usageP->virFHP->fontNameP, usageP->virFHP->usage);
    if (usageP->nextP->prevP != usageP)
      fprintf(stderr, "  *** Bad usageP->nextP->prevP != usageP\n");
    if (usageP->prevP->nextP != usageP)
      fprintf(stderr, "  *** Bad usageP->prevP->nextP != usageP\n");
    if (usageP->virFHP < oVirtualFHArrayP || usageP->virFHP >= &oVirtualFHArrayP[oCurVirtualFHArrayElems])
      fprintf(stderr, "  *** Bad virtual font handle\n");
    }
  if (usageCount != oCurUsageChainElems)
    fprintf(stderr, "  *** Current usage chain length is wrong\n");
  if (usageCount > kMaxUsageChainElems)
    fprintf(stderr, "  *** Current usage chain is too long\n");
  }

if (oVirtualFHArrayP != NULL)
  {
    size_t fhIndex;

  fprintf(stderr, "  Dump virtual font handles:\n");
  for (fhIndex = 0; fhIndex < oCurVirtualFHArrayElems; ++fhIndex)
    {
      const virtual_fh_t *virFHP = &oVirtualFHArrayP[fhIndex];

    fprintf(stderr, "  -%d (%p)- : <%s>, size %d,%d, res %d,%d, usage %d, ref count %d, usage chain ptr %p\n", fhIndex, virFHP, virFHP->fontNameP, virFHP->xsize, virFHP->ysize, virFHP->xres, virFHP->yres, virFHP->usage, virFHP->refCount, virFHP->usageP);
    if (virFHP->usageP != NULL)
      {
        const usage_chain_t *usageP;

      for (usageP = oUsageChain.nextP;
           usageP != virFHP->usageP && usageP != &oUsageChain;
           usageP = usageP->nextP)
        /* no body */;
      if (usageP != virFHP->usageP)
        fprintf(stderr, "  *** Usage chain ptr could not be found in usage chain\n");
      }
    }
  }
}


static int sanity_check(const char *testMsgP)
{
dbg_fprintf(stderr, "Sanity check <%s>\n", testMsgP);
if (oUsageChain.prevP == NULL || oUsageChain.nextP == NULL)
  {
  assert(oUsageChain.prevP == oUsageChain.nextP);
  assert(oCurUsageChainElems == 0);
  }
else
  {
    size_t usageCount;
    const usage_chain_t *usageP;

  /* We should see equal or decreasing usage values.
   */
  for (usageCount = 0, usageP = oUsageChain.nextP; usageP != &oUsageChain; ++usageCount, usageP = usageP->nextP)
    {
    assert(usageP->nextP->prevP == usageP);
    assert(usageP->prevP->nextP == usageP);
    assert(usageP->chainTimer <= oChainTimer);
    assert(usageP->ro_fhandle != 0);
    assert(oVirtualFHArrayP != NULL);
    assert(usageP->virFHP >= oVirtualFHArrayP && usageP->virFHP < &oVirtualFHArrayP[oCurVirtualFHArrayElems]);
    assert(usageP->virFHP->usageP == usageP);
//    dbg_fprintf(stderr, "%d: usageP %p (oUsageChain.prevP %p), timer %d, usage %d, next usage %d\n", usageCount, usageP, oUsageChain.prevP, usageP->chainTimer, usageP->virFHP->usage, usageP->nextP->virFHP->usage);
    assert(usageP == oUsageChain.prevP || usageP->virFHP->usage >= usageP->nextP->virFHP->usage);
    }
  assert(usageCount == oCurUsageChainElems);
  assert(usageCount <= kMaxUsageChainElems);
  }

if (oVirtualFHArrayP != NULL)
  {
    size_t fhIndex;

  for (fhIndex = 0; fhIndex < oCurVirtualFHArrayElems; ++fhIndex)
    {
      const virtual_fh_t *virFHP = &oVirtualFHArrayP[fhIndex];

    if (virFHP->usageP != NULL)
      {
        const usage_chain_t *usageP;

      assert(virFHP->fontNameP != NULL);
      assert(virFHP->xsize > 0 && virFHP->ysize > 0);
      assert(virFHP->xres > 0 && virFHP->yres > 0);
      for (usageP = oUsageChain.nextP;
           usageP != virFHP->usageP && usageP != &oUsageChain;
           usageP = usageP->nextP)
        /* no body */;
      assert(usageP == virFHP->usageP);
      }
    }
  }

return 0;
}

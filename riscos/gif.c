/*******************
 * GIF loader for Netsurf
 * Developed by Philip Pemberton for the Netsurf project
 *
 * Yes, this is an evil hack. You will not be tested on your understanding of
 * this code. Beware - dragons lurketh here.
 *
 * TO-DO:
 *   Add support for GIF transparency
 *   Add better error handling
 *      - especially where bad GIFs are concerned.
 *
 * $Id: gif.c,v 1.2 2003/06/05 14:49:48 philpem Exp $
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include "libungif/gif_lib.h"
#include "oslib/colourtrans.h"
#include "oslib/os.h"
#include "oslib/osspriteop.h"
#include "netsurf/content/content.h"
#include "netsurf/riscos/gif.h"
#include "netsurf/utils/log.h"
#include "netsurf/utils/utils.h"

/* maps colours to 256 mode colour numbers */
static os_colour_number colour_table[4096];

static int
    InterlacedOffset[] = { 0, 4, 2, 1 }, /* The way Interlaced image should. */
    InterlacedJumps[] = { 8, 8, 4, 2 };    /* be read - offsets and jumps... */


void nsgif_init(void)
{
  // Generate a colour lookup table
  // TODO: Use the PNG colour table instead of the GIF table...
	unsigned int red, green, blue;
	for (red = 0; red != 0xf; red++)
		for (green = 0; green != 0xf; green++)
			for (blue = 0; blue != 0xf; blue++)
				colour_table[red << 8 | green << 4 | blue] =
					colourtrans_return_colour_number_for_mode(
						blue << 28 | blue << 24 |
						green << 20 | green << 16 |
						red << 12 | red << 8, 21, 0);
}

// Called when Netsurf wants us to prepare to decode a GIF
void nsgif_create(struct content *c)
{
  // Clear the sprite area
  c->data.gif.sprite_area = 0;

  // Allocate some memory for the GIF file
  c->data.gif.data = xcalloc(0, 1);
  // Set the length and buffer position to zero (we haven't loaded any data
  // yet)
  c->data.gif.length = 0;
  c->data.gif.buffer_pos = 0;

  LOG(("gif object created"));
}

// Called when Netsurf has got some more data for us
void nsgif_process_data(struct content *c, char *data, unsigned long size)
{
  // We've just been given some more data!
  // Reallocate the memory block
  c->data.gif.data = xrealloc(c->data.gif.data, c->data.gif.length + size);
  // Copy the new data into our buffer
  memcpy(c->data.gif.data + c->data.gif.length, data, size);
  // Update the data length variables
  c->data.gif.length += size;
  c->size += size;
}

// This is the callback for the GIF loader. It grabs some data from the buffer
// and hands it over to Libungif.
// Returns the number of bytes successfully read from the buffer
int nsgif_input_callback(GifFileType *giffile, GifByteType *data, int length)
{
  struct content *c;

  // We need to set up the content block pointer first
  c = (struct content *)giffile->UserData;

  // Now check that there's enough data in the buffer
  if ((c->data.gif.buffer_pos + length) > c->data.gif.length)
  {
    // We don't have enough data in the buffer. Give libungif the data we've
    // got.
    memcpy(data, &c->data.gif.data[c->data.gif.buffer_pos], (c->data.gif.length - c->data.gif.buffer_pos));
    c->data.gif.buffer_pos += length;
    return (c->data.gif.length - c->data.gif.buffer_pos);
  }

  // Well, we've got enough data. Give libungif as much as it wants.
  memcpy(data, &c->data.gif.data[c->data.gif.buffer_pos], length);
  c->data.gif.buffer_pos += length;

  return length;
}

// Called when Netsurf wants us to convert the image
int nsgif_convert(struct content *c, unsigned int width, unsigned int height)
{
  char *row, **row_pointers;
  int i, j, bit_depth, color_type, log2bpp, interlace;
  unsigned int rowbytes, sprite_size;
  unsigned long width, height;
  os_palette *palette;
  os_sprite_palette *sprite_palette;
  osspriteop_area *sprite_area;
  osspriteop_header *sprite;
  // The next three lines are for vars. used by the gif decoding engine
  int recordtype, cur_row, extcode, count, got_image_data;
  GifByteType *extension;
  GifColorType *colormap;

  LOG(("Opening GIF file"));

  got_image_data = 0;   // we haven't read any image data yet

  // Now decode the GIF
  c->data.gif.giffile = DGifOpen(c, &nsgif_input_callback);
//  assert(c->data.gif.giffile != NULL);
  if (c->data.gif.giffile == NULL)
  {
    LOG(("ERROR: giffile is null! error %d", GifLastError()));
//    assert(1 == 2);
    return 0;  // more graceful exit
  }
  // TODO: ^^^ Error checking

  // Start creating the sprite
  width = c->data.gif.giffile->SWidth;
  height = c->data.gif.giffile->SHeight;

  LOG(("gif image width = %d, height = %d", width, height));

  sprite_size = sizeof(*sprite_area) + sizeof(*sprite);

  // GIFs can't be more than 256 colours (8 bits).
  sprite_size += 8 * 256 + height * ((width + 3) & ~3u);

  sprite_area = xcalloc(sprite_size + 1000, 1);
  sprite_area->size = sprite_size;
  sprite_area->sprite_count = 1;
  sprite_area->first = sizeof(*sprite_area);
  sprite_area->used = sprite_size;
  sprite = (osspriteop_header *) (sprite_area + 1);
  sprite->size = sprite_size - sizeof(*sprite_area);
  strcpy(sprite->name, "gif");
  sprite->height = height -1;
  c->data.gif.sprite_area = sprite_area;
  sprite->width = ((width + 3) & ~3u) / 4 - 1;
  sprite->left_bit = 0;
  sprite->right_bit = (8 * (((width - 1) % 4) + 1)) -1;
  sprite->mask = sprite->image = sizeof(*sprite) + 8 * 256;
  sprite->mode = (os_mode) 21;
  sprite_palette = (os_sprite_palette *) (sprite + 1);
  c->data.gif.sprite_image = ((char *) sprite) + sprite->image;
  c->width = width;
  c->height = height;

  do
  {
    if (DGifGetRecordType(c->data.gif.giffile, &recordtype) == GIF_ERROR)
    {
      LOG(("gif error %d", GifLastError()));
//      assert(1 == 2);
      return(0); // more graceful exit
    }
    // TODO: ^^^ better error checking
    switch (recordtype)
    {
      case IMAGE_DESC_RECORD_TYPE:
        // The next line is used to handle animated GIFs
        if (!got_image_data)
        {
          assert(DGifGetImageDesc(c->data.gif.giffile) != GIF_ERROR);
        // TODO: ^^^ better error checking

          c->data.gif.sprite_area = sprite_area;

          if (c->data.gif.giffile->Image.Interlace)
          {
            // The image is interlaced, therefore we need to perform four
            // passes over the image
            for (count = i = 0; i < 4; i++)
            {
              LOG(("Interlaced GIF file"));
              for (j=InterlacedOffset[i]; j < height; j += InterlacedJumps[i])
              {
                LOG(("gif line %d", count));
                count++;
                assert (DGifGetLine(c->data.gif.giffile, c->data.gif.sprite_image + j * ((c->width + 3) & ~3u), width) != GIF_ERROR);
                // TODO: ^^^ better error checking
              }
            }
          }
          else
          {
            LOG(("running DGifGetLine"));
            for (i=0; i<height; i++)
            {
  //            assert(DGifGetLine(c->data.gif.giffile, c->data.gif.sprite_image + i * ((c->width + 3) & ~3u), width) != GIF_ERROR);
              if (DGifGetLine(c->data.gif.giffile, c->data.gif.sprite_image + i * ((c->width + 3) & ~3u), width) == GIF_ERROR)
              {
                LOG(("error: gif line %d - error %d", i, GifLastError()));
                LOG(("exp height = %d, width = %d", height, width));
                assert(1 == 2);
              }
              // TODO: ^^^ better error checking
              LOG(("gif line %d", i));
            }
          }
          got_image_data = -1;
        } else recordtype = TERMINATE_RECORD_TYPE;
        break;
      case EXTENSION_RECORD_TYPE:
        // Skip any extension blocks in the file
        assert (DGifGetExtension(c->data.gif.giffile, &extcode, &extension) != GIF_ERROR);
        // TODO: ^^^ better error checking
        while (extension != NULL)
        {
          DGifGetExtensionNext(c->data.gif.giffile, &extension);
          // TODO: ^^^ better error checking
        }
        break;
      case TERMINATE_RECORD_TYPE:
        break;
      default:       // Should be trapped by DGifGetRecordType
        break;
    }
  } while (recordtype != TERMINATE_RECORD_TYPE);

  LOG(("all done"));

/*
   And now for the obligatory quote from "The Matrix"
   "Before you can bend the spoon, you must realise the truth."
   "And what is the truth?"
   "There is no spoon."
 */

//		for (i = 0; i != palette_size; i++)
//			sprite_palette->entries[i].on =
//			sprite_palette->entries[i].off =
//					png_palette[i].blue << 24 |
//					png_palette[i].green << 16 |
//					png_palette[i].red << 8 | 16;

  // FIXME: 255 is the size of the GIF palette
  // FIXME: Load the palette instead of creating a greyscale palette

  colormap = (c->data.gif.giffile->Image.ColorMap ?
              c->data.gif.giffile->Image.ColorMap->Colors :
              c->data.gif.giffile->SColorMap->Colors);

  for (i=0; i < 256; i++)
    sprite_palette->entries[i].on =
    sprite_palette->entries[i].off =
      (colormap[i].Blue << 24) |
      (colormap[i].Green << 16) |
      (colormap[i].Red << 8) |
      16;

  DGifCloseFile(c->data.gif.giffile);

  c->width = width;
  c->height = height;

  c->title = xcalloc(100, 1);
  sprintf(c->title, "GIF image (%ux%u)", c->width, c->height);

//  xosspriteop_save_sprite_file(osspriteop_USER_AREA, c->data.gif.sprite_area,
//          "gif");
  return 0;
}


void nsgif_revive(struct content *c, unsigned int width, unsigned int height)
{
}


void nsgif_reformat(struct content *c, unsigned int width, unsigned int height)
{
}

// Called when Netsurf is finished with us
void nsgif_destroy(struct content *c)
{
  xfree(c->title);
  xfree(c->data.gif.sprite_area);
  xfree(c->data.gif.data);
}

// Called when Netsurf wants us to draw the image
void nsgif_redraw(struct content *c, long x, long y,
		unsigned long width, unsigned long height)
{
	/* TODO: scale to width, height */
	int size;
	osspriteop_trans_tab *table;

  LOG(("Redraw fired"));

	xcolourtrans_generate_table_for_sprite(c->data.gif.sprite_area,
			(osspriteop_id) (c->data.gif.sprite_area + 1),
			colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
			0, colourtrans_GIVEN_SPRITE, 0, 0, &size);
	table = xcalloc(size, 1);
	xcolourtrans_generate_table_for_sprite(c->data.gif.sprite_area,
			(osspriteop_id) (c->data.gif.sprite_area + 1),
			colourtrans_CURRENT_MODE, colourtrans_CURRENT_PALETTE,
			table, colourtrans_GIVEN_SPRITE, 0, 0, 0);

	xosspriteop_put_sprite_scaled(osspriteop_PTR,
			c->data.gif.sprite_area,
			(osspriteop_id) (c->data.gif.sprite_area + 1),
			x, y, 0, 0, table);

	xfree(table);
}

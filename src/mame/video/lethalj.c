/***************************************************************************

    The Game Room Lethal Justice hardware

***************************************************************************/

#include "driver.h"
#include "cpu/tms34010/tms34010.h"
#include "lethalj.h"


#define BLITTER_SOURCE_WIDTH		1024
#define BLITTER_DEST_WIDTH			512
#define BLITTER_DEST_HEIGHT			512


static UINT16 blitter_data[8];

static UINT16 *screenram;
static UINT16 *blitter_base;
static int blitter_rows;

static UINT16 gunx, guny;
static UINT8 blank_palette;



/*************************************
 *
 *  Compute X/Y coordinates
 *
 *************************************/

INLINE void get_crosshair_xy(int player, int *x, int *y)
{
	*x = ((readinputport(2 + player * 2) & 0xff) * Machine->screen[0].width) / 255;
	*y = ((readinputport(3 + player * 2) & 0xff) * Machine->screen[0].height) / 255;
}



/*************************************
 *
 *  Gun input handling
 *
 *************************************/

READ16_HANDLER( lethalj_gun_r )
{
	UINT16 result = 0;
	int beamx, beamy;

	switch (offset)
	{
		case 4:
		case 5:
			/* latch the crosshair position */
			get_crosshair_xy(offset - 4, &beamx, &beamy);
			gunx = beamx;
			guny = beamy;
			blank_palette = 1;
			break;

		case 6:
			result = gunx/2;
			break;

		case 7:
			result = guny + 4;
			break;
	}
/*  logerror("%08X:lethalj_gun_r(%d) = %04X\n", activecpu_get_pc(), offset, result); */
	return result;
}



/*************************************
 *
 *  video startup
 *
 *************************************/

VIDEO_START( lethalj )
{
	/* allocate video RAM for screen */
	screenram = auto_malloc(BLITTER_DEST_WIDTH * BLITTER_DEST_HEIGHT * sizeof(screenram[0]));

	/* predetermine blitter info */
	blitter_base = (UINT16 *)memory_region(REGION_GFX1);
	blitter_rows = memory_region_length(REGION_GFX1) / (2*BLITTER_SOURCE_WIDTH);
}



/*************************************
 *
 *  Memory maps
 *
 *************************************/

static TIMER_CALLBACK( gen_ext1_int )
{
	cpunum_set_input_line(0, 0, ASSERT_LINE);
}



static void do_blit(void)
{
	int dsty = (INT16)blitter_data[1];
	int srcx = (UINT16)blitter_data[2];
	int srcy = (UINT16)blitter_data[3];
	int width = (UINT16)blitter_data[5];
	int dstx = (INT16)blitter_data[6];
	int height = (UINT16)blitter_data[7];
	int y;

	if (srcy == 0xffff)
	{
		return;
	}

/*  logerror("blitter data = %04X %04X %04X %04X %04X %04X %04X %04X\n",
            blitter_data[0], blitter_data[1], blitter_data[2], blitter_data[3],
            blitter_data[4], blitter_data[5], blitter_data[6], blitter_data[7]);*/

	/* loop over Y coordinates */
	for (y = 0; y <= height; y++, srcy++, dsty++)
	{
		UINT16 *source = blitter_base + srcy * BLITTER_SOURCE_WIDTH;
		UINT16 *dest = screenram + dsty * BLITTER_DEST_WIDTH;

		/* clip in Y */
		if (dsty >= 0 && dsty < BLITTER_DEST_HEIGHT)
		{
			int sx = srcx;
			int dx = dstx;
			int x;

			/* loop over X coordinates */
			for (x = 0; x <= width; x++, sx++, dx++)
				if (dx >= 0 && dx < BLITTER_DEST_WIDTH)
				{
					int pix = source[sx % BLITTER_SOURCE_WIDTH];
					if (pix)
						dest[dx] = pix;
				}
		}
	}
}



WRITE16_HANDLER( lethalj_blitter_w )
{
	/* combine the data */
	COMBINE_DATA(&blitter_data[offset]);

	/* blit on a write to offset 7, and signal an IRQ */
	if (offset == 7)
	{
		do_blit();
		mame_timer_set(MAME_TIME_IN_USEC(10), 0, gen_ext1_int);
	}

	/* clear the IRQ on offset 0 */
	else if (offset == 0)
		cpunum_set_input_line(0, 0, CLEAR_LINE);
}



/*************************************
 *
 *  video update
 *
 *************************************/

void lethalj_scanline_update(running_machine *machine, int screen, mame_bitmap *bitmap, int scanline, const tms34010_display_params *params)
{
	UINT16 *src = &screenram[(params->rowaddr << 9) & 0x3fe00];
	UINT16 *dest = BITMAP_ADDR16(bitmap, scanline, 0);
	int coladdr = params->coladdr << 1;
	int x;

	/* blank palette: fill with white */
	if (blank_palette)
	{
		for (x = params->heblnk; x < params->hsblnk; x++)
			dest[x] = 0x7fff;
		if (scanline == machine->screen[0].visarea.max_y)
			blank_palette = 0;
		return;
	}

	/* copy the non-blanked portions of this scanline */
	for (x = params->heblnk; x < params->hsblnk; x++)
		dest[x] = src[coladdr++ & 0x1ff] & 0x7fff;
}

/***************************************************************************

    Atari Blasteroids hardware

****************************************************************************/

#include "driver.h"
#include "machine/atarigen.h"
#include "blstroid.h"



/*************************************
 *
 *  Globals we own
 *
 *************************************/

UINT16 *blstroid_priorityram;



/*************************************
 *
 *  Tilemap callbacks
 *
 *************************************/

static TILE_GET_INFO( get_playfield_tile_info )
{
	UINT16 data = atarigen_playfield[tile_index];
	int code = data & 0x1fff;
	int color = (data >> 13) & 0x07;
	SET_TILE_INFO(0, code, color, 0);
}



/*************************************
 *
 *  Video system start
 *
 *************************************/

VIDEO_START( blstroid )
{
	static const struct atarimo_desc modesc =
	{
		1,					/* index to which gfx system */
		1,					/* number of motion object banks */
		1,					/* are the entries linked? */
		0,					/* are the entries split? */
		0,					/* render in reverse order? */
		0,					/* render in swapped X/Y order? */
		0,					/* does the neighbor bit affect the next object? */
		0,					/* pixels per SLIP entry (0 for no-slip) */
		0,					/* pixel offset for SLIPs */
		0,					/* maximum number of links to visit/scanline (0=all) */

		0x000,				/* base palette entry */
		0x100,				/* maximum number of colors */
		0,					/* transparent pen index */

		{{ 0,0,0x0ff8,0 }},	/* mask for the link */
		{{ 0 }},			/* mask for the graphics bank */
		{{ 0,0x3fff,0,0 }},	/* mask for the code index */
		{{ 0 }},			/* mask for the upper code index */
		{{ 0,0,0,0x000f }},	/* mask for the color */
		{{ 0,0,0,0xffc0 }},	/* mask for the X position */
		{{ 0xff80,0,0,0 }},	/* mask for the Y position */
		{{ 0 }},			/* mask for the width, in tiles*/
		{{ 0x000f,0,0,0 }},	/* mask for the height, in tiles */
		{{ 0,0x8000,0,0 }},	/* mask for the horizontal flip */
		{{ 0,0x4000,0,0 }},	/* mask for the vertical flip */
		{{ 0 }},			/* mask for the priority */
		{{ 0 }},			/* mask for the neighbor */
		{{ 0 }},			/* mask for absolute coordinates */

		{{ 0 }},			/* mask for the special value */
		0,					/* resulting value to indicate "special" */
		0					/* callback routine for special entries */
	};

	/* initialize the playfield */
	atarigen_playfield_tilemap = tilemap_create(get_playfield_tile_info, tilemap_scan_rows, TILEMAP_TYPE_PEN, 16,8, 64,64);

	/* initialize the motion objects */
	atarimo_init(machine, 0, &modesc);
}



/*************************************
 *
 *  Periodic scanline updater
 *
 *************************************/

static TIMER_CALLBACK( irq_off )
{
	/* clear the interrupt */
	atarigen_scanline_int_ack_w(0, 0, 0);
}


static TIMER_CALLBACK( irq_on )
{
	/* generate the interrupt */
	atarigen_scanline_int_gen();
	atarigen_update_interrupts();
}


void blstroid_scanline_update(running_machine *machine, int scrnum, int scanline)
{
	int offset = (scanline / 8) * 64 + 40;

	/* check for interrupts */
	if (offset < 0x1000)
		if (atarigen_playfield[offset] & 0x8000)
		{
			mame_time period_on;
			mame_time period_off;

			/* fix me - the only thing this IRQ does it tweak the starting MO link */
			/* unfortunately, it does it too early for the given MOs! */
			/* perhaps it is not actually hooked up on the real PCB... */
			return;

			/* set a timer to turn the interrupt on at HBLANK of the 7th scanline */
			/* and another to turn it off one scanline later */
			period_on  = video_screen_get_time_until_pos(scrnum, video_screen_get_vpos(scrnum) + 7, machine->screen[scrnum].width * 0.9);
			period_off = video_screen_get_time_until_pos(scrnum, video_screen_get_vpos(scrnum) + 8, machine->screen[scrnum].width * 0.9);

			mame_timer_set(period_on,  0, irq_on);
			mame_timer_set(period_off, 0, irq_off);
		}
}



/*************************************
 *
 *  Main refresh
 *
 *************************************/

VIDEO_UPDATE( blstroid )
{
	struct atarimo_rect_list rectlist;
	mame_bitmap *mobitmap;
	int x, y, r;

	/* draw the playfield */
	tilemap_draw(bitmap, cliprect, atarigen_playfield_tilemap, 0, 0);

	/* draw and merge the MO */
	mobitmap = atarimo_render(machine, 0, cliprect, &rectlist);
	for (r = 0; r < rectlist.numrects; r++, rectlist.rect++)
		for (y = rectlist.rect->min_y; y <= rectlist.rect->max_y; y++)
		{
			UINT16 *mo = (UINT16 *)mobitmap->base + mobitmap->rowpixels * y;
			UINT16 *pf = (UINT16 *)bitmap->base + bitmap->rowpixels * y;
			for (x = rectlist.rect->min_x; x <= rectlist.rect->max_x; x++)
				if (mo[x])
				{
					/* verified via schematics

                        priority address = HPPPMMMM
                    */
					int priaddr = ((pf[x] & 8) << 4) | (pf[x] & 0x70) | ((mo[x] & 0xf0) >> 4);
					if (blstroid_priorityram[priaddr] & 1)
						pf[x] = mo[x];

					/* erase behind ourselves */
					mo[x] = 0;
				}
		}
	return 0;
}

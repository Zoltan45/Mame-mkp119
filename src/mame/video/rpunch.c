/***************************************************************************

  video/rpunch.c

  Functions to emulate the video hardware of the machine.

****************************************************************************/

#include "driver.h"


#define BITMAP_WIDTH	304
#define BITMAP_HEIGHT	224
#define BITMAP_XOFFSET	4


/*************************************
 *
 *  Statics
 *
 *************************************/

UINT16 *rpunch_bitmapram;
size_t rpunch_bitmapram_size;

int rpunch_sprite_palette;

static tilemap *background[2];

static UINT16 videoflags;
static UINT8 crtc_register;
static mame_timer *crtc_timer;
static UINT8 bins, gins;


/*************************************
 *
 *  Tilemap callbacks
 *
 *************************************/

static TILE_GET_INFO( get_bg0_tile_info )
{
	int data = videoram16[tile_index];
	int code;
	if (videoflags & 0x0400)	code = (data & 0x0fff) | 0x2000;
	else						code = (data & 0x1fff);

	SET_TILE_INFO(
			0,
			code,
			((videoflags & 0x0010) >> 1) | ((data >> 13) & 7),
			0);
}

static TILE_GET_INFO( get_bg1_tile_info )
{
	int data = videoram16[videoram_size / 4 + tile_index];
	int code;
	if (videoflags & 0x0800)	code = (data & 0x0fff) | 0x2000;
	else						code = (data & 0x1fff);

	SET_TILE_INFO(
			1,
			code,
			((videoflags & 0x0020) >> 2) | ((data >> 13) & 7),
			0);
}


/*************************************
 *
 *  Video system start
 *
 *************************************/

static TIMER_CALLBACK( crtc_interrupt_gen )
{
	cpunum_set_input_line(0, 1, HOLD_LINE);
	if (param != 0)
		mame_timer_adjust(crtc_timer, make_mame_time(0, machine->screen[0].refresh / param), 0, make_mame_time(0, machine->screen[0].refresh / param));
}


VIDEO_START( rpunch )
{
	/* allocate tilemaps for the backgrounds */
	background[0] = tilemap_create(get_bg0_tile_info,tilemap_scan_cols,TILEMAP_TYPE_PEN,     8,8,64,64);
	background[1] = tilemap_create(get_bg1_tile_info,tilemap_scan_cols,TILEMAP_TYPE_PEN,8,8,64,64);

	/* configure the tilemaps */
	tilemap_set_transparent_pen(background[1],15);

	if (rpunch_bitmapram)
		memset(rpunch_bitmapram, 0xff, rpunch_bitmapram_size);

	/* reset the timer */
	crtc_timer = mame_timer_alloc(crtc_interrupt_gen);
}



/*************************************
 *
 *  Write handlers
 *
 *************************************/

WRITE16_HANDLER( rpunch_videoram_w )
{
	int tmap = offset >> 12;
	int tile_index = offset & 0xfff;
	COMBINE_DATA(&videoram16[offset]);
	tilemap_mark_tile_dirty(background[tmap],tile_index);
}


WRITE16_HANDLER( rpunch_videoreg_w )
{
	int oldword = videoflags;
	COMBINE_DATA(&videoflags);

	if (videoflags != oldword)
	{
		/* invalidate tilemaps */
		if ((oldword ^ videoflags) & 0x0410)
			tilemap_mark_all_tiles_dirty(background[0]);
		if ((oldword ^ videoflags) & 0x0820)
			tilemap_mark_all_tiles_dirty(background[1]);
	}
}


WRITE16_HANDLER( rpunch_scrollreg_w )
{
	if (ACCESSING_LSB && ACCESSING_MSB)
		switch (offset)
		{
			case 0:
				tilemap_set_scrolly(background[0], 0, data & 0x1ff);
				break;

			case 1:
				tilemap_set_scrollx(background[0], 0, data & 0x1ff);
				break;

			case 2:
				tilemap_set_scrolly(background[1], 0, data & 0x1ff);
				break;

			case 3:
				tilemap_set_scrollx(background[1], 0, data & 0x1ff);
				break;
		}
}


WRITE16_HANDLER( rpunch_crtc_data_w )
{
	if (ACCESSING_LSB)
	{
		data &= 0xff;
		switch (crtc_register)
		{
			/* only register we know about.... */
			case 0x0b:
				mame_timer_adjust(crtc_timer, video_screen_get_time_until_pos(0, Machine->screen[0].visarea.max_y + 1, 0), (data == 0xc0) ? 2 : 1, time_zero);
				break;

			default:
				logerror("CRTC register %02X = %02X\n", crtc_register, data & 0xff);
				break;
		}
	}
}


WRITE16_HANDLER( rpunch_crtc_register_w )
{
	if (ACCESSING_LSB)
		crtc_register = data & 0xff;
}


WRITE16_HANDLER( rpunch_ins_w )
{
	if (ACCESSING_LSB)
	{
		if (offset == 0)
		{
			gins = data & 0x3f;
			logerror("GINS = %02X\n", data & 0x3f);
		}
		else
		{
			bins = data & 0x3f;
			logerror("BINS = %02X\n", data & 0x3f);
		}
	}
}


/*************************************
 *
 *  Sprite routines
 *
 *************************************/

static void draw_sprites(running_machine *machine, mame_bitmap *bitmap, const rectangle *cliprect, int start, int stop)
{
	int offs;

	start *= 4;
	stop *= 4;

	/* draw the sprites */
	for (offs = start; offs < stop; offs += 4)
	{
		int data1 = spriteram16[offs + 1];
		int code = data1 & 0x7ff;

		int data0 = spriteram16[offs + 0];
		int data2 = spriteram16[offs + 2];
		int x = (data2 & 0x1ff) + 8;
		int y = 513 - (data0 & 0x1ff);
		int xflip = data1 & 0x1000;
		int yflip = data1 & 0x0800;
		int color = ((data1 >> 13) & 7) | ((videoflags & 0x0040) >> 3);

		if (x >= BITMAP_WIDTH) x -= 512;
		if (y >= BITMAP_HEIGHT) y -= 512;

		drawgfx(bitmap, machine->gfx[2],
				code, color + (rpunch_sprite_palette / 16), xflip, yflip, x, y, cliprect, TRANSPARENCY_PEN, 15);
	}
}


/*************************************
 *
 *  Bitmap routines
 *
 *************************************/

static void draw_bitmap(mame_bitmap *bitmap, const rectangle *cliprect)
{
	int colourbase;
	int xxx=512/4;
	int yyy=256;
	int x,y,count;

	colourbase = 512 + ((videoflags & 15) * 16);

	count = 0;

	for (y=0;y<yyy;y++)
	{
		for(x=0;x<xxx;x++)
		{
			int coldat;
			coldat = (rpunch_bitmapram[count]>>12)&0xf; if (coldat!=15) *BITMAP_ADDR16(bitmap, y, ((x*4+0)-4)&0x1ff) = coldat+colourbase;
			coldat = (rpunch_bitmapram[count]>>8 )&0xf; if (coldat!=15) *BITMAP_ADDR16(bitmap, y, ((x*4+1)-4)&0x1ff) = coldat+colourbase;
			coldat = (rpunch_bitmapram[count]>>4 )&0xf; if (coldat!=15) *BITMAP_ADDR16(bitmap, y, ((x*4+2)-4)&0x1ff) = coldat+colourbase;
			coldat = (rpunch_bitmapram[count]>>0 )&0xf; if (coldat!=15) *BITMAP_ADDR16(bitmap, y, ((x*4+3)-4)&0x1ff) = coldat+colourbase;
  	 		count++;
  		}
	}
}


/*************************************
 *
 *  Main screen refresh
 *
 *************************************/

VIDEO_UPDATE( rpunch )
{
	int effbins;

	/* this seems like the most plausible explanation */
	effbins = (bins > gins) ? gins : bins;

	tilemap_draw(bitmap,cliprect, background[0], 0,0);
	draw_sprites(machine, bitmap,cliprect, 0, effbins);
	tilemap_draw(bitmap,cliprect, background[1], 0,0);
	draw_sprites(machine, bitmap,cliprect, effbins, gins);
	if (rpunch_bitmapram)
		draw_bitmap(bitmap,cliprect);
	return 0;
}

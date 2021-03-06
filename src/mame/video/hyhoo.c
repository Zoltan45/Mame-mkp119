/******************************************************************************

    Video Hardware for Nichibutsu Mahjong series.

    Driver by Takahiro Nogi <nogi@kt.rim.or.jp> 2000/01/28 -

******************************************************************************/

#include "driver.h"
#include "nb1413m3.h"


UINT8 *hyhoo_clut;

static int blitter_destx, blitter_desty;
static int blitter_sizex, blitter_sizey;
static int blitter_src_addr;
static int blitter_direction_x, blitter_direction_y;
static int hyhoo_gfxrom;
static int hyhoo_dispflag;
static int hyhoo_highcolorflag;
static int hyhoo_flipscreen;

static mame_bitmap *hyhoo_tmpbitmap;
static void hyhoo_gfxdraw(void);


WRITE8_HANDLER( hyhoo_blitter_w )
{
	switch (offset)
	{
		case 0x00:	blitter_src_addr = (blitter_src_addr & 0xff00) | data;
					nb1413m3_gfxradr_l_w(0, data); break;
		case 0x01:	blitter_src_addr = (blitter_src_addr & 0x00ff) | (data << 8);
					nb1413m3_gfxradr_h_w(0, data); break;
		case 0x02:	blitter_destx = data; break;
		case 0x03:	blitter_desty = data; break;
		case 0x04:	blitter_sizex = data; break;
		case 0x05:	blitter_sizey = data;
					/* writing here also starts the blit */
					hyhoo_gfxdraw();
					break;
		case 0x06:	blitter_direction_x = (data >> 0) & 0x01;
					blitter_direction_y = (data >> 1) & 0x01;
					hyhoo_flipscreen = (~data >> 2) & 0x01;
					hyhoo_dispflag = (~data >> 3) & 0x01;
					break;
		case 0x07:	break;
	}
}


WRITE8_HANDLER( hyhoo_romsel_w )
{
	hyhoo_gfxrom = (((data & 0xc0) >> 4) + (data & 0x03));
	hyhoo_highcolorflag = data;
	nb1413m3_gfxrombank_w(0, data);

	if ((0x20000 * hyhoo_gfxrom) > (memory_region_length(REGION_GFX1) - 1))
	{
#ifdef MAME_DEBUG
		popmessage("GFXROM BANK OVER!!");
#endif
		hyhoo_gfxrom &= (memory_region_length(REGION_GFX1) / 0x20000 - 1);
	}
}


static TIMER_CALLBACK( blitter_timer_callback )
{
	nb1413m3_busyflag = 1;
}

void hyhoo_gfxdraw(void)
{
	UINT8 *GFX = memory_region(REGION_GFX1);

	int x, y;
	int dx1, dx2, dy;
	int startx, starty;
	int sizex, sizey;
	int skipx, skipy;
	int ctrx, ctry;
	int gfxaddr;
	UINT8 color, color1, color2;
	int r, g, b;
	pen_t pen;

	nb1413m3_busyctr = 0;

	hyhoo_gfxrom |= ((nb1413m3_sndrombank1 & 0x02) << 3);

	startx = blitter_destx + blitter_sizex;
	starty = blitter_desty + blitter_sizey;

	if (blitter_direction_x)
	{
		sizex = blitter_sizex ^ 0xff;
		skipx = 1;
	}
	else
	{
		sizex = blitter_sizex;
		skipx = -1;
	}

	if (blitter_direction_y)
	{
		sizey = blitter_sizey ^ 0xff;
		skipy = 1;
	}
	else
	{
		sizey = blitter_sizey;
		skipy = -1;
	}

	gfxaddr = (hyhoo_gfxrom << 17) + (blitter_src_addr << 1);

	for (y = starty, ctry = sizey; ctry >= 0; y += skipy, ctry--)
	{
		for (x = startx, ctrx = sizex; ctrx >= 0; x += skipx, ctrx--)
		{
			if ((gfxaddr > (memory_region_length(REGION_GFX1) - 1)))
			{
#ifdef MAME_DEBUG
				popmessage("GFXROM ADDRESS OVER!!");
#endif
				gfxaddr = 0;
			}

			color = GFX[gfxaddr++];

			dx1 = (2 * x + 0) & 0x1ff;
			dx2 = (2 * x + 1) & 0x1ff;
			dy = y & 0xff;

			if (hyhoo_highcolorflag & 0x04)
			{
				// direct mode

				if (color != 0xff)
				{
					if (hyhoo_highcolorflag & 0x20)
					{
						/* least significant bits */

						// src xxxxxxxx_bbbggrrr
						// dst xxbbbxxx_ggxxxrrr

						r = ((color & 0x07) >> 0) & 0x07;
						g = ((color & 0x18) >> 3) & 0x03;
						b = ((color & 0xe0) >> 5) & 0x07;

						pen = MAKE_RGB(pal6bit(r), pal5bit(g), pal5bit(b));

						*BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx1) = *BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx1) | pen;
						*BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx2) = *BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx2) | pen;
					}
					else
					{
						/* most significant bits */

						// src xxxxxxxx_bbgggrrr
						// dst bbxxxggg_xxrrrxxx

						r = ((color & 0x07) >> 0) & 0x07;
						g = ((color & 0x38) >> 3) & 0x07;
						b = ((color & 0xc0) >> 6) & 0x03;

						pen = MAKE_RGB(pal6bit(r << 3), pal5bit(g << 2), pal5bit(b << 3));

						*BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx1) = pen;
						*BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx2) = pen;
					}
				}
			}
			else
			{
				// lookup table mode

				if (blitter_direction_x)
				{
					// flip
					color1 = (color & 0x0f) >> 0;
					color2 = (color & 0xf0) >> 4;
				}
				else
				{
					// normal
					color1 = (color & 0xf0) >> 4;
					color2 = (color & 0x0f) >> 0;
				}

				if (hyhoo_clut[color1])
				{
					// src xxxxxxxx_bbgggrrr
					// dst bbxxxggg_xxrrrxxx

					r = ((~hyhoo_clut[color1] & 0x07) >> 0) & 0x07;
					g = ((~hyhoo_clut[color1] & 0x38) >> 3) & 0x07;
					b = ((~hyhoo_clut[color1] & 0xc0) >> 6) & 0x03;

					pen = MAKE_RGB(pal6bit(r << 3), pal5bit(g << 2), pal5bit(b << 3));

					*BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx1) = pen;
				}

				if (hyhoo_clut[color2])
				{
					// src xxxxxxxx_bbgggrrr
					// dst bbxxxggg_xxrrrxxx

					r = ((~hyhoo_clut[color2] & 0x07) >> 0) & 0x07;
					g = ((~hyhoo_clut[color2] & 0x38) >> 3) & 0x07;
					b = ((~hyhoo_clut[color2] & 0xc0) >> 6) & 0x03;

					pen = MAKE_RGB(pal6bit(r << 3), pal5bit(g << 2), pal5bit(b << 3));

					*BITMAP_ADDR32(hyhoo_tmpbitmap, dy, dx2) = pen;
				}
			}

			nb1413m3_busyctr++;
		}
	}

	nb1413m3_busyflag = 0;
	mame_timer_set(scale_up_mame_time(MAME_TIME_IN_HZ(400000), nb1413m3_busyctr), 0, blitter_timer_callback);
}


VIDEO_START( hyhoo )
{
	hyhoo_tmpbitmap = auto_bitmap_alloc(machine->screen[0].width, machine->screen[0].height, machine->screen[0].format);
}


VIDEO_UPDATE( hyhoo )
{
	if (hyhoo_dispflag)
		copybitmap(bitmap, hyhoo_tmpbitmap, hyhoo_flipscreen, hyhoo_flipscreen, 0, 0, cliprect, TRANSPARENCY_NONE, 0);
	else
		fillbitmap(bitmap, RGB_BLACK, cliprect);

	return 0;
}

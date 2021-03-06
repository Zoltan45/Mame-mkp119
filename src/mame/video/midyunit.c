/*************************************************************************

    Williams/Midway Y/Z-unit system

**************************************************************************/

#include "driver.h"
#include "profiler.h"
#include "cpu/tms34010/tms34010.h"
#include "cpu/tms34010/34010ops.h"
#include "midyunit.h"


/* compile-time options */
#define LOG_DMA				0		/* DMAs are logged if the 'L' key is pressed */


/* constants for the DMA chip */
enum
{
	DMA_COMMAND = 0,
	DMA_ROWBYTES,
	DMA_OFFSETLO,
	DMA_OFFSETHI,
	DMA_XSTART,
	DMA_YSTART,
	DMA_WIDTH,
	DMA_HEIGHT,
	DMA_PALETTE,
	DMA_COLOR
};



/* graphics-related variables */
       UINT8 *	midyunit_gfx_rom;
       size_t	midyunit_gfx_rom_size;
static UINT8	autoerase_enable;

/* palette-related variables */
static UINT32	palette_mask;
static pen_t *	pen_map;

/* videoram-related variables */
static UINT16 *	local_videoram;
static UINT8	videobank_select;

/* DMA-related variables */
static UINT8	yawdim_dma;
static UINT16	dma_register[16];
static struct
{
	UINT32		offset;			/* source offset, in bits */
	INT32 		rowbytes;		/* source bytes to skip each row */
	INT32 		xpos;			/* x position, clipped */
	INT32		ypos;			/* y position, clipped */
	INT32		width;			/* horizontal pixel count */
	INT32		height;			/* vertical pixel count */
	UINT16		palette;		/* palette base */
	UINT16		color;			/* current foreground color with palette */
} dma_state;



/*************************************
 *
 *  Video startup
 *
 *************************************/

static VIDEO_START( common )
{
	/* allocate memory */
	midyunit_cmos_ram = auto_malloc(0x2000 * 4);
	local_videoram = auto_malloc(0x80000);
	pen_map = auto_malloc(65536 * sizeof(pen_map[0]));

	/* we have to erase this since we rely on upper bits being 0 */
	memset(local_videoram, 0, 0x80000);

	/* reset all the globals */
	midyunit_cmos_page = 0;
	autoerase_enable = 0;
	yawdim_dma = 0;

	/* reset DMA state */
	memset(dma_register, 0, sizeof(dma_register));
	memset(&dma_state, 0, sizeof(dma_state));

	/* register for state saving */
	state_save_register_global(autoerase_enable);
	state_save_register_global_pointer(local_videoram, 0x80000/sizeof(local_videoram[0]));
	state_save_register_global_pointer(midyunit_cmos_ram, (0x2000 * 4)/sizeof(midyunit_cmos_ram[0]));
	state_save_register_global(videobank_select);
	state_save_register_global_array(dma_register);
}


VIDEO_START( midyunit_4bit )
{
	int i;

	video_start_common(machine);

	/* init for 4-bit */
	for (i = 0; i < 65536; i++)
		pen_map[i] = ((i & 0xf000) >> 8) | (i & 0x000f);
	palette_mask = 0x00ff;
}


VIDEO_START( midyunit_6bit )
{
	int i;

	video_start_common(machine);

	/* init for 6-bit */
	for (i = 0; i < 65536; i++)
		pen_map[i] = ((i & 0xc000) >> 8) | (i & 0x0f3f);
	palette_mask = 0x0fff;
}


VIDEO_START( mkyawdim )
{
	video_start_midyunit_6bit(machine);
	yawdim_dma = 1;
}


VIDEO_START( midzunit )
{
	int i;

	video_start_common(machine);

	/* init for 8-bit */
	for (i = 0; i < 65536; i++)
		pen_map[i] = i & 0x1fff;
	palette_mask = 0x1fff;
}



/*************************************
 *
 *  Banked graphics ROM access
 *
 *************************************/

READ16_HANDLER( midyunit_gfxrom_r )
{
	offset *= 2;
	if (palette_mask == 0x00ff)
		return midyunit_gfx_rom[offset] | (midyunit_gfx_rom[offset] << 4) |
				(midyunit_gfx_rom[offset + 1] << 8) | (midyunit_gfx_rom[offset + 1] << 12);
	else
		return midyunit_gfx_rom[offset] | (midyunit_gfx_rom[offset + 1] << 8);
}



/*************************************
 *
 *  Video/color RAM read/write
 *
 *************************************/

WRITE16_HANDLER( midyunit_vram_w )
{
	offset *= 2;
	if (videobank_select)
	{
		if (ACCESSING_LSB)
			local_videoram[offset] = (data & 0x00ff) | (dma_register[DMA_PALETTE] << 8);
		if (ACCESSING_MSB)
			local_videoram[offset + 1] = (data >> 8) | (dma_register[DMA_PALETTE] & 0xff00);
	}
	else
	{
		if (ACCESSING_LSB)
			local_videoram[offset] = (local_videoram[offset] & 0x00ff) | (data << 8);
		if (ACCESSING_MSB)
			local_videoram[offset + 1] = (local_videoram[offset + 1] & 0x00ff) | (data & 0xff00);
	}
}


READ16_HANDLER( midyunit_vram_r )
{
	offset *= 2;
	if (videobank_select)
		return (local_videoram[offset] & 0x00ff) | (local_videoram[offset + 1] << 8);
	else
		return (local_videoram[offset] >> 8) | (local_videoram[offset + 1] & 0xff00);
}



/*************************************
 *
 *  Shift register read/write
 *
 *************************************/

void midyunit_to_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	memcpy(shiftreg, &local_videoram[address >> 3], 2 * 512 * sizeof(UINT16));
}


void midyunit_from_shiftreg(UINT32 address, UINT16 *shiftreg)
{
	memcpy(&local_videoram[address >> 3], shiftreg, 2 * 512 * sizeof(UINT16));
}



/*************************************
 *
 *  Y/Z-unit control register
 *
 *************************************/

WRITE16_HANDLER( midyunit_control_w )
{
	/*
     * Narc system register
     * ------------------
     *
     *   | Bit              | Use
     * --+-FEDCBA9876543210-+------------
     *   | xxxxxxxx-------- |   7 segment led on CPU board
     *   | --------xx------ |   CMOS page
     *   | ----------x----- | - OBJ PAL RAM select
     *   | -----------x---- | - autoerase enable
     *   | ---------------- | - watchdog
     *
     */

	if (ACCESSING_LSB)
	{
		/* CMOS page is bits 6-7 */
		midyunit_cmos_page = ((data >> 6) & 3) * 0x1000;

		/* video bank select is bit 5 */
		videobank_select = (data >> 5) & 1;

		/* handle autoerase disable (bit 4) */
		autoerase_enable = ((data & 0x10) == 0);
	}
}



/*************************************
 *
 *  Palette handlers
 *
 *************************************/

WRITE16_HANDLER( midyunit_paletteram_w )
{
	int newword;

	COMBINE_DATA(&paletteram16[offset]);
	newword = paletteram16[offset];
	palette_set_color_rgb(Machine, offset & palette_mask, pal5bit(newword >> 10), pal5bit(newword >> 5), pal5bit(newword >> 0));
}



/*************************************
 *
 *  DMA drawing routines
 *
 *************************************/

/*** constant definitions ***/
#define	PIXEL_SKIP		0
#define PIXEL_COLOR		1
#define PIXEL_COPY		2

#define XFLIP_NO		0
#define XFLIP_YES		1


typedef void (*dma_draw_func)(void);


/*** core blitter routine macro ***/
#define DMA_DRAW_FUNC_BODY(name, xflip, zero, nonzero)				 			\
{																				\
	int height = dma_state.height;												\
	int width = dma_state.width;												\
	UINT8 *base = midyunit_gfx_rom;												\
	UINT32 offset = dma_state.offset >> 3;										\
	UINT16 pal = dma_state.palette;												\
	UINT16 color = pal | dma_state.color;										\
	int x, y;																	\
																				\
	/* loop over the height */													\
	for (y = 0; y < height; y++)												\
	{																			\
		int tx = dma_state.xpos;												\
		int ty = dma_state.ypos;												\
		UINT32 o = offset;														\
		UINT16 *dest;															\
																				\
		/* determine Y position */												\
		ty = (ty + y) & 0x1ff;													\
		offset += dma_state.rowbytes;											\
																				\
		/* check for over/underrun */											\
		if (o >= 0x06000000)													\
			continue;															\
																				\
		/* determine destination pointer */										\
		dest = &local_videoram[ty * 512];										\
																				\
		/* loop until we draw the entire width */								\
		for (x = 0; x < width; x++, o++)										\
		{																		\
			/* special case similar handling of zero/non-zero */				\
			if (zero == nonzero)												\
			{																	\
				if (zero == PIXEL_COLOR)										\
					dest[tx] = color;											\
				else if (zero == PIXEL_COPY)									\
					dest[tx] = base[o] | pal;									\
			}																	\
																				\
			/* otherwise, read the pixel and look */							\
			else																\
			{																	\
				int pixel = base[o];											\
																				\
				/* non-zero pixel case */										\
				if (pixel)														\
				{																\
					if (nonzero == PIXEL_COLOR)									\
						dest[tx] = color;										\
					else if (nonzero == PIXEL_COPY)								\
						dest[tx] = pixel | pal;									\
				}																\
																				\
				/* zero pixel case */											\
				else															\
				{																\
					if (zero == PIXEL_COLOR)									\
						dest[tx] = color;										\
					else if (zero == PIXEL_COPY)								\
						dest[tx] = pal;											\
				}																\
			}																	\
																				\
			/* update pointers */												\
			if (xflip) tx--;													\
			else tx++;															\
		}																		\
	}																			\
}

/*** slightly simplified one for most blitters ***/
#define DMA_DRAW_FUNC(name, xflip, zero, nonzero)						\
static void name(void)													\
{																		\
	DMA_DRAW_FUNC_BODY(name, xflip, zero, nonzero)						\
}

/*** empty blitter ***/
static void dma_draw_none(void)
{
}

/*** super macro for declaring an entire blitter family ***/
#define DECLARE_BLITTER_SET(prefix)										\
DMA_DRAW_FUNC(prefix##_p0,      XFLIP_NO,  PIXEL_COPY,  PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_p1,      XFLIP_NO,  PIXEL_SKIP,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0,      XFLIP_NO,  PIXEL_COLOR, PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_c1,      XFLIP_NO,  PIXEL_SKIP,  PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_p0p1,    XFLIP_NO,  PIXEL_COPY,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0c1,    XFLIP_NO,  PIXEL_COLOR, PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_c0p1,    XFLIP_NO,  PIXEL_COLOR, PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_p0c1,    XFLIP_NO,  PIXEL_COPY,  PIXEL_COLOR)	\
																		\
DMA_DRAW_FUNC(prefix##_p0_xf,   XFLIP_YES, PIXEL_COPY,  PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_p1_xf,   XFLIP_YES, PIXEL_SKIP,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0_xf,   XFLIP_YES, PIXEL_COLOR, PIXEL_SKIP)		\
DMA_DRAW_FUNC(prefix##_c1_xf,   XFLIP_YES, PIXEL_SKIP,  PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_p0p1_xf, XFLIP_YES, PIXEL_COPY,  PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_c0c1_xf, XFLIP_YES, PIXEL_COLOR, PIXEL_COLOR)	\
DMA_DRAW_FUNC(prefix##_c0p1_xf, XFLIP_YES, PIXEL_COLOR, PIXEL_COPY)		\
DMA_DRAW_FUNC(prefix##_p0c1_xf, XFLIP_YES, PIXEL_COPY,  PIXEL_COLOR)	\
																												\
static dma_draw_func prefix[32] =																				\
{																												\
/*  B0:N / B1:N         B0:Y / B1:N         B0:N / B1:Y         B0:Y / B1:Y */									\
	dma_draw_none,		prefix##_p0,		prefix##_p1,		prefix##_p0p1,		/* no color */ 				\
	prefix##_c0,		prefix##_c0,		prefix##_c0p1,		prefix##_c0p1,		/* color 0 pixels */		\
	prefix##_c1,		prefix##_p0c1,		prefix##_c1,		prefix##_p0c1,		/* color non-0 pixels */	\
	prefix##_c0c1,		prefix##_c0c1,		prefix##_c0c1,		prefix##_c0c1,		/* fill */ 					\
																												\
	dma_draw_none,		prefix##_p0_xf,		prefix##_p1_xf,		prefix##_p0p1_xf,	/* no color */ 				\
	prefix##_c0_xf,		prefix##_c0_xf,		prefix##_c0p1_xf,	prefix##_c0p1_xf,	/* color 0 pixels */ 		\
	prefix##_c1_xf,		prefix##_p0c1_xf,	prefix##_c1_xf,		prefix##_p0c1_xf,	/* color non-0 pixels */	\
	prefix##_c0c1_xf,	prefix##_c0c1_xf,	prefix##_c0c1_xf,	prefix##_c0c1_xf	/* fill */ 					\
};

/*** blitter family declarations ***/
DECLARE_BLITTER_SET(dma_draw)



/*************************************
 *
 *  DMA finished callback
 *
 *************************************/

static TIMER_CALLBACK( dma_callback )
{
	dma_register[DMA_COMMAND] &= ~0x8000; /* tell the cpu we're done */
	cpunum_set_input_line(0, 0, ASSERT_LINE);
}



/*************************************
 *
 *  DMA reader
 *
 *************************************/

READ16_HANDLER( midyunit_dma_r )
{
	return dma_register[offset];
}



/*************************************
 *
 *  DMA write handler
 *
 *************************************/

/*
 * DMA registers
 * ------------------
 *
 *  Register | Bit              | Use
 * ----------+-FEDCBA9876543210-+------------
 *     0     | x--------------- | trigger write (or clear if zero)
 *           | ---184-1-------- | unknown
 *           | ----------x----- | flip y
 *           | -----------x---- | flip x
 *           | ------------x--- | blit nonzero pixels as color
 *           | -------------x-- | blit zero pixels as color
 *           | --------------x- | blit nonzero pixels
 *           | ---------------x | blit zero pixels
 *     1     | xxxxxxxxxxxxxxxx | width offset
 *     2     | xxxxxxxxxxxxxxxx | source address low word
 *     3     | xxxxxxxxxxxxxxxx | source address high word
 *     4     | xxxxxxxxxxxxxxxx | detination x
 *     5     | xxxxxxxxxxxxxxxx | destination y
 *     6     | xxxxxxxxxxxxxxxx | image columns
 *     7     | xxxxxxxxxxxxxxxx | image rows
 *     8     | xxxxxxxxxxxxxxxx | palette
 *     9     | xxxxxxxxxxxxxxxx | color
 */

WRITE16_HANDLER( midyunit_dma_w )
{
	UINT32 gfxoffset;
	int command;

	/* blend with the current register contents */
	COMBINE_DATA(&dma_register[offset]);

	/* only writes to DMA_COMMAND actually cause actions */
	if (offset != DMA_COMMAND)
		return;

	/* high bit triggers action */
	command = dma_register[DMA_COMMAND];
	cpunum_set_input_line(0, 0, CLEAR_LINE);
	if (!(command & 0x8000))
		return;

#if LOG_DMA
	if (input_code_pressed(KEYCODE_L))
	{
		logerror("----\n");
		logerror("DMA command %04X: (xflip=%d yflip=%d)\n",
				command, (command >> 4) & 1, (command >> 5) & 1);
		logerror("  offset=%08X pos=(%d,%d) w=%d h=%d rb=%d\n",
				dma_register[DMA_OFFSETLO] | (dma_register[DMA_OFFSETHI] << 16),
				(INT16)dma_register[DMA_XSTART], (INT16)dma_register[DMA_YSTART],
				dma_register[DMA_WIDTH], dma_register[DMA_HEIGHT], (INT16)dma_register[DMA_ROWBYTES]);
		logerror("  palette=%04X color=%04X\n",
				dma_register[DMA_PALETTE], dma_register[DMA_COLOR]);
	}
#endif

	profiler_mark(PROFILER_USER1);

	/* fill in the basic data */
	dma_state.rowbytes = (INT16)dma_register[DMA_ROWBYTES];
	dma_state.xpos = (INT16)dma_register[DMA_XSTART];
	dma_state.ypos = (INT16)dma_register[DMA_YSTART];
	dma_state.width = dma_register[DMA_WIDTH];
	dma_state.height = dma_register[DMA_HEIGHT];
	dma_state.palette = dma_register[DMA_PALETTE] << 8;
	dma_state.color = dma_register[DMA_COLOR] & 0xff;

	/* determine the offset and adjust the rowbytes */
	gfxoffset = dma_register[DMA_OFFSETLO] | (dma_register[DMA_OFFSETHI] << 16);
	if (command & 0x10)
	{
		if (!yawdim_dma)
		{
			gfxoffset -= (dma_state.width - 1) * 8;
			dma_state.rowbytes = (dma_state.rowbytes - dma_state.width + 3) & ~3;
		}
		else
			dma_state.rowbytes = (dma_state.rowbytes + dma_state.width + 3) & ~3;
		dma_state.xpos += dma_state.width - 1;
	}
	else
		dma_state.rowbytes = (dma_state.rowbytes + dma_state.width + 3) & ~3;

	/* apply Y clipping */
	if (dma_state.ypos < 0)
	{
		dma_state.height -= -dma_state.ypos;
		dma_state.offset += (-dma_state.ypos * dma_state.rowbytes) << 3;
		dma_state.ypos = 0;
	}
	if (dma_state.ypos + dma_state.height > 512)
		dma_state.height = 512 - dma_state.ypos;

	/* apply X clipping */
	if (!(command & 0x10))
	{
		if (dma_state.xpos < 0)
		{
			dma_state.width -= -dma_state.xpos;
			dma_state.offset += -dma_state.xpos << 3;
			dma_state.xpos = 0;
		}
		if (dma_state.xpos + dma_state.width > 512)
			dma_state.width = 512 - dma_state.xpos;
	}
	else
	{
		if (dma_state.xpos >= 512)
		{
			dma_state.width -= dma_state.xpos - 511;
			dma_state.offset += (dma_state.xpos - 511) << 3;
			dma_state.xpos = 511;
		}
		if (dma_state.xpos - dma_state.width < 0)
			dma_state.width = dma_state.xpos;
	}

	/* special case: drawing mode C doesn't need to know about any pixel data */
	/* shimpact relies on this behavior */
	if ((command & 0x0f) == 0x0c)
		gfxoffset = 0;

	/* determine the location and draw */
	if (gfxoffset < 0x02000000)
		gfxoffset += 0x02000000;
	if (gfxoffset < 0x06000000)
	{
		dma_state.offset = gfxoffset - 0x02000000;
		(*dma_draw[command & 0x1f])();
	}

	/* signal we're done */
	mame_timer_set(MAME_TIME_IN_NSEC(41 * dma_state.width * dma_state.height), 0, dma_callback);

	profiler_mark(PROFILER_END);
}



/*************************************
 *
 *  Core refresh routine
 *
 *************************************/

static TIMER_CALLBACK( autoerase_line )
{
	int scanline = param;

	if (autoerase_enable && scanline >= 0 && scanline < 510)
		memcpy(&local_videoram[512 * scanline], &local_videoram[512 * (510 + (scanline & 1))], 512 * sizeof(UINT16));
}


void midyunit_scanline_update(running_machine *machine, int screen, mame_bitmap *bitmap, int scanline, const tms34010_display_params *params)
{
	UINT16 *src = &local_videoram[(params->rowaddr << 9) & 0x3fe00];
	UINT16 *dest = BITMAP_ADDR16(bitmap, scanline, 0);
	int coladdr = params->coladdr << 1;
	int x;

	/* adjust the display address to account for ignored bits */
	for (x = params->heblnk; x < params->hsblnk; x++)
		dest[x] = pen_map[src[coladdr++ & 0x1ff]];

	/* handle autoerase on the previous line */
	autoerase_line(machine, params->rowaddr - 1);

	/* if this is the last update of the screen, set a timer to clear out the final line */
	/* (since we update one behind) */
	if (scanline == machine->screen[0].visarea.max_y)
		mame_timer_set(video_screen_get_time_until_pos(0, scanline + 1, 0), params->rowaddr, autoerase_line);
}

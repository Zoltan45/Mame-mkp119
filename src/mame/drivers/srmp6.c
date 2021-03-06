/*
    Super Real Mahjong P6 (JPN Ver.)
    (c)1996 Seta

WIP driver by Sebastien Volpe, Tomasz Slanina and David Haywood

Emulation Notes:
The graphics are compressed, using the same 8bpp RLE scheme as CPS3 uses
for the background on Sean's stage of Street Fighter III.

DMA Operations are not fully understood


according prg ROM (offset $0fff80):

    S12 SYSTEM
    SUPER REAL MAJAN P6
    SETA CO.,LTD
    19960410
    V1.00

TODO:
 - sound emulation (appears to be really close to st0016)
 - fix DMA operations
 - fix video emulation

Are there other games on this 'System S12' hardware ???

---------------- dump infos ----------------

[Jun/15/2000]

Super Real Mahjong P6 (JPN Ver.)
(c)1996 Seta

SX011
E47-REV01B

CPU:    68000-16
Sound:  NiLe
OSC:    16.0000MHz
        42.9545MHz
        56.0000MHz

Chips:  ST-0026 NiLe (video, sound)
        ST-0017


SX011-01.22  chr, samples (?)
SX011-02.21
SX011-03.20
SX011-04.19
SX011-05.18
SX011-06.17
SX011-07.16
SX011-08.15

SX011-09.10  68000 data

SX011-10.4   68000 prg.
SX011-11.5


Dumped 06/15/2000

*/


#include "driver.h"

static UINT16* tileram;
static UINT8* dirty_tileram;
static UINT16* dmaram;

static UINT16 *sprram;

#define SRMP6_VERBOSE 0

static const gfx_layout tiles8x8_layout =
{
	8,8,
	(0x100000*16)/0x40,
	8,
	{ 0,1,2,3,4,5,6,7 },
	{ 0*8,1*8,2*8,3*8,4*8,5*8,6*8,7*8 },
	{ 0*64,1*64,2*64,3*64,4*64,5*64,6*64,7*64 },
	8*64
};


static VIDEO_START(srmp6)
{
	/* create the char set (gfx will then be updated dynamically from RAM) */
	machine->gfx[0] = allocgfx(&tiles8x8_layout);
	machine->gfx[0]->total_colors = machine->drv->total_colors / 256;
	machine->gfx[0]->color_granularity=256;

	tileram = auto_malloc(0x100000*16);
	dirty_tileram = auto_malloc((0x100000*16)/0x40); // every 8x8x8 tile is 0x40 bytes

	memset(tileram,(0x100000*16),0x00);
	memset(dirty_tileram,(0x100000*16)/0x40,1);

	dmaram = auto_malloc(0x100);
}

/* Debug code */
static void srmp6_decode_charram(void)
{
	if(input_code_pressed_once(KEYCODE_Z))
	{
		int i;
		for (i=0;i<(0x100000*16)/0x40;i++)
		{
			decodechar(Machine->gfx[0], i, (UINT8*)tileram, &tiles8x8_layout);
			dirty_tileram[i] = 0;
		}
	}
}

#if 0
static int xixi=0;
#endif

static VIDEO_UPDATE(srmp6)
{
	int x,y,tileno,height,width,xw,yw,sprite,xb,yb;
	UINT16 *sprite_list=sprram;
	UINT16 mainlist_offset = 0;

	union
	{
		INT16  a;
		UINT16 b;
	} temp;

	fillbitmap(bitmap,0,&machine->screen[0].visarea);

#if 0
	/* debug */
	srmp6_decode_charram();



	/* debug */
	if(input_code_pressed_once(KEYCODE_Q))
	{
		++xixi;
		printf("%x\n",xixi);
	}

	if(input_code_pressed_once(KEYCODE_W))
	{
		--xixi;
		printf("%x\n",xixi);
	}
#endif

	/* Main spritelist is 0x0000 - 0x1fff in spriteram, sublists follow */
	while (mainlist_offset<0x2000/2)
	{

		UINT16 *sprite_sublist=&sprram[sprite_list[mainlist_offset+1]<<3];
		UINT16 sublist_length=sprite_list[mainlist_offset+0]&0x7fff; //+1 ?
		INT16 global_x,global_y;
		UINT16 global_pal;

		/* end of list marker */
		if (sprite_list[mainlist_offset+0] == 0x8000)
			return 0;


		if(sprite_list[mainlist_offset+0]!=0)
		{
			temp.b=sprite_list[mainlist_offset+2];
			global_x=temp.a;
			temp.b=sprite_list[mainlist_offset+3];
			global_y=temp.a;

			global_pal = sprite_list[mainlist_offset+4] & 0xf;

	//  printf("%x %x \n",sprite_list[mainlist_offset+1],sublist_length);

			while(sublist_length)
			{
				sprite=sprite_sublist[0]&0x3fff;
				temp.b=sprite_sublist[2];
				x=temp.a;
				temp.b=sprite_sublist[3];
				y=temp.a;
				x+=global_x;
				y+=global_y;

				width=((sprite_sublist[1])&0x3);
				height=((sprite_sublist[1]>>2)&0x3);

				height = 1 << height;
				width = 1 << width;

				tileno = sprite&0x3fff;
				//tileno += (sprite_list[4]&0xf)*0x4000; // this makes things worse in places (title screen for example)

				for(xw=0;xw<width;xw++)
				{
					for(yw=0;yw<height;yw++)
					{

						xb=x+xw*8;
						yb=y+yw*8;

						if (dirty_tileram[tileno])
						{
							decodechar(Machine->gfx[0], tileno, (UINT8*)tileram, &tiles8x8_layout);
							dirty_tileram[tileno] = 0;
						}

						drawgfx(bitmap,machine->gfx[0],tileno,global_pal,0,0,xb,yb,cliprect,TRANSPARENCY_PEN,0);
						tileno++;
		 			}
				}

				sprite_sublist+=8;
				--sublist_length;
			}
		}
		mainlist_offset+=8;
	}

	return 0;
}

/***************************************************************************
    audio - VERY preliminary

    TODO: watch similarities with st0016
***************************************************************************/

/*
cpu #0 (PC=00011F7C): unmapped program memory word write to 004E0002 = 0000 & FFFF  0?
cpu #0 (PC=00011F84): unmapped program memory word write to 004E0004 = 0D50 & FFFF  lo word \ sample start
cpu #0 (PC=00011F8A): unmapped program memory word write to 004E0006 = 0070 & FFFF  hi word / address?

cpu #0 (PC=00011FBA): unmapped program memory word write to 004E000A = 0000 & FFFF  0?
cpu #0 (PC=00011FC2): unmapped program memory word write to 004E0018 = 866C & FFFF  lo word \ sample stop
cpu #0 (PC=00011FC8): unmapped program memory word write to 004E001A = 00AA & FFFF  hi word / address?

cpu #0 (PC=00011FD0): unmapped program memory word write to 004E000C = 0FFF & FFFF
cpu #0 (PC=00011FD8): unmapped program memory word write to 004E001C = FFFF & FFFF
cpu #0 (PC=00011FDC): unmapped program memory word write to 004E001E = FFFF & FFFF

cpu #0 (PC=00026EFA): unmapped program memory word write to 004E0100 = 0001 & FFFF  ctrl word r/w: bit 7-0 = voice 8-1 (1=play,0=stop)

voice #1: $4E0000-$4E001F
voice #2: $4E0020-$4E003F
voice #3: $4E0040-$4E005F
...
voice #8: $4E00E0-$4E00FF

voice regs:
offset  description
+00
+02     always 0?
+04     lo-word \ sample
+06     hi-word / counter ?
+0A
+18 \
+1A /
+1C \
+1E /

*/

/***************************************************************************
    Main CPU memory handlers
***************************************************************************/

static UINT16 srmp6_input_select = 0;

static WRITE16_HANDLER( srmp6_input_select_w )
{
	srmp6_input_select = data & 0x0f;
}

static READ16_HANDLER( srmp6_inputs_r )
{
	if (offset == 0)			// DSW
		return readinputport(4);

	switch(srmp6_input_select)	// inputs
	{
		case 1<<0: return readinputport(0);
		case 1<<1: return readinputport(1);
		case 1<<2: return readinputport(2);
		case 1<<3: return readinputport(3);
	}

	return 0;
}


static UINT16 *video_regs;

static WRITE16_HANDLER( video_regs_w )
{
	switch(offset)
	{

		case 0x5e/2: // bank switch, used by ROM check
#if SRMP6_VERBOSE
			printf("%x\n",data);
#endif

			memory_set_bankptr(1,(UINT16 *)(memory_region(REGION_USER2) + (data & 0x0f)*0x200000));
			break;


		/* unknown registers - there are others */

		// set by IT4 (jsr $b3c), according flip screen dsw
		case 0x48/2: //     0 /  0xb0 if flipscreen
		case 0x52/2: //     0 / 0x2ef if flipscreen
		case 0x54/2: // 0x152 / 0x15e if flipscreen

		// set by IT4 ($82e-$846)
		case 0x56/2: // written 8,9,8,9 successively

		// set by IT4
		case 0x5c/2: // either 0x40 explicitely in many places, or according $2083b0 (IT4)

		default:
			logerror("video_regs_w (PC=%06X): %04x = %04x & %04x\n", activecpu_get_previouspc(), offset*2, data, mem_mask);
			break;
	}
	COMBINE_DATA(&video_regs[offset]);
}

static READ16_HANDLER( video_regs_r )
{
	logerror("video_regs_r (PC=%06X): %04x\n", activecpu_get_previouspc(), offset*2);
	return video_regs[offset];
}


/* DMA RLE stuff - the same as CPS3 */
static unsigned short lastb;
static unsigned short lastb2;
static int destl;

static UINT32 process(UINT8 b,UINT32 dst_offset)
{

 	int l=0;

 	UINT8 *tram=(UINT8*)tileram;

 	if(lastb==lastb2)	//rle
 	{
		int i;
 		int rle=(b+1)&0xff;

 		for(i=0;i<rle;++i)
 		{
			tram[dst_offset+destl] = lastb;
			dirty_tileram[(dst_offset+destl)/0x40] = 1;

			dst_offset++;
 			++l;
 		}
 		lastb2=0xffff;

 		return l;
 	}
 	else
 	{
 		lastb2=lastb;
 		lastb=b;
		tram[dst_offset+destl] = b;
		dirty_tileram[(dst_offset+destl)/0x40] = 1;

 		return 1;
 	}
 }


static WRITE16_HANDLER(srmp6_dma_w)
{
	COMBINE_DATA(&dmaram[offset]);
	if(offset==13 && dmaram[offset]==0x40)
	{
		UINT32 srctab=2*((((UINT32)dmaram[5])<<16)|dmaram[4]);
		UINT32 srcdata=2*((((UINT32)dmaram[11])<<16)|dmaram[10]);
		UINT32 len=(((((UINT32)dmaram[7])<<16)|dmaram[6])+1); //??? WRONG!
		int tempidx=0;

		/* show params */
#if SRMP6_VERBOSE
		printf("DMA! %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x\n",
				dmaram[0x00/2],
				dmaram[0x02/2],
				dmaram[0x04/2],
				dmaram[0x06/2],
				dmaram[0x08/2],
				dmaram[0x0a/2],
				dmaram[0x0c/2],
				dmaram[0x0e/2],
				dmaram[0x10/2],
				dmaram[0x12/2],
				dmaram[0x14/2],
				dmaram[0x16/2],
				dmaram[0x18/2],
				dmaram[0x1a/2]);
#endif

		destl=dmaram[9]*0x100000;

		lastb=0xfffe;
		lastb2=0xffff;

		while(1)
		{
			int i;
			UINT8 ctrl=memory_region(REGION_USER2)[srcdata];
 			++srcdata;

			for(i=0;i<8;++i)
			{
				UINT8 p=memory_region(REGION_USER2)[srcdata];

				if(ctrl&0x80)
				{
					UINT8 real_byte;
					p&=0x7f;
					real_byte = memory_region(REGION_USER2)[srctab+p*2];
					tempidx+=process(real_byte,tempidx);
					real_byte = memory_region(REGION_USER2)[srctab+p*2+1];//px[DMA_XOR((current_table_address+p*2+1))];
					tempidx+=process(real_byte,tempidx);
 				}
 				else
 				{
 					tempidx+=process(p,tempidx);
 				}

 				ctrl<<=1;
 				++srcdata;


				if(tempidx>=len)
				{
#if SRMP6_VERBOSE
					printf("%x\n",srcdata);
#endif
					return;
				}
 			}
		}
	}
}

/* if tileram is actually bigger than the mapped area, how do we access the rest? */
static READ16_HANDLER(tileram_r)
{
//  return tileram[offset];
	return 0x0000;
}

static WRITE16_HANDLER(tileram_w)
{
	//UINT16 tmp;
//  COMBINE_DATA(&tileram[offset]);

	/* are the DMA registers enabled some other way, or always mapped here, over RAM? */
	if (offset >= 0xfff00/2 && offset <= 0xfff1a/2 )
	{
		offset &=0x1f;
		srmp6_dma_w(offset,data,mem_mask);
	}
}
static ADDRESS_MAP_START( srmp6, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x0fffff) AM_ROM
	AM_RANGE(0x200000, 0x23ffff) AM_RAM					// work RAM
	AM_RANGE(0x600000, 0x7fffff) AM_READ(MRA16_BANK1)	// banked ROM (used by ROM check)
	AM_RANGE(0x800000, 0x9fffff) AM_ROM AM_REGION(REGION_USER1, 0)

	AM_RANGE(0x300000, 0x300005) AM_READWRITE(srmp6_inputs_r, srmp6_input_select_w)		// inputs
	AM_RANGE(0x480000, 0x480fff) AM_READWRITE(MRA16_RAM, paletteram16_xBBBBBGGGGGRRRRR_word_w) AM_BASE(&paletteram16)
	AM_RANGE(0x4d0000, 0x4d0001) AM_READWRITE(watchdog_reset16_r, watchdog_reset16_w)	// watchdog

	// OBJ RAM: checked [$400000-$47dfff]
	AM_RANGE(0x400000, 0x47ffff) AM_RAM AM_BASE(&sprram)

	// CHR RAM: checked [$500000-$5fffff]
	AM_RANGE(0x500000, 0x5fffff) AM_READWRITE(tileram_r,tileram_w)//AM_RAM AM_BASE(&tileram)
//  AM_RANGE(0x5fff00, 0x5fffff) AM_WRITE(dma_w) AM_BASE(&dmaram)

	AM_RANGE(0x4c0000, 0x4c006f) AM_READWRITE(video_regs_r, video_regs_w) AM_BASE(&video_regs)	// ? gfx regs ST-0026 NiLe
//  AM_RANGE(0x4e0000, 0x4e00ff) AM_READWRITE(sound_regs_r, sound_regs_w) AM_BASE(&sound_regs)  // ? sound regs (data) ST-0026 NiLe
//  AM_RANGE(0x4e0100, 0x4e0101) AM_READWRITE(sndctrl_reg_r, sndctrl_reg_w)                     // ? sound reg  (ctrl) ST-0026 NiLe
//  AM_RANGE(0x4e0110, 0x4e0111) AM_NOP // ? accessed once ($268dc, written $b.w)
//  AM_RANGE(0x5fff00, 0x5fff1f) AM_RAM // ? see routine $5ca8, video_regs related ???

//  AM_RANGE(0xf00004, 0xf00005) AM_RAM // ?
//  AM_RANGE(0xf00006, 0xf00007) AM_RAM // ?

ADDRESS_MAP_END


/***************************************************************************
    Port definitions
***************************************************************************/

INPUT_PORTS_START( srmp6 )

	PORT_START
	PORT_BIT ( 0xfe01, IP_ACTIVE_LOW, IPT_UNUSED ) // explicitely discarded
	PORT_BIT ( 0x0002, IP_ACTIVE_LOW, IPT_MAHJONG_A )
	PORT_BIT ( 0x0004, IP_ACTIVE_LOW, IPT_MAHJONG_E )
	PORT_BIT ( 0x0008, IP_ACTIVE_LOW, IPT_MAHJONG_I )
	PORT_BIT ( 0x0010, IP_ACTIVE_LOW, IPT_MAHJONG_M )
	PORT_BIT ( 0x0020, IP_ACTIVE_LOW, IPT_MAHJONG_KAN )
	PORT_BIT ( 0x0040, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT ( 0x0080, IP_ACTIVE_LOW, IPT_COIN1 ) PORT_IMPULSE(2)
	PORT_BIT ( 0x0100, IP_ACTIVE_LOW, IPT_SERVICE ) PORT_NAME(DEF_STR( Test )) PORT_CODE(KEYCODE_F2)

	PORT_START
	PORT_BIT ( 0xfe41, IP_ACTIVE_LOW, IPT_UNUSED ) // explicitely discarded
	PORT_BIT ( 0x0002, IP_ACTIVE_LOW, IPT_MAHJONG_B )
	PORT_BIT ( 0x0004, IP_ACTIVE_LOW, IPT_MAHJONG_F )
	PORT_BIT ( 0x0008, IP_ACTIVE_LOW, IPT_MAHJONG_J )
	PORT_BIT ( 0x0010, IP_ACTIVE_LOW, IPT_MAHJONG_N )
	PORT_BIT ( 0x0020, IP_ACTIVE_LOW, IPT_MAHJONG_REACH )
	PORT_BIT ( 0x0180, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT ( 0xfe41, IP_ACTIVE_LOW, IPT_UNUSED ) // explicitely discarded
	PORT_BIT ( 0x0002, IP_ACTIVE_LOW, IPT_MAHJONG_C )
	PORT_BIT ( 0x0004, IP_ACTIVE_LOW, IPT_MAHJONG_G )
	PORT_BIT ( 0x0008, IP_ACTIVE_LOW, IPT_MAHJONG_K )
	PORT_BIT ( 0x0010, IP_ACTIVE_LOW, IPT_MAHJONG_CHI )
	PORT_BIT ( 0x0020, IP_ACTIVE_LOW, IPT_MAHJONG_RON )
	PORT_BIT ( 0x0180, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START
	PORT_BIT ( 0xfe61, IP_ACTIVE_LOW, IPT_UNUSED ) // explicitely discarded
	PORT_BIT ( 0x0002, IP_ACTIVE_LOW, IPT_MAHJONG_D )
	PORT_BIT ( 0x0004, IP_ACTIVE_LOW, IPT_MAHJONG_H )
	PORT_BIT ( 0x0008, IP_ACTIVE_LOW, IPT_MAHJONG_L )
	PORT_BIT ( 0x0010, IP_ACTIVE_LOW, IPT_MAHJONG_PON )
	PORT_BIT ( 0x0180, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START	/* 16-bit DSW1+DSW2 */
	PORT_DIPNAME( 0x0007, 0x0007, DEF_STR( Coinage ) )		// DSW1
	PORT_DIPSETTING(      0x0000, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0007, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(      0x0006, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0005, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( 1C_4C ) )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0010, 0x0010, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0020, 0x0020, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0040, 0x0000, "Re-Clothe" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Nudity" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( On ) )
	PORT_DIPNAME( 0x0700, 0x0700, DEF_STR( Difficulty ) )	// DSW2
	PORT_DIPSETTING(      0x0000, "8" )
	PORT_DIPSETTING(      0x0100, "7" )
	PORT_DIPSETTING(      0x0200, "6" )
	PORT_DIPSETTING(      0x0300, "5" )
	PORT_DIPSETTING(      0x0400, "3" )
	PORT_DIPSETTING(      0x0500, "2" )
	PORT_DIPSETTING(      0x0600, "1" )
	PORT_DIPSETTING(      0x0700, "4" )
	PORT_DIPNAME( 0x0800, 0x0000, "Kuitan" )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0800, DEF_STR( On ) )
	PORT_DIPNAME( 0x1000, 0x1000, DEF_STR( Continues ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x1000, DEF_STR( On ) )
	PORT_DIPNAME( 0x2000, 0x2000, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x2000, DEF_STR( On ) )
	PORT_DIPNAME( 0x4000, 0x4000, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(      0x4000, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x8000, IP_ACTIVE_LOW )
INPUT_PORTS_END

/***************************************************************************
    Machine driver
***************************************************************************/

INTERRUPT_GEN(srmp6_interrupt)
{
	if(!cpu_getiloops())
	{
		cpunum_set_input_line(0,3,HOLD_LINE);
	}
	else
	{
		cpunum_set_input_line(0,4,HOLD_LINE);
	}
}

static MACHINE_DRIVER_START( srmp6 )
	MDRV_CPU_ADD(M68000, 16000000)
	MDRV_CPU_PROGRAM_MAP(srmp6,0)
	MDRV_CPU_VBLANK_INT(srmp6_interrupt,2)

	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_SIZE(64*8, 64*8)
	MDRV_SCREEN_VISIBLE_AREA(0*8, 340, 2*8, 256)
	MDRV_PALETTE_LENGTH(0x800)

	MDRV_VIDEO_START(srmp6)
	MDRV_VIDEO_UPDATE(srmp6)

	/* sound hardware */
MACHINE_DRIVER_END


/***************************************************************************
    ROM definition(s)
***************************************************************************/
/*
CHR?
    sx011-05.18 '00':[23a640,300000[
    sx011-04.19
    sx011-03.20
    sx011-02.21
    sx011-01.22 '00':[266670,400000[ (end)

most are 8 bits unsigned PCM 16/32KHz if stereo/mono
some voices in 2nd rom have lower sample rate
    sx011-08.15 <- samples: instruments, musics, sound FXs, voices
    sx011-07.16 <- samples: voices (cont'd), sound FXs, music, theme music
    sx011-06.17 <- samples: theme music (cont'd)
*/
ROM_START( srmp6 )
	ROM_REGION( 0x100000, REGION_CPU1, 0 ) /* 68000 Code */
	ROM_LOAD16_BYTE( "sx011-10.4", 0x000001, 0x080000, CRC(8f4318a5) SHA1(44160968cca027b3d42805f2dd42662d11257ef6) )
	ROM_LOAD16_BYTE( "sx011-11.5", 0x000000, 0x080000, CRC(7503d9cf) SHA1(03ab35f13b6166cb362aceeda18e6eda8d3abf50) )

	ROM_REGION( 0x200000, REGION_USER1, 0 ) /* 68000 Data */
	ROM_LOAD( "sx011-09.10", 0x000000, 0x200000, CRC(58f74438) SHA1(a256e39ca0406e513ab4dbd812fb0b559b4f61f2) )

 	/* these are accessed directly by the 68k, DMA device etc.  NOT decoded */
	ROM_REGION( 0x2000000, REGION_USER2, 0)	/* Banked ROM */
	ROM_LOAD16_WORD_SWAP( "sx011-08.15", 0x0000000, 0x0400000, CRC(01b3b1f0) SHA1(bbd60509c9ba78358edbcbb5953eafafd6e2eaf5) ) // CHR00
	ROM_LOAD16_WORD_SWAP( "sx011-07.16", 0x0400000, 0x0400000, CRC(26e57dac) SHA1(91272268977c5fbff7e8fbe1147bf108bd2ed321) ) // CHR01
	ROM_LOAD16_WORD_SWAP( "sx011-06.17", 0x0800000, 0x0400000, CRC(220ee32c) SHA1(77f39b54891c2381b967534b0f6d380962eadcae) ) // CHR02
	ROM_LOAD16_WORD_SWAP( "sx011-05.18", 0x0c00000, 0x0400000, CRC(87e5fea9) SHA1(abd751b5744d6ac7e697774ea9a7f7455bf3ac7c) ) // CHR03
	ROM_LOAD16_WORD_SWAP( "sx011-04.19", 0x1000000, 0x0400000, CRC(e90d331e) SHA1(d8afb1497cec8fe6de10d23d49427e11c4c57910) ) // CHR04
	ROM_LOAD16_WORD_SWAP( "sx011-03.20", 0x1400000, 0x0400000, CRC(f1f24b35) SHA1(70d6848f77940331e1be8591a33d62ac22a3aee9) ) // CHR05
	ROM_LOAD16_WORD_SWAP( "sx011-02.21", 0x1800000, 0x0400000, CRC(c56d7e50) SHA1(355c64b38e7b266f386b9c0b906c8581fc15374b) ) // CHR06
	ROM_LOAD16_WORD_SWAP( "sx011-01.22", 0x1c00000, 0x0400000, CRC(785409d1) SHA1(3e31254452a30d929161a1ea3a3daa69de058364) ) // CHR07
ROM_END


/***************************************************************************
    Driver initialization
***************************************************************************/

static DRIVER_INIT( srmp6 )
{
}


/***************************************************************************
    Game driver(s)
***************************************************************************/

/*GAME( YEAR,NAME,PARENT,MACHINE,INPUT,INIT,MONITOR,COMPANY,FULLNAME,FLAGS)*/
GAME( 1995, srmp6, 0, srmp6, srmp6, srmp6, ROT0, "Seta", "Super Real Mahjong P6 (Japan)", GAME_NOT_WORKING | GAME_IMPERFECT_GRAPHICS | GAME_NO_SOUND)


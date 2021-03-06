/* Kaneko EXPRO-02 board
  - used by the newer revisions of Gals Panic

 Notes:
 Tile GFX seem to be encrypted / scrambled?  I can't decode them
 Dipswitches are wrong for these sets


Gals Panic
Kaneko, 1990

PCB Layout
----------

EXPRO-02
|-------------------------------------------------------------------------|
|     M6295 PM007E.U47 12MHz          PM000E.U74    PM004E.U86            |
|      VOL  PM008E.U46 16MHz 62256          PM002E.U76   PM109U_U88-01.U88|
|LA4460                                                                   |
|             PAL PAL  PAL                                                |
|             PAL PAL  PAL            PM001E.U73    PM005E.U85            |
|                      PAL   62256          PM003E.U75   PM110U_U87-01.U87|
|    |--------------------|                                               |
|    |       68000        |                                               |
|    |                    |  41464                                        |
|    |--------------------|  41464              PM017E.U84                |
|    GP-U27             PAL  41464  |---------|                           |
|J   PAL   GP-U41            41464  |KANEKO   |                           |
|A MC-8282  PAL              41464  |VU-002   | PM006E.U83     PM018E.U94 |
|M                     6116  41464  |         |                           |
|M                                  |         |          PM019U_U93-01.U93|
|A                     6116         |---------| PM206E.U82                |
|            HM53461                                                      |
|    PAL     HM53461   PAL    |-------|         CALC1-CHIP                |
|            HM53461   PAL    |KANEKO |                      PM016E.U92   |
|    PAL     HM53461   PAL    |VIEW2- |    6264                           |
|            HM53461   PAL    |   CHIP|                      PM015E.U91   |
|    PAL     HM53461   PAL    |-------|    6264                           |
|DSW2         6116     PAL   PAL                             PM014E.U90   |
|                      PAL                 PAL                            |
|DSW1         6116                         PAL               PM013E.U89   |
|-------------------------------------------------------------------------|

    Notes
    -----
    68000 clock - 12.0MHz
    CALC1-CHIP clock - 16.0MHz
    GP-U41 clocks - pins 21 & 22 - 12.0MHz, pins 1 & 2 - 6.0MHz, pins 8 & 9 - 15.6249kHz (HSync?)
    GP-U27 clock - none (so it's not an MCU)
    OKI M6295 clock - 2.0MHz (12/6). pin7 = low
    VSync - 60Hz
    HSync - 15.68kHz
    MC-8282 - Kaneko custom I/O JAMMA ceramic module
    41464 - 64k x4 DRAM
    HM53461 - 64k x4 Multiport CMOS VRAM
    6116 - 2k x8 SRAM
    6264 - 8k x8 SRAM
    62256 - 32k x8 SRAM

*/

#include "driver.h"
#include "includes/kaneko16.h"
#include "sound/okim6295.h"

//extern DRIVER_INIT(berlwall);
extern VIDEO_START(galsnew);
extern VIDEO_UPDATE(galsnew);
extern PALETTE_INIT(berlwall);
extern UINT16* galsnew_bg_pixram;
extern UINT16* galsnew_fg_pixram;

INPUT_PORTS_START( galsnew )
	PORT_START_TAG("DSW1")
	PORT_DIPNAME( 0x0001, 0x0001, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0002, 0x0002, DEF_STR( Unknown ) )	/* flip screen? */
	PORT_DIPSETTING(      0x0002, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_SERVICE( 0x0004, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	/* Coinage - World (0x03ffff.b = 03) */
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(      0x0010, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(      0x0020, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
	PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(      0x0080, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(      0x0040, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( 1C_6C ) )
	/* Coinage - Japan (0x03ffff.b = 01) and US (0x03ffff.b = 02)
    PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Coin_A ) )
    PORT_DIPSETTING(      0x0010, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(      0x0030, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
    PORT_DIPSETTING(      0x0020, DEF_STR( 1C_2C ) )
    PORT_DIPNAME( 0x00c0, 0x00c0, DEF_STR( Coin_B ) )
    PORT_DIPSETTING(      0x0040, DEF_STR( 2C_1C ) )
    PORT_DIPSETTING(      0x00c0, DEF_STR( 1C_1C ) )
    PORT_DIPSETTING(      0x0000, DEF_STR( 2C_3C ) )
    PORT_DIPSETTING(      0x0080, DEF_STR( 1C_2C ) )
    */
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY PORT_PLAYER(1)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )		/* "Shot2" in "test mode" */
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_DIPNAME( 0x0003, 0x0003, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(      0x0002, DEF_STR( Easy ) )
	PORT_DIPSETTING(      0x0003, DEF_STR( Normal ) )
	PORT_DIPSETTING(      0x0001, DEF_STR( Hard ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0x0004, 0x0004, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0004, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0008, 0x0008, DEF_STR( Unused ) )
	PORT_DIPSETTING(      0x0008, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0030, 0x0030, DEF_STR( Lives ) )
	PORT_DIPSETTING(      0x0010, "2" )
	PORT_DIPSETTING(      0x0030, "3" )
	PORT_DIPSETTING(      0x0020, "4" )
	PORT_DIPSETTING(      0x0000, "5" )
	PORT_DIPNAME( 0x0040, 0x0040, DEF_STR( Unknown ) )	/* demo sounds? - see notes */
	PORT_DIPSETTING(      0x0040, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )
	PORT_DIPNAME( 0x0080, 0x0080, "Character Test" )
	PORT_DIPSETTING(      0x0080, DEF_STR( Off ) )
	PORT_DIPSETTING(      0x0000, DEF_STR( On ) )

	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_4WAY PORT_PLAYER(2)
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_4WAY PORT_PLAYER(2)
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_4WAY PORT_PLAYER(2)
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_4WAY PORT_PLAYER(2)
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_UNKNOWN )		/* "Shot2" in "test mode" */
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START
	PORT_BIT( 0x00ff, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0100, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x0200, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x0400, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x0800, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x1000, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x2000, IP_ACTIVE_LOW, IPT_TILT )
	PORT_BIT( 0x4000, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x8000, IP_ACTIVE_LOW, IPT_UNKNOWN )
INPUT_PORTS_END


WRITE16_HANDLER( galsnew_paletteram_w )
{
	data = COMBINE_DATA(&paletteram16[offset]);
	palette_set_color_rgb(Machine,offset,pal5bit(data >> 6),pal5bit(data >> 11),pal5bit(data >> 1));
}

static WRITE16_HANDLER( galsnew_6295_bankswitch_w )
{
	if (ACCESSING_MSB)
	{
		UINT8 *rom = memory_region(REGION_SOUND1);
		memcpy(&rom[0x30000],&rom[0x40000 + ((data >> 8) & 0x0f) * 0x10000],0x10000);
	}
}

static ADDRESS_MAP_START( galsnew, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x03ffff) AM_ROM // main program
	AM_RANGE(0x080000, 0x0fffff) AM_ROM AM_REGION(REGION_USER2,0) // other data
	AM_RANGE(0x100000, 0x3fffff) AM_ROM AM_REGION(REGION_USER1,0) // main data
	AM_RANGE(0x400000, 0x400001) AM_READWRITE(OKIM6295_status_0_lsb_r,OKIM6295_data_0_lsb_w)


	AM_RANGE(0x500000, 0x51ffff) AM_RAM AM_BASE(&galsnew_bg_pixram)
	AM_RANGE(0x520000, 0x53ffff) AM_RAM AM_BASE(&galsnew_fg_pixram)

	AM_RANGE(0x580000, 0x580fff) AM_READWRITE(MRA16_RAM,kaneko16_vram_1_w) AM_BASE(&kaneko16_vram_1)	// Layers 0
	AM_RANGE(0x581000, 0x581fff) AM_READWRITE(MRA16_RAM,kaneko16_vram_0_w) AM_BASE(&kaneko16_vram_0)	//
	AM_RANGE(0x582000, 0x582fff) AM_RAM AM_BASE(&kaneko16_vscroll_1)									//
	AM_RANGE(0x583000, 0x583fff) AM_RAM AM_BASE(&kaneko16_vscroll_0)									//

	AM_RANGE(0x600000, 0x600fff) AM_READWRITE(MRA16_RAM,galsnew_paletteram_w) AM_BASE(&paletteram16) // palette?

	AM_RANGE(0x680000, 0x68001f) AM_READWRITE(MRA16_RAM,kaneko16_layers_0_regs_w) AM_BASE(&kaneko16_layers_0_regs) // sprite regs? tileregs?

	AM_RANGE(0x700000, 0x700fff) AM_RAM AM_BASE(&spriteram16) AM_SIZE(&spriteram_size)	 // sprites? 0x72f words tested

	AM_RANGE(0x780000, 0x78001f) AM_READWRITE(MRA16_RAM,kaneko16_sprites_regs_w) AM_BASE(&kaneko16_sprites_regs	) // sprite regs? tileregs?

	AM_RANGE(0x800000, 0x800001) AM_READ(input_port_0_word_r)
	AM_RANGE(0x800002, 0x800003) AM_READ(input_port_1_word_r)
	AM_RANGE(0x800004, 0x800005) AM_READ(input_port_2_word_r)

	AM_RANGE(0x900000, 0x900001) AM_WRITE(galsnew_6295_bankswitch_w)

	AM_RANGE(0xa00000, 0xa00001) AM_WRITE(MWA16_NOP)	/* ??? */

	AM_RANGE(0xc80000, 0xc8ffff) AM_RAM

	AM_RANGE(0xd80000, 0xd80001) AM_WRITE(MWA16_NOP)	/* ??? */

	AM_RANGE(0xe00000, 0xe00015) AM_READWRITE(galpanib_calc_r,galpanib_calc_w) /* CALC1 MCU interaction (simulated) */

	AM_RANGE(0xe80000, 0xe80001) AM_WRITE(MWA16_NOP)	/* ??? */
ADDRESS_MAP_END


static INTERRUPT_GEN( galsnew_interrupt )
{
	cpunum_set_input_line(0, cpu_getiloops() + 3, HOLD_LINE);	/* IRQs 5, 4, and 3 */
}

static MACHINE_RESET( galsnew )
{
	kaneko16_sprite_type  = 0;

	kaneko16_sprite_xoffs = 0;
	kaneko16_sprite_yoffs = -1*0x40; // align testgrid with bitmap in service mode

	// priorities not verified
	kaneko16_priority.sprite[0] = 8;	// above all
	kaneko16_priority.sprite[1] = 8;	// above all
	kaneko16_priority.sprite[2] = 8;	// above all
	kaneko16_priority.sprite[3] = 8;	// above all
}

static const gfx_layout layout_16x16x4 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP8(8*8*4*0,4),   STEP8(8*8*4*1,4)   },
	{ STEP8(8*8*4*0,8*4), STEP8(8*8*4*2,8*4) },
	16*16*4
};


static const gfx_decode kaneko16_gfx_1x4bit_1x4bit[] =
{
	{ REGION_GFX1, 0, &layout_16x16x4, 0x100,	    0x40 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4, 0,			0x40 }, // [0] bg tiles
	{ -1 }
};

static MACHINE_DRIVER_START( galsnew )

	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, 12000000)
	MDRV_CPU_PROGRAM_MAP(galsnew,0)
	MDRV_CPU_VBLANK_INT(galsnew_interrupt,3)

	/* CALC01 MCU @ 16Mhz (unknown type, simulated) */

	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(DEFAULT_REAL_60HZ_VBLANK_DURATION)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_SCREEN_VISIBLE_AREA(0, 256-1, 0, 256-32-1)
	MDRV_GFXDECODE(kaneko16_gfx_1x4bit_1x4bit)
	MDRV_PALETTE_LENGTH(2048 + 32768)
	MDRV_MACHINE_RESET( galsnew )

	MDRV_VIDEO_START(galsnew)
	MDRV_VIDEO_UPDATE(galsnew)
	MDRV_PALETTE_INIT(berlwall)

	/* arm watchdog */
	MDRV_WATCHDOG_VBLANK_INIT(DEFAULT_60HZ_3S_VBLANK_WATCHDOG)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD_TAG("oki", OKIM6295, 12000000/6)
	MDRV_SOUND_CONFIG(okim6295_interface_region_1_pin7low)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_DRIVER_END

/* the tile roms seem lineswapped.. but I don't know how to descramble them yet */
DRIVER_INIT(galsnew)
{
	UINT8 *src    = memory_region       ( REGION_GFX3 );
	UINT8 *dst    = memory_region       ( REGION_GFX2 );
	int x;

	for (x=0; x<0x200000;x++)
	{
		UINT32 y;

		y = BITSWAP24(x, 23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 );
		y = x ^ 12;
		dst[y] = src[x];

	}

}


ROM_START( galsnew ) /* EXPRO-02 PCB */
	ROM_REGION( 0x40000, REGION_CPU1, 0 )	/* 68000 code */
	ROM_LOAD16_BYTE( "pm110u_u87-01.u87",  0x000000, 0x20000, CRC(b793a57d) SHA1(12d57b2b4add532f0d0453c25b30d34b3449d717) )
	ROM_LOAD16_BYTE( "pm109u_u88-01.u88",  0x000001, 0x20000, CRC(35b936f8) SHA1(d272067f10542d511a777802cafa4d72b93fa5e8) )

	ROM_REGION16_BE( 0x300000, REGION_USER1, 0 )	/* 68000 data */
	ROM_LOAD16_BYTE( "pm004e.u86",    0x000001, 0x80000, CRC(d3af52bc) SHA1(46be057106388578defecab1cdd1793ec76ebe92) )
	ROM_LOAD16_BYTE( "pm005e.u85",    0x000000, 0x80000, CRC(d7ec650c) SHA1(6c2250c74381497154bf516e0cf1db6bb56bb446) )
	ROM_LOAD16_BYTE( "pm000e.u74",    0x100001, 0x80000, CRC(5d220f3f) SHA1(7ff373e01027c8832712f7a2d732f8e49b875878) )
	ROM_LOAD16_BYTE( "pm001e.u73",    0x100000, 0x80000, CRC(90433eb1) SHA1(8688a85747ad9ecac395d782f130baa64fb9d12b) )
	ROM_LOAD16_BYTE( "pm002e.u76",    0x200001, 0x80000, CRC(713ee898) SHA1(c9f608a57fb90e5ee15eb76a74a7afcc406d5b4e) )
	ROM_LOAD16_BYTE( "pm003e.u75",    0x200000, 0x80000, CRC(6bb060fd) SHA1(4fc3946866c5a55e8340b62b5ad9beae723ce0da) )

	ROM_REGION16_BE( 0x80000, REGION_USER2, 0 )	/* contains real (non-cartoon) women, used after each 3rd round */
	ROM_LOAD16_WORD_SWAP( "pm017e.u84",    0x00000, 0x80000, CRC(bc41b6ca) SHA1(0aeaf024dd7c84550e7df27230a1d4f04cc1d61c) )

	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_ERASEFF )	/* sprites */
	/* the 06e rom from the other type gals panic board ends up split across 2 roms here */
	ROM_LOAD( "pm006e.u83",         0x000000, 0x080000, CRC(a7555d9a) SHA1(f95821b3358d9ab03ca9ead38fd358062259d89d) )
	ROM_LOAD( "pm206e.u82",         0x080000, 0x080000, CRC(cc978baa) SHA1(59a95bcbaeca9d356f61ea42af4da116afbb1491) )
	ROM_LOAD( "pm018e.u94",         0x100000, 0x080000, CRC(f542d708) SHA1(f515cca9e96401303ed45b4372f6079f29b7a999) )
	ROM_LOAD( "pm019u_u93-01.u93",  0x180000, 0x010000, CRC(3cb79005) SHA1(05a0b993b9071467265067c3762644f46343d8de) ) // ?? seems to be an extra / replacement enemy?, not sure where it maps, or when it's used, it might load over another rom

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )	/* sprites - encrypted? */
	ROM_LOAD( "pm013e.u89",    0x180000, 0x080000, CRC(10f27b05) SHA1(0f8ade713f6b430b5a23370a17326d53229951de) )
	ROM_LOAD( "pm014e.u90",    0x100000, 0x080000, CRC(2f367106) SHA1(1cd16e286e77e8e1b7668bbb6f2978101656b720) )
	ROM_LOAD( "pm015e.u91",    0x080000, 0x080000, CRC(a563f8ef) SHA1(6e4171746e4d401992bf3a7619d5bed0063d57e5) )
	ROM_LOAD( "pm016e.u92",    0x000000, 0x080000, CRC(c0b9494c) SHA1(f0b066dd78eb9fcf947da90ddb6c7b62299c5743) )


	ROM_REGION( 0x140000, REGION_SOUND1, 0 )	/* OKIM6295 samples */
	/* 00000-2ffff is fixed, 30000-3ffff is bank switched from all the ROMs */
	ROM_LOAD( "pm008e.u46",     0x00000, 0x80000, CRC(d9379ba8) SHA1(5ae7c743319b1a12f2b101a9f0f8fe0728ed1476) )
	ROM_RELOAD(                 0x40000, 0x80000 )
	ROM_LOAD( "pm007e.u47",     0xc0000, 0x80000, CRC(c7ed7950) SHA1(133258b058d3c562208d0d00b9fac71202647c32) )
ROM_END

ROM_START( galsnewa ) /* EXPRO-02 PCB */
	ROM_REGION( 0x40000, REGION_CPU1, 0 )
	/* 68000 code */
	ROM_LOAD16_BYTE( "pm110e.u87-01",  0x000000, 0x20000, CRC(34e1ee0d) SHA1(567df65b04667a6d35725c4a131fb174acb3ad0a) )
	ROM_LOAD16_BYTE( "pm109e.u88-01",  0x000001, 0x20000, CRC(c694255a) SHA1(16faf5ea5ff69a0e7a981021ea5fc09a0aefd7cf) )

	ROM_REGION16_BE( 0x300000, REGION_USER1, 0 )	/* 68000 data */
	ROM_LOAD16_BYTE( "pm004e.u86",    0x000001, 0x80000, CRC(d3af52bc) SHA1(46be057106388578defecab1cdd1793ec76ebe92) )
	ROM_LOAD16_BYTE( "pm005e.u85",    0x000000, 0x80000, CRC(d7ec650c) SHA1(6c2250c74381497154bf516e0cf1db6bb56bb446) )
	ROM_LOAD16_BYTE( "pm000e.u74",    0x100001, 0x80000, CRC(5d220f3f) SHA1(7ff373e01027c8832712f7a2d732f8e49b875878) )
	ROM_LOAD16_BYTE( "pm001e.u73",    0x100000, 0x80000, CRC(90433eb1) SHA1(8688a85747ad9ecac395d782f130baa64fb9d12b) )
	ROM_LOAD16_BYTE( "pm002e.u76",    0x200001, 0x80000, CRC(713ee898) SHA1(c9f608a57fb90e5ee15eb76a74a7afcc406d5b4e) )
	ROM_LOAD16_BYTE( "pm003e.u75",    0x200000, 0x80000, CRC(6bb060fd) SHA1(4fc3946866c5a55e8340b62b5ad9beae723ce0da) )

	ROM_REGION16_BE( 0x80000, REGION_USER2, 0 )	/* contains real (non-cartoon) women, used after each 3rd round */
	ROM_LOAD16_WORD_SWAP( "pm017e.u84",    0x00000, 0x80000, CRC(bc41b6ca) SHA1(0aeaf024dd7c84550e7df27230a1d4f04cc1d61c) )


	ROM_REGION( 0x200000, REGION_GFX1, ROMREGION_ERASEFF )	/* sprites */
	/* the 06e rom from the other type gals panic board ends up split across 2 roms here */
	ROM_LOAD( "pm006e.u83",       0x000000, 0x080000, CRC(a7555d9a) SHA1(f95821b3358d9ab03ca9ead38fd358062259d89d) )
	ROM_LOAD( "pm206e.u82",       0x080000, 0x080000, CRC(cc978baa) SHA1(59a95bcbaeca9d356f61ea42af4da116afbb1491) )
	ROM_LOAD( "pm018e.u94",       0x100000, 0x080000, CRC(f542d708) SHA1(f515cca9e96401303ed45b4372f6079f29b7a999) )
//  ROM_LOAD( "u93",    0x180000, 0x010000  ) // NOT present on this set (empty socket)

	ROM_REGION( 0x200000, REGION_GFX2, ROMREGION_DISPOSE )	/* sprites */

	ROM_REGION( 0x200000, REGION_GFX3, ROMREGION_DISPOSE )	/* tiles - encrypted? */
	ROM_LOAD( "pm013e.u89",    0x180000, 0x080000, CRC(10f27b05) SHA1(0f8ade713f6b430b5a23370a17326d53229951de) )
	ROM_LOAD( "pm014e.u90",    0x100000, 0x080000, CRC(2f367106) SHA1(1cd16e286e77e8e1b7668bbb6f2978101656b720) )
	ROM_LOAD( "pm015e.u91",    0x080000, 0x080000, CRC(a563f8ef) SHA1(6e4171746e4d401992bf3a7619d5bed0063d57e5) )
	ROM_LOAD( "pm016e.u92",    0x000000, 0x080000, CRC(c0b9494c) SHA1(f0b066dd78eb9fcf947da90ddb6c7b62299c5743) )


	ROM_REGION( 0x140000, REGION_SOUND1, 0 )	/* OKIM6295 samples */
	/* 00000-2ffff is fixed, 30000-3ffff is bank switched from all the ROMs */
	ROM_LOAD( "pm008e.u46",     0x00000, 0x80000, CRC(d9379ba8) SHA1(5ae7c743319b1a12f2b101a9f0f8fe0728ed1476) )
	ROM_RELOAD(                 0x40000, 0x80000 )
	ROM_LOAD( "pm007e.u47",     0xc0000, 0x80000, CRC(c7ed7950) SHA1(133258b058d3c562208d0d00b9fac71202647c32) )
ROM_END


GAME( 1990, galsnew, 0,        galsnew, galsnew, galsnew, ROT90, "Kaneko", "Gals Panic (US, EXPRO-02 PCB)", GAME_NOT_WORKING )
GAME( 1990, galsnewa,galsnew,  galsnew, galsnew, galsnew, ROT90, "Kaneko", "Gals Panic (Export, EXPRO-02 PCB)", GAME_NOT_WORKING )

/* Sand Scorpian

    Sand Scorpion (by Face)                MCU protection (collision detection etc.)

*/

#include "driver.h"
#include "machine/eeprom.h"
#include "includes/kaneko16.h"
#include "sound/2203intf.h"
#include "sound/2151intf.h"
#include "sound/okim6295.h"
#include "video/kan_pand.h"

static MACHINE_RESET( sandscrp )
{
	kaneko16_sprite_type  = 0;

	kaneko16_sprite_xoffs = 0;
	kaneko16_sprite_yoffs = 0;

	kaneko16_priority.sprite[0] = 1;	// above tile[0],   below the others
	kaneko16_priority.sprite[1] = 2;	// above tile[0-1], below the others
	kaneko16_priority.sprite[2] = 3;	// above tile[0-2], below the others
	kaneko16_priority.sprite[3] = 8;	// above all
	kaneko16_sprite_type = 3;	// "different" sprites layout
}

/* Sand Scorpion */

static UINT8 sprite_irq;
static UINT8 unknown_irq;
static UINT8 vblank_irq;


/* Update the IRQ state based on all possible causes */
static void update_irq_state(void)
{
	if (vblank_irq || sprite_irq || unknown_irq)
		cpunum_set_input_line(0, 1, ASSERT_LINE);
	else
		cpunum_set_input_line(0, 1, CLEAR_LINE);
}



/* Called once/frame to generate the VBLANK interrupt */
static INTERRUPT_GEN( sandscrp_interrupt )
{
	vblank_irq = 1;
	update_irq_state();
}


static VIDEO_EOF( sandscrp )
{
	sprite_irq = 1;
	update_irq_state();
	pandora_eof(machine);
}

/* Reads the cause of the interrupt */
static READ16_HANDLER( sandscrp_irq_cause_r )
{
	return 	( sprite_irq  ?  0x08  : 0 ) |
			( unknown_irq ?  0x10  : 0 ) |
			( vblank_irq  ?  0x20  : 0 ) ;
}


/* Clear the cause of the interrupt */
static WRITE16_HANDLER( sandscrp_irq_cause_w )
{
	if (ACCESSING_LSB)
	{
		kaneko16_sprite_flipx	=	data & 1;
		kaneko16_sprite_flipy	=	data & 1;

		if (data & 0x08)	sprite_irq  = 0;
		if (data & 0x10)	unknown_irq = 0;
		if (data & 0x20)	vblank_irq  = 0;
	}

	update_irq_state();
}



/***************************************************************************
                                Sand Scorpion
***************************************************************************/

WRITE16_HANDLER( sandscrp_coin_counter_w )
{
	if (ACCESSING_LSB)
	{
		coin_counter_w(0,   data  & 0x0001);
		coin_counter_w(1,   data  & 0x0002);
	}
}

static UINT8 latch1_full;
static UINT8 latch2_full;

static READ16_HANDLER( sandscrp_latchstatus_word_r )
{
	return	(latch1_full ? 0x80 : 0) |
			(latch2_full ? 0x40 : 0) ;
}

static WRITE16_HANDLER( sandscrp_latchstatus_word_w )
{
	if (ACCESSING_LSB)
	{
		latch1_full = data & 0x80;
		latch2_full = data & 0x40;
	}
}

static READ16_HANDLER( sandscrp_soundlatch_word_r )
{
	latch2_full = 0;
	return soundlatch2_r(0);
}

static WRITE16_HANDLER( sandscrp_soundlatch_word_w )
{
	if (ACCESSING_LSB)
	{
		latch1_full = 1;
		soundlatch_w(0, data & 0xff);
		cpunum_set_input_line(1, INPUT_LINE_NMI, PULSE_LINE);
		cpu_spinuntil_time(MAME_TIME_IN_USEC(100));	// Allow the other cpu to reply
	}
}

static ADDRESS_MAP_START( sandscrp, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x07ffff) AM_ROM		// ROM
	AM_RANGE(0x100000, 0x100001) AM_WRITE(sandscrp_irq_cause_w)	// IRQ Ack

	AM_RANGE(0x700000, 0x70ffff) AM_RAM		// RAM
	AM_RANGE(0x200000, 0x20001f) AM_READWRITE(galpanib_calc_r,galpanib_calc_w)	// Protection
	AM_RANGE(0x300000, 0x30000f) AM_READWRITE(MRA16_RAM, kaneko16_layers_0_regs_w) AM_BASE(&kaneko16_layers_0_regs)	// Layers 0 Regs
	AM_RANGE(0x400000, 0x400fff) AM_READWRITE(MRA16_RAM, kaneko16_vram_1_w) AM_BASE(&kaneko16_vram_1)	// Layers 0
	AM_RANGE(0x401000, 0x401fff) AM_READWRITE(MRA16_RAM, kaneko16_vram_0_w) AM_BASE(&kaneko16_vram_0)	//
	AM_RANGE(0x402000, 0x402fff) AM_RAM AM_BASE(&kaneko16_vscroll_1)									//
	AM_RANGE(0x403000, 0x403fff) AM_RAM AM_BASE(&kaneko16_vscroll_0)									//
	AM_RANGE(0x500000, 0x501fff) AM_READWRITE(pandora_spriteram_LSB_r, pandora_spriteram_LSB_w ) // sprites
	AM_RANGE(0x600000, 0x600fff) AM_READWRITE(MRA16_RAM, paletteram16_xGGGGGRRRRRBBBBB_word_w) AM_BASE(&paletteram16)	// Palette
	AM_RANGE(0xa00000, 0xa00001) AM_WRITE(sandscrp_coin_counter_w)	// Coin Counters (Lockout unused)
	AM_RANGE(0xb00000, 0xb00001) AM_READ(input_port_0_word_r)	// Inputs
	AM_RANGE(0xb00002, 0xb00003) AM_READ(input_port_1_word_r)	//
	AM_RANGE(0xb00004, 0xb00005) AM_READ(input_port_2_word_r)	//
	AM_RANGE(0xb00006, 0xb00007) AM_READ(input_port_3_word_r)	//
	AM_RANGE(0xec0000, 0xec0001) AM_READ(watchdog_reset16_r)	//
	AM_RANGE(0x800000, 0x800001) AM_READ(sandscrp_irq_cause_r)	// IRQ Cause
	AM_RANGE(0xe00000, 0xe00001) AM_READWRITE(sandscrp_soundlatch_word_r, sandscrp_soundlatch_word_w)	// From/To Sound CPU
	AM_RANGE(0xe40000, 0xe40001) AM_READWRITE(sandscrp_latchstatus_word_r, sandscrp_latchstatus_word_w)	//
ADDRESS_MAP_END



/***************************************************************************
                                Sand Scorpion
***************************************************************************/

WRITE8_HANDLER( sandscrp_bankswitch_w )
{
	UINT8 *RAM = memory_region(REGION_CPU1);
	int bank = data & 0x07;

	if ( bank != data )	logerror("CPU #1 - PC %04X: Bank %02X\n",activecpu_get_pc(),data);

	if (bank < 3)	RAM = &RAM[0x4000 * bank];
	else			RAM = &RAM[0x4000 * (bank-3) + 0x10000];

	memory_set_bankptr(1, RAM);
}

static READ8_HANDLER( sandscrp_latchstatus_r )
{
	return	(latch2_full ? 0x80 : 0) |	// swapped!?
			(latch1_full ? 0x40 : 0) ;
}

static READ8_HANDLER( sandscrp_soundlatch_r )
{
	latch1_full = 0;
	return soundlatch_r(0);
}

static WRITE8_HANDLER( sandscrp_soundlatch_w )
{
	latch2_full = 1;
	soundlatch2_w(0,data);
}

static ADDRESS_MAP_START( sandscrp_soundmem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM		// ROM
	AM_RANGE(0x8000, 0xbfff) AM_READWRITE(MRA8_BANK1, MWA8_ROM)	// Banked ROM
	AM_RANGE(0xc000, 0xdfff) AM_RAM		// RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( sandscrp_soundport, ADDRESS_SPACE_IO, 8 )
	ADDRESS_MAP_FLAGS( AMEF_ABITS(8) )
	AM_RANGE(0x00, 0x00) AM_WRITE(sandscrp_bankswitch_w)	// ROM Bank
	AM_RANGE(0x02, 0x02) AM_READWRITE(YM2203_status_port_0_r, YM2203_control_port_0_w)	// YM2203
	AM_RANGE(0x03, 0x03) AM_READWRITE(YM2203_read_port_0_r, YM2203_write_port_0_w)		// PORTA/B read
	AM_RANGE(0x04, 0x04) AM_WRITE(OKIM6295_data_0_w)		// OKIM6295
	AM_RANGE(0x06, 0x06) AM_WRITE(sandscrp_soundlatch_w)	//
	AM_RANGE(0x07, 0x07) AM_READ(sandscrp_soundlatch_r)		//
	AM_RANGE(0x08, 0x08) AM_READ(sandscrp_latchstatus_r)	//
ADDRESS_MAP_END


/***************************************************************************
                                Sand Scorpion
***************************************************************************/

INPUT_PORTS_START( sandscrp )
	PORT_START	// IN0 - $b00000.w
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(1)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	 ) PORT_PLAYER(1)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN1 - $b00002.w
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	 ) PORT_PLAYER(2)
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN2 - $b00004.w
	PORT_BIT( 0x0001, IP_ACTIVE_LOW, IPT_START1   )
	PORT_BIT( 0x0002, IP_ACTIVE_LOW, IPT_START2   )
	PORT_BIT( 0x0004, IP_ACTIVE_LOW, IPT_COIN1    )
	PORT_BIT( 0x0008, IP_ACTIVE_LOW, IPT_COIN2    )
	PORT_BIT( 0x0010, IP_ACTIVE_LOW, IPT_TILT     )
	PORT_BIT( 0x0020, IP_ACTIVE_LOW, IPT_UNKNOWN  )
	PORT_BIT( 0x0040, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x0080, IP_ACTIVE_LOW, IPT_UNKNOWN  )

	PORT_BIT( 0xff00, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN3 - $b00006.w
	PORT_BIT( 0xffff, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	// IN4 - DSW 1 read by the Z80 through the sound chip
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x02, "1" )
	PORT_DIPSETTING(    0x01, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, "Bombs" )
	PORT_DIPSETTING(    0x08, "1" )
	PORT_DIPSETTING(    0x04, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Easy )    )
	PORT_DIPSETTING(    0x20, DEF_STR( Normal )  )
	PORT_DIPSETTING(    0x10, DEF_STR( Hard )    )
	PORT_DIPSETTING(    0x00, DEF_STR( Hardest ) )
	PORT_DIPNAME( 0xc0, 0xc0, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x80, "100K, 300K" )
	PORT_DIPSETTING(    0xc0, "200K, 500K" )
	PORT_DIPSETTING(    0x40, "500K, 1000K" )
	PORT_DIPSETTING(    0x00, "1000K, 3000K" )

	PORT_START	// IN5 - DSW 2 read by the Z80 through the sound chip
	PORT_DIPNAME( 0x0f, 0x0f, DEF_STR( Coinage ) )
	PORT_DIPSETTING(    0x0a, DEF_STR( 6C_1C ) )
	PORT_DIPSETTING(    0x0b, DEF_STR( 5C_1C ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( 4C_1C ) )
	PORT_DIPSETTING(    0x0d, DEF_STR( 3C_1C ) )
	PORT_DIPSETTING(    0x01, DEF_STR( 8C_3C ) )
	PORT_DIPSETTING(    0x0e, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(    0x02, DEF_STR( 5C_3C ) )
	PORT_DIPSETTING(    0x03, DEF_STR( 3C_2C ) )
	PORT_DIPSETTING(    0x0f, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(    0x09, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(    0x07, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(    0x06, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(    0x05, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Allow_Continue ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x40, DEF_STR( On ) )
	PORT_SERVICE( 0x80, IP_ACTIVE_LOW )
INPUT_PORTS_END


/* 16x16x4 tiles (made of four 8x8 tiles) */
static const gfx_layout layout_16x16x4_2 =
{
	16,16,
	RGN_FRAC(1,1),
	4,
	{ STEP4(0,1) },
	{ STEP4(8*8*4*0 + 3*4, -4), STEP4(8*8*4*0 + 7*4, -4),
	  STEP4(8*8*4*1 + 3*4, -4), STEP4(8*8*4*1 + 7*4, -4) },
	{ STEP8(8*8*4*0, 8*4),     STEP8(8*8*4*2, 8*4) },
	16*16*4
};

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


static const gfx_decode sandscrp_gfxdecodeinfo[] =
{
	{ REGION_GFX1, 0, &layout_16x16x4,   0x000, 0x10 }, // [0] Sprites
	{ REGION_GFX2, 0, &layout_16x16x4_2, 0x400, 0x40 }, // [1] Layers
	{ -1 }
};



/***************************************************************************
                                Sand Scorpion
***************************************************************************/

/* YM3014B + YM2203C */

static void irq_handler(int irq)
{
	cpunum_set_input_line(1,0,irq ? ASSERT_LINE : CLEAR_LINE);
}

static struct YM2203interface ym2203_intf_sandscrp =
{
	input_port_4_r,	/* Port A Read - DSW 1 */
	input_port_5_r,	/* Port B Read - DSW 2 */
	0,	/* Port A Write */
	0,	/* Port B Write */
	irq_handler	/* IRQ handler */
};


static MACHINE_DRIVER_START( sandscrp )

	/* basic machine hardware */
	MDRV_CPU_ADD(M68000,12000000)	/* TMP68HC000N-12 */
	MDRV_CPU_PROGRAM_MAP(sandscrp,0)
	MDRV_CPU_VBLANK_INT(sandscrp_interrupt,1)

	MDRV_CPU_ADD(Z80,4000000)	/* Z8400AB1, Reads the DSWs: it can't be disabled */
	MDRV_CPU_PROGRAM_MAP(sandscrp_soundmem,0)
	MDRV_CPU_IO_MAP(sandscrp_soundport,0)

	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME( DEFAULT_REAL_60HZ_VBLANK_DURATION )
	MDRV_WATCHDOG_VBLANK_INIT(DEFAULT_60HZ_3S_VBLANK_WATCHDOG)

	MDRV_MACHINE_RESET(sandscrp)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(256, 256)
	MDRV_SCREEN_VISIBLE_AREA(0, 256-1, 0+16, 256-16-1)
	MDRV_GFXDECODE(sandscrp_gfxdecodeinfo)
	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(sandscrp_1xVIEW2)
	MDRV_VIDEO_EOF(sandscrp)
	MDRV_VIDEO_UPDATE(sandscrp)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_STEREO("left", "right")

	MDRV_SOUND_ADD(OKIM6295, 12000000/6)
	MDRV_SOUND_CONFIG(okim6295_interface_region_1_pin7high)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "left", 0.25)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "right", 0.25)

	MDRV_SOUND_ADD(YM2203, 4000000)
	MDRV_SOUND_CONFIG(ym2203_intf_sandscrp)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "left", 0.25)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "right", 0.25)
MACHINE_DRIVER_END



/***************************************************************************

                                Sand Scorpion

(C) FACE
68HC000N-12
Z8400AB1
OKI6295, YM2203C
OSC:  16.000mhz,   12.000mhz

SANDSC03.BIN     27C040
SANDSC04.BIN     27C040
SANDSC05.BIN     27C040
SANDSC06.BIN     27C040
SANDSC07.BIN     27C2001
SANDSC08.BIN     27C1001
SANDSC11.BIN     27C2001
SANDSC12.BIN     27C2001

***************************************************************************/

ROM_START( sandscrp )
	ROM_REGION( 0x080000, REGION_CPU1, 0 )		/* 68000 Code */
	ROM_LOAD16_BYTE( "sandsc11.bin", 0x000000, 0x040000, CRC(9b24ab40) SHA1(3187422dbe8b15d8053be4cb20e56d3e6afbd5f2) )
	ROM_LOAD16_BYTE( "sandsc12.bin", 0x000001, 0x040000, CRC(ad12caee) SHA1(83267445b89c3cf4dc317106aa68763d2f29eff7) )

	ROM_REGION( 0x24000, REGION_CPU2, 0 )		/* Z80 Code */
	ROM_LOAD( "sandsc08.bin", 0x00000, 0x0c000, CRC(6f3e9db1) SHA1(06a04fa17f44319986913bff70433510c89e38f1) )
	ROM_CONTINUE(             0x10000, 0x14000             )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "sandsc05.bin", 0x000000, 0x080000, CRC(9bb675f6) SHA1(c3f6768cfd99a0e19ca2224fff9aa4e27ec0da24) )
	ROM_LOAD( "sandsc06.bin", 0x080000, 0x080000, CRC(7df2f219) SHA1(e2a59e201bfededa92d6c86f8dc1b212527ef66f) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Layers */
	ROM_LOAD16_BYTE( "sandsc04.bin", 0x000000, 0x080000, CRC(b9222ff2) SHA1(a445da3f7f5dea5ff64bb0b048f624f947875a39) )
	ROM_LOAD16_BYTE( "sandsc03.bin", 0x000001, 0x080000, CRC(adf20fa0) SHA1(67a7a2be774c86916cbb97e4c9b16c2e48125780) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sandsc07.bin", 0x000000, 0x040000, CRC(9870ab12) SHA1(5ea3412cbc57bfaa32a1e2552b2eb46f4ceb5fa8) )
ROM_END


ROM_START( sandscra )
	ROM_REGION( 0x080000, REGION_CPU1, 0 )		/* 68000 Code */
	ROM_LOAD16_BYTE( "11.ic4", 0x000000, 0x040000, CRC(80020cab) SHA1(4f1f4d8ea07ad745f2d6d3f800686f07fe4bf20f) )
	ROM_LOAD16_BYTE( "12.ic5", 0x000001, 0x040000, CRC(8df1d42f) SHA1(2a9db5c4b99a8a3f62bffa9ddd96a95e2042602b) )

	ROM_REGION( 0x24000, REGION_CPU2, 0 )		/* Z80 Code */
	ROM_LOAD( "sandsc08.bin", 0x00000, 0x0c000, CRC(6f3e9db1) SHA1(06a04fa17f44319986913bff70433510c89e38f1) )
	ROM_CONTINUE(             0x10000, 0x14000             )

	ROM_REGION( 0x100000, REGION_GFX1, ROMREGION_DISPOSE )	/* Sprites */
	ROM_LOAD( "ss502.ic16", 0x000000, 0x100000, CRC(d8012ebb) SHA1(975bbb3b57a09e41d2257d4fa3a64097144de554) )

	ROM_REGION( 0x100000, REGION_GFX2, ROMREGION_DISPOSE )	/* Layers */
	ROM_LOAD16_WORD_SWAP( "ss501.ic30", 0x000000, 0x100000, CRC(0cf9f99d) SHA1(47f7f120d2bc075bedaff0a44306a8f46a1d848c) )

	ROM_REGION( 0x040000, REGION_SOUND1, 0 )	/* Samples */
	ROM_LOAD( "sandsc07.bin", 0x000000, 0x040000, CRC(9870ab12) SHA1(5ea3412cbc57bfaa32a1e2552b2eb46f4ceb5fa8) )
ROM_END


GAME( 1992, sandscrp, 0,        sandscrp, sandscrp, 0,          ROT90, "Face",   "Sand Scorpion (set 1)", 0 )
GAME( 1992, sandscra, sandscrp, sandscrp, sandscrp, 0,          ROT90, "Face",   "Sand Scorpion (set 2)", 0 )

/**************************************************************************
Change Lanes
(C) Taito 1983

Jarek Burczynski
Phil Stroffolino
Tomasz Slanina

***************************************************************************/

#include "driver.h"
#include "cpu/z80/z80.h"
#include "cpu/m6805/m6805.h"
#include "sound/ay8910.h"


VIDEO_START( changela );
VIDEO_UPDATE( changela );

WRITE8_HANDLER( changela_colors_w );
WRITE8_HANDLER( changela_mem_device_select_w );
WRITE8_HANDLER( changela_mem_device_w );
READ8_HANDLER( changela_mem_device_r );
WRITE8_HANDLER( changela_slope_rom_addr_hi_w );
WRITE8_HANDLER( changela_slope_rom_addr_lo_w );


static UINT8 portA_in,portA_out,ddrA;
static UINT8 portB_out,ddrB;
static UINT8 portC_in,portC_out,ddrC;

static UINT8 mcu_out;
static UINT8 mcu_in;
static UINT8 mcu_PC1;
static UINT8 mcu_PC0;

UINT8 changela_tree0_col;
UINT8 changela_tree1_col;
UINT8 changela_left_bank_col;
UINT8 changela_right_bank_col;
UINT8 changela_boat_shore_col;
UINT8 changela_collision_reset;
UINT8 changela_tree_collision_reset;

MACHINE_RESET (changela)
{
	mcu_PC1=0;
	mcu_PC0=0;

	changela_tree0_col = 0;
	changela_tree1_col = 0;
	changela_left_bank_col = 0;
	changela_right_bank_col = 0;
	changela_boat_shore_col = 0;
	changela_collision_reset = 0;
	changela_tree_collision_reset = 0;
}

static READ8_HANDLER( mcu_r )
{
	//mame_printf_debug("Z80 MCU  R = %x\n",mcu_out);
	return mcu_out;
}

/* latch LS374 at U39 */
static WRITE8_HANDLER( mcu_w )
{
	mcu_in = data;
}



/*********************************
        MCU
*********************************/



READ8_HANDLER( changela_68705_portA_r )
{
	return (portA_out & ddrA) | (portA_in & ~ddrA);
}

WRITE8_HANDLER( changela_68705_portA_w )
{
	portA_out = data;
}

WRITE8_HANDLER( changela_68705_ddrA_w )
{
	ddrA = data;
}


READ8_HANDLER( changela_68705_portB_r )
{
	return (portB_out & ddrB) | (readinputport(4) & ~ddrB);
}

WRITE8_HANDLER( changela_68705_portB_w )
{
	portB_out = data;
}

WRITE8_HANDLER( changela_68705_ddrB_w )
{
	ddrB = data;
}


READ8_HANDLER( changela_68705_portC_r )
{
	return (portC_out & ddrC) | (portC_in & ~ddrC);
}

WRITE8_HANDLER( changela_68705_portC_w )
{
	/* PC3 is connected to the CLOCK input of the LS374,
        so we latch the data on positive going edge of the clock */

/* this is strange because if we do this corectly - it just doesn't work */
	if( (data&8) /*& (!(portC_out&8))*/ )
		mcu_out  = portA_out;


	/* PC2 is connected to the /OE input of the LS374 */

	if(!(data&4))
		portA_in = mcu_in;

	portC_out = data;
}

WRITE8_HANDLER( changela_68705_ddrC_w )
{
	ddrC = data;
}


static ADDRESS_MAP_START( mcu_readmem, ADDRESS_SPACE_PROGRAM, 8 )
	ADDRESS_MAP_FLAGS( AMEF_ABITS(11) )
	AM_RANGE(0x0000, 0x0000) AM_READ(changela_68705_portA_r)
	AM_RANGE(0x0001, 0x0001) AM_READ(changela_68705_portB_r)
	AM_RANGE(0x0002, 0x0002) AM_READ(changela_68705_portC_r)

	AM_RANGE(0x0000, 0x007f) AM_READ(MRA8_RAM)
	AM_RANGE(0x0080, 0x07ff) AM_READ(MRA8_ROM)
ADDRESS_MAP_END

static ADDRESS_MAP_START( mcu_writemem, ADDRESS_SPACE_PROGRAM, 8 )
	ADDRESS_MAP_FLAGS( AMEF_ABITS(11) )
	AM_RANGE(0x0000, 0x0000) AM_WRITE(changela_68705_portA_w)
	AM_RANGE(0x0001, 0x0001) AM_WRITE(changela_68705_portB_w)
	AM_RANGE(0x0002, 0x0002) AM_WRITE(changela_68705_portC_w)
	AM_RANGE(0x0004, 0x0004) AM_WRITE(changela_68705_ddrA_w)
	AM_RANGE(0x0005, 0x0005) AM_WRITE(changela_68705_ddrB_w)
	AM_RANGE(0x0006, 0x0006) AM_WRITE(changela_68705_ddrC_w)

	AM_RANGE(0x0000, 0x007f) AM_WRITE(MWA8_RAM)
	AM_RANGE(0x0080, 0x07ff) AM_WRITE(MWA8_ROM)
ADDRESS_MAP_END



/* U30 */
static READ8_HANDLER( changela_24_r )
{
	return ((portC_out&2)<<2) | 7;	/* bits 2,1,0-N/C inputs */
}

static READ8_HANDLER( changela_25_r )
{
	//collisions on bits 3,2, bits 1,0-N/C inputs
	return (changela_tree1_col << 3) | (changela_tree0_col << 2) | 0x03;
}

static READ8_HANDLER( changela_30_r )
{
	return readinputport(7) & 0x0f;	//wheel control (clocked input) signal on bits 3,2,1,0
}

static READ8_HANDLER( changela_31_r )
{
	/* If the new value is less than the old value, and it did not wrap around,
       or if the new value is greater than the old value, and it did wrap around,
       then we are moving LEFT. */
	static UINT8 prev_value = 0;
	UINT8 curr_value = readinputport(7);
	static int dir = 0;

	if( (curr_value < prev_value && (prev_value - curr_value) < 0x80)
	||  (curr_value > prev_value && (curr_value - prev_value) > 0x80) )
		dir = 1;
	if( (prev_value < curr_value && (curr_value - prev_value) < 0x80)
	||  (prev_value > curr_value && (prev_value - curr_value) > 0x80) )
		dir = 0;

	prev_value = curr_value;

	//wheel UP/DOWN control signal on bit 3, collisions on bits:2,1,0
	return (dir << 3) | (changela_left_bank_col << 2) | (changela_right_bank_col << 1) | changela_boat_shore_col;
}

static READ8_HANDLER( changela_2c_r )
{
	int val = readinputport(5);

    val = (val&0x30) | ((val&1)<<7) | (((val&1)^1)<<6);

	return val;
}

static READ8_HANDLER( changela_2d_r )
{
	/* the schems are unreadable - i'm not sure it is V8 (page 74, SOUND I/O BOARD SCHEMATIC 1 OF 2, FIGURE 24 - in the middle on the right side) */
	int v8 = 0;
	int gas;

	if ((video_screen_get_vpos(0) & 0xf8)==0xf8)
		v8 = 1;

	/* Gas pedal is made up of 2 switches, 1 active low, 1 active high */
	switch(readinputport(6) & 0x03)
	{
		case 0x02:
			gas = 0x80;
			break;
		case 0x01:
			gas = 0x00;
			break;
		default:
			gas = 0x40;
			break;
	}

	return (readinputport(6) & 0x20) | gas | (v8<<4);
}

static WRITE8_HANDLER( mcu_PC0_w )
{
	portC_in = (portC_in&0xfe) | (data&1);
}

static WRITE8_HANDLER( changela_collision_reset_0 )
{
	changela_collision_reset = data & 0x01;
}

static WRITE8_HANDLER( changela_collision_reset_1 )
{
	changela_tree_collision_reset = data & 0x01;
}

static WRITE8_HANDLER( changela_coin_counter_w )
{
	coin_counter_w(offset,data);
}


static ADDRESS_MAP_START( readmem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_READ(MRA8_ROM)
	AM_RANGE(0x8000, 0x83ff) AM_READ(MRA8_RAM)				/* OBJ0 RAM */
	AM_RANGE(0x9000, 0x97ff) AM_READ(MRA8_RAM)				/* OBJ1 RAM */
	AM_RANGE(0xb000, 0xbfff) AM_READ(MRA8_ROM)

	AM_RANGE(0xc000, 0xc7ff) AM_READ(changela_mem_device_r)			/* RAM4 (River Bed RAM); RAM5 (Tree RAM) */

	AM_RANGE(0xd000, 0xd000) AM_READ(AY8910_read_port_0_r)
	AM_RANGE(0xd010, 0xd010) AM_READ(AY8910_read_port_1_r)

	/* LS139 - U24 */
	AM_RANGE(0xd024, 0xd024) AM_READ(changela_24_r)
	AM_RANGE(0xd025, 0xd025) AM_READ(changela_25_r)
	AM_RANGE(0xd028, 0xd028) AM_READ(mcu_r)
	AM_RANGE(0xd02c, 0xd02c) AM_READ(changela_2c_r)
	AM_RANGE(0xd02d, 0xd02d) AM_READ(changela_2d_r)

	AM_RANGE(0xd030, 0xd030) AM_READ(changela_30_r)
	AM_RANGE(0xd031, 0xd031) AM_READ(changela_31_r)

	AM_RANGE(0xf000, 0xf7ff) AM_READ(MRA8_RAM)	/* RAM2 (Processor RAM) */
ADDRESS_MAP_END


static ADDRESS_MAP_START( writemem, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_WRITE(MWA8_ROM) 				/* Processor ROM */
	AM_RANGE(0x8000, 0x83ff) AM_WRITE(MWA8_RAM) AM_BASE(&spriteram) 	/* OBJ0 RAM (A10 = GND )*/
	AM_RANGE(0x9000, 0x97ff) AM_WRITE(MWA8_RAM) AM_BASE(&videoram)		/* OBJ1 RAM */
	AM_RANGE(0xa000, 0xa07f) AM_WRITE(changela_colors_w) AM_BASE(&colorram)		/* Color 93419 RAM 64x9(nine!!!) bits A0-used as the 8-th bit data input (d0-d7->normal, a0->d8) */
	AM_RANGE(0xb000, 0xbfff) AM_WRITE(MWA8_ROM)				/* Processor ROM */

	AM_RANGE(0xc000, 0xc7ff) AM_WRITE(changela_mem_device_w)			/* River-Tree RAMs, slope ROM, tree ROM */

	/* LS138 - U16 */
	AM_RANGE(0xc800, 0xc800) AM_WRITE(MWA8_NOP)				/* not connected */
	AM_RANGE(0xc900, 0xc900) AM_WRITE(changela_mem_device_select_w)	/* selects the memory device to be accessible at 0xc000-0xc7ff */
	AM_RANGE(0xca00, 0xca00) AM_WRITE(changela_slope_rom_addr_hi_w)
	AM_RANGE(0xcb00, 0xcb00) AM_WRITE(changela_slope_rom_addr_lo_w)

	AM_RANGE(0xd000, 0xd000) AM_WRITE(AY8910_control_port_0_w)
	AM_RANGE(0xd001, 0xd001) AM_WRITE(AY8910_write_port_0_w)
	AM_RANGE(0xd010, 0xd010) AM_WRITE(AY8910_control_port_1_w)
	AM_RANGE(0xd011, 0xd011) AM_WRITE(AY8910_write_port_1_w)

	/* LS259 - U44 */
	AM_RANGE(0xd020, 0xd020) AM_WRITE(changela_collision_reset_0)
	AM_RANGE(0xd021, 0xd022) AM_WRITE(changela_coin_counter_w)
//AM_RANGE(0xd023, 0xd023) AM_WRITE(MWA8_NOP)
	AM_RANGE(0xd024, 0xd024) AM_WRITE(mcu_PC0_w)
	AM_RANGE(0xd025, 0xd025) AM_WRITE(changela_collision_reset_1)
	AM_RANGE(0xd026, 0xd026) AM_WRITE(MWA8_NOP)

	AM_RANGE(0xd030, 0xd030) AM_WRITE(mcu_w)
	AM_RANGE(0xe000, 0xe000) AM_WRITE(watchdog_reset_w)		/* Watchdog */
	AM_RANGE(0xf000, 0xf7ff) AM_WRITE(MWA8_RAM)				/* Processor RAM */
ADDRESS_MAP_END


INPUT_PORTS_START( changela )
	PORT_START /* 0 */ /* DSWA */
	PORT_DIPNAME( 0x07, 0x01, "Steering Wheel Ratio" )
	PORT_DIPSETTING(    0x01, "Recommended" )
	PORT_DIPNAME( 0x08, 0x00, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x10, 0x00, "Ignore Memory Failures" )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x00, DEF_STR( Controls ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Joystick ) )
	PORT_DIPSETTING(    0x00, "Steering Wheel" )
	PORT_DIPNAME( 0x40, 0x40, "Diagnostic" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Players ) )
	PORT_DIPSETTING(    0x80, "1" )
	PORT_DIPSETTING(    0x00, "2" )

	PORT_START /* 1 */ /* DSWB */
	PORT_DIPNAME( 0x03, 0x00, "Max Bonus Fuels" )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x02, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x00, "99" )
	PORT_DIPNAME( 0x0c, 0x08, "Game Difficulty" )
	PORT_DIPSETTING(    0x00, DEF_STR( Very_Easy) )
	PORT_DIPSETTING(    0x04, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x08, DEF_STR( Medium ) )
	PORT_DIPSETTING(    0x0c, DEF_STR( Hard ) )
	PORT_DIPNAME( 0x30, 0x20, "Traffic Difficulty" )
	PORT_DIPSETTING(    0x00, DEF_STR( Very_Easy) )
	PORT_DIPSETTING(    0x10, DEF_STR( Easy ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Medium ) )
	PORT_DIPSETTING(    0x30, DEF_STR( Hard ) )
	PORT_DIPNAME( 0x40, 0x00, "Land Collisions Enabled" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, "Car Collisions Enabled" )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START /* 2 */ /* DSWC - coinage */
	PORT_DIPNAME( 0xf0, 0x10, DEF_STR( Coin_A ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(	0x90, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0xa0, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(	0x10, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0xb0, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(	0xc0, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(	0x20, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0xd0, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(	0xe0, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(	0x30, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0xf0, DEF_STR( 2C_7C ) )
	PORT_DIPSETTING(	0x40, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x50, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(	0x60, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(	0x70, DEF_STR( 1C_7C ) )
	PORT_DIPNAME( 0x0f, 0x01, DEF_STR( Coin_B ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Free_Play ) )
	PORT_DIPSETTING(	0x09, DEF_STR( 2C_1C ) )
	PORT_DIPSETTING(	0x0a, DEF_STR( 2C_2C ) )
	PORT_DIPSETTING(	0x01, DEF_STR( 1C_1C ) )
	PORT_DIPSETTING(	0x0b, DEF_STR( 2C_3C ) )
	PORT_DIPSETTING(	0x0c, DEF_STR( 2C_4C ) )
	PORT_DIPSETTING(	0x02, DEF_STR( 1C_2C ) )
	PORT_DIPSETTING(	0x0d, DEF_STR( 2C_5C ) )
	PORT_DIPSETTING(	0x0e, DEF_STR( 2C_6C ) )
	PORT_DIPSETTING(	0x03, DEF_STR( 1C_3C ) )
	PORT_DIPSETTING(	0x0f, DEF_STR( 2C_7C ) )
	PORT_DIPSETTING(	0x04, DEF_STR( 1C_4C ) )
	PORT_DIPSETTING(	0x05, DEF_STR( 1C_5C ) )
	PORT_DIPSETTING(	0x06, DEF_STR( 1C_6C ) )
	PORT_DIPSETTING(	0x07, DEF_STR( 1C_7C ) )

	PORT_START /* 3 */ /* DSWD - bonus */
	PORT_DIPNAME( 0x01, 0x01, "Right Coin Counter" )
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x02, 0x02, "Left Coin Counter" )
	PORT_DIPSETTING(    0x02, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x1c, 0x00, "Credits for Bonus" )
	PORT_DIPSETTING(    0x00, "0" )
	PORT_DIPSETTING(    0x04, "1" )
	PORT_DIPSETTING(    0x08, "2" )
	PORT_DIPSETTING(    0x0c, "3" )
	PORT_DIPSETTING(    0x10, "4" )
	PORT_DIPSETTING(    0x14, "5" )
	PORT_DIPSETTING(    0x18, "6" )
	PORT_DIPSETTING(    0x1c, "7" )
	PORT_DIPUNUSED( 0x20, IP_ACTIVE_LOW )
	PORT_DIPNAME( 0xc0, 0x00, "King of the World" )
	PORT_DIPSETTING(    0x80, "No Name" )
	PORT_DIPSETTING(    0x40, "Short Name" )
	PORT_DIPSETTING(    0x00, "Long Name" )

	PORT_START /* port 4 */ /* MCU */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_START2 )
	PORT_SERVICE( 0x04, IP_ACTIVE_LOW )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_DIPNAME( 0x10, 0x00, DEF_STR(Free_Play) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x10, DEF_STR( On ) )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN3 )	/* NC */
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_COIN1 )

	PORT_START /* 5 */ /* 0xDx2C */
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_NAME("Gear Shift") PORT_CODE(KEYCODE_SPACE) PORT_TOGGLE /* Gear shift */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* FWD - negated bit 7 */
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_UNKNOWN ) /* REV - gear position */

	PORT_START /* 6 */ /* 0xDx2D */
	PORT_BIT( 0x03, 0x00, IPT_PEDAL ) PORT_MINMAX(0x00, 0x02) PORT_SENSITIVITY(10) PORT_KEYDELTA(1) //gas pedal
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,  IPT_TILT )
	//PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 ) //gas1
	//PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_BUTTON2 ) //gas2

	PORT_START /* 7 */ /* 0xDx30 DRIVING_WHEEL */
	PORT_BIT( 0xff, 0x00, IPT_DIAL ) PORT_MINMAX(0x00, 0xff) PORT_SENSITIVITY(50) PORT_KEYDELTA(8)


INPUT_PORTS_END


static struct AY8910interface ay8910_interface_1 = {
	input_port_0_r,
	input_port_1_r
};

static struct AY8910interface ay8910_interface_2 = {
	input_port_2_r,
	input_port_3_r
};


INTERRUPT_GEN( chl_interrupt )
{
	int vector = video_screen_get_vblank(0) ? 0xdf : 0xcf; /* 4 irqs per frame: 3 times 0xcf, 1 time 0xdf */

//    video_screen_update_partial(0, video_screen_get_vpos(0));

	cpunum_set_input_line_and_vector(0, 0, HOLD_LINE, vector);

	/* it seems the V8 == Vblank and it is connected to the INT on the 68705 */
	//so we should cause an INT on the cpu 1 here, as well.
	//but only once per frame !
	if (vector == 0xdf) /* only on vblank */
		cpunum_set_input_line(1, 0, PULSE_LINE );

}

static MACHINE_DRIVER_START( changela )

	MDRV_CPU_ADD(Z80,5000000)
	MDRV_CPU_PROGRAM_MAP(readmem,writemem)
	MDRV_CPU_VBLANK_INT(chl_interrupt,4)

	MDRV_CPU_ADD(M68705,2500000/M68705_CLOCK_DIVIDER)
	MDRV_CPU_PROGRAM_MAP(mcu_readmem,mcu_writemem)

	MDRV_SCREEN_REFRESH_RATE(60)

	MDRV_MACHINE_RESET(changela)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_INDEXED16)
	MDRV_SCREEN_SIZE(32*8, 262)  /* vert size is a guess */
	MDRV_SCREEN_VISIBLE_AREA(0*8, 32*8-1, 4*8, 32*8-1)
	MDRV_PALETTE_LENGTH(0x40)

	MDRV_VIDEO_START(changela)
	MDRV_VIDEO_UPDATE(changela)

	MDRV_SPEAKER_STANDARD_MONO("mono")

	MDRV_SOUND_ADD(AY8910, 1250000)
	MDRV_SOUND_CONFIG(ay8910_interface_1)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)

	MDRV_SOUND_ADD(AY8910, 1250000)
	MDRV_SOUND_CONFIG(ay8910_interface_2)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)
MACHINE_DRIVER_END


ROM_START( changela )
	ROM_REGION( 0x10000, REGION_CPU1, 0 )	/* Z80 code */
	ROM_LOAD( "cl25a",	0x0000, 0x2000, CRC(38530a60) SHA1(0b0ef1abe11c5271fcd1671322b77165217553c3) )
	ROM_LOAD( "cl24a",	0x2000, 0x2000, CRC(2fcf4a82) SHA1(c33355e2d4d3fab32c8d713a680ec0fceedab341) )
	ROM_LOAD( "cl23",	0x4000, 0x2000, CRC(08385891) SHA1(d8d66664ec25db067d5a4a6c35ec0ac65b9e0c6a) )
	ROM_LOAD( "cl22",	0x6000, 0x2000, CRC(796e0abd) SHA1(64dd9fc1f9bc44519a253ef0c02e181dd13904bf) )
	ROM_LOAD( "cl27",	0xb000, 0x1000, CRC(3668afb8) SHA1(bcfb788baf806edcb129ea9f9dcb1d4260684773) )

	ROM_REGION( 0x10000, REGION_CPU2, 0 )	/* 68705U3 */
	ROM_LOAD( "cl38a",	0x0000, 0x800, CRC(b70156ce) SHA1(c5eab8bbd65c4f587426298da4e22f991ce01dde) )

	ROM_REGION( 0x4000, REGION_GFX1, 0 )	/* tile data */
	ROM_LOAD( "cl111",	0x0000, 0x2000, CRC(41c0149d) SHA1(3ea53a3821b044b3d0451fec1b4ee2c28da393ca) )
	ROM_LOAD( "cl113",	0x2000, 0x2000, CRC(ddf99926) SHA1(e816b88302c5639c7284f4845d450f232d63a10c) )

	ROM_REGION( 0x1000, REGION_GFX2, 0 )	/* obj 1 data */
	ROM_LOAD( "cl46",	0x0000, 0x1000, CRC(9c0a7d28) SHA1(fac9180ea0d9aeea56a84b35cc0958f0dd86a801) )

	ROM_REGION( 0x8000, REGION_USER1, 0 )	/* obj 0 data */
	ROM_LOAD( "cl100",	0x0000, 0x2000, CRC(3fa9e4fa) SHA1(9abd7df5fcf143a0c476bd8c8753c5ea294b9f74) )
	ROM_LOAD( "cl99",	0x2000, 0x2000, CRC(67b27b9e) SHA1(7df0f93851959359218c8d2272e30d242a77039d) )
	ROM_LOAD( "cl98",	0x4000, 0x2000, CRC(bffe4149) SHA1(5cf0b98f9d342bd06d575c565ea01bbd79f5e04b) )
	ROM_LOAD( "cl97",	0x6000, 0x2000, CRC(5abab8f9) SHA1(f5156855bbcdf0740fd44520386318ee53ebbf9a) )

	ROM_REGION( 0x1000, REGION_USER2, 0 )	/* math tables: SLOPE ROM (river-tree schematic page 1/3) */
	ROM_LOAD( "cl44",	0x0000, 0x1000, CRC(160d2bc7) SHA1(2609208c2bd4618ea340923ee01af69278980c36) ) /* first and 2nd half identical */

	ROM_REGION( 0x3000, REGION_USER3, 0 )	/* math tables: TREE ROM (river-tree schematic page 3/3)*/
	ROM_LOAD( "cl7",	0x0000, 0x0800, CRC(01e3efca) SHA1(b26203787f105ba32773e37035c39253050f9c82) ) /* fixed bits: 0xxxxxxx */
	ROM_LOAD( "cl9",	0x1000, 0x2000, CRC(4e53cdd0) SHA1(6255411cfdccbe2c581c83f9127d582623453c3a) )

	ROM_REGION( 0x0020, REGION_PROMS, 0 )
	ROM_LOAD( "cl88",	0x0000, 0x0020, CRC(da4d6625) SHA1(2d9a268973518252eb36f479ab650af8c16c885c) ) /* math train state machine */
ROM_END

static DRIVER_INIT(changela)
{
	state_save_register_global(portA_in);
	state_save_register_global(portA_out);
	state_save_register_global(ddrA);
	state_save_register_global(portB_out);
	state_save_register_global(ddrB);
	state_save_register_global(portC_in);
	state_save_register_global(portC_out);
	state_save_register_global(ddrC);

	state_save_register_global(mcu_out);
	state_save_register_global(mcu_in);
	state_save_register_global(mcu_PC1);
	state_save_register_global(mcu_PC0);

	state_save_register_global(changela_tree0_col);
	state_save_register_global(changela_tree1_col);
	state_save_register_global(changela_left_bank_col);
	state_save_register_global(changela_right_bank_col);
	state_save_register_global(changela_boat_shore_col);
	state_save_register_global(changela_collision_reset);
	state_save_register_global(changela_tree_collision_reset);
}

GAME( 1983, changela, 0, changela, changela, changela, ROT180, "Taito Corporation", "Change Lanes", GAME_SUPPORTS_SAVE )

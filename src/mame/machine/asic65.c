/*************************************
 *
 *  Implementation of ASIC65
 *
 *************************************/

#include "driver.h"
#include "includes/atarig42.h"
#include "cpu/tms32010/tms32010.h"
#include "asic65.h"
#include <math.h>


#define LOG_ASIC		0


/*************************************
 *
 *  !$#@$ asic
 *
 *************************************/

static UINT8  	asic65_type;
static int    	asic65_command;
static UINT16 	asic65_param[32];
static UINT16 	asic65_yorigin = 0x1800;
static UINT8  	asic65_param_index;
static UINT8  	asic65_result_index;
static UINT8  	asic65_reset_state;
static UINT8  	asic65_last_bank;

/* ROM-based interface states */
static UINT8	asic65_cpunum;
static UINT8	asic65_tfull;
static UINT8 	asic65_68full;
static UINT8 	asic65_cmd;
static UINT8 	asic65_xflg;
static UINT16 	asic65_68data;
static UINT16 	asic65_tdata;

static FILE * asic65_log;

WRITE16_HANDLER( asic65_w );
WRITE16_HANDLER( asic65_data_w );

READ16_HANDLER( asic65_r );
READ16_HANDLER( asic65_io_r );


#define PARAM_WRITE		0
#define COMMAND_WRITE	1
#define DATA_READ		2

#define OP_UNKNOWN		0
#define OP_REFLECT		1
#define OP_CHECKSUM		2
#define OP_VERSION		3
#define OP_RAMTEST		4
#define OP_RESET		5
#define OP_SIN			6
#define OP_COS			7
#define OP_ATAN			8
#define OP_TMATRIXMULT	9
#define OP_MATRIXMULT	10
#define OP_TRANSFORM	11
#define OP_YORIGIN		12
#define OP_INITBANKS	13
#define OP_SETBANK		14
#define OP_VERIFYBANK	15

#define MAX_COMMANDS	0x2b

static const UINT8 command_map[3][MAX_COMMANDS] =
{
	{
		/* standard version */
		OP_UNKNOWN,		OP_REFLECT,		OP_CHECKSUM, 	OP_VERSION,		/* 00-03 */
		OP_RAMTEST,		OP_UNKNOWN,		OP_UNKNOWN,		OP_RESET,		/* 04-07 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 08-0b */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_TMATRIXMULT,	OP_UNKNOWN,		/* 0c-0f */
		OP_MATRIXMULT,	OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 10-13 */
		OP_SIN,			OP_COS,			OP_YORIGIN,		OP_TRANSFORM,	/* 14-17 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 18-1b */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 1c-1f */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 20-23 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 24-27 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN						/* 28-2a */
	},
	{
		/* Steel Talons version */
		OP_UNKNOWN,		OP_REFLECT,		OP_CHECKSUM, 	OP_VERSION,		/* 00-03 */
		OP_RAMTEST,		OP_UNKNOWN,		OP_UNKNOWN,		OP_RESET,		/* 04-07 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 08-0b */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 0c-0f */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 10-13 */
		OP_TMATRIXMULT,	OP_UNKNOWN,		OP_MATRIXMULT,	OP_UNKNOWN,		/* 14-17 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 18-1b */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_SIN,			OP_COS,			/* 1c-1f */
		OP_ATAN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 20-23 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 24-27 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN						/* 28-2a */
	},
	{
		/* Guardians version */
		OP_UNKNOWN,		OP_REFLECT,		OP_CHECKSUM, 	OP_VERSION,		/* 00-03 */
		OP_RAMTEST,		OP_UNKNOWN,		OP_UNKNOWN,		OP_RESET,		/* 04-07 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 08-0b */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_INITBANKS,	OP_SETBANK,		/* 0c-0f */
		OP_VERIFYBANK,	OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 10-13 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 14-17 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 18-1b */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 1c-1f */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 20-23 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN,		/* 24-27 */
		OP_UNKNOWN,		OP_UNKNOWN,		OP_UNKNOWN						/* 28-2a */
	}
};



/*************************************
 *
 *  Configure the chip
 *
 *************************************/

void asic65_config(int asictype)
{
	asic65_type = asictype;
	if (asic65_type == ASIC65_ROMBASED)
		asic65_cpunum = mame_find_cpu_index(Machine, "asic65");
}



/*************************************
 *
 *  Reset the chip
 *
 *************************************/

void asic65_reset(int state)
{
	/* rom-based means reset and clear states */
	if (asic65_type == ASIC65_ROMBASED)
		cpunum_set_input_line(asic65_cpunum, INPUT_LINE_RESET, state ? ASSERT_LINE : CLEAR_LINE);

	/* otherwise, do it manually */
	else
	{
		cpunum_suspend(mame_find_cpu_index(Machine, "asic65"), SUSPEND_REASON_DISABLE, 1);

		/* if reset is being signalled, clear everything */
		if (state && !asic65_reset_state)
			asic65_command = -1;

		/* if reset is going high, latch the command */
		else if (!state && asic65_reset_state)
		{
			if (asic65_command != -1)
				asic65_data_w(1, asic65_command, 0);
		}

		/* update the state */
		asic65_reset_state = state;
	}
}



/*************************************
 *
 *  Handle writes to the chip
 *
 *************************************/

static TIMER_CALLBACK( m68k_asic65_deferred_w )
{
	asic65_tfull = 1;
	asic65_cmd = param >> 16;
	asic65_tdata = param;
	cpunum_set_input_line(asic65_cpunum, 0, ASSERT_LINE);
}


WRITE16_HANDLER( asic65_data_w )
{
	/* logging */
#if LOG_ASIC
	if (!asic65_log) asic65_log = fopen("asic65.log", "w");
#endif

	/* rom-based use a deferred write mechanism */
	if (asic65_type == ASIC65_ROMBASED)
	{
		timer_call_after_resynch(data | (offset << 16), m68k_asic65_deferred_w);
		cpu_boost_interleave(time_zero, MAME_TIME_IN_USEC(20));
		return;
	}

	/* parameters go to offset 0 */
	if (!(offset & 1))
	{
		if (asic65_log) fprintf(asic65_log, " W=%04X", data);

		/* add to the parameter list, but don't overflow */
		asic65_param[asic65_param_index++] = data;
		if (asic65_param_index >= 32)
			asic65_param_index = 32;
	}

	/* commands go to offset 2 */
	else
	{
		int command = (data < MAX_COMMANDS) ? command_map[asic65_type][data] : OP_UNKNOWN;
		if (asic65_log) fprintf(asic65_log, "\n(%06X)%c%04X:", activecpu_get_previouspc(), (command == OP_UNKNOWN) ? '*' : ' ', data);

		/* set the command number and reset the parameter/result indices */
		asic65_command = data;
		asic65_result_index = asic65_param_index = 0;
	}
}


READ16_HANDLER( asic65_r )
{
	int command = (asic65_command < MAX_COMMANDS) ? command_map[asic65_type][asic65_command] : OP_UNKNOWN;
	INT64 element, result64 = 0;
	UINT16 result = 0;

	/* rom-based just returns latched data */
	if (asic65_type == ASIC65_ROMBASED)
	{
		asic65_68full = 0;
		cpu_boost_interleave(time_zero, MAME_TIME_IN_USEC(5));
		return asic65_68data;
	}

	/* update results */
	switch (command)
	{
		case OP_UNKNOWN:	/* return bogus data */
			popmessage("ASIC65: Unknown cmd %02X", asic65_command);
			break;

		case OP_REFLECT:	/* reflect data */
			if (asic65_param_index >= 1)
				result = asic65_param[--asic65_param_index];
			break;

		case OP_CHECKSUM:	/* compute checksum (should be XX27) */
			result = 0x0027;
			break;

		case OP_VERSION:	/* get version (returns 1.3) */
			result = 0x0013;
			break;

		case OP_RAMTEST:	/* internal RAM test (result should be 0) */
			result = 0;
			break;

		case OP_RESET:	/* reset */
			asic65_result_index = asic65_param_index = 0;
			break;

		case OP_SIN:	/* sin */
			if (asic65_param_index >= 1)
				result = (int)(16384. * sin(M_PI * (double)(INT16)asic65_param[0] / 32768.));
			break;

		case OP_COS:	/* cos */
			if (asic65_param_index >= 1)
				result = (int)(16384. * cos(M_PI * (double)(INT16)asic65_param[0] / 32768.));
			break;

		case OP_ATAN:	/* vector angle */
			if (asic65_param_index >= 4)
			{
				INT32 xint = (INT32)((asic65_param[0] << 16) | asic65_param[1]);
				INT32 yint = (INT32)((asic65_param[2] << 16) | asic65_param[3]);
				double a = atan2((double)yint, (double)xint);
				result = (INT16)(a * 32768. / M_PI);
			}
			break;

		case OP_TMATRIXMULT:	/* matrix multiply by transpose */
			/* if this is wrong, the labels on the car selection screen */
			/* in Race Drivin' will be off */
			if (asic65_param_index >= 9+6)
			{
				INT32 v0 = (INT32)((asic65_param[9] << 16) | asic65_param[10]);
				INT32 v1 = (INT32)((asic65_param[11] << 16) | asic65_param[12]);
				INT32 v2 = (INT32)((asic65_param[13] << 16) | asic65_param[14]);

				/* 2 results per element */
				switch (asic65_result_index / 2)
				{
					case 0:
						result64 = (INT64)v0 * (INT16)asic65_param[0] +
								   (INT64)v1 * (INT16)asic65_param[3] +
								   (INT64)v2 * (INT16)asic65_param[6];
						break;

					case 1:
						result64 = (INT64)v0 * (INT16)asic65_param[1] +
								   (INT64)v1 * (INT16)asic65_param[4] +
								   (INT64)v2 * (INT16)asic65_param[7];
						break;

					case 2:
						result64 = (INT64)v0 * (INT16)asic65_param[2] +
								   (INT64)v1 * (INT16)asic65_param[5] +
								   (INT64)v2 * (INT16)asic65_param[8];
						break;
				}

				/* remove lower 14 bits and pass back either upper or lower words */
				result64 >>= 14;
				result = (asic65_result_index & 1) ? (result64 & 0xffff) : ((result64 >> 16) & 0xffff);
				asic65_result_index++;
			}
			break;

		case OP_MATRIXMULT:	/* matrix multiply???? */
			if (asic65_param_index >= 9+6)
			{
				INT32 v0 = (INT32)((asic65_param[9] << 16) | asic65_param[10]);
				INT32 v1 = (INT32)((asic65_param[11] << 16) | asic65_param[12]);
				INT32 v2 = (INT32)((asic65_param[13] << 16) | asic65_param[14]);

				/* 2 results per element */
				switch (asic65_result_index / 2)
				{
					case 0:
						result64 = (INT64)v0 * (INT16)asic65_param[0] +
								   (INT64)v1 * (INT16)asic65_param[1] +
								   (INT64)v2 * (INT16)asic65_param[2];
						break;

					case 1:
						result64 = (INT64)v0 * (INT16)asic65_param[3] +
								   (INT64)v1 * (INT16)asic65_param[4] +
								   (INT64)v2 * (INT16)asic65_param[5];
						break;

					case 2:
						result64 = (INT64)v0 * (INT16)asic65_param[6] +
								   (INT64)v1 * (INT16)asic65_param[7] +
								   (INT64)v2 * (INT16)asic65_param[8];
						break;
				}

				/* remove lower 14 bits and pass back either upper or lower words */
				result64 >>= 14;
				result = (asic65_result_index & 1) ? (result64 & 0xffff) : ((result64 >> 16) & 0xffff);
				asic65_result_index++;
			}
			break;

		case OP_YORIGIN:
			if (asic65_param_index >= 1)
				asic65_yorigin = asic65_param[asic65_param_index - 1];
			break;

		case OP_TRANSFORM:	/* 3d transform */
			if (asic65_param_index >= 2)
			{
				/* param 0 == 1/z */
				/* param 1 == height */
				/* param 2 == X */
				/* param 3 == Y */
				/* return 0 == scale factor for 1/z */
				/* return 1 == transformed X */
				/* return 2 == transformed Y, taking height into account */
				element = (INT16)asic65_param[0];
				if (asic65_param_index == 2)
				{
					result64 = (element * (INT16)asic65_param[1]) >> 8;
					result64 -= 1;
					if (result64 > 0x3fff) result64 = 0;
				}
				else if (asic65_param_index == 3)
				{
					result64 = (element * (INT16)asic65_param[2]) >> 15;
					result64 += 0xa8;
				}
				else if (asic65_param_index == 4)
				{
					result64 = (INT16)((element * (INT16)asic65_param[3]) >> 10);
					result64 = (INT16)asic65_yorigin - result64 - (result64 << 1);
				}
				result = result64 & 0xffff;
			}
			break;

		case OP_INITBANKS:	/* initialize banking */
			asic65_last_bank = 0;
			break;

		case OP_SETBANK:	/* set a bank */
		{
			static const UINT8 banklist[] =
			{
				1,4,0,4,4,3,4,2, 4,4,4,4,4,4,4,4,
				3,3,4,4,1,1,0,0, 4,4,4,4,2,2,4,4,
				4,4
			};
			static const UINT16 bankaddr[][8] =
			{
				{ 0x77c0,0x77ce,0x77c2,0x77cc,0x77c4,0x77ca,0x77c6,0x77c8 },
				{ 0x77d0,0x77de,0x77d2,0x77dc,0x77d4,0x77da,0x77d6,0x77d8 },
				{ 0x77e0,0x77ee,0x77e2,0x77ec,0x77e4,0x77ea,0x77e6,0x77e8 },
				{ 0x77f0,0x77fe,0x77f2,0x77fc,0x77f4,0x77fa,0x77f6,0x77f8 },
				{ 0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000,0x0000 },
			};
			if (asic65_param_index >= 1)
			{
				if (asic65_param_index < sizeof(banklist) && banklist[asic65_param[0]] < 4)
					asic65_last_bank = banklist[asic65_param[0]];
				result = bankaddr[asic65_last_bank][(asic65_result_index < 8) ? asic65_result_index : 7];
				asic65_result_index++;
			}
			break;
		}

		case OP_VERIFYBANK:	/* verify a bank */
		{
			static const UINT16 bankverify[] =
			{
				0x0eb2,0x1000,0x171b,0x3d28
			};
			result = bankverify[asic65_last_bank];
			break;
		}
	}

#if LOG_ASIC
	if (!asic65_log) asic65_log = fopen("asic65.log", "w");
#endif
	if (asic65_log) fprintf(asic65_log, " (R=%04X)", result);

	return result;
}


READ16_HANDLER( asic65_io_r )
{
	if (asic65_type == ASIC65_ROMBASED)
	{
		/* bit 15 = TFULL */
		/* bit 14 = 68FULL */
		/* bit 13 = XFLG */
		/* bit 12 = controlled by jumper */
		cpu_boost_interleave(time_zero, MAME_TIME_IN_USEC(5));
		return (asic65_tfull << 15) | (asic65_68full << 14) | (asic65_xflg << 13) | 0x0000;
	}
	else
	{
		/* indicate that we always are ready to accept data and always ready to send */
		return 0x4000;
	}
}



/*************************************
 *
 *  Read/write handlers for TMS32015
 *
 *************************************/

static WRITE16_HANDLER( asic65_68k_w )
{
	asic65_68full = 1;
	asic65_68data = data;
}


static READ16_HANDLER( asic65_68k_r )
{
	asic65_tfull = 0;
	cpunum_set_input_line(asic65_cpunum, 0, CLEAR_LINE);
	return asic65_tdata;
}


static WRITE16_HANDLER( asic65_stat_w )
{
	asic65_xflg = data & 1;
}


static READ16_HANDLER( asic65_stat_r )
{
	/* bit 15 = 68FULL */
	/* bit 14 = TFULL */
	/* bit 13 = CMD */
	/* bit 12 = controlled by jumper (0 = test?) */
	return (asic65_68full << 15) | (asic65_tfull << 14) | (asic65_cmd << 13) | 0x1000;
}


static READ16_HANDLER( asci65_get_bio )
{
	if (!asic65_tfull)
		cpu_spinuntil_int();
	return asic65_tfull ? CLEAR_LINE : ASSERT_LINE;
}



/*************************************
 *
 *  Address maps for TMS32015
 *
 *************************************/

static ADDRESS_MAP_START( asic65_program_map, ADDRESS_SPACE_PROGRAM, 16 )
	ADDRESS_MAP_FLAGS( AMEF_UNMAP(1) )
	AM_RANGE(0x000, 0xfff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( asic65_io_map, ADDRESS_SPACE_IO, 16 )
	AM_RANGE(0, 0) AM_MIRROR(6) AM_READWRITE(asic65_68k_r, asic65_68k_w)
	AM_RANGE(1, 1) AM_MIRROR(6) AM_READWRITE(asic65_stat_r, asic65_stat_w)
	AM_RANGE(TMS32010_BIO, TMS32010_BIO) AM_READ(asci65_get_bio)
ADDRESS_MAP_END



/*************************************
 *
 *  Machine driver for ROM-based
 *
 *************************************/

MACHINE_DRIVER_START( asic65 )

	/* ASIC65 */
	MDRV_CPU_ADD_TAG("asic65", TMS32010, 20000000/TMS32010_CLOCK_DIVIDER)
	MDRV_CPU_PROGRAM_MAP(asic65_program_map,0)
	MDRV_CPU_IO_MAP(asic65_io_map,0)
MACHINE_DRIVER_END



/***********************************************************************

    Information about various versions:

    Notation:
        C = command write
        W = write
        R = read
        7 = wait for bit 7
        6 = wait for bit 6

    Guardians of the Hood:
        Version = 0040
        Checksum = ????

        Command $08: C7
        Command $09: C7W6R
        Command $0a: C7
        Command $0b: C7W7W7W7W7WW7W7W7W
        Command $0c: C7W7W7W7W7W7W
        Command $0d: C7W7W
        Command $0e: C7W7W7W7W7WW7W7W7WWWWWWW6R6R6R6R6R6R
        Command $0f: C7W7W7W7W7W7W6R6R6R6R6R6R
        Command $10: C7W7W7W7W7WW7W7W7WWWWWWW6R6R6R6R6R6R
        Command $11: C7W7W7W7W7W7W6R6R6R6R6R6R
        Command $12: C7W7W7W7W7WW7W7W7W6R6R6R6R6R6R6R6R6R
        Command $13: C7W7W7W7W7WW7W7W7W
        Command $14: C7W6R
        Command $15: C7W6R
        Command $16: C7W6R6R
        Command $17: C7W
        Command $18: C76R6R6R6R6R6R6R6R6R
        Command $19: C7
        Command $1a: C76R6R6R6R6R6R6R6R6R
        Command $1b: C7
        Command $1c: C76R6R6R6R6R6R6R6R6R
        Command $1d: C7
        Command $1e: C76R6R6R6R6R6R6R6R6R
        Command $1f: C7
        Command $20: C76R6R6R6R6R6R6R6R6R
        Command $21: C7
        Command $22: C76R6R6R6R6R6R6R6R6R
        Command $23: C7
        Command $24: C7

        Command $0e: C76RRRRRRRRR
        Command $0f: C7W6RRRRRRRR
        Command $16: C7W
        Command $17: C7W7W7W7W6R6R6R

    Road Riot 4WD:
        Version = ????
        Checksum = ????

        Command $08: C7
        Command $09: C7W6R
        Command $0a: C7
        Command $0b: C7W7WWWWWWWW
        Command $0c: C7W7WWWWW
        Command $0d: C7W7W
        Command $0e: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $0f: C7W7WWWWW6RRRRRR
        Command $10: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $11: C7W7WWWWW6RRRRRR
        Command $12: C7W7WWWWWWWW6RRRRRRRRR
        Command $13: C7W7WWWWWWWW
        Command $14: C7W6R
        Command $15: C7W6R
        Command $16: C7W6RR
        Command $17: C7W
        Command $18: C76RRRRRRRRR
        Command $19: C7
        Command $1a: C76RRRRRRRRR
        Command $1b: C7
        Command $1c: C76RRRRRRRRR
        Command $1d: C7
        Command $1e: C76RRRRRRRRR
        Command $1f: C7
        Command $20: C76RRRRRRRRR
        Command $21: C7
        Command $22: C76RRRRRRRRR
        Command $23: C7
        Command $24: C7

        Command $16: C7W
        Command $17: C7W7WWW6RRR
        Command $17: C7WRWRWR

    Race Drivin':
        Version = ????
        Checksum = ????

        Command $08: C7
        Command $09: C7W6R
        Command $0a: C7
        Command $0b: C7W7WWWWWWWW
        Command $0c: C7W7WWWWW
        Command $0d: C7W7W
        Command $0e: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $0f: C7W7WWWWW6RRRRRR
        Command $10: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $11: C7W7WWWWW6RRRRRR
        Command $12: C7W7WWWWWWWW6RRRRRRRRR
        Command $13: C7W7WWWWWWWW
        Command $14: C7W6R
        Command $15: C7W6R
        Command $16: C7W6RR
        Command $17: C7W
        Command $18: C76RRRRRRRRR
        Command $19: C7
        Command $1a: C76RRRRRRRRR
        Command $1b: C7
        Command $1c: C76RRRRRRRRR
        Command $1d: C7
        Command $1e: C76RRRRRRRRR
        Command $1f: C7
        Command $20: C76RRRRRRRRR
        Command $21: C7
        Command $22: C76RRRRRRRRR
        Command $23: C7
        Command $24: C7

        Command $0e: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $14: C7W6R
        Command $15: C7W6R

    Steel Talons:
        Version = ????
        Checksum = ????

        Command $08: C7
        Command $09: C7W6R
        Command $0a: C7
        Command $0b: C7W7WWWWWWWW
        Command $0c: C7W7WWWWW
        Command $0d: C7W7W
        Command $14: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $15: C7W7WWWWW6RRRRRR
        Command $16: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $17: C7W7WWWWW6RRRRRR
        Command $12: C7W7WWWWWWWW6RRRRRRRRR
        Command $13: C7W7WWWWWWWW
        Command $1e: C7W6R
        Command $1f: C7W6R
        Command $16: C7W6RR
        Command $17: C7W
        Command $20: C7W7WWW6R
        Command $18: C76RRRRRRRRR
        Command $19: C7
        Command $1a: C76RRRRRRRRR
        Command $1b: C7
        Command $1c: C76RRRRRRRRR
        Command $1d: C7
        Command $1e: C76RRRRRRRRR
        Command $1f: C7
        Command $20: C76RRRRRRRRR
        Command $21: C7
        Command $22: C76RRRRRRRRR
        Command $23: C7
        Command $24: C7

    Hard Drivin's Airborne:
        Version = ????
        Checksum = ????

        Command $0e: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $14: C7W6R

        Command $08: C7
        Command $09: C7W6R
        Command $0a: C7
        Command $0b: C7W7WWWWWWWW
        Command $0c: C7W7WWWWW
        Command $0d: C7W7W
        Command $0e: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $0f: C7W7WWWWW6RRRRRR
        Command $10: C7W7WWWWWWWWWWWWWW6RRRRRR
        Command $11: C7W7WWWWW6RRRRRR
        Command $12: C7W7WWWWWWWW6RRRRRRRRR
        Command $13: C7W7WWWWWWWW
        Command $14: C7W6R
        Command $15: C7W6R
        Command $16: C7W6RR
        Command $17: C7W
        Command $18: C76RRRRRRRRR
        Command $19: C7
        Command $1a: C76RRRRRRRRR
        Command $1b: C7
        Command $1c: C76RRRRRRRRR
        Command $1d: C7
        Command $1e: C76RRRRRRRRR
        Command $1f: C7
        Command $20: C76RRRRRRRRR
        Command $21: C7
        Command $22: C76RRRRRRRRR
        Command $23: C7
        Command $24: C7
        Command $26: C7W
        Command $27: C7W7WWWWW
        Command $28: C7
        Command $2a: C76RRRRRRRRR

***********************************************************************/

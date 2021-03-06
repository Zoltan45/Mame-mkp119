/*
 * DS2401
 *
 * Dallas Semiconductor
 * Silicon Serial Number
 *
 */

#include <stdarg.h>
#include "driver.h"
#include "state.h"
#include "machine/ds2401.h"

#define VERBOSE_LEVEL ( 0 )

INLINE void verboselog( int n_level, const char *s_fmt, ... )
{
	if( VERBOSE_LEVEL >= n_level )
	{
		va_list v;
		char buf[ 32768 ];
		va_start( v, s_fmt );
		vsprintf( buf, s_fmt, v );
		va_end( v );
		if( cpu_getactivecpu() != -1 )
		{
			logerror( "%08x: %s", activecpu_get_pc(), buf );
		}
		else
		{
			logerror( "(timer) : %s", buf );
		}
	}
}

struct ds2401_chip
{
	int state;
	int bit;
	int byte;
	int shift;
	int rx;
	int tx;
	UINT8 *data;
	mame_timer *timer;
	mame_timer *reset_timer;
	mame_time t_samp;
	mame_time t_rdv;
	mame_time t_rstl;
	mame_time t_pdh;
	mame_time t_pdl;
};

#define SIZE_DATA ( 8 )

#define STATE_IDLE ( 0 )
#define STATE_RESET ( 1 )
#define STATE_RESET1 ( 2 )
#define STATE_RESET2 ( 3 )
#define STATE_COMMAND ( 4 )
#define STATE_READROM ( 5 )

#define COMMAND_READROM ( 0x33 )

static struct ds2401_chip ds2401[ DS2401_MAXCHIP ];

static TIMER_CALLBACK( ds2401_reset )
{
	int which = param;
	struct ds2401_chip *c = &ds2401[ which ];

	verboselog( 1, "ds2401_reset(%d)\n", which );

	c->state = STATE_RESET;
	mame_timer_adjust( c->timer, time_never, which, time_never );
}

static TIMER_CALLBACK( ds2401_tick )
{
	int which = param;
	struct ds2401_chip *c = &ds2401[ which ];

	switch( c->state )
	{
	case STATE_RESET1:
		verboselog( 2, "ds2401_tick(%d) state_reset1 %d\n", which, c->rx );
		c->tx = 0;
		c->state = STATE_RESET2;
		mame_timer_adjust( c->timer, c->t_pdl, which, time_never );
		break;
	case STATE_RESET2:
		verboselog( 2, "ds2401_tick(%d) state_reset2 %d\n", which, c->rx );
		c->tx = 1;
		c->bit = 0;
		c->shift = 0;
		c->state = STATE_COMMAND;
		break;
	case STATE_COMMAND:
		verboselog( 2, "ds2401_tick(%d) state_command %d\n", which, c->rx );
		c->shift >>= 1;
		if( c->rx != 0 )
		{
			c->shift |= 0x80;
		}
		c->bit++;
		if( c->bit == 8 )
		{
			switch( c->shift )
			{
			case COMMAND_READROM:
				verboselog( 1, "ds2401_tick(%d) readrom\n", which );
				c->bit = 0;
				c->byte = 0;
				c->state = STATE_READROM;
				break;
			default:
				verboselog( 0, "ds2401_tick(%d) command not handled %02x\n", which, c->shift );
				c->state = STATE_IDLE;
				break;
			}
		}
		break;
	case STATE_READROM:
		c->tx = 1;
		if( c->byte == 8 )
		{
			verboselog( 1, "ds2401_tick(%d) readrom finished\n", which );
			c->state = STATE_IDLE;
		}
		else
		{
			verboselog( 2, "ds2401_tick(%d) readrom window closed\n", which );
		}
		break;
	default:
		verboselog( 0, "ds2401_tick(%d) state not handled: %d\n", which, c->state );
		break;
	}
}

void ds2401_init( int which, UINT8 *data )
{
	struct ds2401_chip *c = &ds2401[ which ];

	if( data == NULL )
	{
		data = auto_malloc( SIZE_DATA );
	}

	c->state = STATE_IDLE;
	c->bit = 0;
	c->byte = 0;
	c->shift = 0;
	c->rx = 1;
	c->tx = 1;
	c->data = data;
	c->t_samp = MAME_TIME_IN_USEC( 15 );
	c->t_rdv = MAME_TIME_IN_USEC( 15 );
	c->t_rstl = MAME_TIME_IN_USEC( 480 );
	c->t_pdh = MAME_TIME_IN_USEC( 15 );
	c->t_pdl = MAME_TIME_IN_USEC( 60 );

	state_save_register_item( "ds2401", which, c->state );
	state_save_register_item( "ds2401", which, c->bit );
	state_save_register_item( "ds2401", which, c->byte );
	state_save_register_item( "ds2401", which, c->shift );
	state_save_register_item( "ds2401", which, c->rx );
	state_save_register_item( "ds2401", which, c->tx );
	state_save_register_item_pointer( "ds2401", which, data, SIZE_DATA );

	c->timer = mame_timer_alloc( ds2401_tick );
	c->reset_timer = mame_timer_alloc( ds2401_reset );
}

void ds2401_write( int which, int data )
{
	struct ds2401_chip *c = &ds2401[ which ];

	verboselog( 1, "ds2401_write( %d, %d )\n", which, data );

	if( data == 0 && c->rx != 0 )
	{
		switch( c->state )
		{
		case STATE_IDLE:
			break;
		case STATE_COMMAND:
			verboselog( 2, "ds2401_write(%d) state_command\n", which );
			mame_timer_adjust( c->timer, c->t_samp, which, time_never );
			break;
		case STATE_READROM:
			if( c->bit == 0 )
			{
				c->shift = c->data[ 7 - c->byte ];
				verboselog( 1, "ds2401_write(%d) <- data %02x\n", which, c->shift );
			}
			c->tx = c->shift & 1;
			c->shift >>= 1;
			c->bit++;
			if( c->bit == 8 )
			{
				c->bit = 0;
				c->byte++;
			}
			verboselog( 2, "ds2401_write(%d) state_readrom %d\n", which, c->tx );
			mame_timer_adjust( c->timer, c->t_rdv, which, time_never );
			break;
		default:
			verboselog( 0, "ds2401_write(%d) state not handled: %d\n", which, c->state );
			break;
		}
		mame_timer_adjust( c->reset_timer, c->t_rstl, which, time_never );
	}
	else if( data == 1 && c->rx == 0 )
	{
		switch( c->state )
		{
		case STATE_RESET:
			c->state = STATE_RESET1;
			mame_timer_adjust( c->timer, c->t_pdh, which, time_never );
			break;
		}
		mame_timer_adjust( c->reset_timer, time_never, which, time_never );
	}
	c->rx = data;
}

int ds2401_read( int which )
{
	struct ds2401_chip *c = &ds2401[ which ];

	verboselog( 2, "ds2401_read( %d ) %d\n", which, c->tx & c->rx );
	return c->tx & c->rx;
}

/*

app74.pdf

Under normal circumstances an ibutton will sample the line 30us after the falling edge of the start condition.
The internal time base of ibutton may deviate from its nominal value. The allowed tollerance band ranges from 15us to 60us.
This means that the actual slave sampling may occur anywhere from 15 and 60us after the start condition, which is a ratio of 1 to 4.
During this time frame the voltage on the data line must stay below Vilmax or above Vihmin.

In the 1-Wire system, the logical values 1 and 0 are represented by certain voltages in special waveforms.
The waveforms needed to write commands or data to ibuttons are called write-1 and write-0 time slots.
The duration of a low pulse to write a 1 must be shorter than 15us.
To write a 0, the duration of the low pulse must be at least 60us to cope with worst-case conditions.

The duration of the active part of a time slot can be extended beyond 60us.
The maximum extension is limited by the fact that a low pulse of a duration of at least eight active time slots ( 480us ) is defined as a Reset Pulse.
Allowing the same worst-case tolerance ratio, a low pulse of 120us might be sufficient for a reset.
This limits the extension of the active part of a time slot to a maximum of 120us to prevent misinterpretation with reset.

Commands and data are sent to ibuttons by combining write-0 and write-1 time slots.
To read data, the master has to generate read-data time slots to define the start condition of each bit.
The read-data time slots looks essentially the same as a write-1 time slot from the masters point of view.
Starting at the high-to-low transition, the ibuttons sends 1 bit of its addressed contents.
If the data bit is a 1, the ibutton leaves the pulse unchanged.
If the data bit is a 0, the ibutton will pull the data line low for 15us.
In this time frame data is valid for reading by the master.
The duration of the low pulse sent by the master should be a minimum of 1us with a maximum value as short as possible to maximize the master sampling window.

The Reset Pulse provides a clear starting condition that supersedes any time slot synchronisation.
It is defined as single low pulse of minimum duration of eight time slots or 480us followed by a Reset-high time tRSTH of another 480us.
After a Reset Pulse has been sent, the ibutton will wait for the time tPDH and then generate a Pulse-Presence Pulse of duration tPDL.
No other communication on the 1-Wire bus is allowed during tRSTH.

There are 1,000 microseconds in a millisecond, and 1,000 milliseconds in a second.
Thus, there are 1,000,000 microseconds in a second. Why is it "usec"?
The "u" is supposed to look like the Greek letter Mu that we use for "micro". .
*/

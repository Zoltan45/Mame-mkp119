/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "sound/samples.h"
#include "includes/galaga.h"

/***************************************************************************

 BATTLES CPU4(custum I/O Emulation) I/O Handlers

***************************************************************************/

static mame_timer *nmi_timer;

static UINT8 customio[16];
static char battles_customio_command;
static char battles_customio_prev_command;
static char battles_customio_command_count;
static char battles_customio_data;
static char battles_sound_played;

static TIMER_CALLBACK( battles_nmi_generate );


void battles_customio_init(void)
{
	battles_customio_command = 0;
	battles_customio_prev_command = 0;
	battles_customio_command_count = 0;
	battles_customio_data = 0;
	battles_sound_played = 0;
	nmi_timer = mame_timer_alloc(battles_nmi_generate);
}


static TIMER_CALLBACK( battles_nmi_generate )
{

	battles_customio_prev_command = battles_customio_command;

	if( battles_customio_command & 0x10 ){
		if( battles_customio_command_count == 0 ){
			cpunum_set_input_line(3, INPUT_LINE_NMI, PULSE_LINE);
		}else{
			cpunum_set_input_line(0, INPUT_LINE_NMI, PULSE_LINE);
			cpunum_set_input_line(3, INPUT_LINE_NMI, PULSE_LINE);
		}
	}else{
		cpunum_set_input_line(0, INPUT_LINE_NMI, PULSE_LINE);
		cpunum_set_input_line(3, INPUT_LINE_NMI, PULSE_LINE);
	}
	battles_customio_command_count++;
}


READ8_HANDLER( battles_customio0_r )
{
	logerror("CPU0 %04x: custom I/O Read = %02x\n",activecpu_get_pc(),battles_customio_command);
	return battles_customio_command;
}

READ8_HANDLER( battles_customio3_r )
{
	int	return_data;

	if( activecpu_get_pc() == 0xAE ){
		/* CPU4 0xAA - 0xB9 : waiting for MB8851 ? */
		return_data =	( (battles_customio_command & 0x10) << 3)
						| 0x00
						| (battles_customio_command & 0x0f);
	}else{
		return_data =	( (battles_customio_prev_command & 0x10) << 3)
						| 0x60
						| (battles_customio_prev_command & 0x0f);
	}
	logerror("CPU3 %04x: custom I/O Read = %02x\n",activecpu_get_pc(),return_data);

	return return_data;
}


WRITE8_HANDLER( battles_customio0_w )
{
	logerror("CPU0 %04x: custom I/O Write = %02x\n",activecpu_get_pc(),data);

	battles_customio_command = data;
	battles_customio_command_count = 0;

	switch (data)
	{
		case 0x10:
			mame_timer_adjust(nmi_timer, time_never, 0, time_never);
			return; /* nop */
	}
	mame_timer_adjust(nmi_timer, MAME_TIME_IN_USEC(166), 0, MAME_TIME_IN_USEC(166));

}

WRITE8_HANDLER( battles_customio3_w )
{
	logerror("CPU3 %04x: custom I/O Write = %02x\n",activecpu_get_pc(),data);

	battles_customio_command = data;
}



READ8_HANDLER( battles_customio_data0_r )
{
	logerror("CPU0 %04x: custom I/O parameter %02x Read = %02x\n",activecpu_get_pc(),offset,battles_customio_data);

	return battles_customio_data;
}

READ8_HANDLER( battles_customio_data3_r )
{
	logerror("CPU3 %04x: custom I/O parameter %02x Read = %02x\n",activecpu_get_pc(),offset,battles_customio_data);
	return battles_customio_data;
}


WRITE8_HANDLER( battles_customio_data0_w )
{
	logerror("CPU0 %04x: custom I/O parameter %02x Write = %02x\n",activecpu_get_pc(),offset,data);
	battles_customio_data = data;
}

WRITE8_HANDLER( battles_customio_data3_w )
{
	logerror("CPU3 %04x: custom I/O parameter %02x Write = %02x\n",activecpu_get_pc(),offset,data);
	battles_customio_data = data;
}


WRITE8_HANDLER( battles_CPU4_coin_w )
{
	set_led_status(0,data & 0x02);	// Start 1
	set_led_status(1,data & 0x01);	// Start 2

	coin_counter_w(0,data & 0x20);
	coin_counter_w(1,data & 0x10);
	coin_lockout_global_w(~data & 0x04);
}


WRITE8_HANDLER( battles_noise_sound_w )
{
	logerror("CPU3 %04x: 50%02x Write = %02x\n",activecpu_get_pc(),offset,data);
	if( (battles_sound_played == 0) && (data == 0xFF) ){
		if( customio[0] == 0x40 ){
			sample_start (0, 0, 0);
		}
		else{
			sample_start (0, 1, 0);
		}
	}
	battles_sound_played = data;
}


READ8_HANDLER( battles_input_port_r )
{
	switch ( offset )
	{
		default:
		case 0: return ~BITSWAP8(readinputport(0),6,7,5,4,3,2,1,0) >> 4;
		case 1: return ~readinputport(1) & 0x0f;
		case 2: return ~readinputport(1) >> 4;
		case 3: return ~readinputport(0) & 0x0f;
	}
}


INTERRUPT_GEN( battles_interrupt_4 )
{
	cpunum_set_input_line(3, 0, HOLD_LINE);
}


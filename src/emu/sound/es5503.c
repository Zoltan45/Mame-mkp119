/*

  ES5503 - Ensoniq ES5503 "DOC" emulator v1.0
  By R. Belmont.

  Copyright (c) 2005-2007 R. Belmont.

  This software is dual-licensed: it may be used in MAME and properly licensed
  MAME derivatives under the terms of the MAME license.  For use outside of
  MAME and properly licensed derivatives, it is available under the
  terms of the GNU Lesser General Public License (LGPL), version 2.1.
  You may read the LGPL at http://www.gnu.org/licenses/lgpl.html

  History: the ES5503 was the next design after the famous C64 "SID" by Bob Yannes.
  It powered the legendary Mirage sampler (the first affordable pro sampler) as well
  as the ESQ-1 synth/sequencer.  The ES5505 (used in Taito's F3 System) and 5506
  (used in the "Soundscape" series of ISA PC sound cards) followed on a fundamentally
  similar architecture.

  Bugs: On the real silicon, oscillators 30 and 31 have random volume fluctuations and are
  unusable for playback.  We don't attempt to emulate that. :-)

  Additionally, in "swap" mode, there's one cycle when the switch takes place where the
  oscillator's output is 0x80 (centerline) regardless of the sample data.  This can
  cause audible clicks and a general degradation of audio quality if the correct sample
  data at that point isn't 0x80 or very near it.

  Changes:
  0.2 (RB) - improved behavior for volumes > 127, fixes missing notes in Nucleus & missing voices in Thexder
  0.3 (RB) - fixed extraneous clicking, improved timing behavior for e.g. Music Construction Set & Music Studio
  0.4 (RB) - major fixes to IRQ semantics and end-of-sample handling.
  0.5 (RB) - more flexible wave memory hookup (incl. banking) and save state support.
  1.0 (RB) - properly respects the input clock
*/

#include <math.h>
#include "sndintrf.h"
#include "cpuintrf.h"
#include "es5503.h"
#include "streams.h"
#include "state.h"

typedef struct
{
	void *chip;

	UINT16 freq;
	UINT16 wtsize;
	UINT8  control;
	UINT8  vol;
	UINT8  data;
	UINT32 wavetblpointer;
	UINT8  wavetblsize;
	UINT8  resolution;

	UINT32 accumulator;
	UINT8  irqpend;
	mame_timer *timer;
} ES5503Osc;

typedef struct
{
	ES5503Osc oscillators[32];

	UINT8 *docram;

	int index;
	sound_stream * stream;

	void (*irq_callback)(int);	// IRQ callback

	read8_handler adc_read;		// callback for the 5503's built-in analog to digital converter

	INT8  oscsenabled;		// # of oscillators enabled

	int   rege0;			// contents of register 0xe0

	UINT32 clock;
	UINT32 output_rate;
} ES5503Chip;

static UINT16 wavesizes[8] = { 256, 512, 1024, 2048, 4096, 8192, 16384, 32768 };
static UINT32 wavemasks[8] = { 0x1ff00, 0x1fe00, 0x1fc00, 0x1f800, 0x1f000, 0x1e000, 0x1c000, 0x18000 };
static UINT32 accmasks[8]  = { 0xff, 0x1ff, 0x3ff, 0x7ff, 0xfff, 0x1fff, 0x3fff, 0x7fff };
static int    resshifts[8] = { 9, 10, 11, 12, 13, 14, 15, 16 };

enum
{
	MODE_FREE = 0,
	MODE_ONESHOT = 1,
	MODE_SYNCAM = 2,
	MODE_SWAP = 3
};

// halt_osc: handle halting an oscillator
// chip = chip ptr
// onum = oscillator #
// type = 1 for 0 found in sample data, 0 for hit end of table size
static void es5503_halt_osc(ES5503Chip *chip, int onum, int type, UINT32 *accumulator)
{
	ES5503Osc *pOsc = &chip->oscillators[onum];
	ES5503Osc *pPartner = &chip->oscillators[onum^1];
	int mode = (pOsc->control>>1) & 3;

	// if 0 found in sample data or mode is not free-run, halt this oscillator
	if ((type != MODE_FREE) || (mode > 0))
	{
		pOsc->control |= 1;
	}
	else
	{
		// reset the accumulator if not halting
		*accumulator = 0;
	}

	// if swap mode, start the partner
	if (mode == MODE_SWAP)
	{
		pPartner->control &= ~1;	// clear the halt bit
		pPartner->accumulator = 0;	// and make sure it starts from the top
	}

	// IRQ enabled for this voice?
	if (pOsc->control & 0x08)
	{
		pOsc->irqpend = 1;

		if (chip->irq_callback)
		{
			chip->irq_callback(1);
		}
	}
}

static TIMER_CALLBACK_PTR( es5503_timer_cb )
{
	ES5503Osc *osc = param;
	ES5503Chip *chip = (ES5503Chip *)osc->chip;

	stream_update(chip->stream);
}

static void es5503_pcm_update(void *param, stream_sample_t **inputs, stream_sample_t **outputs, int length)
{
	INT32 mix[48000*2];
	INT32 *mixp;
	int osc, snum, i;
	UINT32 ramptr;
	ES5503Chip *chip = param;

	memset(mix, 0, sizeof(mix));

	for (osc = 0; osc < (chip->oscsenabled+1); osc++)
	{
		ES5503Osc *pOsc = &chip->oscillators[osc];

		mixp = &mix[0];

		if (!(pOsc->control & 1))
		{
			UINT32 wtptr = pOsc->wavetblpointer & wavemasks[pOsc->wavetblsize], altram;
			UINT32 acc = pOsc->accumulator;
			UINT16 wtsize = pOsc->wtsize - 1;
			UINT8 ctrl = pOsc->control;
			UINT16 freq = pOsc->freq;
			INT16 vol = pOsc->vol;
			INT8 data = -128;
			int resshift = resshifts[pOsc->resolution] - pOsc->wavetblsize;
			UINT32 sizemask = accmasks[pOsc->wavetblsize];

			for (snum = 0; snum < length; snum++)
			{
				ramptr = (acc >> resshift) & sizemask;
				altram = acc >> resshift;

				acc += freq;

				data = (INT32)chip->docram[ramptr + wtptr] ^ 0x80;

				if (chip->docram[ramptr + wtptr] == 0x00)
				{
					es5503_halt_osc(chip, osc, 1, &acc);
				}
				else
				{
					if (pOsc->control & 0x10)
					{
						*mixp++ += (data * vol);
						mixp++;
					}
					else
					{
						mixp++;
						*mixp++ += (data * vol);
					}

					if (altram >= wtsize)
					{
						es5503_halt_osc(chip, osc, 0, &acc);
					}
				}

				// if oscillator halted, we've got no more samples to generate
				if (pOsc->control & 1)
				{
					ctrl |= 1;
					break;
				}
			}

			pOsc->control = ctrl;
			pOsc->accumulator = acc;
			pOsc->data = data ^ 0x80;
		}
	}

	mixp = &mix[0];
	for (i = 0; i < length; i++)
	{
		outputs[0][i] = (*mixp++)>>1;
		outputs[1][i] = (*mixp++)>>1;
	}
}


static void *es5503_start(int sndindex, int clock, const void *config)
{
	const struct ES5503interface *intf;
	int osc;
	ES5503Chip *chip;

	chip = auto_malloc(sizeof(*chip));
	memset(chip, 0, sizeof(*chip));
	chip->index = sndindex;

	intf = config;

	chip->irq_callback = intf->irq_callback;
	chip->adc_read = intf->adc_read;
	chip->docram = intf->wave_memory;
	chip->clock = clock;

	chip->rege0 = 0x80;

	for (osc = 0; osc < 32; osc++)
	{
		char sname[32];
		sprintf(sname, "ES5503 %d osc %d", sndindex, osc);

		state_save_register_item(sname, sndindex, chip->oscillators[osc].freq);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].wtsize);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].control);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].vol);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].data);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].wavetblpointer);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].wavetblsize);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].resolution);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].accumulator);
		state_save_register_item(sname, sndindex, chip->oscillators[osc].irqpend);

		chip->oscillators[osc].data = 0x80;
		chip->oscillators[osc].irqpend = 0;
		chip->oscillators[osc].accumulator = 0;

		chip->oscillators[osc].timer = mame_timer_alloc_ptr(es5503_timer_cb, &chip->oscillators[osc]);
		chip->oscillators[osc].chip = (void *)chip;
	}

	chip->oscsenabled = 1;

	chip->output_rate = (clock/8)/34;	// (input clock / 8) / # of oscs. enabled + 2
	chip->stream = stream_create(0, 2, chip->output_rate, chip, es5503_pcm_update);

	return chip;
}

READ8_HANDLER(ES5503_reg_0_r)
{
	UINT8 retval;
	int i;
	ES5503Chip *chip = sndti_token(SOUND_ES5503, 0);

	stream_update(chip->stream);

	if (offset < 0xe0)
	{
		int osc = offset & 0x1f;

		switch(offset & 0xe0)
		{
			case 0:		// freq lo
				return (chip->oscillators[osc].freq & 0xff);
				break;

			case 0x20:     	// freq hi
				return (chip->oscillators[osc].freq >> 8);
				break;

			case 0x40:	// volume
				return chip->oscillators[osc].vol;
				break;

			case 0x60:	// data
				return chip->oscillators[osc].data;
				break;

			case 0x80:	// wavetable pointer
				return (chip->oscillators[osc].wavetblpointer>>8) & 0xff;
				break;

			case 0xa0:	// oscillator control
				return chip->oscillators[osc].control;
				break;

			case 0xc0:	// bank select / wavetable size / resolution
				retval = 0;
				if (chip->oscillators[osc].wavetblpointer & 0x10000)
				{
					retval |= 0x40;
				}

				retval |= (chip->oscillators[osc].wavetblsize<<3);
				retval |= chip->oscillators[osc].resolution;
				return retval;
				break;
		}
	}
	else	 // global registers
	{
		switch (offset)
		{
			case 0xe0:	// interrupt status
				retval = chip->rege0;

				// scan all oscillators
				for (i = 0; i < chip->oscsenabled+1; i++)
				{
					if (chip->oscillators[i].irqpend)
					{
						// signal this oscillator has an interrupt
						retval = i<<1;

						chip->rege0 = retval | 0x80;

						// and clear its flag
						chip->oscillators[i].irqpend--;

						if (chip->irq_callback)
						{
							chip->irq_callback(0);
						}
						break;
					}
				}

				// if any oscillators still need to be serviced, assert IRQ again immediately
				for (i = 0; i < chip->oscsenabled+1; i++)
				{
					if (chip->oscillators[i].irqpend)
					{
						if (chip->irq_callback)
						{
							chip->irq_callback(1);
						}
						break;
					}
				}

				return retval;
				break;

			case 0xe1:	// oscillator enable
				return chip->oscsenabled<<1;
				break;

			case 0xe2:	// A/D converter
				if (chip->adc_read)
				{
					return chip->adc_read(0);
				}
				break;
		}
	}

	return 0;
}

WRITE8_HANDLER(ES5503_reg_0_w)
{
	ES5503Chip *chip = sndti_token(SOUND_ES5503, 0);

	stream_update(chip->stream);

	if (offset < 0xe0)
	{
		int osc = offset & 0x1f;

		switch(offset & 0xe0)
		{
			case 0:		// freq lo
				chip->oscillators[osc].freq &= 0xff00;
				chip->oscillators[osc].freq |= data;
				break;

			case 0x20:     	// freq hi
				chip->oscillators[osc].freq &= 0x00ff;
				chip->oscillators[osc].freq |= (data<<8);
				break;

			case 0x40:	// volume
				chip->oscillators[osc].vol = data;
				break;

			case 0x60:	// data - ignore writes
				break;

			case 0x80:	// wavetable pointer
				chip->oscillators[osc].wavetblpointer = (data<<8);
				break;

			case 0xa0:	// oscillator control
				// if a fresh key-on, reset the ccumulator
				if ((chip->oscillators[osc].control & 1) && (!(data&1)))
				{
					chip->oscillators[osc].accumulator = 0;

					// if this voice generates interrupts, set a timer to make sure we service it on time
					if (((data & 0x09) == 0x08) && (chip->oscillators[osc].freq > 0))
					{
						UINT32 length, run;
						UINT32 wtptr = chip->oscillators[osc].wavetblpointer & wavemasks[chip->oscillators[osc].wavetblsize];
						UINT32 acc = 0;
						UINT16 wtsize = chip->oscillators[osc].wtsize-1;
						UINT16 freq = chip->oscillators[osc].freq;
						INT8 data = -128;
						int resshift = resshifts[chip->oscillators[osc].resolution] - chip->oscillators[osc].wavetblsize;
						UINT32 sizemask = accmasks[chip->oscillators[osc].wavetblsize];
						UINT32 ramptr, altram;
						mame_time period;

						run = 1;
						length = 0;
						while (run)
						{
							ramptr = (acc >> resshift) & sizemask;
							altram = (acc >> resshift);
							acc += freq;
							data = (INT32)chip->docram[ramptr + wtptr];

							if ((data == 0) || (altram >= wtsize))
							{
								run = 0;
							}
							else
							{
								length++;
							}
						}

						// ok, we run for this long
						period = scale_up_mame_time(MAME_TIME_IN_HZ(chip->output_rate), length);

						mame_timer_adjust_ptr(chip->oscillators[osc].timer, period, period);
					}
				}
				else if (!(chip->oscillators[osc].control & 1) && (data&1))
				{
					// key off
					mame_timer_adjust_ptr(chip->oscillators[osc].timer, time_never, time_never);
				}

				chip->oscillators[osc].control = data;
				break;

			case 0xc0:	// bank select / wavetable size / resolution
				if (data & 0x40)	// bank select - not used on the Apple IIgs
				{
					chip->oscillators[osc].wavetblpointer |= 0x10000;
				}
				else
				{
					chip->oscillators[osc].wavetblpointer &= 0xffff;
				}

				chip->oscillators[osc].wavetblsize = ((data>>3) & 7);
				chip->oscillators[osc].wtsize = wavesizes[chip->oscillators[osc].wavetblsize];
				chip->oscillators[osc].resolution = (data & 7);
				break;
		}
	}
	else	 // global registers
	{
		switch (offset)
		{
			case 0xe0:	// interrupt status
				break;

			case 0xe1:	// oscillator enable
				chip->oscsenabled = (data>>1);

				chip->output_rate = (chip->clock/8)/(2+chip->oscsenabled);
				stream_set_sample_rate(chip->stream, chip->output_rate);
				break;

			case 0xe2:	// A/D converter
				break;
		}
	}
}

void ES5503_set_base_0(UINT8 *wavemem)
{
	ES5503Chip *chip = sndti_token(SOUND_ES5503, 0);

	chip->docram = wavemem;
}

/**************************************************************************
 * Generic get_info
 **************************************************************************/

static void es5503_set_info(void *token, UINT32 state, sndinfo *info)
{
	switch (state)
	{
		/* no parameters to set */
	}
}


void es5503_get_info(void *token, UINT32 state, sndinfo *info)
{
	switch (state)
	{
		/* --- the following bits of info are returned as 64-bit signed integers --- */

		/* --- the following bits of info are returned as pointers to data or functions --- */
		case SNDINFO_PTR_SET_INFO:						info->set_info = es5503_set_info;		break;
		case SNDINFO_PTR_START:							info->start = es5503_start;				break;
		case SNDINFO_PTR_STOP:							/* Nothing */							break;
		case SNDINFO_PTR_RESET:							/* Nothing */							break;

		/* --- the following bits of info are returned as NULL-terminated strings --- */
		case SNDINFO_STR_NAME:							info->s = "ES5503";						break;
		case SNDINFO_STR_CORE_FAMILY:					info->s = "Ensoniq ES550x";					break;
		case SNDINFO_STR_CORE_VERSION:					info->s = "1.0";						break;
		case SNDINFO_STR_CORE_FILE:						info->s = __FILE__;						break;
		case SNDINFO_STR_CORE_CREDITS:					info->s = "Copyright (c) 2005-2007 R. Belmont"; break;
	}
}


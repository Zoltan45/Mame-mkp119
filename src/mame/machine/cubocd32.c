#include "driver.h"
#include "includes/amiga.h"
#include "cdrom.h"
#include "coreutil.h"
#include "sound/cdda.h"


/*********************************************************************************

    Akiko custom chip emulation

The Akiko chip has:
- built in 1KB NVRAM
- chunky to planar converter
- custom CDROM controller

TODO: Add CDDA support

*********************************************************************************/

#define LOG_AKIKO		0
#define LOG_AKIKO_I2C	0
#define LOG_AKIKO_CD	0

#define CD_SECTOR_TIME		(1000/((150*1024)/2048))	/* 1X CDROM sector time in msec (300KBps) */


/*************************************
 *
 * Akiko I2C NVRAM implementation
 *
 ************************************/

enum {
	I2C_WAIT = 0,
	I2C_DEVICEADDR,
	I2C_WORDADDR,
	I2C_DATA
};

#define	NVRAM_SIZE 1024
#define	NVRAM_PAGE_SIZE	16	/* max size of one write request */

struct akiko_def
{
	/* chunky to planar converter */
	UINT32	c2p_input_buffer[8];
	UINT32	c2p_output_buffer[8];
	UINT32	c2p_input_index;
	UINT32	c2p_output_index;

	/* i2c bus */
	int		i2c_state;
	int		i2c_bitcounter;
	int		i2c_direction;
	int		i2c_sda_dir_nvram;
	int		i2c_scl_out;
	int		i2c_scl_in;
	int		i2c_scl_dir;
	int		i2c_oscl;
	int		i2c_sda_out;
	int		i2c_sda_in;
	int		i2c_sda_dir;
	int		i2c_osda;

	/* nvram */
	UINT8	nvram[NVRAM_SIZE];
	UINT8	nvram_writetmp[NVRAM_PAGE_SIZE];
	int		nvram_address;
	int		nvram_writeaddr;
	int		nvram_rw;
	UINT8	nvram_byte;

	/* cdrom */
	UINT32	cdrom_status[2];
	UINT32	cdrom_address[2];
	UINT32	cdrom_track_index;
	UINT32	cdrom_lba_start;
	UINT32	cdrom_lba_end;
	UINT32	cdrom_lba_cur;
	UINT16	cdrom_readmask;
	UINT16	cdrom_readreqmask;
	UINT32	cdrom_dmacontrol;
	UINT32	cdrom_numtracks;
	UINT8	cdrom_speed;
	UINT8	cdrom_cmd_start;
	UINT8	cdrom_cmd_end;
	UINT8	cdrom_cmd_resp;
	cdrom_file *cdrom;
	UINT8 *	cdrom_toc;
	mame_timer *dma_timer;
	mame_timer *frame_timer;
} akiko;

static TIMER_CALLBACK(akiko_dma_proc);
static TIMER_CALLBACK(akiko_frame_proc);

void amiga_akiko_init(void)
{
	akiko.c2p_input_index = 0;
	akiko.c2p_output_index = 0;

	akiko.i2c_state = I2C_WAIT;
	akiko.i2c_bitcounter = -1;
	akiko.i2c_direction = -1;
	akiko.i2c_sda_dir_nvram = 0;
	akiko.i2c_scl_out = 0;
	akiko.i2c_scl_in = 0;
	akiko.i2c_scl_dir = 0;
	akiko.i2c_oscl = 0;
	akiko.i2c_sda_out = 0;
	akiko.i2c_sda_in = 0;
	akiko.i2c_sda_dir = 0;
	akiko.i2c_osda = 0;

	akiko.nvram_address = 0;
	akiko.nvram_writeaddr = 0;
	akiko.nvram_rw = 0;
	akiko.nvram_byte = 0;
	memset( akiko.nvram, 0, NVRAM_SIZE );

	akiko.cdrom_status[0] = akiko.cdrom_status[1] = 0;
	akiko.cdrom_address[0] = akiko.cdrom_address[1] = 0;
	akiko.cdrom_track_index = 0;
	akiko.cdrom_lba_start = 0;
	akiko.cdrom_lba_end = 0;
	akiko.cdrom_lba_cur = 0;
	akiko.cdrom_readmask = 0;
	akiko.cdrom_readreqmask = 0;
	akiko.cdrom_dmacontrol = 0;
	akiko.cdrom_numtracks = 0;
	akiko.cdrom_speed = 0;
	akiko.cdrom_cmd_start = 0;
	akiko.cdrom_cmd_end = 0;
	akiko.cdrom_cmd_resp = 0;
	akiko.cdrom = cdrom_open(get_disk_handle(0));
	akiko.cdrom_toc = NULL;
	akiko.dma_timer = mame_timer_alloc(akiko_dma_proc);
	akiko.frame_timer = mame_timer_alloc(akiko_frame_proc);

	/* create the TOC table */
	if ( akiko.cdrom != NULL && cdrom_get_last_track(akiko.cdrom) )
	{
		UINT8 *p;
		int		i, addrctrl = cdrom_get_adr_control( akiko.cdrom, 0 );
		UINT32	discend;

		discend = cdrom_get_track_start(akiko.cdrom,cdrom_get_last_track(akiko.cdrom)-1);
		discend += cdrom_get_toc(akiko.cdrom)->tracks[cdrom_get_last_track(akiko.cdrom)-1].frames;
		discend = lba_to_msf(discend);

		akiko.cdrom_numtracks = cdrom_get_last_track(akiko.cdrom)+3;

		akiko.cdrom_toc = (UINT8*)auto_malloc(13*akiko.cdrom_numtracks);
		memset( akiko.cdrom_toc, 0, 13*akiko.cdrom_numtracks);

		p = akiko.cdrom_toc;
		p[1] = ((addrctrl & 0x0f) << 4) | ((addrctrl & 0xf0) >> 4);
		p[3] = 0xa0; /* first track */
		p[8] = 1;
		p += 13;
		p[1] = 0x01;
		p[3] = 0xa1; /* last track */
		p[8] = cdrom_get_last_track(akiko.cdrom);
		p += 13;
		p[1] = 0x01;
		p[3] = 0xa2; /* disc end */
		p[8] = (discend >> 16 ) & 0xff;
		p[9] = (discend >> 8 ) & 0xff;
		p[10] = discend & 0xff;
		p += 13;

		for( i = 0; i < cdrom_get_last_track(akiko.cdrom); i++ )
		{
			UINT32	trackpos = cdrom_get_track_start(akiko.cdrom,i);

			trackpos = lba_to_msf(trackpos);
			addrctrl = cdrom_get_adr_control( akiko.cdrom, i );

			p[1] = ((addrctrl & 0x0f) << 4) | ((addrctrl & 0xf0) >> 4);
			p[3] = dec_2_bcd( i+1 );
			p[8] = (trackpos >> 16 ) & 0xff;
			p[9] = (trackpos >> 8 ) & 0xff;
			p[10] = trackpos & 0xff;

			p += 13;
		}
	}
}

static void akiko_i2c( void )
{
    akiko.i2c_sda_in = 1;

    if ( !akiko.i2c_sda_dir_nvram && akiko.i2c_scl_out && akiko.i2c_oscl )
    {
		if ( !akiko.i2c_sda_out && akiko.i2c_osda )	/* START-condition? */
		{
	    	akiko.i2c_state = I2C_DEVICEADDR;
	    	akiko.i2c_bitcounter = 0;
	    	akiko.i2c_direction = -1;
#if LOG_AKIKO_I2C
	    	logerror("START\n");
#endif
	    	return;
		}
		else if ( akiko.i2c_sda_out && !akiko.i2c_osda ) /* STOP-condition? */
		{
	    	akiko.i2c_state = I2C_WAIT;
	    	akiko.i2c_bitcounter = -1;
#if LOG_AKIKO_I2C
	    	logerror("STOP\n");
#endif
	    	if ( akiko.i2c_direction > 0 )
	    	{
				memcpy( akiko.nvram + ( akiko.nvram_address & ~(NVRAM_PAGE_SIZE - 1) ), akiko.nvram_writetmp, NVRAM_PAGE_SIZE );
				akiko.i2c_direction = -1;
#if LOG_AKIKO_I2C
				{
					int i;

					logerror("NVRAM write address %04X:", akiko.nvram_address & ~(NVRAM_PAGE_SIZE - 1));
					for (i = 0; i < NVRAM_PAGE_SIZE; i++)
		    			logerror("%02X", akiko.nvram_writetmp[i]);
					logerror("\n");
				}
#endif
	    	}
	    	return;
		}
    }

    if ( akiko.i2c_bitcounter >= 0 )
    {
		if ( akiko.i2c_direction )
		{
	    	/* Amiga -> NVRAM */
	    	if ( akiko.i2c_scl_out && !akiko.i2c_oscl )
	    	{
				if ( akiko.i2c_bitcounter == 8 )
				{
				    akiko.i2c_sda_in = 0; /* ACK */

				    if ( akiko.i2c_direction > 0 )
				    {
						akiko.nvram_writetmp[akiko.nvram_writeaddr++] = akiko.nvram_byte;
						akiko.nvram_writeaddr &= (NVRAM_PAGE_SIZE - 1);
						akiko.i2c_bitcounter = 0;
		    		}
		    		else
		    		{
		    			akiko.i2c_bitcounter = -1;
		    		}
		    	}
		    	else
		    	{
				    akiko.nvram_byte <<= 1;
				    akiko.nvram_byte |= akiko.i2c_sda_out;
				    akiko.i2c_bitcounter++;
				}
	    	}
		}
		else
		{
			/* NVRAM -> Amiga */
			if ( akiko.i2c_scl_out && !akiko.i2c_oscl && akiko.i2c_bitcounter < 8 )
			{
				if ( akiko.i2c_bitcounter == 0 )
					akiko.nvram_byte = akiko.nvram[akiko.nvram_address];

				akiko.i2c_sda_dir_nvram = 1;
				akiko.i2c_sda_in = (akiko.nvram_byte & 0x80) ? 1 : 0;
				akiko.nvram_byte <<= 1;
				akiko.i2c_bitcounter++;

				if ( akiko.i2c_bitcounter == 8 )
				{
#if LOG_AKIKO_I2C
		    		logerror("NVRAM sent byte %02X address %04X\n", akiko.nvram[akiko.nvram_address], akiko.nvram_address);
#endif
		    		akiko.nvram_address++;
		    		akiko.nvram_address &= NVRAM_SIZE - 1;
		    		akiko.i2c_sda_dir_nvram = 0;
				}
	    	}

	    	if ( !akiko.i2c_sda_out && akiko.i2c_sda_dir && !akiko.i2c_scl_out ) /* ACK from Amiga */
				akiko.i2c_bitcounter = 0;
		}

		if ( akiko.i2c_bitcounter >= 0 )
			return;
    }

    switch( akiko.i2c_state )
    {
		case I2C_DEVICEADDR:
			if ( ( akiko.nvram_byte & 0xf0 ) != 0xa0 )
			{
#if LOG_AKIKO_I2C
				logerror("WARNING: I2C_DEVICEADDR: device address != 0xA0\n");
#endif
				akiko.i2c_state = I2C_WAIT;
	    		return;
			}

			akiko.nvram_rw = (akiko.nvram_byte & 1) ? 0 : 1;

			if ( akiko.nvram_rw )
			{
				/* 2 high address bits, only fetched if WRITE = 1 */
				akiko.nvram_address &= 0xff;
				akiko.nvram_address |= ((akiko.nvram_byte >> 1) & 3) << 8;
	    		akiko.i2c_state = I2C_WORDADDR;
	    		akiko.i2c_direction = -1;
	    	}
	    	else
	    	{
	    		akiko.i2c_state = I2C_DATA;
	    		akiko.i2c_direction = 0;
	    		akiko.i2c_sda_dir_nvram = 1;
			}

			akiko.i2c_bitcounter = 0;
#if LOG_AKIKO_I2C
			logerror("I2C_DEVICEADDR: rw %d, address %02Xxx\n", akiko.nvram_rw, akiko.nvram_address >> 8);
#endif
		break;

		case I2C_WORDADDR:
			akiko.nvram_address &= 0x300;
			akiko.nvram_address |= akiko.nvram_byte;

#if LOG_AKIKO_I2C
			logerror("I2C_WORDADDR: address %04X\n", akiko.nvram_address);
#endif
			if ( akiko.i2c_direction < 0 )
			{
				memcpy( akiko.nvram_writetmp, akiko.nvram + (akiko.nvram_address & ~(NVRAM_PAGE_SIZE - 1)), NVRAM_PAGE_SIZE);
	    		akiko.nvram_writeaddr = akiko.nvram_address & (NVRAM_PAGE_SIZE - 1);
			}

			akiko.i2c_state = I2C_DATA;
			akiko.i2c_bitcounter = 0;
			akiko.i2c_direction = 1;
		break;
    }
}

static void akiko_nvram_write(UINT32 data)
{
	int		sda;

	akiko.i2c_oscl = akiko.i2c_scl_out;
	akiko.i2c_scl_out = BIT(data,31);
	akiko.i2c_osda = akiko.i2c_sda_out;
	akiko.i2c_sda_out = BIT(data,30);
	akiko.i2c_scl_dir = BIT(data,15);
	akiko.i2c_sda_dir = BIT(data,14);

	sda = akiko.i2c_sda_out;
    if ( akiko.i2c_oscl != akiko.i2c_scl_out || akiko.i2c_osda != sda )
    {
    	akiko_i2c();
    	akiko.i2c_oscl = akiko.i2c_scl_out;
    	akiko.i2c_osda = sda;
    }
}

static UINT32 akiko_nvram_read(void)
{
	UINT32	v = 0;

	if ( !akiko.i2c_scl_dir )
	    v |= akiko.i2c_scl_in ? (1<<31) : 0x00;
	else
	    v |= akiko.i2c_scl_out ? (1<<31) : 0x00;

	if ( !akiko.i2c_sda_dir )
	    v |= akiko.i2c_sda_in ? (1<<30) : 0x00;
	else
	    v |= akiko.i2c_sda_out ? (1<<30) : 0x00;

	v |= akiko.i2c_scl_dir ? (1<<15) : 0x00;
	v |= akiko.i2c_sda_dir ? (1<<14) : 0x00;

    return v;
}

NVRAM_HANDLER( cd32 )
{
	if (read_or_write)
		/* save the SRAM settings */
		mame_fwrite(file, akiko.nvram, NVRAM_SIZE);
	else
	{
		/* load the SRAM settings */
		if (file)
			mame_fread(file, akiko.nvram, NVRAM_SIZE);
		else
			memset(akiko.nvram, 0, NVRAM_SIZE);
	}
}

/*************************************
 *
 * Akiko Chunky to Planar converter
 *
 ************************************/

static void akiko_c2p_write(UINT32 data)
{
	akiko.c2p_input_buffer[akiko.c2p_input_index] = data;
	akiko.c2p_input_index++;
	akiko.c2p_input_index &= 7;
	akiko.c2p_output_index = 0;
}

static UINT32 akiko_c2p_read(void)
{
	UINT32 val;

	if ( akiko.c2p_output_index == 0 )
	{
		int i;

		for ( i = 0; i < 8; i++ )
			akiko.c2p_output_buffer[i] = 0;

		for (i = 0; i < 8 * 32; i++) {
			if (akiko.c2p_input_buffer[7 - (i >> 5)] & (1 << (i & 31)))
				akiko.c2p_output_buffer[i & 7] |= 1 << (i >> 3);
		}
	}
	akiko.c2p_input_index = 0;
	val = akiko.c2p_output_buffer[akiko.c2p_output_index];
	akiko.c2p_output_index++;
	akiko.c2p_output_index &= 7;
	return val;
}

#if LOG_AKIKO
static const char* akiko_reg_names[] =
{
	/*0*/	"ID",
	/*1*/	"CDROM STATUS 1",
	/*2*/	"CDROM_STATUS 2",
	/*3*/	"???",
	/*4*/	"CDROM ADDRESS 1",
	/*5*/	"CDROM ADDRESS 2",
	/*6*/	"CDROM COMMAND 1",
	/*7*/	"CDROM COMMAND 2",
	/*8*/	"CDROM READMASK",
	/*9*/	"CDROM DMACONTROL",
	/*A*/	"???",
	/*B*/	"???",
	/*C*/	"NVRAM",
	/*D*/	"???",
	/*E*/	"C2P"
};

const char* get_akiko_reg_name(int reg)
{
	if (reg < 0xf )
	{
		return akiko_reg_names[reg];
	}
	else
	{
		return "???";
	}
}
#endif

/*************************************
 *
 * Akiko CDROM Controller
 *
 ************************************/

static void akiko_cdda_stop( void )
{
	int cddanum = cdda_num_from_cdrom(akiko.cdrom);

	if (cddanum != -1)
	{
		cdda_stop_audio(cddanum);
		mame_timer_reset( akiko.frame_timer, time_never );
	}
}

static void akiko_cdda_play( UINT32 lba, UINT32 num_blocks )
{
	int cddanum = cdda_num_from_cdrom(akiko.cdrom);
	if (cddanum != -1)
	{
		cdda_start_audio(cddanum, lba, num_blocks);
		mame_timer_adjust( akiko.frame_timer, MAME_TIME_IN_HZ( 75 ), 0, time_zero );
	}
}

static void akiko_cdda_pause( int pause )
{
	int cddanum = cdda_num_from_cdrom(akiko.cdrom);
	if (cddanum != -1)
	{
		if (cdda_audio_active(cddanum) && cdda_audio_paused(cddanum) != pause )
		{
			cdda_pause_audio(cddanum, pause);

			if ( pause )
			{
				mame_timer_reset( akiko.frame_timer, time_never );
			}
			else
			{
				mame_timer_adjust( akiko.frame_timer, MAME_TIME_IN_HZ( 75 ), 0, time_zero );
			}
		}
	}
}

static UINT8 akiko_cdda_getstatus( UINT32 *lba )
{
	int cddanum = cdda_num_from_cdrom(akiko.cdrom);

	if ( lba ) *lba = 0;

	if (cddanum != -1)
	{
		if (cdda_audio_active(cddanum))
		{
			if ( lba ) *lba = cdda_get_audio_lba(cddanum);

			if (cdda_audio_paused(cddanum))
			{
				return 0x12;	/* audio paused */
			}
			else
			{
				return 0x11;	/* audio in progress */
			}
		}
		else if (cdda_audio_ended(cddanum))
		{
			return 0x13;	/* audio ended */
		}
	}

	return 0x15;	/* no audio status */
}

static void akiko_set_cd_status( UINT32 status )
{
	akiko.cdrom_status[0] |= status;

	if ( akiko.cdrom_status[0] & akiko.cdrom_status[1] )
	{
#if LOG_AKIKO_CD
		logerror( "Akiko CD IRQ\n" );
#endif
		amiga_custom_w(REG_INTREQ, 0x8000 | INTENA_PORTS, 0);
	}
}

static TIMER_CALLBACK(akiko_frame_proc)
{
	int cddanum = cdda_num_from_cdrom(akiko.cdrom);

	(void)param;

	if (cddanum != -1)
	{
		UINT8	s = akiko_cdda_getstatus(NULL);

		if ( s == 0x11 )
		{
			akiko_set_cd_status( 0x80000000 );	/* subcode ready */
		}

		mame_timer_adjust( akiko.frame_timer, MAME_TIME_IN_HZ( 75 ), 0, time_zero );
	}
}

static UINT32 lba_from_triplet( UINT8 *triplet )
{
	UINT32	r;

	r = bcd_2_dec(triplet[0]) * (60*75);
	r += bcd_2_dec(triplet[1]) * 75;
	r += bcd_2_dec(triplet[2]);

	return r;
}

static TIMER_CALLBACK(akiko_dma_proc)
{
	UINT8	buf[2352];
	int		index;

	if ( (akiko.cdrom_dmacontrol & 0x04000000) == 0 )
		return;

	if ( akiko.cdrom_readreqmask == 0 )
		return;

	index = (akiko.cdrom_lba_cur - akiko.cdrom_lba_start) & 0x0f;

	if ( akiko.cdrom_readreqmask & ( 1 << index ) )
	{
		UINT32	track = cdrom_get_track( akiko.cdrom, akiko.cdrom_lba_cur );
		UINT32	datasize = cdrom_get_toc( akiko.cdrom )->tracks[track].datasize;
		UINT32	subsize = cdrom_get_toc( akiko.cdrom )->tracks[track].subsize;
		UINT32	secsize = datasize + subsize;
		int		i;

		if ( secsize <= (2352-16) )
		{
			UINT32	curmsf = lba_to_msf( akiko.cdrom_lba_cur );
			memset( buf, 0, 16 );

			buf[3] = akiko.cdrom_lba_cur - akiko.cdrom_lba_start;
			memset( &buf[4], 0xff, 8 );

			buf[12] = (curmsf >> 16) & 0xff;
			buf[13] = (curmsf >> 8) & 0xff;
			buf[14] = curmsf & 0xff;
			buf[15] = 0x01; /* mode1 */

			if ( !cdrom_read_data( akiko.cdrom, akiko.cdrom_lba_cur, &buf[16], CD_TRACK_RAW_DONTCARE ) )
			{
				logerror( "AKIKO: Read error trying to read sector %08x!\n", akiko.cdrom_lba_cur );
				return;
			}

			if ( subsize )
			{
				if ( !cdrom_read_subcode( akiko.cdrom, akiko.cdrom_lba_cur, &buf[16+datasize] ) )
				{
					logerror( "AKIKO: Read error trying to read subcode for sector %08x!\n", akiko.cdrom_lba_cur );
					return;
				}
			}
		}
		else
		{
			if ( !cdrom_read_data( akiko.cdrom, akiko.cdrom_lba_cur, buf, CD_TRACK_RAW_DONTCARE) )
			{
				logerror( "AKIKO: Read error trying to read sector %08x!\n", akiko.cdrom_lba_cur );
				return;
			}

			if ( subsize )
			{
				if ( !cdrom_read_subcode( akiko.cdrom, akiko.cdrom_lba_cur, &buf[datasize] ) )
				{
					logerror( "AKIKO: Read error trying to read subcode for sector %08x!\n", akiko.cdrom_lba_cur );
					return;
				}
			}
		}

#if LOG_AKIKO_CD
		logerror( "DMA: sector %d - address %08x\n", akiko.cdrom_lba_cur, akiko.cdrom_address[0] + (index*4096) );
#endif

		for( i = 0; i < 2352; i += 2 )
		{
			UINT16	data;

			data = buf[i];
			data <<= 8;
			data |= buf[i+1];

			amiga_chip_ram_w( akiko.cdrom_address[0] + (index*4096) + i, data );
		}

		akiko.cdrom_readmask |= ( 1 << index );
		akiko.cdrom_readreqmask &= ~( 1 << index );
		akiko.cdrom_lba_cur++;
	}

	if ( akiko.cdrom_readreqmask == 0 )
		akiko_set_cd_status(0x04000000);
	else
		mame_timer_adjust( akiko.dma_timer, MAME_TIME_IN_USEC( CD_SECTOR_TIME / akiko.cdrom_speed ), 0, time_zero );
}

static void akiko_start_dma( void )
{
	if ( akiko.cdrom_readreqmask == 0 )
		return;

	if ( akiko.cdrom_lba_start > akiko.cdrom_lba_end )
		return;

	if ( akiko.cdrom_speed == 0 )
		return;

	akiko.cdrom_lba_cur = akiko.cdrom_lba_start;

	mame_timer_adjust( akiko.dma_timer, MAME_TIME_IN_USEC( CD_SECTOR_TIME / akiko.cdrom_speed ), 0, time_zero );
}

static void akiko_setup_response( int len, UINT8 *r1 )
{
	int		resp_addr = akiko.cdrom_address[1];
	UINT8	resp_csum = 0xff;
	UINT8	resp_buffer[32];
	int		i;

	memset( resp_buffer, 0, sizeof( resp_buffer ) );

	for( i = 0; i < len; i++ )
	{
		resp_buffer[i] = r1[i];
		resp_csum -= resp_buffer[i];
	}

	resp_buffer[len++] = resp_csum;

	for( i = 0; i < len; i++ )
	{
		program_write_byte( resp_addr + ((akiko.cdrom_cmd_resp + i) & 0xff), resp_buffer[i] );
	}

	akiko.cdrom_cmd_resp = (akiko.cdrom_cmd_resp+len) & 0xff;

	akiko_set_cd_status( 0x10000000 ); /* new data available */
}

static TIMER_CALLBACK( akiko_cd_delayed_cmd )
{
	UINT8	resp[32];
	UINT8	cddastatus;

	if ( akiko.cdrom_status[0] & 0x10000000 )
		return;

	cddastatus = akiko_cdda_getstatus(NULL);

	if ( cddastatus == 0x11 || cddastatus == 0x12 )
		return;

	memset( resp, 0, sizeof( resp ) );
	resp[0] = param;

	param &= 0x0f;

	if ( param == 0x05 )
	{
#if LOG_AKIKO_CD
		logerror( "AKIKO: Completing Command %d\n", param );
#endif

		resp[0] = 0x06;

		if ( akiko.cdrom == NULL || akiko.cdrom_numtracks == 0 )
		{
			resp[1] = 0x80;
			akiko_setup_response( 15, resp );
		}
		else
		{
			resp[1] = 0x00;
			memcpy( &resp[2], &akiko.cdrom_toc[13*akiko.cdrom_track_index], 13 );

			akiko.cdrom_track_index = ( akiko.cdrom_track_index + 1 ) % akiko.cdrom_numtracks;

			akiko_setup_response( 15, resp );
		}
	}
}

static void akiko_update_cdrom( void )
{
	UINT8	resp[32], cmdbuf[32];

	if ( akiko.cdrom_status[0] & 0x10000000 )
		return;

	while ( akiko.cdrom_cmd_start != akiko.cdrom_cmd_end )
	{
		UINT32	cmd_addr = akiko.cdrom_address[1] + 0x200 + akiko.cdrom_cmd_start;
		int		cmd = program_read_byte( cmd_addr );

		memset( resp, 0, sizeof( resp ) );
		resp[0] = cmd;

		cmd &= 0x0f;

#if LOG_AKIKO_CD
		logerror( "CDROM command: %02X\n", cmd );
#endif

		if ( cmd == 0x02 ) /* pause audio */
		{
			resp[1] = 0x00;

			if ( akiko_cdda_getstatus(NULL) == 0x11 )
				resp[1] = 0x08;

			akiko_cdda_pause( 1 );

			akiko.cdrom_cmd_start = (akiko.cdrom_cmd_start+2) & 0xff;

			akiko_setup_response( 2, resp );
		}
		else if ( cmd == 0x03 ) /* unpause audio (and check audiocd playing status) */
		{
			resp[1] = 0x00;

			if ( akiko_cdda_getstatus(NULL) == 0x11 )
				resp[1] = 0x08;

			akiko_cdda_pause( 0 );

			akiko.cdrom_cmd_start = (akiko.cdrom_cmd_start+2) & 0xff;

			akiko_setup_response( 2, resp );
		}
		else if ( cmd == 0x04 ) /* seek/read/play cd multi command */
		{
			int	i;
			UINT32	startpos, endpos;

			for( i = 0; i < 13; i++ )
			{
				cmdbuf[i] = program_read_byte( cmd_addr );
				cmd_addr &= 0xffffff00;
				cmd_addr += ( akiko.cdrom_cmd_start + i + 1 ) & 0xff;
			}

			akiko.cdrom_cmd_start = (akiko.cdrom_cmd_start+13) & 0xff;

			if ( akiko.cdrom == NULL || akiko.cdrom_numtracks == 0 )
			{
				resp[1] = 0x80;
				akiko_setup_response( 2, resp );
			}
			else
			{
				startpos = lba_from_triplet( &cmdbuf[1] );
				endpos = lba_from_triplet( &cmdbuf[4] );

				akiko_cdda_stop();

				resp[1] = 0x00;

				if ( cmdbuf[7] == 0x80 )
				{
#if LOG_AKIKO_CD
					logerror( "AKIKO CD: PC:%06x Data read - start lba: %08x - end lba: %08x\n", safe_activecpu_get_pc(), startpos, endpos );
#endif
					akiko.cdrom_speed = (cmdbuf[8] & 0x40) ? 2 : 1;
					akiko.cdrom_lba_start = startpos;
					akiko.cdrom_lba_end = endpos;

					resp[1] = 0x02;
				}
				else if ( cmdbuf[10] & 0x04 )
				{
					logerror( "AKIKO CD: Audio Play - start lba: %08x - end lba: %08x\n", startpos, endpos );
					akiko_cdda_play( startpos, endpos - startpos );
					resp[1] = 0x08;
				}
				else
				{
#if LOG_AKIKO_CD
					logerror( "AKIKO CD: Seek - start lba: %08x - end lba: %08x\n", startpos, endpos );
#endif
					akiko.cdrom_track_index = 0;

					for( i = 0; i < cdrom_get_last_track(akiko.cdrom); i++ )
					{
						if ( startpos <= cdrom_get_track_start( akiko.cdrom, i ) )
						{
							/* reset to 0 */
							akiko.cdrom_track_index = i + 2;
							akiko.cdrom_track_index %= akiko.cdrom_numtracks;
							break;
						}
					}
				}

				akiko_setup_response( 2, resp );
			}
		}
		else if ( cmd == 0x05 ) /* read toc */
		{
			akiko.cdrom_cmd_start = (akiko.cdrom_cmd_start+3) & 0xff;

			mame_timer_set( MAME_TIME_IN_MSEC(1), resp[0], akiko_cd_delayed_cmd );

			break;
		}
		else if ( cmd == 0x06 ) /* read subq */
		{
			UINT32	lba;

			resp[1] = 0x00;

			(void)akiko_cdda_getstatus( &lba );

			if ( lba > 0 )
			{
				UINT32	disk_pos;
				UINT32	track_pos;
				UINT32	track;
				int		addrctrl;

				track = cdrom_get_track(akiko.cdrom, lba);
				addrctrl = cdrom_get_adr_control(akiko.cdrom, track);

				resp[2] = 0x00;
				resp[3] = ((addrctrl & 0x0f) << 4) | ((addrctrl & 0xf0) >> 4);
				resp[4] = dec_2_bcd(track+1);
				resp[5] = 0; /* index */

				disk_pos = lba_to_msf(lba);
				track_pos = lba_to_msf(lba - cdrom_get_track_start(akiko.cdrom, track));

				/* track position */
				resp[6] = (track_pos >> 16) & 0xff;
				resp[7] = (track_pos >> 8) & 0xff;
				resp[8] = track_pos & 0xff;

				/* disk position */
				resp[9] = (disk_pos >> 24) & 0xff;
				resp[10] = (disk_pos >> 16) & 0xff;
				resp[11] = (disk_pos >> 8) & 0xff;
				resp[12] = disk_pos & 0xff;
			}
			else
			{
				resp[1] = 0x80;
			}

			akiko_setup_response( 15, resp );
		}
		else if ( cmd == 0x07 )	/* check door status */
		{
			resp[1] = 0x01;

			akiko.cdrom_cmd_start = (akiko.cdrom_cmd_start+2) & 0xff;

			if ( akiko.cdrom == NULL || akiko.cdrom_numtracks == 0 )
				resp[1] = 0x80;

			akiko_setup_response( 20, resp );
			break;
		}
		else
		{
			break;
		}
	}
}

READ32_HANDLER(amiga_akiko32_r)
{
	UINT32		retval;

#if LOG_AKIKO
	if ( offset < (0x30/4) )
	{
		logerror( "Reading AKIKO reg %0x [%s] at PC=%06x\n", offset, get_akiko_reg_name(offset), activecpu_get_pc() );
	}
#endif

	switch( offset )
	{
		case 0x00/4:	/* ID */
			if ( akiko.cdrom != NULL ) cdda_set_cdrom(0, akiko.cdrom);
			return 0x0000cafe;

		case 0x04/4:	/* CDROM STATUS 1 */
			return akiko.cdrom_status[0];

		case 0x08/4:	/* CDROM STATUS 2 */
			return akiko.cdrom_status[1];

		case 0x10/4:	/* CDROM ADDRESS 1 */
			return akiko.cdrom_address[0];

		case 0x14/4:	/* CDROM ADDRESS 2 */
			return akiko.cdrom_address[1];

		case 0x18/4:	/* CDROM COMMAND 1 */
			akiko_update_cdrom();
			retval = akiko.cdrom_cmd_start;
			retval <<= 8;
			retval |= akiko.cdrom_cmd_resp;
			retval <<= 8;
			return retval;

		case 0x1C/4:	/* CDROM COMMAND 2 */
			akiko_update_cdrom();
			retval = akiko.cdrom_cmd_end;
			retval <<= 16;
			return retval;

		case 0x20/4:	/* CDROM DMA SECTOR READ MASK */
			retval = akiko.cdrom_readmask << 16;
			return retval;

		case 0x24/4:	/* CDROM DMA ENABLE? */
			retval = akiko.cdrom_dmacontrol;
			return retval;

		case 0x30/4:	/* NVRAM */
			return akiko_nvram_read();

		case 0x38/4:	/* C2P */
			return akiko_c2p_read();

		default:
			break;
	}

	return 0;
}

WRITE32_HANDLER(amiga_akiko32_w)
{
#if LOG_AKIKO
	if ( offset < (0x30/4) )
	{
		logerror( "Writing AKIKO reg %0x [%s] with %08x at PC=%06x\n", offset, get_akiko_reg_name(offset), data, activecpu_get_pc() );
	}
#endif

	switch( offset )
	{
		case 0x04/4:	/* CDROM STATUS 1 */
			akiko.cdrom_status[0] = data;
			break;

		case 0x08/4:	/* CDROM STATUS 2 */
			akiko.cdrom_status[1] = data;
			akiko.cdrom_status[0] &= data;
			break;

		case 0x10/4:	/* CDROM ADDRESS 1 */
			akiko.cdrom_address[0] = data;
			break;

		case 0x14/4:	/* CDROM ADDRESS 2 */
			akiko.cdrom_address[1] = data;
			break;

		case 0x18/4:	/* CDROM COMMAND 1 */
			if ( ( mem_mask & 0x00ff0000 ) == 0 )
				akiko.cdrom_cmd_start = ( data >> 16 ) & 0xff;

			if ( ( mem_mask & 0x0000ff00 ) == 0 )
				akiko.cdrom_cmd_resp = ( data >> 8 ) & 0xff;

			akiko_update_cdrom();
			break;

		case 0x1C/4:	/* CDROM COMMAND 2 */
			if ( ( mem_mask & 0x00ff0000 ) == 0 )
				akiko.cdrom_cmd_end = ( data >> 16 ) & 0xff;

			akiko_update_cdrom();
			break;

		case 0x20/4:	/* CDROM DMA SECTOR READ REQUEST WRITE */
#if LOG_AKIKO_CD
			logerror( "Read Req mask W: data %08x - mem mask %08x\n", data, mem_mask );
#endif
			if ( ( mem_mask & 0xffff0000 ) == 0 )
			{
				akiko.cdrom_readreqmask = (data >> 16);
				akiko.cdrom_readmask = 0;
			}
			break;

		case 0x24/4:	/* CDROM DMA ENABLE? */
#if LOG_AKIKO_CD
			logerror( "DMA enable W: data %08x - mem mask %08x\n", data, mem_mask );
#endif
			if ( ( akiko.cdrom_dmacontrol ^ data ) & 0x04000000 )
			{
				if ( data & 0x04000000 )
					akiko_start_dma();
			}
			akiko.cdrom_dmacontrol = data;
			break;

		case 0x30/4:
			akiko_nvram_write(data);
			break;

		case 0x38/4:
			akiko_c2p_write(data);
			break;

		default:
			break;
	}
}

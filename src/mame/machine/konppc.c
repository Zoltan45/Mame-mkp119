/* Konami PowerPC-based 3D games common functions */

#include "driver.h"
#include "cpu/sharc/sharc.h"
#include "video/voodoo.h"
#include "konppc.h"

#define MAX_CG_BOARDS	2

static UINT32 dsp_comm_ppc[MAX_CG_BOARDS][2];
static UINT32 dsp_comm_sharc[MAX_CG_BOARDS][2];
static int dsp_shared_ram_bank[MAX_CG_BOARDS];

static int cgboard_id;
static int cgboard_type;
static int num_cgboards;

static UINT32 *dsp_shared_ram[MAX_CG_BOARDS];

#define DSP_BANK_SIZE			0x10000
#define DSP_BANK_SIZE_WORD		(DSP_BANK_SIZE / 4)

static UINT32 dsp_state[MAX_CG_BOARDS];
static UINT32 pci_bridge_enable[MAX_CG_BOARDS];
static UINT32 nwk_device_sel[MAX_CG_BOARDS];
static int texture_bank[MAX_CG_BOARDS];

static int nwk_fifo_half_full_r;
static int nwk_fifo_half_full_w;
static int nwk_fifo_full;
static int nwk_fifo_mask;


static UINT32 *nwk_fifo[MAX_CG_BOARDS];
static int nwk_fifo_read_ptr[MAX_CG_BOARDS];
static int nwk_fifo_write_ptr[MAX_CG_BOARDS];

static UINT32 *nwk_ram[MAX_CG_BOARDS];

/*****************************************************************************/

void init_konami_cgboard(int num_boards, int type)
{
	int i;
	num_cgboards = num_boards;

	for (i=0; i < num_boards; i++)
	{
		dsp_comm_ppc[i][0] = 0x00;
		dsp_shared_ram[i] = auto_malloc(DSP_BANK_SIZE * 2);
		dsp_shared_ram_bank[i] = 0;

		dsp_state[i] = 0x80;
		texture_bank[i] = -1;

		pci_bridge_enable[i] = 0;
		nwk_device_sel[i] = 0;
		nwk_fifo_read_ptr[i] = 0;
		nwk_fifo_write_ptr[i] = 0;

		nwk_fifo[i] = auto_malloc(sizeof(UINT32) * 0x800);
		nwk_ram[i] = auto_malloc(sizeof(UINT32) * 0x2000);
	}
	cgboard_type = type;

	if (type == CGBOARD_TYPE_NWKTR)
	{
		nwk_fifo_half_full_r = 0x100;
		nwk_fifo_half_full_w = 0xff;
		nwk_fifo_full = 0x1ff;
		nwk_fifo_mask = 0x1ff;
	}
	if (type == CGBOARD_TYPE_HANGPLT)
	{
		nwk_fifo_half_full_r = 0x3ff;
		nwk_fifo_half_full_w = 0x400;
		nwk_fifo_full = 0x7ff;
		nwk_fifo_mask = 0x7ff;
	}
}

void set_cgboard_id(int board_id)
{
	if (board_id > num_cgboards-1)
	{
		cgboard_id = MAX_CG_BOARDS;
	}
	else
	{
		cgboard_id = board_id;
	}
}

int get_cgboard_id(void)
{
	if (cgboard_id > num_cgboards-1)
	{
		return 0;
	}
	else
	{
		return cgboard_id;
	}
}

void set_cgboard_texture_bank(int board, int bank, UINT8 *rom)
{
	texture_bank[board] = bank;

	memory_configure_bank(bank, 0, 2, rom, 0x800000);
}

/*****************************************************************************/

/* CG Board DSP interface for PowerPC */

READ32_HANDLER( cgboard_dsp_comm_r_ppc )
{
	if (cgboard_id < MAX_CG_BOARDS)
	{
//      mame_printf_debug("dsp_cmd_r: (board %d) %08X, %08X at %08X\n", cgboard_id, offset, mem_mask, activecpu_get_pc());
		return dsp_comm_sharc[cgboard_id][offset] | (dsp_state[cgboard_id] << 16);
	}
	else
	{
		return 0;
	}
}

WRITE32_HANDLER( cgboard_dsp_comm_w_ppc )
{
	int dsp = (cgboard_id == 0) ? 2 : 3;
//  mame_printf_debug("dsp_cmd_w: (board %d) %08X, %08X, %08X at %08X\n", cgboard_id, data, offset, mem_mask, activecpu_get_pc());

	if (cgboard_id < MAX_CG_BOARDS)
	{
		if (offset == 0)
		{
			if (!(mem_mask & 0xff000000))
			{
				dsp_shared_ram_bank[cgboard_id] = (data >> 24) & 0x1;

				if (data & 0x80000000)
				{
					dsp_state[cgboard_id] |= 0x10;
				}

				pci_bridge_enable[cgboard_id] = (data & 0x20000000) ? 1 : 0;

				if (data & 0x10000000)
				{
					cpunum_set_input_line(dsp, INPUT_LINE_RESET, CLEAR_LINE);
				}
				else
				{
					cpunum_set_input_line(dsp, INPUT_LINE_RESET, ASSERT_LINE);
				}

				if (data & 0x02000000)
				{
					cpunum_set_input_line(dsp, INPUT_LINE_IRQ0, ASSERT_LINE);
				}
				if (data & 0x04000000)
				{
					cpunum_set_input_line(dsp, INPUT_LINE_IRQ1, ASSERT_LINE);
				}
			}

			if (!(mem_mask & 0x000000ff))
			{
				dsp_comm_ppc[cgboard_id][offset] = data & 0xff;
			}
		}
		else
		{
			dsp_comm_ppc[cgboard_id][offset] = data;
		}
	}
}



READ32_HANDLER( cgboard_dsp_shared_r_ppc )
{
	if (cgboard_id < MAX_CG_BOARDS)
	{
		return dsp_shared_ram[cgboard_id][offset + (dsp_shared_ram_bank[cgboard_id] * DSP_BANK_SIZE_WORD)];
	}
	else
	{
		return 0;
	}
}

WRITE32_HANDLER( cgboard_dsp_shared_w_ppc )
{
	if (cgboard_id < MAX_CG_BOARDS)
	{
		cpu_trigger(10000);		// Remove the timeout (a part of the GTI Club FIFO test workaround)
		COMBINE_DATA(dsp_shared_ram[cgboard_id] + (offset + (dsp_shared_ram_bank[cgboard_id] * DSP_BANK_SIZE_WORD)));
	}
}

/*****************************************************************************/

/* CG Board DSP interface for SHARC */

static UINT32 dsp_comm_sharc_r(int board, int offset)
{
	return dsp_comm_ppc[board][offset];
}

static void dsp_comm_sharc_w(int board, int offset, UINT32 data)
{
	if (offset >= 2)
	{
		fatalerror("dsp_comm_w: %08X, %08X", data, offset);
	}

	switch (cgboard_type)
	{
		case CGBOARD_TYPE_ZR107:
		case CGBOARD_TYPE_GTICLUB:
		{
			//cpunum_set_input_line(2, SHARC_INPUT_FLAG0, ASSERT_LINE);
			cpuintrf_push_context(2);
			sharc_set_flag_input(0, ASSERT_LINE);
			cpuintrf_pop_context();

			if (offset == 1)
			{
				if (data & 0x03)
				{
					cpunum_set_input_line(2, INPUT_LINE_IRQ2, ASSERT_LINE);
				}
			}
			break;
		}

		case CGBOARD_TYPE_NWKTR:
		case CGBOARD_TYPE_HANGPLT:
		{
			if (offset == 1)
			{
				nwk_device_sel[board] = data;

				if (data & 0x01 || data & 0x10)
				{
					cpuintrf_push_context((board == 0) ? 2 : 3);
					sharc_set_flag_input(1, ASSERT_LINE);
					cpuintrf_pop_context();
				}

				if (texture_bank[board] != -1)
				{
					int offset = (data & 0x08) ? 1 : 0;

					memory_set_bank(texture_bank[board], offset);
				}
			}
			break;
		}

		case CGBOARD_TYPE_HORNET:
		{
			if (offset == 1)
			{
				if (texture_bank[board] != -1)
				{
					int offset = (data & 0x08) ? 1 : 0;

					memory_set_bank(texture_bank[board], offset);
				}
			}
			break;
		}
	}

//  printf("cgboard_dsp_comm_w_sharc: %08X, %08X, %08X at %08X\n", data, offset, mem_mask, activecpu_get_pc());

	dsp_comm_sharc[board][offset] = data;
}

static UINT32 dsp_shared_ram_r_sharc(int board, int offset)
{
//  printf("dsp_shared_r: (board %d) %08X, (%08X, %08X)\n", cgboard_id, offset, (UINT32)dsp_shared_ram[(offset >> 1)], (UINT32)dsp_shared_ram[offset]);

	if (offset & 0x1)
	{
		return (dsp_shared_ram[board][(offset >> 1) + ((dsp_shared_ram_bank[board] ^ 1) * DSP_BANK_SIZE_WORD)] >> 0) & 0xffff;
	}
	else
	{
		return (dsp_shared_ram[board][(offset >> 1) + ((dsp_shared_ram_bank[board] ^ 1) * DSP_BANK_SIZE_WORD)] >> 16) & 0xffff;
	}
}

static void dsp_shared_ram_w_sharc(int board, int offset, UINT32 data)
{
//  printf("dsp_shared_w: (board %d) %08X, %08X\n", cgboard_id, offset, data);
	if (offset & 0x1)
	{
		dsp_shared_ram[board][(offset >> 1) + ((dsp_shared_ram_bank[board] ^ 1) * DSP_BANK_SIZE_WORD)] &= 0xffff0000;
		dsp_shared_ram[board][(offset >> 1) + ((dsp_shared_ram_bank[board] ^ 1) * DSP_BANK_SIZE_WORD)] |= (data & 0xffff);
	}
	else
	{
		dsp_shared_ram[board][(offset >> 1) + ((dsp_shared_ram_bank[board] ^ 1) * DSP_BANK_SIZE_WORD)] &= 0x0000ffff;
		dsp_shared_ram[board][(offset >> 1) + ((dsp_shared_ram_bank[board] ^ 1) * DSP_BANK_SIZE_WORD)] |= ((data & 0xffff) << 16);
	}
}

READ32_HANDLER( cgboard_0_comm_sharc_r )
{
	return dsp_comm_sharc_r(0, offset);
}

WRITE32_HANDLER( cgboard_0_comm_sharc_w )
{
	dsp_comm_sharc_w(0, offset, data);
}

READ32_HANDLER( cgboard_0_shared_sharc_r )
{
	return dsp_shared_ram_r_sharc(0, offset);
}

WRITE32_HANDLER( cgboard_0_shared_sharc_w )
{
	dsp_shared_ram_w_sharc(0, offset, data);
}

READ32_HANDLER( cgboard_1_comm_sharc_r )
{
	return dsp_comm_sharc_r(1, offset);
}

WRITE32_HANDLER( cgboard_1_comm_sharc_w )
{
	dsp_comm_sharc_w(1, offset, data);
}

READ32_HANDLER( cgboard_1_shared_sharc_r )
{
	return dsp_shared_ram_r_sharc(1, offset);
}

WRITE32_HANDLER( cgboard_1_shared_sharc_w )
{
	dsp_shared_ram_w_sharc(1, offset, data);
}

/*****************************************************************************/

UINT32 nwk_fifo_r(int board)
{
	int cpu = (board == 0) ? 2 : 3;
	UINT32 data;

	if (nwk_fifo_read_ptr[board] < nwk_fifo_half_full_r)
	{
		cpuintrf_push_context(cpu);
		sharc_set_flag_input(1, CLEAR_LINE);
		cpuintrf_pop_context();
	}
	else
	{
		cpuintrf_push_context(cpu);
		sharc_set_flag_input(1, ASSERT_LINE);
		cpuintrf_pop_context();
	}

	if (nwk_fifo_read_ptr[board] < nwk_fifo_full)
	{
		cpuintrf_push_context(cpu);
		sharc_set_flag_input(2, ASSERT_LINE);
		cpuintrf_pop_context();
	}
	else
	{
		cpuintrf_push_context(cpu);
		sharc_set_flag_input(2, CLEAR_LINE);
		cpuintrf_pop_context();
	}

	data = nwk_fifo[board][nwk_fifo_read_ptr[board]];
	nwk_fifo_read_ptr[board]++;
	nwk_fifo_read_ptr[board] &= nwk_fifo_mask;

	return data;
}

void nwk_fifo_w(int board, UINT32 data)
{
	int cpu = (board == 0) ? 2 : 3;

	if (nwk_fifo_write_ptr[board] < nwk_fifo_half_full_w)
	{
		cpuintrf_push_context(cpu);
		sharc_set_flag_input(1, ASSERT_LINE);
		cpuintrf_pop_context();
	}
	else
	{
		cpuintrf_push_context(cpu);
		sharc_set_flag_input(1, CLEAR_LINE);
		cpuintrf_pop_context();
	}

	cpuintrf_push_context(cpu);
	sharc_set_flag_input(2, ASSERT_LINE);
	cpuintrf_pop_context();

	nwk_fifo[board][nwk_fifo_write_ptr[board]] = data;
	nwk_fifo_write_ptr[board]++;
	nwk_fifo_write_ptr[board] &= nwk_fifo_mask;
}

/*****************************************************************************/

/* Konami K033906 PCI bridge */

#define MAX_K033906_CHIPS	2

static UINT32 *K033906_reg[MAX_K033906_CHIPS];
static UINT32 *K033906_ram[MAX_K033906_CHIPS];

void K033906_init(void)
{
	int i;
	for (i=0; i < MAX_K033906_CHIPS; i++)
	{
		K033906_reg[i] = auto_malloc(sizeof(UINT32) * 256);
		K033906_ram[i] = auto_malloc(sizeof(UINT32) * 32768);
	}
}

static UINT32 K033906_r(int chip, int reg)
{
	switch(reg)
	{
		case 0x00:		return 0x0001121a;					// PCI Vendor ID (0x121a = 3dfx), Device ID (0x0001 = Voodoo)
		case 0x02:		return 0x04000000;					// Revision ID
		case 0x04:		return K033906_reg[chip][0x04];		// memBaseAddr
		case 0x0f:		return K033906_reg[chip][0x0f];		// interrupt_line, interrupt_pin, min_gnt, max_lat

		default:
			fatalerror("K033906_r: %d, %08X at %08X", chip, reg, activecpu_get_pc());
	}
	return 0;
}

static void K033906_w(int chip, int reg, UINT32 data)
{
	switch(reg)
	{
		case 0x00:
			break;

		case 0x01:		// command register
			break;

		case 0x04:		// memBaseAddr
		{
			if (data == 0xffffffff)
			{
				K033906_reg[chip][0x04] = 0xff000000;
			}
			else
			{
				K033906_reg[chip][0x04] = data & 0xff000000;
			}
			break;
		}

		case 0x0f:		// interrupt_line, interrupt_pin, min_gnt, max_lat
		{
			K033906_reg[chip][0x0f] = data;
			break;
		}

		case 0x10:		// initEnable
		{
			voodoo_set_init_enable(chip, data);
			break;
		}

		case 0x11:		// busSnoop0
		case 0x12:		// busSnoop1
			break;

		case 0x38:		// ???
			break;

		default:
			fatalerror("K033906_w: %d, %08X, %08X at %08X", chip, data, reg, activecpu_get_pc());
	}
}

READ32_HANDLER(K033906_0_r)
{
	if (nwk_device_sel[0] & 0x01)
	{
		return nwk_fifo_r(0);
	}
	else
	{
		if (pci_bridge_enable[0])
		{
			return K033906_r(0, offset);
		}
		else
		{
			return K033906_ram[0][offset];
		}
	}
}

WRITE32_HANDLER(K033906_0_w)
{
	if (pci_bridge_enable[0])
	{
		K033906_w(0, offset, data);
	}
	else
	{
		K033906_ram[0][offset] = data;
	}
}

READ32_HANDLER(K033906_1_r)
{
	if (nwk_device_sel[1] & 0x01)
	{
		return nwk_fifo_r(1);
	}
	else
	{
		if (pci_bridge_enable[1])
		{
			return K033906_r(1, offset);
		}
		else
		{
			return K033906_ram[1][offset];
		}
	}
}

WRITE32_HANDLER(K033906_1_w)
{
	if (pci_bridge_enable[1])
	{
		K033906_w(1, offset, data);
	}
	else
	{
		K033906_ram[1][offset] = data;
	}
}

/*****************************************************************************/

WRITE32_HANDLER(nwk_fifo_0_w)
{
	if (nwk_device_sel[0] & 0x01)
	{
		nwk_fifo_w(0, data);
	}
	else if (nwk_device_sel[0] & 0x02)
	{
		int addr = ((offset >> 8) << 9) | (offset & 0xff);
		nwk_ram[0][addr] = data;
	}
	else
	{
		voodoo_0_w(offset ^ 0x80000, data, mem_mask);
	}
}

WRITE32_HANDLER(nwk_fifo_1_w)
{
	if (nwk_device_sel[1] & 0x01)
	{
		nwk_fifo_w(1, data);
	}
	else if (nwk_device_sel[1] & 0x02)
	{
		int addr = ((offset >> 8) << 9) | (offset & 0xff);
		nwk_ram[1][addr] = data;
	}
	else
	{
		voodoo_1_w(offset ^ 0x80000, data, mem_mask);
	}
}

READ32_HANDLER(nwk_voodoo_0_r)
{
	if ((nwk_device_sel[0] == 0x4) && offset >= 0x100000 && offset < 0x200000)
	{
		return nwk_ram[0][offset & 0x1fff];
	}
	else
	{
		return voodoo_0_r(offset, mem_mask);
	}
}

READ32_HANDLER(nwk_voodoo_1_r)
{
	if ((nwk_device_sel[1] == 0x4) && offset >= 0x100000 && offset < 0x200000)
	{
		return nwk_ram[1][offset & 0x1fff];
	}
	else
	{
		return voodoo_1_r(offset, mem_mask);
	}
}

WRITE32_HANDLER(nwk_voodoo_0_w)
{
	if (nwk_device_sel[0] & 0x01)
	{
		nwk_fifo_w(0, data);
	}
	else if (nwk_device_sel[0] & 0x02)
	{
		int addr = ((offset >> 8) << 9) | (offset & 0xff);
		nwk_ram[0][addr] = data;
	}
	else
	{
		voodoo_0_w(offset, data, mem_mask);
	}
}

WRITE32_HANDLER(nwk_voodoo_1_w)
{
	if (nwk_device_sel[1] & 0x01)
	{
		nwk_fifo_w(1, data);
	}
	else if (nwk_device_sel[1] & 0x02)
	{
		int addr = ((offset >> 8) << 9) | (offset & 0xff);
		nwk_ram[1][addr] = data;
	}
	else
	{
		voodoo_1_w(offset, data, mem_mask);
	}
}

/*****************************************************************************/

#define LED_ON		0xff00ff00

void draw_7segment_led(mame_bitmap *bitmap, int x, int y, UINT8 value)
{
	if ((value & 0x7f) == 0x7f)
	{
		return;
	}

	plot_box(bitmap, x-1, y-1, 7, 11, 0x00000000);

	/* Top */
	if( (value & 0x40) == 0 ) {
		plot_box(bitmap, x+1, y+0, 3, 1, LED_ON);
	}
	/* Middle */
	if( (value & 0x01) == 0 ) {
		plot_box(bitmap, x+1, y+4, 3, 1, LED_ON);
	}
	/* Bottom */
	if( (value & 0x08) == 0 ) {
		plot_box(bitmap, x+1, y+8, 3, 1, LED_ON);
	}
	/* Top Left */
	if( (value & 0x02) == 0 ) {
		plot_box(bitmap, x+0, y+1, 1, 3, LED_ON);
	}
	/* Top Right */
	if( (value & 0x20) == 0 ) {
		plot_box(bitmap, x+4, y+1, 1, 3, LED_ON);
	}
	/* Bottom Left */
	if( (value & 0x04) == 0 ) {
		plot_box(bitmap, x+0, y+5, 1, 3, LED_ON);
	}
	/* Bottom Right */
	if( (value & 0x10) == 0 ) {
		plot_box(bitmap, x+4, y+5, 1, 3, LED_ON);
	}
}

/***************************************************************************

    mips3com.h

    Common MIPS III/IV definitions and functions

***************************************************************************/

#ifndef __MIPS3COM_H__
#define __MIPS3COM_H__

#include "cpuintrf.h"
#include "mips3.h"


/***************************************************************************
    CONSTANTS
***************************************************************************/

/* core parameters */
#define MIPS3_MIN_PAGE_SHIFT	12
#define MIPS3_MIN_PAGE_SIZE		(1 << MIPS3_MIN_PAGE_SHIFT)
#define MIPS3_MIN_PAGE_MASK		(MIPS3_MIN_PAGE_SIZE - 1)
#define MIPS3_MAX_PADDR_SHIFT	32

/* MIPS flavors */
enum _mips3_flavor
{
	/* MIPS III variants */
	MIPS3_TYPE_MIPS_III,
	MIPS3_TYPE_R4600,
	MIPS3_TYPE_R4650,
	MIPS3_TYPE_R4700,

	/* MIPS IV variants */
	MIPS3_TYPE_MIPS_IV,
	MIPS3_TYPE_R5000,
	MIPS3_TYPE_QED5271,
	MIPS3_TYPE_RM7000
};
typedef enum _mips3_flavor mips3_flavor;

/* COP0 registers */
#define COP0_Index			0
#define COP0_Random			1
#define COP0_EntryLo		2
#define COP0_EntryLo0		2
#define COP0_EntryLo1		3
#define COP0_Context		4
#define COP0_PageMask		5
#define COP0_Wired			6
#define COP0_BadVAddr		8
#define COP0_Count			9
#define COP0_EntryHi		10
#define COP0_Compare		11
#define COP0_Status			12
#define COP0_Cause			13
#define COP0_EPC			14
#define COP0_PRId			15
#define COP0_Config			16
#define COP0_XContext		20

/* Status register bits */
#define SR_IE				0x00000001
#define SR_EXL				0x00000002
#define SR_ERL				0x00000004
#define SR_KSU_MASK			0x00000018
#define SR_KSU_KERNEL		0x00000000
#define SR_KSU_SUPERVISOR	0x00000008
#define SR_KSU_USER			0x00000010
#define SR_IMSW0			0x00000100
#define SR_IMSW1			0x00000200
#define SR_IMEX0			0x00000400
#define SR_IMEX1			0x00000800
#define SR_IMEX2			0x00001000
#define SR_IMEX3			0x00002000
#define SR_IMEX4			0x00004000
#define SR_IMEX5			0x00008000
#define SR_DE				0x00010000
#define SR_CE				0x00020000
#define SR_CH				0x00040000
#define SR_SR				0x00100000
#define SR_TS				0x00200000
#define SR_BEV				0x00400000
#define SR_RE				0x02000000
#define SR_FR				0x04000000
#define SR_RP				0x08000000
#define SR_COP0				0x10000000
#define SR_COP1				0x20000000
#define SR_COP2				0x40000000
#define SR_COP3				0x80000000

/* exception types */
#define EXCEPTION_INTERRUPT	0
#define EXCEPTION_TLBMOD	1
#define EXCEPTION_TLBLOAD	2
#define EXCEPTION_TLBSTORE	3
#define EXCEPTION_ADDRLOAD	4
#define EXCEPTION_ADDRSTORE	5
#define EXCEPTION_BUSINST	6
#define EXCEPTION_BUSDATA	7
#define EXCEPTION_SYSCALL	8
#define EXCEPTION_BREAK		9
#define EXCEPTION_INVALIDOP	10
#define EXCEPTION_BADCOP	11
#define EXCEPTION_OVERFLOW	12
#define EXCEPTION_TRAP		13



/***************************************************************************
    HELPER MACROS
***************************************************************************/

#define RSREG			((op >> 21) & 31)
#define RTREG			((op >> 16) & 31)
#define RDREG			((op >> 11) & 31)
#define SHIFT			((op >> 6) & 31)

#define FRREG			((op >> 21) & 31)
#define FTREG			((op >> 16) & 31)
#define FSREG			((op >> 11) & 31)
#define FDREG			((op >> 6) & 31)

#define IS_SINGLE(o) 	(((o) & (1 << 21)) == 0)
#define IS_DOUBLE(o) 	(((o) & (1 << 21)) != 0)
#define IS_FLOAT(o) 	(((o) & (1 << 23)) == 0)
#define IS_INTEGRAL(o) 	(((o) & (1 << 23)) != 0)

#define SIMMVAL			((INT16)op)
#define UIMMVAL			((UINT16)op)
#define LIMMVAL			(op & 0x03ffffff)



/***************************************************************************
    STRUCTURES & TYPEDEFS
***************************************************************************/

/* memory access function table */
typedef struct _memory_handlers memory_handlers;
struct _memory_handlers
{
	UINT8			(*readbyte)(offs_t);
	UINT16			(*readword)(offs_t);
	UINT32			(*readlong)(offs_t);
	UINT32			(*readlong_masked)(offs_t, UINT32);
	UINT64			(*readdouble)(offs_t);
	UINT64			(*readdouble_masked)(offs_t, UINT64);
	void			(*writebyte)(offs_t, UINT8);
	void			(*writeword)(offs_t, UINT16);
	void			(*writelong)(offs_t, UINT32);
	void			(*writelong_masked)(offs_t, UINT32, UINT32);
	void			(*writedouble)(offs_t, UINT64);
	void			(*writedouble_masked)(offs_t, UINT64, UINT64);
};


/* MIPS3 TLB entry */
typedef struct _mips3_tlb_entry mips3_tlb_entry;
struct _mips3_tlb_entry
{
	UINT64			page_mask;
	UINT64			entry_hi;
	UINT64			entry_lo[2];
};


/* MIPS3 state */
typedef struct _mips3_state mips3_state;
struct _mips3_state
{
	/* core registers */
	UINT32			pc;
	UINT64			hi;
	UINT64			lo;
	UINT64			r[32];
	int				icount;

	/* COP registers */
	UINT64			cpr[3][32];
	UINT64			ccr[3][32];
	UINT8			cf[3][8];

	/* internal stuff */
	mips3_flavor	flavor;
	int 			(*irq_callback)(int irqline);
	UINT32			system_clock;
	UINT32			cpu_clock;
	UINT64			count_zero_time;
	mame_timer *	compare_int_timer;

	/* memory accesses */
	UINT8			bigendian;
	memory_handlers memory;

	/* cache memory */
	UINT32 *		icache;
	UINT32 *		dcache;
	size_t			icache_size;
	size_t			dcache_size;

	/* MMU */
	mips3_tlb_entry tlb[48];
	UINT32 *		tlb_table;
};



/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

void mips3com_init(mips3_state *mips, mips3_flavor flavor, int bigendian, int index, int clock, const struct mips3_config *config, int (*irqcallback)(int));
void mips3com_reset(mips3_state *mips);
#ifdef MAME_DEBUG
offs_t mips3com_dasm(mips3_state *mips, char *buffer, offs_t pc, const UINT8 *oprom, const UINT8 *opram);
#endif
void mips3com_update_cycle_counting(mips3_state *mips);

void mips3com_map_tlb_entries(mips3_state *mips);
void mips3com_unmap_tlb_entries(mips3_state *mips);
void mips3com_recompute_tlb_table(mips3_state *mips);
int mips3com_translate_address(mips3_state *mips, int space, offs_t *address);
void mips3com_tlbr(mips3_state *mips);
void mips3com_tlbwi(mips3_state *mips);
void mips3com_tlbwr(mips3_state *mips);
void mips3com_tlbp(mips3_state *mips);

void mips3com_set_info(mips3_state *mips, UINT32 state, cpuinfo *info);
void mips3com_get_info(mips3_state *mips, UINT32 state, cpuinfo *info);



/***************************************************************************
    INLINE FUNCTIONS
***************************************************************************/

/*-------------------------------------------------
    mips3com_set_irq_line - set or clear the given
    IRQ line
-------------------------------------------------*/

INLINE void mips3com_set_irq_line(mips3_state *mips, int irqline, int state)
{
	if (state != CLEAR_LINE)
		mips->cpr[0][COP0_Cause] |= 0x400 << irqline;
	else
		mips->cpr[0][COP0_Cause] &= ~(0x400 << irqline);
}

#endif

/*****************************************************************************
 *
 *   opsce02.h
 *   Addressing mode and opcode macros for 65ce02 CPU
 *
 *   Copyright (c) 2000 Peter Trauner, all rights reserved.
 *   documentation preliminary databook
 *   documentation by michael steil mist@c64.org
 *   available at ftp://ftp.funet.fi/pub/cbm/c65
 *
 *   - This source code is released as freeware for non-commercial purposes.
 *   - You are free to use and redistribute this code in modified or
 *     unmodified form, provided you list me in the credits.
 *   - If you modify this source code, you must add a notice to each modified
 *     source file that it has been changed.  If you're a nice person, you
 *     will clearly mark each change too.  :)
 *   - If you wish to use this for commercial purposes, please contact me at
 *     pullmoll@t-online.de
 *   - The author of this copywritten work reserves the right to change the
 *     terms of its usage and license at any time, including retroactively
 *   - This entire notice must remain in the source code.
 *
 *****************************************************************************/

/* 65ce02 flags */
#define F_C 0x01
#define F_Z 0x02
#define F_I 0x04
#define F_D 0x08
#define F_B 0x10
#define F_E 0x20
#define F_V 0x40
#define F_N 0x80

/* some shortcuts for improved readability */
#define A	m65ce02.a
#define X	m65ce02.x
#define Y	m65ce02.y
#define P	m65ce02.p
#define Z	m65ce02.z
#define B	m65ce02.zp.b.h
#define SW	m65ce02.sp.w.l
#define SPL	m65ce02.sp.b.l
#define SPH	m65ce02.sp.b.h
#define SPD	m65ce02.sp.d

#define NZ	m65ce02.nz

#define EAL	m65ce02.ea.b.l
#define EAH	m65ce02.ea.b.h
#define EAW	m65ce02.ea.w.l
#define EAD	m65ce02.ea.d

#define ZPL	m65ce02.zp.b.l
#define ZPH	m65ce02.zp.b.h
#define ZPW	m65ce02.zp.w.l
#define ZPD	m65ce02.zp.d

#define PCL	m65ce02.pc.b.l
#define PCH	m65ce02.pc.b.h
#define PCW	m65ce02.pc.w.l
#define PCD	m65ce02.pc.d

#define PPC	m65ce02.ppc.d

#define RDMEM_ID	m65ce02.rdmem_id
#define WRMEM_ID	m65ce02.wrmem_id

#define SET_NZ(n)	if ((n) == 0) P = (P & ~F_N) | F_Z; else P = (P & ~(F_N | F_Z)) | ((n) & F_N)
#define SET_Z(n)	if ((n) == 0) P |= F_Z; else P &= ~F_Z

#define CHANGE_PC	change_pc(PCD)

#define PEEK_OP() cpu_readop(PCW)

#define RDMEM(addr)		program_read_byte_8(addr); m65ce02_ICount -= 1
#define WRMEM(addr,data)	program_write_byte_8(addr,data); m65ce02_ICount -= 1

#define RDMEM_WORD(addr, pair)	pair.b.l=RDMEM( addr ); pair.b.h=RDMEM( (addr+1) & 0xffff )
#define WRMEM_WORD(addr,pair)	WRMEM(addr,pair.b.l); WRMEM((addr+1)&0xffff,pair.b.h)

#define SET_NZ_WORD(n)											\
	if( n.w.l == 0 )											\
		P = (P & ~F_N) | F_Z;									\
	else														\
		P = (P & ~(F_N | F_Z)) | ((n.b.h) & F_N)

/***************************************************************
 *  EA = absolute address
 ***************************************************************/
#define EA_ABS										\
	EAL = RDOPARG();								\
	EAH = RDOPARG()

/***************************************************************
 *  EA = absolute address + X
 ***************************************************************/
#define EA_ABX										\
	EA_ABS;										\
	EAW += X

/***************************************************************
 *  EA = absolute address + Y
 * one additional read if page boundary is crossed
 ***************************************************************/
#define EA_ABY										\
	EA_ABS;										\
	EAW += Y;

/***************************************************************
 *  EA = zero page address
 ***************************************************************/
#define EA_ZPG										\
	ZPL = RDOPARG();								\
	EAD = ZPD

/***************************************************************
 *  EA = zero page address + X
 ***************************************************************/
#define EA_ZPX										\
	ZPL = RDOPARG();								\
	ZPL = X + ZPL;									\
	EAD = ZPD

/***************************************************************
 *  EA = zero page address + Y
 ***************************************************************/
#define EA_ZPY										\
	ZPL = RDOPARG();								\
	ZPL = Y + ZPL;									\
	EAD = ZPD

/***************************************************************
 *  EA = zero page + X indirect (pre indexed)
 ***************************************************************/
#define EA_IDX										\
	ZPL = RDOPARG();								\
	ZPL = ZPL + X;									\
	EAL = RDMEM(ZPD);								\
	ZPL++;										\
	EAH = RDMEM(ZPD)

/***************************************************************
 *  EA = zero page indirect + Y (post indexed)
 ***************************************************************/
#define EA_IDY										\
	ZPL = RDOPARG();								\
	EAL = RDMEM(ZPD);								\
	ZPL++;										\
	EAH = RDMEM(ZPD);								\
	EAW += Y

/***************************************************************
 *  EA = zero page indirect + Z (post indexed)
 *  ???? subtract 1 cycle if page boundary is crossed
 ***************************************************************/
#define EA_IDZ										\
	ZPL = RDOPARG();								\
	EAL = RDMEM(ZPD);								\
	ZPL++;										\
	EAH = RDMEM(ZPD);								\
	EAW += Z

/***************************************************************
 *  EA = zero page indexed stack, indirect + Y (post indexed)
 ***************************************************************/
#define EA_INSY										\
	{										\
		PAIR pair={{0}};							\
		pair.b.l = SPL+RDOPARG();						\
		pair.b.h = SPH;								\
		EAL = RDMEM(pair.d);							\
		if( P & F_E )								\
			pair.b.l++;							\
		else									\
			pair.w.l++;							\
		EAH = RDMEM(pair.d);							\
		RDMEM(PCW-1);								\
		EAW += Y;								\
	}

/***************************************************************
 *  EA = indirect (only used by JMP)
 ***************************************************************/
#define EA_IND										\
	EA_ABS;										\
	tmp = RDMEM(EAD);								\
	EAD++;										\
	EAH = RDMEM(EAD);								\
	EAL = tmp

/* 65ce02 ******************************************************
 *  EA = indirect plus x (only used by 65c02 JMP)
 ***************************************************************/
#define EA_IAX										\
	EA_ABS;										\
	EAW += X;									\
	tmp = RDMEM(EAD);								\
	EAD++;										\
	EAH = RDMEM(EAD);								\
	EAL = tmp

/* read a value into tmp */
/* Base number of cycles taken for each mode (including reading of opcode):
   RD_ACC/WR_ACC        0
   RD_IMM           2
   RD_DUM           2
   RD_ABS/WR_ABS        4
   RD_ABS_WORD/WR_ABS_WORD  5
   RD_ABX/WR_ABX        4
   RD_ABY/WR_ABY        4
   RD_IDX/WR_IDX        5
   RD_IDY/WR_IDY        5
   RD_IDZ           5
   RD_INSY/WR_INSY      6
   RD_ZPG/WR_ZPG        3
   RD_ZPG_WORD          4
   RD_ZPX/WR_ZPX        3
   RD_ZPY/WR_ZPY        3
 */
#define RD_ACC		tmp = A
#define RD_IMM		tmp = RDOPARG()
#define RD_IMM_WORD	tmp.b.l = RDOPARG(); tmp.b.h=RDOPARG()
#define RD_DUM		RDMEM(PCW-1)
#define RD_ABS		EA_ABS; tmp = RDMEM(EAD)
#define RD_ABS_WORD	EA_ABS; RDMEM_WORD(EAD, tmp)
#define RD_ABX		EA_ABX; tmp = RDMEM(EAD)
#define RD_ABY		EA_ABY; tmp = RDMEM(EAD)
#define RD_IDX		EA_IDX; tmp = RDMEM(EAD)
#define RD_IDY		EA_IDY; tmp = RDMEM(EAD)
#define RD_IDZ		EA_IDZ; tmp = RDMEM(EAD)
#define RD_INSY		EA_INSY; tmp = RDMEM(EAD)
#define RD_ZPG		EA_ZPG; tmp = RDMEM(EAD)
#define RD_ZPG_WORD	EA_ZPG; RDMEM_WORD(EAD, tmp)
#define RD_ZPX		EA_ZPX; tmp = RDMEM(EAD)
#define RD_ZPY		EA_ZPY; tmp = RDMEM(EAD)

/* write a value from tmp */
#define WR_ACC		A = (UINT8)tmp
#define WR_ABS		EA_ABS; WRMEM(EAD, tmp)
#define WR_ABS_WORD	EA_ABS; WRMEM_WORD(EAD, tmp)
#define WR_ABX		EA_ABX; WRMEM(EAD, tmp)
#define WR_ABY		EA_ABY; WRMEM(EAD, tmp)
#define WR_IDX		EA_IDX; WRMEM(EAD, tmp)
#define WR_IDY		EA_IDY; WRMEM(EAD, tmp)
#define WR_INSY		EA_INSY; WRMEM(EAD, tmp)
#define WR_ZPG		EA_ZPG; WRMEM(EAD, tmp)
#define WR_ZPX		EA_ZPX; WRMEM(EAD, tmp)
#define WR_ZPY		EA_ZPY; WRMEM(EAD, tmp)

#define WB_EA		WRMEM(EAD, tmp)
#define WB_EA_WORD	WRMEM_WORD(EAD, tmp)

/***************************************************************
 * push a register onto the stack
 ***************************************************************/
#define PUSH(Rg) WRMEM(SPD, Rg); if (P&F_E) { SPL--; } else { SW--; }

/***************************************************************
 * pull a register from the stack
 ***************************************************************/
#define PULL(Rg) if (P&F_E) { SPL++; } else { SW++; } Rg = RDMEM(SPD)

/* the order in which the args are pushed is correct! */
#define PUSH_WORD(pair) PUSH(pair.b.l);PUSH(pair.b.h)

/* 65ce02 *******************************************************
 *  ADC Add with carry
 * correct setting of flags in decimal mode
 ***************************************************************/
#define ADC										\
	if (P & F_D) {									\
		int c = (P & F_C);							\
		int lo = (A & 0x0f) + (tmp & 0x0f) + c;					\
		int hi = (A & 0xf0) + (tmp & 0xf0);					\
		P &= ~(F_V | F_C);							\
		if( lo > 0x09 ) {							\
			hi += 0x10;							\
			lo += 0x06;							\
		}									\
		if( ~(A^tmp) & (A^hi) & F_N )						\
			P |= F_V;							\
		if( hi > 0x90 )								\
			hi += 0x60;							\
		if( hi & 0xff00 )							\
			P |= F_C;							\
		A = (lo & 0x0f) + (hi & 0xf0);						\
	} else {									\
		int c = (P & F_C);							\
		int sum = A + tmp + c;							\
		P &= ~(F_V | F_C);							\
		if( ~(A^tmp) & (A^sum) & F_N )						\
			P |= F_V;							\
		if( sum & 0xff00 )							\
			P |= F_C;							\
		A = (UINT8) sum;							\
	}										\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  AND Logical and
 ***************************************************************/
#define AND										\
	A = (UINT8)(A & tmp);								\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  ASL Arithmetic shift left
 ***************************************************************/
#define ASL										\
	P = (P & ~F_C) | ((tmp >> 7) & F_C);						\
	tmp = (UINT8)(tmp << 1);							\
	SET_NZ(tmp)

/* 65ce02 ******************************************************
 *  ASR arithmetic (signed) shift right
 *  [7] -> [7][6][5][4][3][2][1][0] -> C
 ***************************************************************/
#define ASR_65CE02									\
	P = (P & ~F_C) | (tmp & F_C);							\
	tmp = (signed char)tmp >> 1;							\
	SET_NZ(tmp)

/* 65ce02 ******************************************************
 *  ASW arithmetic shift left word
 *  [c] <- [15]..[6][5][4][3][2][1][0]
 ***************************************************************/
/* not sure about how 16 bit memory modifying is executed */
#define ASW										\
	tmp.w.l = tmp.w.l << 1;								\
	P = (P & ~F_C) | (tmp.b.h2 & F_C);						\
	SET_NZ_WORD(tmp);


/* 65ce02 ******************************************************
 *  augment
 ***************************************************************/
#define AUG										\
	t1=RDOPARG();									\
	t2=RDOPARG();									\
	t3=RDOPARG();									\
	logerror("m65ce02 at pc:%.4x reserved op aug %.2x %.2x %.2x\n", t1,t2,t3);

/* 65ce02 ******************************************************
 *  BBR Branch if bit is reset
 ***************************************************************/
#define BBR(bit)									\
	BRA(!(tmp & (1<<bit)))

/* 65ce02 ******************************************************
 *  BBS Branch if bit is set
 ***************************************************************/
#define BBS(bit)									\
	BRA(tmp & (1<<bit))

/* 65ce02 ******************************************************
 *  BIT Bit test
 ***************************************************************/
#undef BIT
#define BIT										\
	P &= ~(F_N|F_V|F_Z);								\
	P |= tmp & (F_N|F_V);								\
	if ((tmp & A) == 0)								\
		P |= F_Z

/***************************************************************
 *  BRA  branch relative
 ***************************************************************/
#define BRA(cond)									\
	tmp = RDOPARG();								\
	if (cond)									\
	{										\
		EAW = PCW + (signed char)tmp;						\
		PCD = EAD;								\
		CHANGE_PC;								\
	}

/***************************************************************
 *  BRA  branch relative
 ***************************************************************/
#define BRA_WORD(cond)									\
	EAL = RDOPARG();								\
	EAH = RDOPARG();								\
	if (cond)									\
	{										\
		EAW = PCW + (short)(EAW-1); 						\
		PCD = EAD;								\
		CHANGE_PC;								\
	}

/* 65ce02 ******************************************************
 *  BRK Break
 *  increment PC, push PC hi, PC lo, flags (with B bit set),
 *  set I flag, jump via IRQ vector
 ***************************************************************/
#define BRK										\
	RDOPARG();									\
	PUSH(PCH);									\
	PUSH(PCL);									\
	PUSH(P | F_B);									\
	P = (P | F_I);									\
	PCL = RDMEM(M6502_IRQ_VEC);							\
	PCH = RDMEM(M6502_IRQ_VEC+1);							\
	CHANGE_PC

/* 65ce02 ********************************************************
 *  BSR Branch to subroutine
 ***************************************************************/
#define BSR										\
	EAL = RDOPARG();								\
	PUSH(PCH);									\
	PUSH(PCL);									\
	EAH = RDOPARG();								\
	EAW = PCW + (INT16)(EAW-1);							\
	PCD = EAD;									\
	CHANGE_PC

/* 65ce02 ******************************************************
 * CLC  Clear carry flag
 ***************************************************************/
#define CLC										\
	P &= ~F_C

/* 65ce02 ******************************************************
 * CLD  Clear decimal flag
 ***************************************************************/
#define CLD										\
	P &= ~F_D

/* 65ce02 ******************************************************
 *  cle clear disable extended stack flag
 ***************************************************************/
#define CLE										\
	P|=F_E

/* 65ce02 ******************************************************
 * CLI  Clear interrupt flag
 ***************************************************************/
#define CLI										\
	if ((m65ce02.irq_state != CLEAR_LINE) && (P & F_I)) {				\
		m65ce02.after_cli = 1;							\
	}										\
	P &= ~F_I

/* 65ce02 ******************************************************
 * CLV  Clear overflow flag
 ***************************************************************/
#define CLV										\
	P &= ~F_V

/* 65ce02 ******************************************************
 *  CMP Compare accumulator
 ***************************************************************/
#define CMP										\
	P &= ~F_C;									\
	if (A >= tmp)									\
		P |= F_C;								\
	SET_NZ((UINT8)(A - tmp))

/* 65ce02 ******************************************************
 *  CPX Compare index X
 ***************************************************************/
#define CPX										\
	P &= ~F_C;									\
	if (X >= tmp)									\
		P |= F_C;								\
	SET_NZ((UINT8)(X - tmp))

/* 65ce02 ******************************************************
 *  CPY Compare index Y
 ***************************************************************/
#define CPY										\
	P &= ~F_C;									\
	if (Y >= tmp)									\
		P |= F_C;								\
	SET_NZ((UINT8)(Y - tmp))

/* 65ce02 ******************************************************
 *  CPZ Compare index Z
 ***************************************************************/
#define CPZ										\
	P &= ~F_C;									\
	if (Z >= tmp)									\
		P |= F_C;								\
	SET_NZ((UINT8)(Z - tmp))

/* 65ce02 ******************************************************
 *  DEA Decrement accumulator
 ***************************************************************/
#define DEA										\
	A = (UINT8)--A;									\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  DEC Decrement memory
 ***************************************************************/
#define DEC										\
	tmp = (UINT8)(tmp-1);								\
	SET_NZ(tmp)

/* 65ce02 ******************************************************
 *  DEW Decrement memory word
 ***************************************************************/
#define DEW										\
	tmp.w.l = --tmp.w.l;								\
	SET_NZ_WORD(tmp)

/* 65ce02 ******************************************************
 *  DEX Decrement index X
 ***************************************************************/
#define DEX										\
	X = (UINT8)(X-1);								\
	SET_NZ(X)

/* 65ce02 ******************************************************
 *  DEY Decrement index Y
 ***************************************************************/
#define DEY										\
	Y = (UINT8)(Y-1);								\
	SET_NZ(Y)

/* 65ce02 ******************************************************
 *  DEZ Decrement index Z
 ***************************************************************/
#define DEZ										\
	Z = (UINT8)(Z-1);								\
	SET_NZ(Z)

/* 65ce02 ******************************************************
 *  EOR Logical exclusive or
 ***************************************************************/
#define EOR										\
	A = (UINT8)(A ^ tmp);								\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  INA Increment accumulator
 ***************************************************************/
#define INA										\
	A = (UINT8)++A;									\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  INC Increment memory
 ***************************************************************/
#define INC										\
	tmp = (UINT8)(tmp+1);								\
	SET_NZ(tmp)

/* 65ce02 ******************************************************
 *  INW Increment memory word
 ***************************************************************/
#define INW										\
	tmp.w.l = ++tmp.w.l;								\
	SET_NZ_WORD(tmp)

/* 65ce02 ******************************************************
 *  INX Increment index X
 ***************************************************************/
#define INX										\
	X = (UINT8)(X+1);								\
	SET_NZ(X)

/* 65ce02 ******************************************************
 *  INY Increment index Y
 ***************************************************************/
#define INY										\
	Y = (UINT8)(Y+1);								\
	SET_NZ(Y)

/* 65ce02 ******************************************************
 *  INZ Increment index Z
 ***************************************************************/
#define INZ										\
	Z = (UINT8)(Z+1);								\
	SET_NZ(Z)

/* 65ce02 ******************************************************
 *  JMP Jump to address
 *  set PC to the effective address
 ***************************************************************/
#define JMP										\
	PCD = EAD;									\
	CHANGE_PC

/* 65ce02 ******************************************************
 *  JSR Jump to subroutine
 *  decrement PC (sic!) push PC hi, push PC lo and set
 *  PC to the effective address
 ***************************************************************/
#define JSR										\
	EAL = RDOPARG();								\
	PUSH(PCH);									\
	PUSH(PCL);									\
	EAH = RDOPARG();								\
	PCD = EAD;									\
	CHANGE_PC

/* 65ce02 ******************************************************
 *  JSR Jump to subroutine
 *  decrement PC (sic!) push PC hi, push PC lo and set
 *  PC to the effective address
 ***************************************************************/
#define JSR_IND										\
	EAL = RDOPARG();								\
	PUSH(PCH);									\
	PUSH(PCL);									\
	EAH = RDOPARG();								\
	PCL = RDMEM(EAD);								\
	PCH = RDMEM(EAD);								\
	CHANGE_PC

/* 65ce02 ******************************************************
 *  JSR Jump to subroutine
 *  decrement PC (sic!) push PC hi, push PC lo and set
 *  PC to the effective address
 ***************************************************************/
#define JSR_INDX									\
	EAL = RDOPARG()+X;								\
	PUSH(PCH);									\
	PUSH(PCL);									\
	EAH = RDOPARG();								\
	PCL = RDMEM(EAD);								\
	PCH = RDMEM(EAD);								\
	CHANGE_PC

/* 65ce02 ******************************************************
 *  LDA Load accumulator
 ***************************************************************/
#define LDA										\
	A = (UINT8)tmp;									\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  LDX Load index X
 ***************************************************************/
#define LDX										\
	X = (UINT8)tmp;									\
	SET_NZ(X)

/* 65ce02 ******************************************************
 *  LDY Load index Y
 ***************************************************************/
#define LDY										\
	Y = (UINT8)tmp;									\
	SET_NZ(Y)

/* 65ce02 ******************************************************
 *  LDZ Load index Z
 ***************************************************************/
#define LDZ										\
	Z = (UINT8)tmp;									\
	SET_NZ(Z)

/* 65ce02 ******************************************************
 *  LSR Logic shift right
 *  0 -> [7][6][5][4][3][2][1][0] -> C
 ***************************************************************/
#define LSR										\
	P = (P & ~F_C) | (tmp & F_C);							\
	tmp = (UINT8)tmp >> 1;								\
	SET_NZ(tmp)

/* 65ce02 ******************************************************
 *  NEG accu
 * twos complement
 ***************************************************************/
#define NEG										\
	A= (A^0xff)+1;									\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  NOP No operation
 ***************************************************************/
#define NOP

/* 65ce02 ******************************************************
 *  ORA Logical inclusive or
 ***************************************************************/
#define ORA										\
	A = (UINT8)(A | tmp);								\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  PHA Push accumulator
 ***************************************************************/
#define PHA										\
	PUSH(A)

/* 65ce02 ******************************************************
 *  PHP Push processor status (flags)
 ***************************************************************/
#define PHP										\
	PUSH(P)

/* 65ce02 ******************************************************
 *  PHX Push index X
 ***************************************************************/
#define PHX										\
	PUSH(X)

/* 65ce02 ******************************************************
 *  PHY Push index Y
 ***************************************************************/
#define PHY										\
	PUSH(Y)

/* 65ce02 ******************************************************
 *  PHZ Push index z
 ***************************************************************/
#define PHZ										\
	PUSH(Z)

/* 65ce02 ******************************************************
 *  PLA Pull accumulator
 ***************************************************************/
#define PLA										\
	PULL(A);									\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  PLP Pull processor status (flags)
 ***************************************************************/
#define PLP										\
	if ( P & F_I ) {								\
		UINT8 temp;								\
		PULL(temp);								\
		P=(P&F_E)|F_B|(temp&~F_E);						\
		if( m65ce02.irq_state != CLEAR_LINE && !(P & F_I) ) {			\
			LOG(("M65ce02#%d PLP sets after_cli\n", cpu_getactivecpu()));	\
			m65ce02.after_cli = 1;						\
		}									\
	} else {									\
		UINT8 temp;								\
		PULL(temp);								\
		P=(P&F_E)|F_B|(temp&~F_E);						\
	}

/* 65ce02 ******************************************************
 *  PLX Pull index X
 ***************************************************************/
#define PLX										\
	PULL(X);									\
	SET_NZ(X)

/* 65ce02 ******************************************************
 *  PLY Pull index Y
 ***************************************************************/
#define PLY										\
	PULL(Y);									\
	SET_NZ(Y)

/* 65ce02 ******************************************************
 *  PLZ Pull index Z
 ***************************************************************/
#define PLZ										\
	PULL(Z);									\
	SET_NZ(Z)

/* 65ce02 ******************************************************
 *  RMB Reset memory bit
 ***************************************************************/
#define RMB(bit)									\
	tmp &= ~(1<<bit)

/* 65ce02 ******************************************************
 * ROL  Rotate left
 *  new C <- [7][6][5][4][3][2][1][0] <- C
 ***************************************************************/
#define ROL										\
	tmp = (tmp << 1) | (P & F_C);							\
	P = (P & ~F_C) | ((tmp >> 8) & F_C);						\
	tmp = (UINT8)tmp;								\
	SET_NZ(tmp)

/* 65ce02 ******************************************************
 * ROR  Rotate right
 *  C -> [7][6][5][4][3][2][1][0] -> new C
 ***************************************************************/
#define ROR										\
	tmp |= (P & F_C) << 8;								\
	P = (P & ~F_C) | (tmp & F_C);							\
	tmp = (UINT8)(tmp >> 1);							\
	SET_NZ(tmp)

/* 65ce02 ******************************************************
 *  ROW rotate left word
 *  [c] <- [15]..[6][5][4][3][2][1][0] <- C
 ***************************************************************/
/* not sure about how 16 bit memory modifying is executed */
#define ROW										\
	tmp.d =(tmp.d << 1);								\
	tmp.w.l |= (P & F_C);								\
	P = (P & ~F_C) | (tmp.w.l & F_C);						\
	SET_NZ_WORD(tmp);


/* 65ce02 ********************************************************
 * RTI  Return from interrupt
 * pull flags, pull PC lo, pull PC hi
 ***************************************************************/
#define RTI										\
	RDMEM(SPD);									\
	PULL(tmp);									\
	P = (P&F_E)|F_B|(tmp&~F_E);							\
	PULL(PCL);									\
	PULL(PCH);									\
	if( m65ce02.irq_state != CLEAR_LINE && !(P & F_I) )				\
	{										\
		LOG(("M65ce02#%d RTI sets after_cli\n", cpu_getactivecpu()));		\
		m65ce02.after_cli = 1;							\
	}										\
	CHANGE_PC

/* 65ce02 ******************************************************
 *  RTS Return from subroutine
 *  pull PC lo, PC hi and increment PC
 ***************************************************************/
#define RTS										\
	PULL(PCL);									\
	PULL(PCH);									\
	RDMEM(PCW); PCW++;								\
	CHANGE_PC

/* 65ce02 ******************************************************
 *  RTS imm
 * Is the stack adjustment done before or after the return actions?
 * Exact order of the read instructions unknown
 ***************************************************************/
#define RTN										\
	if (P&F_E) {									\
		SPL+=tmp;								\
	} else {									\
		SW+=tmp;								\
	}										\
	RDMEM(PCW-1);									\
	RDMEM(PCW-1);									\
	RDMEM(SPD);									\
	P = (P&F_E)|F_B|(tmp&~F_E);							\
	PULL(PCL);									\
	PULL(PCH);									\
	if( m65ce02.irq_state != CLEAR_LINE && !(P & F_I) ) {				\
	LOG(("M65ce02#%d RTI sets after_cli\n", cpu_getactivecpu()));			\
		m65ce02.after_cli = 1;							\
	}										\
	CHANGE_PC


/* 65ce02 *******************************************************
 *  SBC Subtract with carry
 * correct setting of flags in decimal mode
 ***************************************************************/
#define SBC										\
	if (P & F_D) {									\
		int c = (P & F_C) ^ F_C;						\
		int sum = A - tmp - c;							\
		int lo = (A & 0x0f) - (tmp & 0x0f) - c;					\
		int hi = (A & 0xf0) - (tmp & 0xf0);					\
		P &= ~(F_V | F_C);							\
		if( (A^tmp) & (A^sum) & F_N )						\
			P |= F_V;							\
		if( lo & 0xf0 )								\
			lo -= 6;							\
		if( lo & 0x80 )								\
			hi -= 0x10;							\
		if( hi & 0x0f00 )							\
			hi -= 0x60;							\
		if( (sum & 0xff00) == 0 )						\
			P |= F_C;							\
		A = (lo & 0x0f) + (hi & 0xf0);						\
	} else {									\
		int c = (P & F_C) ^ F_C;						\
		int sum = A - tmp - c;							\
		P &= ~(F_V | F_C);							\
		if( (A^tmp) & (A^sum) & F_N )						\
			P |= F_V;							\
		if( (sum & 0xff00) == 0 )						\
			P |= F_C;							\
		A = (UINT8) sum;							\
	}										\
	SET_NZ(A)

/* 65ce02 ******************************************************
 *  SEC Set carry flag
 ***************************************************************/
#define SEC										\
	P |= F_C

/* 65ce02 ******************************************************
 *  SED Set decimal flag
 ***************************************************************/
#define SED										\
	P |= F_D

/* 65ce02 ******************************************************
 *  see set disable extended stack flag
 ***************************************************************/
#define SEE										\
	P&=~F_E

/* 65ce02 ******************************************************
 *  SEI Set interrupt flag
 ***************************************************************/
#define SEI										\
	P |= F_I

/* 65ce02 ******************************************************
 *  SMB Set memory bit
 ***************************************************************/
#define SMB(bit)									\
	tmp |= (1<<bit)

/* 65ce02 ******************************************************
 * STA  Store accumulator
 ***************************************************************/
#define STA										\
	tmp = A

/* 65ce02 ******************************************************
 * STX  Store index X
 ***************************************************************/
#define STX										\
	tmp = X

/* 65ce02 ******************************************************
 * STY  Store index Y
 ***************************************************************/
#define STY										\
	tmp = Y

/* 65ce02 ******************************************************
 * STZ  Store index Z
 ***************************************************************/
#define STZ_65CE02									\
	tmp = Z

/* 65ce02 ******************************************************
 * TAB  Transfer accumulator to Direct Page
 ***************************************************************/
#define TAB										\
	B = A;										\
	SET_NZ(B)

/* 65ce02 ******************************************************
 * TAX  Transfer accumulator to index X
 ***************************************************************/
#define TAX										\
	X = A;										\
	SET_NZ(X)

/* 65ce02 ******************************************************
 * TAY  Transfer accumulator to index Y
 ***************************************************************/
#define TAY										\
	Y = A;										\
	SET_NZ(Y)

/* 65ce02 ******************************************************
 * TAZ  Transfer accumulator to index z
 ***************************************************************/
#define TAZ										\
	Z = A;										\
	SET_NZ(Z)

/* 65ce02 ******************************************************
 * TBA  Transfer direct page to accumulator
 ***************************************************************/
#define TBA										\
	A = B;										\
	SET_NZ(A)

/* 65ce02 ******************************************************
 * TRB  Test and reset bits
 ***************************************************************/
#define TRB										\
	SET_Z(tmp&A);									\
	tmp &= ~A

/* 65ce02 ******************************************************
 * TSB  Test and set bits
 ***************************************************************/
#define TSB										\
	SET_Z(tmp&A);									\
	tmp |= A

/* 65ce02 ******************************************************
 * TSX  Transfer stack LSB to index X
 ***************************************************************/
#define TSX										\
	X = SPL;									\
	SET_NZ(X)

/* 65ce02 ******************************************************
 * TSY  Transfer stack pointer to index y
 ***************************************************************/
#define TSY										\
	Y = SPL;									\
	SET_NZ(Y)

/* 65ce02 ******************************************************
 * TXA  Transfer index X to accumulator
 ***************************************************************/
#define TXA										\
	A = X;										\
	SET_NZ(A)

/* 65ce02 ********************************************************
 * TXS  Transfer index X to stack LSB
 * no flags changed (sic!)
 * txs tys not interruptable
 ***************************************************************/
#define TXS										\
	SPL = X;									\
	if (PEEK_OP() == 0x2b /*TYS*/ ) {						\
		UINT8 op = RDOP();							\
		(*m65ce02.insn[op])();							\
	}


/* 65ce02 ******************************************************
 * TYA  Transfer index Y to accumulator
 ***************************************************************/
#define TYA										\
	A = Y;										\
	SET_NZ(A)

/* 65ce02 ******************************************************
 * TYS  Transfer index y to stack pointer
 ***************************************************************/
#define TYS										\
	SPH = Y;

/* 65ce02 ******************************************************
 * TZA  Transfer index z to accumulator
 ***************************************************************/
#define TZA										\
	A = Z;										\
	SET_NZ(A)


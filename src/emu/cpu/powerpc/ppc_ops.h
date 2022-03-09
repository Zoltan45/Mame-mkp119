static PPC_OPCODE ppc_opcode_common[] =
{
	/*code  subcode         handler             */
	{ 31,	266,			ppc_addx			},
	{ 31,	266 | 512,		ppc_addx			},
	{ 31,	10,				ppc_addcx			},
	{ 31,	10 | 512,		ppc_addcx			},
	{ 31,	138,			ppc_addex			},
	{ 31,	138 | 512,		ppc_addex			},
	{ 14,	-1,				ppc_addi			},
	{ 12,	-1,				ppc_addic			},
	{ 13,	-1,				ppc_addic_rc		},
	{ 15,	-1,				ppc_addis			},
	{ 31,	234,			ppc_addmex			},
	{ 31,	234 | 512,		ppc_addmex			},
	{ 31,	202,			ppc_addzex			},
	{ 31,	202 | 512,		ppc_addzex			},
	{ 31,	28,				ppc_andx			},
	{ 31,	28 | 512,		ppc_andx			},
	{ 31,	60,				ppc_andcx			},
	{ 28,	-1,				ppc_andi_rc			},
	{ 29,	-1,				ppc_andis_rc		},
	{ 18,	-1,				ppc_bx				},
	{ 16,	-1,				ppc_bcx				},
	{ 19,	528,			ppc_bcctrx			},
	{ 19,	16,				ppc_bclrx			},
	{ 31,	0,				ppc_cmp				},
	{ 11,	-1,				ppc_cmpi			},
	{ 31,	32,				ppc_cmpl			},
	{ 10,	-1,				ppc_cmpli			},
	{ 31,	26,				ppc_cntlzw			},
	{ 19,	257,			ppc_crand			},
	{ 19,	129,			ppc_crandc			},
	{ 19,	289,			ppc_creqv			},
	{ 19,	225,			ppc_crnand			},
	{ 19,	33,				ppc_crnor			},
	{ 19,	449,			ppc_cror			},
	{ 19,	417,			ppc_crorc			},
	{ 19,	193,			ppc_crxor			},
	{ 31,	86,				ppc_dcbf			},
	{ 31,	470,			ppc_dcbi			},
	{ 31,	54,				ppc_dcbst			},
	{ 31,	278,			ppc_dcbt			},
	{ 31,	246,			ppc_dcbtst			},
	{ 31,	1014,			ppc_dcbz			},
	{ 31,	491,			ppc_divwx			},
	{ 31,	491 | 512,		ppc_divwx			},
	{ 31,	459,			ppc_divwux			},
	{ 31,	459 | 512,		ppc_divwux			},
	{ 31,	854,			ppc_eieio			},
	{ 31,	284,			ppc_eqvx			},
	{ 31,	954,			ppc_extsbx			},
	{ 31,	922,			ppc_extshx			},
	{ 31,	982,			ppc_icbi			},
	{ 19,	150,			ppc_isync			},
	{ 34,	-1,				ppc_lbz				},
	{ 35,	-1,				ppc_lbzu			},
	{ 31,	119,			ppc_lbzux			},
	{ 31,	87,				ppc_lbzx			},
	{ 42,	-1,				ppc_lha				},
	{ 43,	-1,				ppc_lhau			},
	{ 31,	375,			ppc_lhaux			},
	{ 31,	343,			ppc_lhax			},
	{ 31,	790,			ppc_lhbrx			},
	{ 40,	-1,				ppc_lhz				},
	{ 41,	-1,				ppc_lhzu			},
	{ 31,	311,			ppc_lhzux			},
	{ 31,	279,			ppc_lhzx			},
	{ 46,	-1,				ppc_lmw				},
	{ 31,	597,			ppc_lswi			},
	{ 31,	533,			ppc_lswx			},
	{ 31,	20,				ppc_lwarx			},
	{ 31,	534,			ppc_lwbrx			},
	{ 32,	-1,				ppc_lwz				},
	{ 33,	-1,				ppc_lwzu			},
	{ 31,	55,				ppc_lwzux			},
	{ 31,	23,				ppc_lwzx			},
	{ 19,	0,				ppc_mcrf			},
	{ 31,	512,			ppc_mcrxr			},
	{ 31,	19,				ppc_mfcr			},
	{ 31,	83,				ppc_mfmsr			},
	{ 31,	339,			ppc_mfspr			},
	{ 31,	144,			ppc_mtcrf			},
	{ 31,	146,			ppc_mtmsr			},
	{ 31,	467,			ppc_mtspr			},
	{ 31,	75,				ppc_mulhwx			},
	{ 31,	11,				ppc_mulhwux			},
	{ 7,	-1,				ppc_mulli			},
	{ 31,	235,			ppc_mullwx			},
	{ 31,	235 | 512,		ppc_mullwx			},
	{ 31,	476,			ppc_nandx			},
	{ 31,	104,			ppc_negx			},
	{ 31,	104 | 512,		ppc_negx			},
	{ 31,	124,			ppc_norx			},
	{ 31,	444,			ppc_orx				},
	{ 31,	412,			ppc_orcx			},
	{ 24,	-1,				ppc_ori				},
	{ 25,	-1,				ppc_oris			},
	{ 19,	50,				ppc_rfi				},
	{ 20,	-1,				ppc_rlwimix			},
	{ 21,	-1,				ppc_rlwinmx			},
	{ 23,	-1,				ppc_rlwnmx			},
	{ 17,	-1,				ppc_sc				},
	{ 31,	24,				ppc_slwx			},
	{ 31,	792,			ppc_srawx			},
	{ 31,	824,			ppc_srawix			},
	{ 31,	536,			ppc_srwx			},
	{ 38,	-1,				ppc_stb				},
	{ 39,	-1,				ppc_stbu			},
	{ 31,	247,			ppc_stbux			},
	{ 31,	215,			ppc_stbx			},
	{ 44,	-1,				ppc_sth				},
	{ 31,	918,			ppc_sthbrx			},
	{ 45,	-1,				ppc_sthu			},
	{ 31,	439,			ppc_sthux			},
	{ 31,	407,			ppc_sthx			},
	{ 47,	-1,				ppc_stmw			},
	{ 31,	725,			ppc_stswi			},
	{ 31,	661,			ppc_stswx			},
	{ 36,	-1,				ppc_stw				},
	{ 31,	662,			ppc_stwbrx			},
	{ 31,	150,			ppc_stwcx_rc		},
	{ 37,	-1,				ppc_stwu			},
	{ 31,	183,			ppc_stwux			},
	{ 31,	151,			ppc_stwx			},
	{ 31,	40,				ppc_subfx			},
	{ 31,	40 | 512,		ppc_subfx			},
	{ 31,	8,				ppc_subfcx			},
	{ 31,	8 | 512,		ppc_subfcx			},
	{ 31,	136,			ppc_subfex			},
	{ 31,	136 | 512,		ppc_subfex			},
	{ 8,	-1,				ppc_subfic			},
	{ 31,	232,			ppc_subfmex			},
	{ 31,	232 | 512,		ppc_subfmex			},
	{ 31,	200,			ppc_subfzex			},
	{ 31,	200 | 512,		ppc_subfzex			},
	{ 31,	598,			ppc_sync			},
	{ 31,	4,				ppc_tw				},
	{ 3,	-1,				ppc_twi				},
	{ 31,	316,			ppc_xorx			},
	{ 26,	-1,				ppc_xori			},
	{ 27,	-1,				ppc_xoris			}
};

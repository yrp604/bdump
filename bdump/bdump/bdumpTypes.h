#pragma once
#include <minwindef.h>

enum MsrValues : UINT32
{
	IA32_TIME_STAMP_COUNTER = 0x10,
	IA32_APIC_BASE = 0x1b,
	IA32_SYSENTER_CS = 0x174,
	IA32_SYSENTER_ESP = 0x175,
	IA32_SYSENTER_EIP = 0x176,
	IA32_PAT = 0x277,
	IA32_EFER = 0xc0000080,
	IA32_STAR = 0xc0000081,
	IA32_LSTAR = 0xc0000082,
	IA32_CSTAR = 0xc0000083,
	IA32_FMASK = 0xc0000084,
	IA32_FS_BASE = 0xc0000100,
	IA32_GS_BASE = 0xc0000101,
	IA32_KERNEL_GS_BASE = 0xc0000102,
	IA32_TSC_AUX = 0xc0000103
};

union SEGMENT_ATTRS
{
	struct
	{
		UINT8 Low;
		UINT8 High;
	};
	UINT16 value;
};

union SEGMENT_LIMIT
{
	struct
	{
		UINT16 Low;
		UINT16 High;
	};
	UINT32 value;
};

// 
// The structure of the Segment Selector as stated in Figure 3-6 of 3.4.2 in the Intel SDM Vol. 3A. 
// 
union SEGMENT_SELECTOR
{
	struct
	{
		UINT64 RPL : 2;		// Requested Privilege Level: Specifies the privilege level of the selector. The privilege level can range from 0 to 3, with 0 being the most privileged level. 
		UINT64 TI : 1;		// Table Indicator: Specifies the descriptor table to use. 0 is the GDT and 1 is the LDT. 
		UINT64 Index : 13;	// Index: Selects one of 8192 descriptors in the GDT or LDT. 
	};
	UINT64 value;
};

//
// This structure was shamelessly taken from:
// https://github.com/ntdiff/headers/blob/master/Win10_1507_TS1/x64/System32/hal.dll/Standalone/_KGDTENTRY64.h
//
typedef union _KGDTENTRY64
{
	union
	{
		struct
		{
			UINT16 LimitLow;
			UINT16 BaseLow;

			union
			{
				struct
				{
					UCHAR BaseMiddle;
					UCHAR Flags1;
					UCHAR Flags2;
					UCHAR BaseHigh;
				} Bytes;

				struct
				{
					struct
					{
						struct
						{
							ULONG BaseMiddle : 8;	/// bits 0 - 7
							ULONG Type : 5;			/// bits 8 - 12
							ULONG Dpl : 2;			/// bits 13 - 14
							ULONG Present : 1;		/// bit 15
							ULONG LimitHigh : 4;	/// bits 16 - 19
							ULONG System : 1;		/// bit 20
							ULONG LongMode : 1;		/// bit 21
							ULONG DefaultBig : 1;	/// bit 22
							ULONG Granularity : 1;	/// bit 23
							ULONG BaseHigh : 8;		/// bits 24 - 31
						};
					} Bits;

					ULONG BaseUpper;
					ULONG MustBeZero;
				};
			};
		};

		struct
		{
			INT64 DataLow;
			INT64 DataHigh;
		};
	};
} KGDTENTRY64, * PKGDTENTRY64;

union REGISTER
{
	struct
	{
		UINT64 Reserved : 62;	// Reserved
		UINT64 MSB : 1;			// Most significant bit: Used to determine if UM or KM.
	};
	UINT64 value;
};


/// EOF
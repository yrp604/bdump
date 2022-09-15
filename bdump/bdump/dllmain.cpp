// dllmain.cpp : Defines the entry point for the DLL application.
#include "engextcpp.hpp"
#include "bdumpTypes.h"

#include <ShlObj.h>

#include <sstream>
#include <array>

class StringBuilder
{
public:
	// come back to the datatype
	template <typename T>
	std::string BuildFloatingPointRegsString(
		_In_ std::array<T, 8>& FpRegs
	)
	{
		std::ostringstream ss{};

		//
		// Not really sure how to get the fpop register either...so setting to zero!
		// 
		ss << "  \"fpop\": \"0x0\", " << std::endl;
		ss << "  \"fpst\": [" << std::endl;

		for (const auto& i : FpRegs)
		{
			ss << std::hex << "    \"0x" << i << "\", " << std::endl;
		}

		//
		// Easier to erase last character. Using -3 because newline, space, comma. 
		// 
		ss.seekp(-3, std::ios_base::end);

		ss << "\n  ]," << std::endl;
		return ss.str();
	}

	std::string BuildDescriptorTableString(
		_In_ const char* DescriptorName,
		_In_ UINT64 DescriptorBase,
		_In_ UINT64 DescriptorLimit
	)
	{
		std::ostringstream ss{};

		ss << "  \"" << DescriptorName << "\": {" << std::endl;
		ss << "    \"base\": \"0x" << std::hex << DescriptorBase << "\", " << std::endl;
		ss << "    \"limit\": \"0x" << DescriptorLimit << "\"" << std::endl;
		ss << "  }, " << std::endl;

		return ss.str();
	}

	std::string BuildSegmentSelectorString(
		_In_ const char* SegmentName,
		_In_ UINT64 SegmentValue,
		_In_ ULARGE_INTEGER GdtrBase,
		_In_opt_ bool IsMsrReg = false,
		_In_opt_ UINT64 MsrBase = 0
	)
	{
		std::ostringstream ss{};

		SEGMENT_SELECTOR SegmentSelector = { 0 };
		SEGMENT_LIMIT Limit = { 0 };
		SEGMENT_ATTRS Attributes = { 0 };
		ULARGE_INTEGER SegmentBase = { 0 };

		PKGDTENTRY64 SegmentDescriptor = nullptr;

		UINT64 RegisterValue = 0;
		UINT64 GdtrOffset = 0;

		UINT32 BaseHigh = 0;
		UINT32 BaseMiddle = 0;
		UINT32 BaseLow = 0;

		SegmentSelector.value = SegmentValue;

		//
		// Since these selectors could be either GDT or LDT, make sure we implement dis hoe. 
		//
		if (!SegmentSelector.TI)
		{
			//
			// The processor multiplies the index by 8 and adds the result to the base address of the 
			// GDT or LDT depending on what the table indicator is. 
			//
			GdtrOffset = GdtrBase.QuadPart;
			GdtrOffset += (SegmentSelector.Index * 8);

			ReadMemory(GdtrOffset, &RegisterValue, sizeof(RegisterValue), NULL);
		}
		else
		{
			//
			// Something something LDT implementation.
			// 
		}

		SegmentDescriptor = reinterpret_cast<PKGDTENTRY64>(&RegisterValue);

		BaseHigh = SegmentDescriptor->Bytes.BaseHigh;
		BaseMiddle = SegmentDescriptor->Bytes.BaseMiddle;
		BaseLow = SegmentDescriptor->BaseLow;

		SegmentBase.QuadPart = (BaseHigh << 24) | (BaseMiddle << 16) | BaseLow;

		if (IsMsrReg)
		{
			SegmentBase.QuadPart = MsrBase;
		}

		Limit.Low = SegmentDescriptor->LimitLow;
		Limit.High = SegmentDescriptor->Bits.LimitHigh;

		//
		// Fix up the usermode segment selectors limit only if it is present.
		// 
		if (SegmentSelector.RPL && SegmentDescriptor->Bits.Present)
		{
			//
			// The granularity flag needs to be checked to see if the limit should be 
			// interpreted by either byte units or 4kb units. If it's set, align dat 
			// hoe by page.
			//
			if (SegmentDescriptor->Bits.Granularity)
			{
				Limit.value <<= 12;
				Limit.value |= 0xfff;
			}
		}

		//
		// Fix up the privileged selectors only if it is present. 
		// 
		if (!SegmentSelector.RPL && SegmentDescriptor->Bits.Present)
		{
			//
			// Make the address cannonical since it won't be. Check the low part. 
			// 
			if (SegmentDescriptor->BaseLow)
			{
				SegmentBase.HighPart |= GdtrBase.HighPart;
			}

			if (SegmentDescriptor->Bits.Granularity)
			{
				Limit.value <<= 12;
				Limit.value |= 0xfff;
			}
		}

		Attributes.Low = SegmentDescriptor->Bytes.Flags1;
		Attributes.High = SegmentDescriptor->Bytes.Flags2;

		//
		// Now that everything is calculated, build the segment string.
		// 
		ss << "  \"" << SegmentName << "\": { " << std::endl;
		ss << "    \"present\": " << ((SegmentDescriptor->Bits.Present) ? "true" : "false") << ", " << std::endl;
		ss << "    \"selector\": " << std::hex << "\"0x" << SegmentValue << "\", " << std::endl;
		ss << "    \"base\": " << "\"0x" << SegmentBase.QuadPart << "\", " << std::endl;
		ss << "    \"limit\": " << "\"0x" << Limit.value << "\", " << std::endl;
		ss << "    \"attr\": " << "\"0x" << Attributes.value << "\" " << std::endl;
		ss << "  }, ";

		return ss.str();
	}

	std::string BuildMSRString(
		_In_ const char* MsrName,
		_In_ UINT32 RegisterName
	)
	{
		std::ostringstream ss{};
		UINT64 MsrValue = 0;

		ReadMsr(RegisterName, &MsrValue);

		ss << "  \"" << MsrName << "\": \"0x" << std::hex << MsrValue << "\"";

		return ss.str();
	}

	template <typename T>
	std::string BuildNormalString(
		_In_ const char* RegisterName,
		_In_ T RegisterValue
	)
	{
		std::ostringstream ss{};

		ss << std::hex << "  \"" << RegisterName << "\": \"0x";
		ss << RegisterValue << "\", " << std::endl;

		return ss.str();
	}
};

class EXT_CLASS : public ExtExtension, StringBuilder
{
public:
	EXT_COMMAND_METHOD(bdump);

	HRESULT Initialize()
	{
		HRESULT hResult = DebugCreate(
			__uuidof(IDebugClient),
			reinterpret_cast<PVOID*>(&m_Client)
		);
		if (FAILED(hResult) || !m_Client.IsSet())
		{
			return hResult;
		}

		hResult = m_Client->QueryInterface(
			__uuidof(IDebugControl),
			reinterpret_cast<PVOID*>(&m_Control)
		);
		if (FAILED(hResult) || !m_Control.IsSet())
		{
			return hResult;
		}

		ExtensionApis.nSize = sizeof(ExtensionApis);
		hResult = m_Control->GetWindbgExtensionApis64(&ExtensionApis);
		if (FAILED(hResult))
		{
			return hResult;
		}

		m_Control->Output(
			DEBUG_OUTPUT_DEBUGGEE,
			"%s%s%s%s",
			"\nbdump is an extension that will create a dump directory and fill it with a memory dump and cpu state.\n",
			"Usage:\n",
			"    !bdump /full\n",
			"    !bdump /active /path \"C:\\path\\to\\dump\"\n"
		);

		Rip.value = 0;
		GsBase.value = 0;
		GdtrBase.QuadPart = 0;
		MxcsrMask = 0;
		Rflags = "rflags";

		return hResult;
	}

protected:
	_Ret_maybenull_
	HRESULT GetMxcsrMask(
		_Out_ PUINT32 RegisterValue
	)
	{
		CONTEXT ctx = { 0 };
		UINT32 MxcsrHigh = 0;

		//
		// Capture the current context to capture the mask. 
		//
		HRESULT hResult = m_Advanced->GetThreadContext(&ctx, sizeof(CONTEXT));
		if (FAILED(hResult))
		{
			return hResult;
		}

		//
		// The floating point state has a header that consists of two M128A vars. 
		// We are interested in the second one as that contains both MXCSR mask and
		// the MXCSR register value. 
		// 
		MxcsrHigh = (ctx.Header[1].High >> 32) & 0xffffffff;

		//
		// 11.6.6 Guidelines for Writing to the MXCSR register states that if the 
		// MXCSR_MASK field is 00000000h, then the default value for this mask is
		// 0000ffbfh. Implement dat hoe.
		//
		if (MxcsrHigh == NULL)
		{
			MxcsrHigh = 0xffbf;
		}

		*RegisterValue = MxcsrHigh;

		return hResult;
	}

	_Ret_maybenull_
	template <typename T>
	HRESULT GetRegisterValueByIndex(
		_In_ ULONG Index,
		_Out_ T* RegisterValue
	)
	{
		DEBUG_VALUE dv = { 0 };

		HRESULT hResult = m_Registers->GetValue(Index, &dv);
		if (FAILED(hResult))
		{
			return hResult;
		}

		switch (sizeof(T))
		{
		case 1:
			*RegisterValue = dv.I8;
			break;
		case 2:
			*RegisterValue = dv.I16;
			break;
		case 4:
			*RegisterValue = dv.I32;
			break;
		case 8:
			*RegisterValue = dv.I64;
			break;
		default:
			hResult = S_FALSE;
			break;
		}

		return hResult;
	}

	_Ret_maybenull_
	template <typename T>
	HRESULT GetRegisterValueByName(
		_In_ PCSTR RegisterName,
		_Out_ T* RegisterValue
	)
	{
		ULONG Index = 0;
		DEBUG_VALUE dv = { 0 };

		HRESULT hResult = m_Registers->GetIndexByName(RegisterName, &Index);
		if (FAILED(hResult))
		{
			return hResult;
		}

		hResult = m_Registers->GetValue(Index, &dv);
		if (FAILED(hResult))
		{
			return hResult;
		}

		switch (sizeof(T))
		{
		case 1:
			*RegisterValue = dv.I8;
			break;
		case 2:
			*RegisterValue = dv.I16;
			break;
		case 4:
			*RegisterValue = dv.I32;
			break;
		case 8:
			*RegisterValue = dv.I64;
			break;
		default:
			hResult = S_FALSE;
			break;
		}

		return hResult;
	}

private:
	REGISTER Rip;
	REGISTER GsBase;
	UINT64 FsBase;

	ULARGE_INTEGER GdtrBase;
	UINT32 MxcsrMask;

	const char* Rflags;

	bool bNeedsSwappin;
};

EXT_DECLARE_GLOBALS();

EXT_COMMAND(
	bdump,
	"",
	"{full;b,o;full;does a complete kernel memory dump.}"
	"{active;b,o;active;does an active kernel memory dump. This takes an incredible amount of time.}"
	"{path;s,o;path;path to dump. Dumps to desktop by default}"
)
{
	ULONG NumberOfRegisters = 0;
	UINT64 DescriptorLimit = 0;

	std::ostringstream ss{};
	std::string DirPath(MAX_PATH, '\0');
	std::string RegsPath(MAX_PATH, '\0');
	std::string DmpPath(MAX_PATH, '\0');

	HRESULT hResult = S_FALSE;

	if (!HasArg("path"))
	{
		//
		// Get the current user's Desktop path to output regs.json and mem.dmp.
		//
		hResult = SHGetFolderPathA(NULL, CSIDL_DESKTOP, NULL, NULL, &DirPath[0]);
		if (FAILED(hResult))
		{
			throw ExtStatusException(hResult, "SHGetFolderPathA failed");
		}
	}
	else
	{
		DirPath = GetArgStr("path", FALSE);
	}

	//
	// Resize the directory path
	//
	DirPath.resize(strlen(DirPath.c_str()));

	//
	// Build the reg.json path.
	//
	RegsPath = DirPath;
	RegsPath.append("\\regs.json");
	RegsPath.resize(strlen(RegsPath.c_str()));

	//
	// Now build the mem.dmp path.
	//
	DmpPath = DirPath;
	DmpPath.append("\\mem.dmp");
	DmpPath.resize(strlen(DmpPath.c_str()));

	//
	// Get the value of the GDTR register. This will be needed to correctly calculate 
	// the Segment Selector registers.  
	// 
	hResult = GetRegisterValueByName("gdtr", &GdtrBase.QuadPart);
	if (FAILED(hResult))
	{
		throw ExtStatusException(hResult, "GetRegisterValue failed");
	}

	//
	// Get the value of RIP to determine if usermode or kernelmode. Will need it later.
	//
	hResult = GetRegisterValueByName("rip", &Rip.value);
	if (FAILED(hResult))
	{
		throw ExtStatusException(hResult, "GetRegisterValue failed");
	}

	hResult = m_Registers->GetNumberRegisters(&NumberOfRegisters);
	if (FAILED(hResult))
	{
		throw ExtStatusException(hResult, "GetNumberRegisters failed");
	}

	//
	// Get the base addresses for FS/GS via MSRs. 
	//
	ReadMsr(IA32_FS_BASE, &FsBase);
	ReadMsr(IA32_GS_BASE, &GsBase.value);

	//
	// Start building regs.json. 
	//
	ss << "{\n";
	for (ULONG i = 0; i < NumberOfRegisters; i++)
	{
		char RegisterName[8] = { 0 };

		hResult = m_Registers->GetDescription(i, RegisterName, sizeof(RegisterName), NULL, NULL);
		if (SUCCEEDED(hResult))
		{
			DEBUG_VALUE Value = { 0 };

			hResult = m_Registers->GetValue(i, &Value);
			if (SUCCEEDED(hResult))
			{
				UINT64 RegisterValue = Value.F128Parts64.LowPart;

				//
				// If the register name is a segment selector, build the string for that.
				//
				if (strcmp(RegisterName, "cs") == NULL)
				{
					ss << BuildSegmentSelectorString("cs", RegisterValue, GdtrBase).c_str() << std::endl;
					continue;
				}
				else if (strcmp(RegisterName, "ds") == NULL)
				{
					ss << BuildSegmentSelectorString("ds", RegisterValue, GdtrBase).c_str() << std::endl;
					continue;
				}
				else if (strcmp(RegisterName, "es") == NULL)
				{
					ss << BuildSegmentSelectorString("es", RegisterValue, GdtrBase).c_str() << std::endl;
					continue;
				}
				else if (strcmp(RegisterName, "fs") == NULL)
				{
					ss << BuildSegmentSelectorString("fs", RegisterValue, GdtrBase, true, FsBase).c_str() << std::endl;
					continue;
				}
				else if (strcmp(RegisterName, "gs") == NULL)
				{
					if (Rip.MSB != GsBase.MSB)
					{
						bNeedsSwappin = true;

						ReadMsr(IA32_KERNEL_GS_BASE, &GsBase.value);
					}

					ss << BuildSegmentSelectorString("gs", RegisterValue, GdtrBase, true, GsBase.value).c_str() << std::endl;
					continue;
				}
				else if (strcmp(RegisterName, "ss") == NULL)
				{
					ss << BuildSegmentSelectorString("ss", RegisterValue, GdtrBase).c_str() << std::endl;
					continue;
				}
				else if (strcmp(RegisterName, "tr") == NULL)
				{
					ss << BuildSegmentSelectorString("tr", RegisterValue, GdtrBase).c_str() << std::endl;
					continue;
				}
				else if (strcmp(RegisterName, "ldtr") == NULL)
				{
					ss << BuildSegmentSelectorString("ldtr", RegisterValue, GdtrBase).c_str() << std::endl;
					continue;
				}

				//
				// Check if the register name is mxcsr. If it is, input the mask.
				// 
				else if (strcmp(RegisterName, "mxcsr") == NULL)
				{
					hResult = GetMxcsrMask(&MxcsrMask);
					if (SUCCEEDED(hResult))
					{
						ss << BuildNormalString("mxcsr_mask", MxcsrMask).c_str();
					}
					else
					{
						throw ExtStatusException(hResult, "GetMxcsrMask failed");
					}
				}
				else if (strcmp(RegisterName, "kmxcsr") == NULL)
				{
					//
					// Assign the kmxcsr mask the same value as its UM counterpart. 
					// 
					ss << BuildNormalString("kmxcsr_mask", MxcsrMask).c_str();
				}

				//
				// Check if the IRQL is correct for usermode and if not, put it back in DISPATCH_LEVEL.
				//
				else if (strcmp(RegisterName, "cr8") == NULL && !Rip.MSB)
				{
					ss << BuildNormalString("cr8", NULL).c_str();
					continue;
				}

				//
				// The rflags register is named to efl and it throws off the parser in wtf. Rename
				// dat hoe to rflags.
				// 
				else if (strcmp(RegisterName, "efl") == NULL)
				{
					RtlCopyMemory(RegisterName, Rflags, strlen(Rflags));
				}

				//
				// The following registers need to be reformatted to be able to get processed by wtf's parser:
				//	 1. GDT
				//   2. IDT
				// 
				// These are listed as GDTR/GDTL and IDTR/IDTL. Fix deez hoes.
				//
				else if (strcmp(RegisterName, "gdtr") == NULL ||
					strcmp(RegisterName, "idtr") == NULL)
				{
					//
					// The current register's state is "R". The index needs to be incremented to get
					// the next register of "L", which contains the limit. 
					// 
					i++;

					hResult = GetRegisterValueByIndex(i, &DescriptorLimit);
					if (SUCCEEDED(hResult))
					{
						std::string testing = BuildDescriptorTableString(RegisterName, RegisterValue, DescriptorLimit);
						if (testing.empty())
						{
							throw ExtStatusException(hResult, "BuildDescriptorTableString failed");
						}

						ss << testing.c_str();

						continue;
					}
					else
					{
						throw ExtStatusException(hResult, "GetRegisterValueByIndex failed");
					}
				}

				//
				// Another register that needs to be fixed up to be processed correctly are the stX registers.
				// Fix dem hoes up!
				// 
				else if (strcmp(RegisterName, "st0") == NULL)
				{
					// 
					// There are eight stX registers. 
					// TODO: Work on this because they aren't UINT64. Placeholder. 
					// 
					std::array<UINT64, 8> FpArray{};

					FpArray.data()[0] = RegisterValue;

					int index = 1;

					do
					{
						hResult = GetRegisterValueByIndex(i, &RegisterValue);
						if (FAILED(hResult))
						{
							throw ExtStatusException(hResult, "GetRegisterValueByIndex failed");
						}

						FpArray.data()[index] = RegisterValue;

						index++;
						i++;
					} while (index < FpArray.size());

					std::string testing = BuildFloatingPointRegsString(FpArray);
					if (testing.empty())
					{
						throw ExtStatusException(hResult, "BuildFloatingPointRegsString failed");
					}

					ss << testing.c_str();

					continue;
				}

				ss << BuildNormalString(RegisterName, RegisterValue).c_str();
			}
			else
			{
				throw ExtStatusException(hResult, "GetValue failed");
			}
		}
		else
		{
			throw ExtStatusException(hResult, "GetDescription failed");
		}
	}

	//
	// The string for all the registers have been built. Now on to building all 
	// the MSRs... 
	//
	ss << BuildMSRString("tsc", IA32_TIME_STAMP_COUNTER).c_str() << ", " << std::endl;
	ss << BuildMSRString("apic_base", IA32_APIC_BASE).c_str() << ", " << std::endl;
	ss << BuildMSRString("sysenter_cs", IA32_SYSENTER_CS).c_str() << ", " << std::endl;
	ss << BuildMSRString("sysenter_esp", IA32_SYSENTER_ESP).c_str() << ", " << std::endl;
	ss << BuildMSRString("sysenter_eip", IA32_SYSENTER_EIP).c_str() << ", " << std::endl;
	ss << BuildMSRString("pat", IA32_PAT).c_str() << ", " << std::endl;
	ss << BuildMSRString("efer", IA32_EFER).c_str() << ", " << std::endl;
	ss << BuildMSRString("star", IA32_STAR).c_str() << ", " << std::endl;
	ss << BuildMSRString("lstar", IA32_LSTAR).c_str() << ", " << std::endl;
	ss << BuildMSRString("cstar", IA32_CSTAR).c_str() << ", " << std::endl;
	ss << BuildMSRString("sfmask", IA32_FMASK).c_str() << ", " << std::endl;
	ss << BuildMSRString("tsc_aux", IA32_TSC_AUX).c_str() << ", " << std::endl;

	if (bNeedsSwappin)
	{
		ss << BuildMSRString("kernel_gs_base", IA32_GS_BASE).c_str() << std::endl;
	}
	else
	{
		ss << BuildMSRString("kernel_gs_base", IA32_KERNEL_GS_BASE).c_str() << std::endl;
	}

	ss << "}" << std::endl;

	HANDLE hFile = CreateFileA(
		RegsPath.c_str(),
		GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	if (hFile != INVALID_HANDLE_VALUE)
	{
		bool bStatus = WriteFile(
			hFile,
			ss.str().c_str(),
			ss.str().size(),
			NULL,
			NULL
		);
		if (!bStatus)
		{
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;

			throw ExtStatusException(hResult, "WriteFile failed");
		}

		CloseHandle(hFile);
		hFile = INVALID_HANDLE_VALUE;
	}
	else
	{
		throw ExtStatusException(hResult, "CreateFileA failed");
	}

	if (HasArg("full"))
	{
		hResult = m_Client->WriteDumpFile(DmpPath.c_str(), DEBUG_KERNEL_FULL_DUMP);
		if (FAILED(hResult))
		{
			throw ExtStatusException(hResult, "WriteDumpFile failed");
		}
	}
	else if (HasArg("active"))
	{
		//
		// As of right now, WriteDumpFile fails with E_INVALIDARG. This is because IDebugClient, 
		// and variants, hit WriteDumpFileInternal. A check is done against a global named g_Target 
		// at offset 0xff0. If the DWORD located at the offset is 1 and the Qualifier minus 1024
		// is greater than 2, it will fail with E_INVALIDARG. If the DWORD minus 2 is less than or
		// equal to 1 and the Qualifier minus 1024 is greater than 1, it will fail with E_INVALIDARG. 
		// Whatever the value is at that location would need to be 4 <=. 
		// 
		// Pseudo code based on ida:
		// if (g_Target && ((v10 = *((DWORD*)g_Target + 0x3fc), v10 == 1) && Qualifier - 1024 > 2 ||
		//     v10 - 1 <= 1 && Qualifier - 1024 > 1))
		// {
		//     hResult = E_INVALIDARG
		// }
		// 
		// I did not see anything in the cross references to influence this item. This struct is opaque.  
		// 
		// DEBUG_DUMP_ACTIVE is 1030 and fails both checks because the result is 6. Doing it directly
		// from the debugger or the js script like the original script satisfies this check. 
		// 
		// Using Execute(".dump ... "); fails this check. 
		//
		hResult = m_Client->WriteDumpFile(DmpPath.c_str(), DEBUG_DUMP_ACTIVE);
		if (FAILED(hResult))
		{
			throw ExtStatusException(hResult, "WriteDumpFile failed");
		}
	}
}


/// EOF
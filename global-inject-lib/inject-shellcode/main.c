//===============================================================================================//
// The code is based on code from the ReflectiveDLLInjection project:
// https://github.com/stephenfewer/ReflectiveDLLInjection/tree/master/dll/src
// Original license can be found below.
//===============================================================================================//
// Copyright (c) 2012, Stephen Fewer of Harmony Security (www.harmonysecurity.com)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are permitted
// provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice, this list of
// conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright notice, this list of
// conditions and the following disclaimer in the documentation and/or other materials provided
// with the distribution.
//
//     * Neither the name of Harmony Security nor the names of its contributors may be used to
// endorse or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//===============================================================================================//

#include <windows.h>
#include "../dll_inject.h"

#define DEREF( name )*(UINT_PTR *)(name)
#define DEREF_64( name )*(DWORD64 *)(name)
#define DEREF_32( name )*(DWORD *)(name)
#define DEREF_16( name )*(WORD *)(name)
#define DEREF_8( name )*(BYTE *)(name)

// NOTE: module hashes are computed using all-caps unicode strings
#define KERNEL32DLL_HASH                0x6A4ABC5B

#define LOADLIBRARYW_HASH               0xEC0E4EA4
#define GETPROCADDRESS_HASH             0x7C0DFCAA
#define FREELIBRARY_HASH                0x4DC9D5A0
#define VIRTUALFREE_HASH                0x030633AC

#define HASH_KEY						13
//===============================================================================================//
#pragma intrinsic( _rotr )

__forceinline DWORD ror(DWORD d)
{
	return _rotr(d, HASH_KEY);
}

__forceinline DWORD hash(const char* c)
{
	register DWORD h = 0;
	do
	{
		h = ror(h);
		h += *c;
	} while (*++c);

	return h;
}
//===============================================================================================//
typedef struct _UNICODE_STR
{
	USHORT Length;
	USHORT MaximumLength;
	PWSTR pBuffer;
} UNICODE_STR, * PUNICODE_STR;

// WinDbg> dt -v ntdll!_LDR_DATA_TABLE_ENTRY
//__declspec( align(8) )
typedef struct _LDR_DATA_TABLE_ENTRY
{
	//LIST_ENTRY InLoadOrderLinks; // As we search from PPEB_LDR_DATA->InMemoryOrderModuleList we dont use the first entry.
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STR FullDllName;
	UNICODE_STR BaseDllName;
	ULONG Flags;
	SHORT LoadCount;
	SHORT TlsIndex;
	LIST_ENTRY HashTableEntry;
	ULONG TimeDateStamp;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

// WinDbg> dt -v ntdll!_PEB_LDR_DATA
typedef struct _PEB_LDR_DATA //, 7 elements, 0x28 bytes
{
	DWORD dwLength;
	DWORD dwInitialized;
	LPVOID lpSsHandle;
	LIST_ENTRY InLoadOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	LPVOID lpEntryInProgress;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

// WinDbg> dt -v ntdll!_PEB_FREE_BLOCK
typedef struct _PEB_FREE_BLOCK // 2 elements, 0x8 bytes
{
	struct _PEB_FREE_BLOCK* pNext;
	DWORD dwSize;
} PEB_FREE_BLOCK, * PPEB_FREE_BLOCK;

// struct _PEB is defined in Winternl.h but it is incomplete
// WinDbg> dt -v ntdll!_PEB
typedef struct __PEB // 65 elements, 0x210 bytes
{
	BYTE bInheritedAddressSpace;
	BYTE bReadImageFileExecOptions;
	BYTE bBeingDebugged;
	BYTE bSpareBool;
	LPVOID lpMutant;
	LPVOID lpImageBaseAddress;
	PPEB_LDR_DATA pLdr;
	LPVOID lpProcessParameters;
	LPVOID lpSubSystemData;
	LPVOID lpProcessHeap;
	PRTL_CRITICAL_SECTION pFastPebLock;
	LPVOID lpFastPebLockRoutine;
	LPVOID lpFastPebUnlockRoutine;
	DWORD dwEnvironmentUpdateCount;
	LPVOID lpKernelCallbackTable;
	DWORD dwSystemReserved;
	DWORD dwAtlThunkSListPtr32;
	PPEB_FREE_BLOCK pFreeList;
	DWORD dwTlsExpansionCounter;
	LPVOID lpTlsBitmap;
	DWORD dwTlsBitmapBits[2];
	LPVOID lpReadOnlySharedMemoryBase;
	LPVOID lpReadOnlySharedMemoryHeap;
	LPVOID lpReadOnlyStaticServerData;
	LPVOID lpAnsiCodePageData;
	LPVOID lpOemCodePageData;
	LPVOID lpUnicodeCaseTableData;
	DWORD dwNumberOfProcessors;
	DWORD dwNtGlobalFlag;
	LARGE_INTEGER liCriticalSectionTimeout;
	DWORD dwHeapSegmentReserve;
	DWORD dwHeapSegmentCommit;
	DWORD dwHeapDeCommitTotalFreeThreshold;
	DWORD dwHeapDeCommitFreeBlockThreshold;
	DWORD dwNumberOfHeaps;
	DWORD dwMaximumNumberOfHeaps;
	LPVOID lpProcessHeaps;
	LPVOID lpGdiSharedHandleTable;
	LPVOID lpProcessStarterHelper;
	DWORD dwGdiDCAttributeList;
	LPVOID lpLoaderLock;
	DWORD dwOSMajorVersion;
	DWORD dwOSMinorVersion;
	WORD wOSBuildNumber;
	WORD wOSCSDVersion;
	DWORD dwOSPlatformId;
	DWORD dwImageSubsystem;
	DWORD dwImageSubsystemMajorVersion;
	DWORD dwImageSubsystemMinorVersion;
	DWORD dwImageProcessAffinityMask;
	DWORD dwGdiHandleBuffer[34];
	LPVOID lpPostProcessInitRoutine;
	LPVOID lpTlsExpansionBitmap;
	DWORD dwTlsExpansionBitmapBits[32];
	DWORD dwSessionId;
	ULARGE_INTEGER liAppCompatFlags;
	ULARGE_INTEGER liAppCompatFlagsUser;
	LPVOID lppShimData;
	LPVOID lpAppCompatInfo;
	UNICODE_STR usCSDVersion;
	LPVOID lpActivationContextData;
	LPVOID lpProcessAssemblyStorageMap;
	LPVOID lpSystemDefaultActivationContextData;
	LPVOID lpSystemAssemblyStorageMap;
	DWORD dwMinimumStackCommit;
} _PEB, * _PPEB;

__declspec(dllexport)
BOOL __stdcall InjectShellcode(void* pParameter)
{
	LOAD_LIBRARY_REMOTE_DATA* pInjData = (LOAD_LIBRARY_REMOTE_DATA*)pParameter;
	HMODULE hModule;
	char szInjectInit[] = { 'I', 'n', 'j', 'e', 'c', 't', 'I', 'n', 'i', 't', '\0' };
	void* pInjectInit;

	pInjData->dwError = INJ_ERR_BEFORE_ENUM_IMPORTS;

	//////////////////////////////////////////////////////////////////////////
	// Based on code from ImprovedReflectiveDLLInjection

	// the functions we need
	void* pLoadLibraryW = NULL;
	void* pGetProcAddress = NULL;
	void* pFreeLibrary = NULL;
	void* pVirtualFree = NULL;

	USHORT usCounter;

	// the kernels base address and later this images newly loaded base address
	ULONG_PTR uiBaseAddress;

	// variables for processing the kernels export table
	ULONG_PTR uiAddressArray;
	ULONG_PTR uiNameArray;
	ULONG_PTR uiExportDir;
	ULONG_PTR uiNameOrdinals;
	DWORD dwHashValue;
	DWORD dwNumberOfNames;

	// variables for loading this image
	PLIST_ENTRY pleInLoadHead;
	PLIST_ENTRY pleInLoadIter;
	ULONG_PTR uiValueA;
	ULONG_PTR uiValueB;
	ULONG_PTR uiValueC;

	// STEP 1: process the kernels exports for the functions our loader needs...

	// get the Process Environment Block
#ifdef _WIN64
	uiBaseAddress = __readgsqword(0x60);
#else
	uiBaseAddress = __readfsdword(0x30);
#endif

	// get the processes loaded modules. ref: http://msdn.microsoft.com/en-us/library/aa813708(VS.85).aspx
	uiBaseAddress = (ULONG_PTR)((_PPEB)uiBaseAddress)->pLdr;

	// get the first entry of the InMemoryOrder module list
	pleInLoadHead = &((PPEB_LDR_DATA)uiBaseAddress)->InMemoryOrderModuleList;
	pleInLoadIter = pleInLoadHead->Flink;
	while (pleInLoadIter != pleInLoadHead)
	{
		uiValueA = (ULONG_PTR)pleInLoadIter;

		// get pointer to current modules name (unicode string)
		uiValueB = (ULONG_PTR)((PLDR_DATA_TABLE_ENTRY)uiValueA)->BaseDllName.pBuffer;
		// set bCounter to the length for the loop
		usCounter = ((PLDR_DATA_TABLE_ENTRY)uiValueA)->BaseDllName.Length;
		// clear uiValueC which will store the hash of the module name
		uiValueC = 0;

		// compute the hash of the module name...
		do
		{
			uiValueC = ror((DWORD)uiValueC);
			// normalize to uppercase if the module name is in lowercase
			if (*((BYTE*)uiValueB) >= 'a')
				uiValueC += *((BYTE*)uiValueB) - 0x20;
			else
				uiValueC += *((BYTE*)uiValueB);
			uiValueB++;
		} while (--usCounter);

		// compare the hash with that of kernel32.dll
		if ((DWORD)uiValueC == KERNEL32DLL_HASH)
		{
			// get this modules base address
			uiBaseAddress = (ULONG_PTR)((PLDR_DATA_TABLE_ENTRY)uiValueA)->DllBase;

			// get the VA of the modules NT Header
			uiExportDir = uiBaseAddress + ((PIMAGE_DOS_HEADER)uiBaseAddress)->e_lfanew;

			// uiNameArray = the address of the modules export directory entry
			uiNameArray = (ULONG_PTR) & ((PIMAGE_NT_HEADERS)uiExportDir)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

			// get the VA of the export directory
			uiExportDir = (uiBaseAddress + ((PIMAGE_DATA_DIRECTORY)uiNameArray)->VirtualAddress);

			// get the VA for the array of name pointers
			uiNameArray = (uiBaseAddress + ((PIMAGE_EXPORT_DIRECTORY)uiExportDir)->AddressOfNames);

			// get the VA for the array of name ordinals
			uiNameOrdinals = (uiBaseAddress + ((PIMAGE_EXPORT_DIRECTORY)uiExportDir)->AddressOfNameOrdinals);

			// get total number of named exports
			dwNumberOfNames = ((PIMAGE_EXPORT_DIRECTORY)uiExportDir)->NumberOfNames;

			usCounter = 4;

			// loop while we still have imports to find
			while (usCounter > 0 && dwNumberOfNames > 0)
			{
				// compute the hash values for this function name
				dwHashValue = hash((char*)(uiBaseAddress + DEREF_32(uiNameArray)));

				// if we have found a function we want we get its virtual address
				if (dwHashValue == LOADLIBRARYW_HASH || dwHashValue == GETPROCADDRESS_HASH || dwHashValue == FREELIBRARY_HASH || dwHashValue == VIRTUALFREE_HASH)
				{
					// get the VA for the array of addresses
					uiAddressArray = (uiBaseAddress + ((PIMAGE_EXPORT_DIRECTORY)uiExportDir)->AddressOfFunctions);

					// use this functions name ordinal as an index into the array of name pointers
					uiAddressArray += (DEREF_16(uiNameOrdinals) * sizeof(DWORD));

					// store this functions VA
					if (dwHashValue == LOADLIBRARYW_HASH)
						pLoadLibraryW = (void*)(uiBaseAddress + DEREF_32(uiAddressArray));
					else if (dwHashValue == GETPROCADDRESS_HASH)
						pGetProcAddress = (void*)(uiBaseAddress + DEREF_32(uiAddressArray));
					else if (dwHashValue == FREELIBRARY_HASH)
						pFreeLibrary = (void*)(uiBaseAddress + DEREF_32(uiAddressArray));
					else if (dwHashValue == VIRTUALFREE_HASH)
						pVirtualFree = (void*)(uiBaseAddress + DEREF_32(uiAddressArray));

					// decrement our counter
					usCounter--;
				}

				// get the next exported function name
				uiNameArray += sizeof(DWORD);

				// get the next exported function name ordinal
				uiNameOrdinals += sizeof(WORD);

				// decrement our # of names counter
				dwNumberOfNames--;
			}
		}

		// we stop searching when we have found everything we need.
		if (pLoadLibraryW && pGetProcAddress && pFreeLibrary && pVirtualFree)
			break;

		// get the next entry
		pleInLoadIter = pleInLoadIter->Flink;
	}

	pInjData->pVirtualFree = pVirtualFree;

	if (!pLoadLibraryW || !pGetProcAddress || !pFreeLibrary || !pVirtualFree)
	{
		pInjData->dwError = INJ_ERR_ENUM_IMPORTS;
		return 1;
	}

	pInjData->dwError = INJ_ERR_BEFORE_LOADLIBRARY;

	hModule = ((HMODULE(WINAPI*)(LPCTSTR))pLoadLibraryW)(pInjData->szDllName);
	if (hModule)
	{
		pInjData->dwError = INJ_ERR_BEFORE_GETPROCADDR;

		pInjectInit = ((FARPROC(WINAPI*)(HMODULE, LPCSTR))pGetProcAddress)(hModule, szInjectInit);
		if (pInjectInit)
		{
			pInjData->dwError = INJ_ERR_BEFORE_LIBINIT;

			pInjData->dwError = ((DWORD(__stdcall*)(void))pInjectInit)();
			if (pInjData->dwError == 0)
				return 0;
		}
		else
			pInjData->dwError = INJ_ERR_GETPROCADDR;

		((BOOL(WINAPI*)(HMODULE))pFreeLibrary)(hModule);
	}
	else
		pInjData->dwError = INJ_ERR_LOADLIBRARY;

	return 1;
}

int CALLBACK wWinMain(
	_In_ HINSTANCE hInstance,
	_In_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow
)
{
	InjectShellcode((void*)0x1234);
	return 0;
}

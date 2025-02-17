#include "common_functions.h"

#include <ntsecapi.h>
#include <sddl.h>
#include <tlhelp32.h>

#include <wil/stl.h> // must be included before other wil includes
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/win32_helpers.h>

BOOL GetFullAccessSecurityDescriptor(_Outptr_ PSECURITY_DESCRIPTOR* SecurityDescriptor, _Out_opt_ PULONG SecurityDescriptorSize)
{
	// http://rsdn.org/forum/winapi/7510772.flat
	//
	// For full access maniacs :)
	// Full access for the "Everyone" group and for the "All [Restricted] App Packages" groups.
	// The integrity label is Untrusted (lowest level).
	//
	// D - DACL
	// P - Protected
	// A - Access Allowed
	// GA - GENERIC_ALL
	// WD - 'All' Group (World)
	// S-1-15-2-1 - All Application Packages
	// S-1-15-2-2 - All Restricted Application Packages
	//
	// S - SACL
	// ML - Mandatory Label
	// NW - No Write-Up policy
	// S-1-16-0 - Untrusted Mandatory Level
	PCWSTR pszStringSecurityDescriptor = L"D:P(A;;GA;;;WD)(A;;GA;;;S-1-15-2-1)(A;;GA;;;S-1-15-2-2)S:(ML;;NW;;;S-1-16-0)";

	return ConvertStringSecurityDescriptorToSecurityDescriptor(
		pszStringSecurityDescriptor, SDDL_REVISION_1, SecurityDescriptor, SecurityDescriptorSize);
}

// SetPrivilege enables/disables process token privilege.
// https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/debug-privilege
BOOL SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege)
{
	LUID luid;
	BOOL bRet = FALSE;

	if (LookupPrivilegeValue(nullptr, lpszPrivilege, &luid)) {
		TOKEN_PRIVILEGES tp;

		tp.PrivilegeCount = 1;
		tp.Privileges[0].Luid = luid;
		tp.Privileges[0].Attributes = (bEnablePrivilege) ? SE_PRIVILEGE_ENABLED : 0;

		// Enable the privilege or disable all privileges.
		if (AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr)) {
			// Check to see if you have proper access.
			// You may get "ERROR_NOT_ALL_ASSIGNED".
			bRet = (GetLastError() == ERROR_SUCCESS);
		}
	}

	return bRet;
}

BOOL SetDebugPrivilege(BOOL bEnablePrivilege)
{
	wil::unique_handle token;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token)) {
		return FALSE;
	}

	return SetPrivilege(token.get(), SE_DEBUG_NAME, bEnablePrivilege);
}

std::filesystem::path GetEnginePath(USHORT* pMachine)
{
	// Use current architecture.
	#ifdef _WIN64
	USHORT machine = IMAGE_FILE_MACHINE_AMD64;
	#else // !_WIN64
	USHORT machine = IMAGE_FILE_MACHINE_I386;
	#endif // _WIN64

	// Use given architecture.
	if (pMachine != nullptr)
	{
		machine = *pMachine;
	}

	PCWSTR folderName;
	switch (machine) {
	case IMAGE_FILE_MACHINE_I386:
		folderName = L"32";
		break;

	case IMAGE_FILE_MACHINE_AMD64:
		folderName = L"64";
		break;

	default:
		throw std::logic_error("Unknown architecture");
	}

	std::filesystem::path modulePath = wil::GetModuleFileName<std::wstring>();
	return modulePath.parent_path() / folderName;
}

std::filesystem::path GetDllFileName()
{
	std::filesystem::path result;
	#ifdef _DEBUG // Debug
	result = L"global-inject-lib_D.dll";
	#else // Release
	result = L"global-inject-lib.dll";
	#endif
	return result;
}

//
// https://docs.microsoft.com/en-us/windows/win32/sysinfo/verifying-the-system-version
//
BOOL CheckWindowsVersion(DWORD dwMajorVersion, DWORD dwMinorVersion, WORD wServicePackMajor, WORD wServicePackMinor, int op)
{
	// Initialize the OSVERSIONINFOEX structure
	OSVERSIONINFOEX osvi;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	osvi.dwMajorVersion = dwMajorVersion;
	osvi.dwMinorVersion = dwMinorVersion;
	osvi.wServicePackMajor = wServicePackMajor;
	osvi.wServicePackMinor = wServicePackMinor;

	// Initialize the type mask
	DWORD dwTypeMask = VER_MAJORVERSION | VER_MINORVERSION |
		VER_SERVICEPACKMAJOR | VER_SERVICEPACKMINOR;

	// Initialize the condition mask
	DWORDLONG dwlConditionMask = 0;

	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, op);
	VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, op);
	VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMAJOR, op);
	VER_SET_CONDITION(dwlConditionMask, VER_SERVICEPACKMINOR, op);

	// Perform the test
	return VerifyVersionInfo(&osvi, dwTypeMask, dwlConditionMask);
}

USHORT GetProcessArch(HANDLE hProcess)
{
	// For now, only IMAGE_FILE_MACHINE_I386 and IMAGE_FILE_MACHINE_AMD64.
	// TODO: Use IsWow64Process2 if available.

	#ifndef _WIN64
	SYSTEM_INFO siSystemInfo;
	GetNativeSystemInfo(&siSystemInfo);
	if (siSystemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
		// 32-bit machine, only one option.
		return IMAGE_FILE_MACHINE_I386;
	}
	#endif // _WIN64

	BOOL bIsWow64Process;
	if (IsWow64Process(hProcess, &bIsWow64Process) && bIsWow64Process) {
		return IMAGE_FILE_MACHINE_I386;
	}

	return IMAGE_FILE_MACHINE_AMD64;
}

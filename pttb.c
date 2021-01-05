// # pttb - Pin To TaskBar

// Pin To TaskBar for command line:
//   - Minimal reverse engineering of syspin.exe from https://www.technosys.net/products/utils/pintotaskbar
//   - With only "Pin to taskbar" functionality included, as I didnt need the others
//   - It does Unpin/Re-Pin however to overwrite shortcuts in Taskbar, but your program gets re-pinned in last position
//   - It works on my Windows 10 Pro, build 19041.685, locale en-US
//   - Syspin.exe was decompiled using Retargetable Decompiler from https://retdec.com
//   - Another helpful reverse engineering project of syspin.exe in C++, which is much more faithful to the source: https://github.com/airwolf2026/Win10Pin2TB

// Compiled with MSYS2/MinGW-w64:
//   $ gcc -o pttb pttb.c -Lmingw64/x86_64-w64-mingw32/lib -lole32 -loleaut32 -luuid -s -O3 -Wl,--gc-sections -nostartfiles --entry=pttb

// Usage:
//   > pttb PATH\TO\THE\PROGRAM\OR\SHORTCUT\TO\PIN\TO\TASKBAR

// Notes:
//   - 1st tried the registry method described here:
//     - https://stackoverflow.com/questions/31720595/pin-program-to-taskbar-using-ps-in-windows-10
//     - Doesn't work anymore
//   - Then tried the PEB method described here:
//     - https://alexweinberger.com/main/pinning-network-program-taskbar-programmatically-windows-10/
//     - Doesn't work anymore either
//   - So I ended up using the PE injection method used by syspin.exe from https://www.technosys.net
//     - Thanks Microsoft for making it a bit more difficult, I learned quite a bit with this little project

// #include <windows.h>
#include <Shldisp.h>
#include <stdint.h>
// #include <stdio.h>

// -------------------- Project Function Prototypes --------------------
static unsigned long __stdcall PinToTaskBar_func(char* pdata);											// "Pin to tas&kbar" Function to call once injected in "Progman"
void PinToTaskBar_core (char* pcFolder, char* pcFile, wchar_t* pwcPinToTaskBar, wchar_t* pwcUnPinFromTaskBar, IShellDispatch* pISD);	// Core Function of "PinToTaskBar_func"
void ExecuteVerb(wchar_t* pwcVerb, FolderItem* pFI);													// Execute Verb if found
void GetCommandLineArgvA(char* pCommandLine, char** aArgs);												// Get arguments from command line.. just a personal preference for char* instead of the wchar_t* type provided by "CommandLineToArgvW()"
void WriteToConsoleA(char* lpMsg);																		// "Write to Console A" function to save >20KB compared to printf and <stdio.h>
// void WriteIntToConsoleA(int num);																	// "Write Integer as Hex to Console A" function to save >20KB compared to printf and <stdio.h>
// void WriteToConsoleW(wchar_t* lpMsg);																// "Write to Console W" function to save >20KB compared to printf and <stdio.h>

// -------------------- C Function Prototypes --------------------
int access(const char* path, int mode);																	// https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/access-waccess?view=msvc-160
int sprintf(char* buffer, const char* format, ...);

// -------------------- Global Variables --------------------
void* __stdcall hdConsoleOut;

// -------------------- entry point function --------------------
void pttb() {
	hdConsoleOut = GetStdHandle(-11);
// Get arguments from command line
	char* pCommandLine = GetCommandLineA();
	const int nbArgs = 1;																				// number of expected arguments
	char* aArgs[nbArgs+2];																				// 1st "argument" isnt really one: it's this program path
	GetCommandLineArgvA(pCommandLine, aArgs);															// Get arguments from command line
// Check that an argument was passed
	if(!aArgs[1]) {
		WriteToConsoleA("ERROR: Argument missing\n");
		WriteToConsoleA("Usage: > pttb PATH\\TO\\THE\\PROGRAM\\OR\\SHORTCUT\\TO\\PIN\\TO\\TASKBAR\n");
		ExitProcess(0xA0); } // ERROR_BAD_ARGUMENTS	
// Check if 1st argument is a path to a program or shortcut that exists
	if(access(aArgs[1], 0) < 0 ) {
		WriteToConsoleA("ERROR: \""); WriteToConsoleA(aArgs[1]); WriteToConsoleA("\" not found\n");
		ExitProcess(0x2); } // ERROR_FILE_NOT_FOUND
// Get a Handle to the "Progman" process
	unsigned long dwProcessId;
	GetWindowThreadProcessId(FindWindowA("Progman", NULL), &dwProcessId);
	void* hdProcess = OpenProcess(42, 0, dwProcessId);
// Get relevant addresses to this current process, as well as the image size
	void*	hdModule	= GetModuleHandle(NULL);		
	int64_t	adrModule	= (int64_t)hdModule;
	int64_t	adrPE		= adrModule + *(int32_t*)(adrModule + 0x3C);									// 0x3C: IMAGE_DOS_HEADER -> e_lfanew // Gives offset to IMAGE_NT_HEADERS
	unsigned long szImage = *(unsigned long*)(adrPE + 0x50);											// 0x50: IMAGE_NT_HEADERS -> IMAGE_OPTIONAL_HEADER -> SizeOfImage // Gives the size of this current process in memory
	// WriteIntToConsoleA(szImage);
// Reserve a local region of memory equal to "szImage" and make a copy of itself into it
	void*	lpVirtAlloc		= VirtualAlloc(NULL, szImage, 0x3000, 64);
	int64_t	adrVirtAlloc	= (int64_t)lpVirtAlloc;
	memcpy(lpVirtAlloc, hdModule, szImage);
// Reserve a region of memory equal to "szImage + MAX_PATH" in the "Progman" process
	void* lpVirtAllocEx	= VirtualAllocEx(hdProcess, NULL, szImage + MAX_PATH, 0x3000, 64);
	int64_t adrVirtAllocEx	= (int64_t)lpVirtAllocEx;
// Check if any Virtual Address in the current process need to be relocated 
	int64_t vRelocVirtAdd	= (*(int32_t*)(adrPE + 180) != 0) ? *(int32_t*)(adrPE + 176) : 0;			// 176/180: IMAGE_NT_HEADERS -> IMAGE_OPTIONAL_HEADER -> IMAGE_DATA_DIRECTORY -> Base relocation table address/size
	int64_t pRelocVirtAdd	= adrVirtAlloc + vRelocVirtAdd;												// Address of the Relocation Table
	int64_t delta 			= adrVirtAllocEx - adrModule;												// Relocation Offset between the current process and the reserved memory region in the "Progman" process
// Relocate every block of virtual address
	while (vRelocVirtAdd != 0) {
		int32_t RelocBlockSize	= *(int32_t*)(pRelocVirtAdd + 4);										// Block size to relocate from Virtual Address (size of struct _IMAGE_BASE_RELOCATION included)
		vRelocVirtAdd		= adrVirtAlloc + *(int32_t*)pRelocVirtAdd;									// Virtual Address relocation offset according ImageBase
		pRelocVirtAdd	   += 8;																		// 8: size of struct _IMAGE_BASE_RELOCATION // jump to 1st Descriptor to relocate
		if (RelocBlockSize >= 8) {																		// Block size must be > size of struct _IMAGE_BASE_RELOCATION in order to have any Descriptor
			int32_t RelocNbDesc	= (RelocBlockSize - 8) / 2;												// number of descriptors in this block: RelocBlockSize in BYTE, but descriptors in int16_t
			for (int ct=0; ct<RelocNbDesc; ct++) {
				int16_t vRelocDescOffset = *(int16_t*)pRelocVirtAdd & 0x0FFF;							// Get descriptor offset of Virtual address to relocate
				if (vRelocDescOffset != 0) { *(int64_t*)(vRelocVirtAdd + vRelocDescOffset) += delta; }	// Add "delta" to the value at this address
				pRelocVirtAdd += 2; }}																	// Go to next descriptor
		vRelocVirtAdd = *(int32_t*)pRelocVirtAdd;}														// Get Virtual Address of next Block
// Remove wild breakpoint at beginning of main function -> still works fine without this line, probably because I dont use a main function
	// *(int8_t*)(adrVirtAlloc+adrPE-adrModule+0x28) = 0x55;											// 0x28: IMAGE_NT_HEADERS -> IMAGE_OPTIONAL_HEADER -> AddressOfEntryPoint; 0x55 -> push %rbp
// Inject local region of memory into "Progman" process region of memory
	WriteProcessMemory(hdProcess, lpVirtAllocEx, lpVirtAlloc, szImage, NULL);
	void* pCommandBaseAdd = (void*)(adrVirtAllocEx + szImage);
	WriteProcessMemory(hdProcess, pCommandBaseAdd, aArgs[1], MAX_PATH, NULL);							// Copy the path to the file to pin to taskbar, into the extra memory of size "MAX_PATH"
// Run the "PinToTaskBar_func" in the "Progman" process, with the path to the file to pin to taskbar as a parameter
	LPTHREAD_START_ROUTINE lpStartAddress = (LPTHREAD_START_ROUTINE)(delta + PinToTaskBar_func);
	void* hdThread = CreateRemoteThread(hdProcess, NULL, 0, lpStartAddress, pCommandBaseAdd, 0, NULL);
// Wait for the Thread to finish and clean it up
	WaitForSingleObject(hdThread, 5000);
	TerminateThread(hdThread, 0);
	CloseHandle(hdThread);
// Clean Up Everything
	VirtualFree(lpVirtAlloc, 0, 0x8000);
	VirtualFreeEx(hdProcess, lpVirtAllocEx, 0, 0x8000);
	CloseHandle(hdProcess);
	ExitProcess(0);
}

// -------------------- "Pin to tas&kbar" -------------------- Function to call once injected in "Progman"
static unsigned long __stdcall PinToTaskBar_func(char* lpdata) {
// Get directory and Filename from pdata
	char* lpDir = lpdata;
	char* lpFile = NULL;
	while (*lpdata) {
		if(*lpdata == '\\') lpFile = lpdata;
		lpdata++;}
	*lpFile = 0;
	lpFile += 1;
// Get "Pin to tas&kbar" and "Unpin from tas&kbar" Verbs in Windows locale
	wchar_t* pwcPinToTaskBar = L"Pin to tas&kbar";
	wchar_t* pwcUnPinFromTaskBar = L"Unpin from tas&kbar";
	void* hmLoadLibShell = LoadLibraryW(L"shell32.dll");
	LoadStringW(GetModuleHandleW(L"shell32.dll"), 5386, pwcPinToTaskBar, MAX_PATH);						// Should be "Pin to tas&kbar" in en-us locale versions of Windows
	LoadStringW(GetModuleHandleW(L"shell32.dll"), 5387, pwcUnPinFromTaskBar, MAX_PATH);					// Should be "Unpin from tas&kbar" in en-us locale versions of Windows
	FreeLibrary(hmLoadLibShell);
// Create COM Objects
	CoInitialize(NULL);
	IShellDispatch* pISD;
	CoCreateInstance(&CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, &IID_IShellDispatch, (void**)&pISD);
// Check if Shorcut is already pinned, and if so: unpin it directly from %AppData%\Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar\shorcut.lnk, because Windows föks up when Unpinning+Pinning shorcuts with same path/name.lnk, but whose target/arguments have been modified..
	char acTaskBarStorage[MAX_PATH] = {'\0'};
	sprintf(acTaskBarStorage, "%s\\Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar", getenv("AppData"));
	char acTaskBarShorCut[MAX_PATH] = {'\0'};
	sprintf(acTaskBarShorCut, "%s\\%s", acTaskBarStorage, lpFile);
	if (access(acTaskBarShorCut, 0) == 0) PinToTaskBar_core(acTaskBarStorage, lpFile, NULL, pwcUnPinFromTaskBar, pISD);
// Unpin Prog from taskbar if pinned and Pin Prog/ShorCut
	PinToTaskBar_core(lpDir, lpFile, pwcPinToTaskBar, pwcUnPinFromTaskBar, pISD);
// Clean Up
	pISD->lpVtbl->Release(pISD);
	CoUninitialize();
	// MessageBox(NULL, "Check the TaskBar", "Done", 0);
	return 0;
}

// -------------------- "Pin to tas&kbar" Core -------------------- Function
void PinToTaskBar_core (char* pcFolder, char* pcFile, wchar_t* pwcPinToTaskBar, wchar_t* pwcUnPinFromTaskBar, IShellDispatch* pISD) {
// Convert to wchar_t for Variant VT_BSTR type
	wchar_t awcFolder[MAX_PATH] = {'\0'};
	mbstowcs(awcFolder, pcFolder, MAX_PATH);
	wchar_t awcFileName[MAX_PATH] = {'\0'};
	mbstowcs(awcFileName, pcFile, MAX_PATH);
// Create a "Folder" Object of the directory containing the file to Pin/Unpin
	Folder *pFolder;
	VARIANTARG varTmp;
	VariantInit(&varTmp);
	varTmp.vt = VT_BSTR;
	varTmp.bstrVal = (BSTR)awcFolder;
	pISD->lpVtbl->NameSpace(pISD, varTmp, &pFolder);
// Create a "FolderItem" Object of the file to Pin/Unpin
	FolderItem* pFI;
	pFolder->lpVtbl->ParseName(pFolder, (BSTR)awcFileName, &pFI);
// Initialise the list of Verbs and search for "Unpin from tas&kbar". If found: execute it
	if(pwcUnPinFromTaskBar) ExecuteVerb(pwcUnPinFromTaskBar, pFI);
// Initialise the list of Verbs and search for "Pin to tas&kbar". If found: execute it
	if(pwcPinToTaskBar) ExecuteVerb(pwcPinToTaskBar, pFI);
// Clean Up
	pFI->lpVtbl->Release(pFI);
	pFolder->lpVtbl->Release(pFolder);
}

// -------------------- "Execute Verb" -------------------- Function
void ExecuteVerb(wchar_t* pwcVerb, FolderItem* pFI) {
	int lgtVerb = wcsnlen(pwcVerb, MAX_PATH);
	wchar_t* pwcTmp = pwcVerb;
// Create a "FolderItemVerbs" Object of the Verbs corresponding to the file, including "Pin to tas&kbar" or "Unpin from tas&kbar"
	FolderItemVerbs* pFIVs;
	pFI->lpVtbl->Verbs(pFI, &pFIVs);
// Get the number of Verbs corresponding to the file to Pin/Unpin
	LONG nbVerbs;
	pFIVs->lpVtbl->get_Count(pFIVs, &nbVerbs);
// Create a "FolderItemVerb" Object to go through the list of Verbs until pwcVerb is found, and if found: execute it
	FolderItemVerb* pFIV;
	BSTR pFIVName;
	VARIANTARG varTmp;
	VariantInit(&varTmp);
	varTmp.vt = VT_I4;
	for (int ct = 0; ct < nbVerbs; ct++) {
		varTmp.lVal = ct;
		pFIVs->lpVtbl->Item(pFIVs, varTmp, &pFIV);
		pFIV->lpVtbl->get_Name(pFIV, &pFIVName);
		if (wcsnlen(pFIVName, MAX_PATH) == lgtVerb) {
			while (*pwcTmp && *pwcTmp == *pFIVName) { pwcTmp++; pFIVName++; }
			if (!*pwcTmp && !*pFIVName) { pFIV->lpVtbl->DoIt(pFIV); break; }
			pwcTmp = pwcVerb; }}
// Clean Up
	pFIV->lpVtbl->Release(pFIV);
	pFIVs->lpVtbl->Release(pFIVs);	
}

// -------------------- Get arguments from command line A -------------------- function.. just a personal preference for char* instead of the wchar_t* type provided by "CommandLineToArgvW()"
void GetCommandLineArgvA(char* pCommandLine, char** aArgs) {
	while (*pCommandLine) {
		while (*pCommandLine && *pCommandLine == ' ') pCommandLine++;									// Trim white-spaces before the argument
		char cEnd = ' ';																				// end of argument is defined as white-space..
		if (*pCommandLine == '\"') { pCommandLine++; cEnd = '\"'; }										// ..or as a double quote if argument is between double quotes
		*aArgs = pCommandLine;																			// Save argument pointer
		while (*pCommandLine && *pCommandLine != cEnd) pCommandLine++;
		if (*pCommandLine) *pCommandLine = 0;															// Set NULL separator between arguments
		pCommandLine++;
		aArgs++; }
	*aArgs = 0;
}

// -------------------- "Write to Console A" -------------------- function to save >20KB compared to printf and <stdio.h>
void WriteToConsoleA(char* lpMsg) {
	WriteConsoleA(hdConsoleOut, lpMsg, strlen(lpMsg), NULL, NULL);
}

// -------------------- "Write Integer as Hex to Console A" -------------------- function to save >20KB compared to printf and <stdio.h>
// void WriteIntToConsoleA(int num) {
	// char ahex[19] = {'\0'};
	// char* phex = &ahex[17];
	// *phex = '\n';
	// while(num != 0) {
		// phex--;
		// int var = num % 16;
		// if( var < 10 ) *phex = var + 48;
		// else *phex = var + 55;
		// num = num / 16;}
	// phex--; *phex = 'x';
	// phex--; *phex = '0';
	// WriteConsoleA(hdConsoleOut, phex, strlen(phex), NULL, NULL);
// }

// -------------------- "Write to Console W" -------------------- function to save >20KB compared to printf and <stdio.h>
// void WriteToConsoleW(wchar_t* lpMsg) {
	// WriteConsoleW(hdConsoleOut, lpMsg, wcslen(lpMsg), NULL, NULL);
// }
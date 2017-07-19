#include <nan.h>
#include <stdio.h>
#include <ShlObj.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <locale>
#include <codecvt>
#include <chrono>
#include <thread>
#include <initguid.h>

// Check windows
#if _WIN32 || _WIN64
#if _WIN64
#define ENVIRONMENT64
#else
#define ENVIRONMENT32
#endif
#endif

// This is needed for SHGetKnownFolderPath to work as it supports only Windows Vista and up.
// Not sure what this does actually - are we faking that we are at least in Windows Vista?
#if (_WIN32_WINNT < 0x0600)
	#define _WIN32_WINNT _WIN32_WINNT_VISTA
#endif

// Needed for FileExists
#ifdef _WIN32
	#include <io.h> 
	#define access    _access_s
#else
	#include <unistd.h>
#endif

// Type definition of KNOWNFOLDERID ->
// Borrowed from ShlObj.h - for some reason compiler errors when this is not present.
typedef GUID KNOWNFOLDERID;
#if 0
typedef KNOWNFOLDERID *REFKNOWNFOLDERID;
#endif // 0

#ifdef __cplusplus

#define REFKNOWNFOLDERID const KNOWNFOLDERID &
#else // !__cplusplus
#define REFKNOWNFOLDERID const KNOWNFOLDERID * __MIDL_CONST
#endif // __cplusplus

#define WM_COMMAND                      0x0111

// For some reason SHGetKnownFolderPath is not found if not declared here, even though it is declared in one of ShlObj.h imports...

STDAPI SHGetKnownFolderPath(_In_ REFKNOWNFOLDERID rfid,
	_In_ DWORD /* KNOWN_FOLDER_FLAG */ dwFlags,
	_In_opt_ HANDLE hToken,
	_Outptr_ PWSTR *ppszPath); // free *ppszPath with CoTaskMemFree

// 4ce576fa-83dc-4F88-951c-9d0782b4e376
DEFINE_GUID(CLSID_UIHostNoLaunch,
	0x4CE576FA, 0x83DC, 0x4f88, 0x95, 0x1C, 0x9D, 0x07, 0x82, 0xB4, 0xE3, 0x76);

// 37c994e7_432b_4834_a2f7_dce1f13b834b
DEFINE_GUID(IID_ITipInvocation,
	0x37c994e7, 0x432b, 0x4834, 0xa2, 0xf7, 0xdc, 0xe1, 0xf1, 0x3b, 0x83, 0x4b);



using namespace std;



// Checks if file exists.
bool FileExists(const std::string &Filename)
{
	return access(Filename.c_str(), 0) == 0;
}

struct ITipInvocation : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Toggle(HWND wnd) = 0;
};


wstring ReadTabTipPathFromReg()
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("SOFTWARE\\Classes\\CLSID\\{054AAE20-4BEA-4347-8A35-64A533254A9D}\\LocalServer32"), 0, KEY_READ, &hKey) != ERROR_SUCCESS)
		throw "Could not open registry key";

	DWORD type;
	DWORD cbData;
	if (RegQueryValueEx(hKey, NULL, NULL, &type, NULL, &cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		throw "Could not read registry value";
	}

	if (type != REG_SZ && type != REG_EXPAND_SZ)
	{
		RegCloseKey(hKey);
		throw "Incorrect registry value type";
	}

	wstring value(cbData / sizeof(wchar_t), L'\0');
	if (RegQueryValueEx(hKey, NULL, NULL, NULL, reinterpret_cast<LPBYTE>(&value[0]), &cbData) != ERROR_SUCCESS)
	{
		RegCloseKey(hKey);
		throw "Could not read registry value";
	}

	RegCloseKey(hKey);

	size_t firstNull = value.find_first_of(L'\0');
	if (firstNull != string::npos)
		value.resize(firstNull);
	return value;
}

string ws2s(const std::wstring& wstr)
{
	using convert_typeX = std::codecvt_utf8<wchar_t>;
	std::wstring_convert<convert_typeX, wchar_t> converterX;

	return converterX.to_bytes(wstr);
}

void ShowByCom() {
	HRESULT hr;
	hr = CoInitialize(0);

	ITipInvocation *tip;
	hr = CoCreateInstance(CLSID_UIHostNoLaunch, 0, CLSCTX_INPROC_HANDLER | CLSCTX_LOCAL_SERVER, IID_ITipInvocation,
		(void **)&tip);
	HWND desktop = GetDesktopWindow();
	if (tip) {
		tip->Toggle(desktop);
		tip->Release();
	}
	else {
		HWND handle = FindWindow("IPTIP_Main_Window", "");
		// 10023 float mode
		WPARAM param = 10023;
		bool x = PostMessage(handle, WM_COMMAND, param, 0);
	}
}

NAN_METHOD(TabTipStartComServer) {
	v8::Local<v8::Object> obj = Nan::New<v8::Object>();
	string tabTipPathString;

	wstring tabTipPath;
	try {
		tabTipPath = ReadTabTipPathFromReg();
		Nan::Set(obj,
			Nan::New("tabTipPathFromRegistry").ToLocalChecked(),
			Nan::True()
		);
		string tabTipPathStringTmp((const char*)&tabTipPath[0], sizeof(wchar_t) / sizeof(char)*tabTipPath.size());
		tabTipPathString = tabTipPathStringTmp;
	}
	catch (const char* error) {
		Nan::Set(obj,
			Nan::New("tabTipPathFromRegistry").ToLocalChecked(),
			Nan::False()
		);

		Nan::Set(obj,
			Nan::New("registryError").ToLocalChecked(),
			Nan::New(error).ToLocalChecked()
		);
		tabTipPathString = "%CommonProgramFiles%\\microsoft shared\\ink\\TabTip.exe";
	}


	// Cut out the " from the beginning and end if present.
	char prnth = '"';
	if ((int)tabTipPathString.length() > 0) {
		if (tabTipPathString[0] == prnth) {
			tabTipPathString = tabTipPathString.substr(1);
		}
		if (tabTipPathString[tabTipPathString.length() - 1] == prnth) {
			tabTipPathString = tabTipPathString.substr(0, tabTipPathString.length() - 1);
		}
	}

	Nan::Set(obj,
		Nan::New("tabTipPath").ToLocalChecked(),
		Nan::New(tabTipPathString).ToLocalChecked()
	);

	OutputDebugString("\n");
	OutputDebugString("TabTIPReg:");
	OutputDebugString(tabTipPathString.c_str());


	string pathInLowerCase;
	pathInLowerCase.resize(tabTipPathString.size());

	std::transform(tabTipPathString.begin(), tabTipPathString.end(), pathInLowerCase.begin(), ::tolower);
	std::size_t found = pathInLowerCase.find("%commonprogramfiles%");

	string finalTabTipPath = "";

	if (found != string::npos) {
		// Strip the %commonprogramfiles%
		string pathWithoutCommonProgramFiles = tabTipPathString.substr(20);

		OutputDebugString("\n");
		OutputDebugString("pathWithoutCommonProgramFiles:\n");
		OutputDebugString(pathWithoutCommonProgramFiles.c_str());
		OutputDebugString("\n");

		// If CommonProgramW6432 env var is available use that.
		DWORD bufferSize =
			GetEnvironmentVariable("CommonProgramW6432", nullptr, 0);

		if (bufferSize) {
			string commonProgramPath;
			commonProgramPath.resize(bufferSize);

			GetEnvironmentVariable(
				"CommonProgramW6432",
				&commonProgramPath[0],
				bufferSize);

			if (commonProgramPath[commonProgramPath.length() - 1] == '\0') {
				commonProgramPath = commonProgramPath.substr(0, commonProgramPath.length() - 1);
			}

			Nan::Set(obj,
				Nan::New("commonProgramFilesPath").ToLocalChecked(),
				Nan::New(commonProgramPath).ToLocalChecked()
			);

			Nan::Set(obj,
				Nan::New("commonProgramFilesPathSource").ToLocalChecked(),
				Nan::New("CommonProgramW6432").ToLocalChecked()
			);

			finalTabTipPath.append(commonProgramPath);
			finalTabTipPath.append(pathWithoutCommonProgramFiles);
		}
		else {
			wchar_t* commonProgramFilesPath;
			HRESULT hr = SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL, &commonProgramFilesPath);

			if (hr != S_OK) {
				finalTabTipPath = "C:\\Program Files\\microsoft shared\\ink\\TabTip.exe";

				Nan::Set(obj,
					Nan::New("commonProgramFilesPath").ToLocalChecked(),
					Nan::New(finalTabTipPath).ToLocalChecked()
				);

				Nan::Set(obj,
					Nan::New("commonProgramFilesPathSource").ToLocalChecked(),
					Nan::New("default").ToLocalChecked()
				);
			}
			else {
				wstring commonProgramFilesPathWString(commonProgramFilesPath);
				string commonProgramFilesPathString = ws2s(commonProgramFilesPathWString);

				Nan::Set(obj,
					Nan::New("commonProgramFilesPath").ToLocalChecked(),
					Nan::New(commonProgramFilesPathString).ToLocalChecked()
				);

				Nan::Set(obj,
					Nan::New("commonProgramFilesPathSource").ToLocalChecked(),
					Nan::New("SHGetKnownFolderPath").ToLocalChecked()
				);

				finalTabTipPath.append(commonProgramFilesPathString);
				finalTabTipPath.append(pathWithoutCommonProgramFiles);
			}
		}
		OutputDebugString("\n");
		OutputDebugString(finalTabTipPath.c_str());
		OutputDebugString("\n");

	}


	if (FileExists(finalTabTipPath)) {
		ShellExecute(NULL, "open", finalTabTipPath.c_str(), NULL, NULL, SW_SHOWDEFAULT);

		HWND handle = FindWindow("IPTIP_Main_Window", "");
		int retries = 0;
		int maxRetries = 100; // 10 sec
		while (handle && retries != maxRetries)
		{
			std::this_thread::sleep_for(100ms);
			handle = FindWindow("IPTIP_Main_Window", "");
			retries++;
		}

		if (retries == maxRetries) {
			OutputDebugString("timeout");
			Nan::Set(obj,
				Nan::New("result").ToLocalChecked(),
				Nan::False()
			);
			Nan::Set(obj,
				Nan::New("error").ToLocalChecked(),
				Nan::New("Timeout while waiting for TabTip to start.").ToLocalChecked()
			);
		}
		else {
			OutputDebugString("tab tip run");
			Nan::Set(obj,
				Nan::New("result").ToLocalChecked(),
				Nan::True()
			);
		}
	}
	else {
		OutputDebugString("tab tip does not exists");
		Nan::Set(obj,
			Nan::New("result").ToLocalChecked(),
			Nan::False()	
		);
		Nan::Set(obj,
			Nan::New("error").ToLocalChecked(),
			Nan::New("TabTip executable was not found.").ToLocalChecked()
		);

	}
	info.GetReturnValue().Set(obj);
}


NAN_METHOD(TabTipShowThroughCom) {
	ShowByCom();
}

NAN_METHOD(TabTipIsVisible) {
	HWND handle;
	handle = FindWindow("IPTIP_Main_Window", "");
	v8::Local<v8::Boolean> result;
	if (handle == NULL) {
		info.GetReturnValue().Set(Nan::Null());
	}
	else {
		long style = GetWindowLong(handle, GWL_STYLE);
		if ((style & WS_DISABLED) != 0)
		{
			result = Nan::False();
		}
		else {
			if (IsWindowVisible(handle)) {
				result = Nan::True();
			}
			else {
				result = Nan::False();
			}
		}
		info.GetReturnValue().Set(result);
	}
}


NAN_METHOD(TabTipMoveWindow) {
	HWND handle = FindWindow("IPTIP_Main_Window", "");

	RECT rect = { NULL };
	bool res1 = IsWindow(handle);
	bool res2 = IsWindowEnabled(handle);

	if (GetWindowRect(handle, &rect)) {
		OutputDebugString("\n");
		OutputDebugString(to_string(rect.left).c_str());
		OutputDebugString(to_string(rect.top).c_str());
		OutputDebugString(to_string(rect.bottom).c_str());
		OutputDebugString(to_string(rect.right).c_str());
		OutputDebugString("\n");

		while (rect.left == 0 && rect.right == 0)
		{
			std::this_thread::sleep_for(100ms);
			GetWindowRect(handle, &rect);
		}
		OutputDebugString("\n");
		OutputDebugString(to_string(rect.left).c_str());
		OutputDebugString(to_string(rect.top).c_str());
		OutputDebugString(to_string(rect.bottom).c_str());
		OutputDebugString(to_string(rect.right).c_str());
		OutputDebugString("\n");

		bool res = MoveWindow(handle, 10, 10, rect.right - rect.left, rect.bottom - rect.top, TRUE);
		res = SetWindowPos(handle, nullptr, 10, 10, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		
		std::this_thread::sleep_for(500ms);
		res = MoveWindow(handle, 100, 100, rect.right - rect.left, rect.bottom - rect.top, TRUE);
		res = SetWindowPos(handle, nullptr, 100, 100, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
}

NAN_METHOD(TabTipIsComServerRunning) {
	HWND handle;
	handle = FindWindow("IPTIP_Main_Window", "");
	v8::Local<v8::Boolean> result;
	
	if (handle == NULL) {
		result = Nan::False();
	}
	else {
		result = Nan::True();
	}
	info.GetReturnValue().Set(result);
}

typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);

LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL IsWow64()
{
	BOOL bIsWow64 = FALSE;

	//IsWow64Process is not available on all supported versions of Windows.
	//Use GetModuleHandle to get a handle to the DLL that contains the function
	//and GetProcAddress to get a pointer to the function if available.

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(
		GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (NULL != fnIsWow64Process)
	{
		if (!fnIsWow64Process(GetCurrentProcess(), &bIsWow64))
		{
			//handle error
		}
	}
	return bIsWow64;
}

NAN_METHOD(OSKShow) {
	
	PVOID OldValue = NULL;
	
	bool set = false;
	if (IsWow64())
	{
		 set = Wow64DisableWow64FsRedirection(&OldValue);
	}
	ShellExecute(NULL, "open", "osk.exe", NULL, NULL, SW_SHOWDEFAULT);
	
	if (set) {
		Wow64RevertWow64FsRedirection(OldValue);
	}

	HWND handle = FindWindow("OSKMainClass", NULL);
	
	int retries = 0;
	int maxRetries = 100; // 10 sec
	while (handle && retries != maxRetries)
	{
		std::this_thread::sleep_for(100ms);
		handle = FindWindow("OSKMainClass", NULL);
		retries++;
	}
	if (retries == maxRetries) {
		info.GetReturnValue().Set(Nan::False());
	}
	else {
		info.GetReturnValue().Set(Nan::True());
	}
}

NAN_METHOD(OSKClose) {
	HWND handle = FindWindow("OSKMainClass", NULL);
	if (handle) {
		PostMessage(handle, WM_SYSCOMMAND, SC_CLOSE, 0);
	}
	info.GetReturnValue().Set(Nan::True());
}

void setRegistryValue(LPCSTR key, DWORD value) {
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("SOFTWARE\\Microsoft\\Osk"), 0, KEY_WRITE, &hKey) != ERROR_SUCCESS)
		throw "Could not open registry key";
	if (RegSetValueEx(hKey, TEXT(key), 0, REG_DWORD, (const BYTE*)&value, sizeof(value)) != ERROR_SUCCESS)
		throw "Could not save registry key";
	RegCloseKey(hKey);
}

NAN_METHOD(GetSystemTopBarThickness) {
    int borderThickness = GetSystemMetrics(SM_CYCAPTION);
	info.GetReturnValue().Set(borderThickness);
}

NAN_METHOD(GetSystemBorderThickness) {
	int frameSize = GetSystemMetrics(SM_CXSIZEFRAME);
	info.GetReturnValue().Set(frameSize);
}

NAN_METHOD(GetSystemMenuHeight) {
	int menuHeight = GetSystemMetrics(SM_CYMENU);
	info.GetReturnValue().Set(menuHeight);
}

NAN_METHOD(OSKSetPosition) {
	Nan::HandleScope scope;
	try {
		if (!info[0]->IsUndefined()) {
			unsigned int left = info[0]->Uint32Value();
			setRegistryValue("WindowLeft", left);
		}
		if (!info[1]->IsUndefined()) {
			unsigned int top = info[1]->Uint32Value();
			setRegistryValue("WindowTop", top);
		}
		if (!info[2]->IsUndefined()) {
			unsigned int width = info[2]->Uint32Value();
			setRegistryValue("WindowWidth", width);
		}
		if (!info[3]->IsUndefined()) {
			unsigned int height = info[3]->Uint32Value();
			setRegistryValue("WindowHeight", height);
		}
		info.GetReturnValue().Set(Nan::True());
	}
	catch (...) {
		info.GetReturnValue().Set(Nan::False());
	}
}

NAN_METHOD(OSKGetPosition) {
	HWND handle = FindWindow("OSKMainClass", NULL);
	v8::Local<v8::Object> obj = Nan::New<v8::Object>();
	RECT rect = { NULL };
	if (GetWindowRect(handle, &rect)) {
		Nan::Set(obj,
			Nan::New("x").ToLocalChecked(),
			Nan::New(rect.left)
		);
		Nan::Set(obj,
			Nan::New("y").ToLocalChecked(),
			Nan::New(rect.top)
		);
		Nan::Set(obj,
			Nan::New("width").ToLocalChecked(),
			Nan::New(rect.right - rect.left)
		);
		Nan::Set(obj,
			Nan::New("height").ToLocalChecked(),
			Nan::New(rect.bottom - rect.top)
		);
		Nan::Set(obj,
			Nan::New("result").ToLocalChecked(),
			Nan::True()
		);
	}
	else {
		Nan::Set(obj,
			Nan::New("result").ToLocalChecked(),
			Nan::False()
		);
	}
	info.GetReturnValue().Set(obj);
}

NAN_METHOD(OSKIsVisible) {
	HWND handle;
	handle = FindWindow("OSKMainClass", NULL);
	v8::Local<v8::Boolean> result;
	if (handle == NULL) {
		info.GetReturnValue().Set(Nan::Null());
	}
	else {
		long style = GetWindowLong(handle, GWL_STYLE);
		if ((style & WS_DISABLED) != 0)
		{
			result = Nan::False();
		}
		else {
			if (IsWindowVisible(handle)) {
				result = Nan::True();
			}
			else {
				result = Nan::False();
			}
		}
		info.GetReturnValue().Set(result);
	}
}

// Module initialization logic
NAN_MODULE_INIT(Initialize) {
    // Export the `Hello` function (equivalent to `export function Hello (...)` in JS)
	
	NAN_EXPORT(target, TabTipIsComServerRunning);
	NAN_EXPORT(target, TabTipIsVisible);
	NAN_EXPORT(target, TabTipShowThroughCom);
	NAN_EXPORT(target, TabTipStartComServer);
	NAN_EXPORT(target, TabTipMoveWindow);
	NAN_EXPORT(target, OSKShow);
	NAN_EXPORT(target, OSKClose);
	NAN_EXPORT(target, OSKSetPosition);
	NAN_EXPORT(target, OSKIsVisible);
	NAN_EXPORT(target, GetSystemTopBarThickness);
	NAN_EXPORT(target, GetSystemBorderThickness);
	NAN_EXPORT(target, GetSystemMenuHeight);
}

// Create the module called "addon" and initialize it with `Initialize` function (created with NAN_MODULE_INIT macro)
NODE_MODULE(addon, Initialize);

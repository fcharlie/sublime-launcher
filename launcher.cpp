/**
* Sublime Text launcher
* Copyright (C) 2016, ForceStudio All Rights Reserved.
**/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <Windows.h>
#include <Shlwapi.h>
#include <string>
#include <vector>
#include <iostream>
#define  SECURITY_WIN32
#include <Security.h>
#include <StrSafe.h>

#if defined(_MSC_VER)
#if defined(_WIN32_WINNT) &&_WIN32_WINNT>=_WIN32_WINNT_WIN8
#include <Processthreadsapi.h>
#endif
#endif

#include "cpptoml.h"

struct SublimeStartupStructure {
	std::wstring executeFile;
	std::wstring pwd;
	std::vector<std::wstring> appendPath;
	bool clearEnvironment;
};

class Characters {
private:
	char *str;
public:
	Characters(const wchar_t *wstr) :str(nullptr)
	{
		if (wstr == nullptr) return;
		int iTextLen = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
		str = new char[iTextLen + 1];
		if (str == nullptr) return;
		str[iTextLen] = 0;
		WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, iTextLen, NULL, NULL);
	}
	const char *Get()
	{
		if (!str)
			return nullptr;
		return const_cast<const char *>(str);
	}
	~Characters()
	{
		if (str)
			delete[] str;
	}
};



class WCharacters {
private:
	wchar_t *wstr;
public:
	WCharacters(const char *str) :wstr(nullptr)
	{
		if (str == nullptr)
			return;
		int unicodeLen = ::MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
		if (unicodeLen == 0)
			return;
		wstr = new wchar_t[unicodeLen + 1];
		if (wstr == nullptr) return;
		wstr[unicodeLen] = 0;
		::MultiByteToWideChar(CP_UTF8, 0, str, -1, (LPWSTR)wstr, unicodeLen);
	}
	const wchar_t *Get()
	{
		if (!wstr)
			return nullptr;
		return const_cast<const wchar_t *>(wstr);
	}
	~WCharacters()
	{
		if (wstr)
			delete[] wstr;
	}
};

bool GetProcessImageFileFolder(std::wstring &wstr)
{
	wchar_t szbuf[32767];
	if (!GetModuleFileNameW(nullptr, szbuf, 32767))
		return false;
	PathRemoveFileSpecW(szbuf);
	wstr = szbuf;
	return true;
}


bool SublimeStartupProfile(const wchar_t *cf, SublimeStartupStructure &config)
{
	std::wstring pfile;
	if (cf == nullptr) {
		if (PathFileExistsW(L"launcher.toml")) {
			pfile = L"launcher.toml";
		}
		else if (PathFileExistsW(L"launcher.exe.toml")) {
			pfile = L"launcher.exe.toml";
		}
		else {
			std::wstring folder;
			if (!GetProcessImageFileFolder(folder)) {
				MessageBoxW(nullptr,
					L"GetModuleFileNameW Failed!",
					L"Internal System Error",
					MB_OK | MB_ICONERROR);
				return false;
			}
			pfile = folder + L"/launcher.toml";
			if (!PathFileExistsW(pfile.c_str())) {
				pfile = folder + L"/launcher.exe.toml";
				if (!PathFileExistsW(pfile.c_str())) {
					MessageBoxW(nullptr,
						folder.c_str(),
						L"Cannot open launcher.toml or launcher.exe.toml on this path",
						MB_OK | MB_ICONERROR);
					return false;
				}
			}
		}
	}
	else {
		if (PathFileExistsW(cf)) {
			pfile = cf;
		}
		else {
			return false;
		}
	}
	/////
	Characters chars(pfile.c_str());
	std::shared_ptr<cpptoml::table> g;
	try {
		g = cpptoml::parse_file(chars.Get());
		std::cout << (*g) << std::endl;
	}
	catch (const cpptoml::parse_exception &e) {
		(void)e;
		MessageBoxW(nullptr,
			pfile.c_str(),
			L"can't parser toml-format profile!",
			MB_OK | MB_ICONERROR);
		return false;
	}
	auto Strings = [&](const char *key, const wchar_t *v, std::wstring &sv) {
		if (g->contains_qualified(key)) {
			std::string astr = g->get_qualified(key)->as<std::string>()->get();
			WCharacters was(astr.c_str());
			sv = was.Get();
			return;
		}
		if (v) {
			sv = v;
		}
	};
	auto Boolean = [&](const char *key, bool b) {
		if (g->contains_qualified(key)) {
			return g->get_qualified(key)->as<bool>()->get();
		}
		return b;
	};
	auto Vector = [&](const char *key, std::vector<std::wstring> &v) {
		if (g->contains_qualified(key) && g->get_qualified(key)->is_array()) {
			auto av = g->get_qualified(key)->as_array();
			for (auto &e : av->get()) {
				WCharacters was(e->as<std::string>()->get().c_str());
				v.push_back(was.Get());
			}
		}
	};
	Strings("Sublime.ExecuteFile", L"sublime_text.exe", config.executeFile);
	Strings("Sublime.Pwd", L"", config.pwd);
	Vector("Sublime.AppendPath", config.appendPath);
	config.clearEnvironment = Boolean("Sublime.UseClearEnv", false);
	return true;
}

enum KEnvStateMachine : int {
	kClearReset = 0,
	kEscapeAllow = 1,
	kMarkAllow = 2,
	kBlockBegin = 3,
	kBlockEnd = 4
};

static bool ResolveEnvironment(const std::wstring &k, std::wstring &v) {
	wchar_t buffer[32767];
	auto dwSize = GetEnvironmentVariableW(k.c_str(), buffer, 32767);
	if (dwSize == 0 || dwSize >= 32767)
		return false;
	buffer[dwSize] = 0;
	v = buffer;
	return true;
}

static bool EnvironmentExpend(std::wstring &va) {
	if (va.empty())
		return false;
	std::wstring ns, ks;
	auto p = va.c_str();
	auto n = va.size();
	int pre = 0;
	size_t i = 0;
	KEnvStateMachine state = kClearReset;
	for (; i < n; i++) {
		switch (p[i]) {
		case '`': {
			switch (state) {
			case kClearReset:
				state = kEscapeAllow;
				break;
			case kEscapeAllow:
				ns.push_back('`');
				state = kClearReset;
				break;
			case kMarkAllow:
				state = kEscapeAllow;
				ns.push_back('$');
				break;
			case kBlockBegin:
				continue;
			default:
				ns.push_back('`');
				continue;
			}
		} break;
		case '$': {
			switch (state) {
			case kClearReset:
				state = kMarkAllow;
				break;
			case kEscapeAllow:
				ns.push_back('$');
				state = kClearReset;
				break;
			case kMarkAllow:
				ns.push_back('$');
				state = kClearReset;
				break;
			case kBlockBegin:
			case kBlockEnd:
			default:
				ns.push_back('$');
				continue;
			}
		} break;
		case '{': {
			switch (state) {
			case kClearReset:
			case kEscapeAllow:
				ns.push_back('{');
				state = kClearReset;
				break;
			case kMarkAllow: {
				state = kBlockBegin;
				pre = i;
			} break;
			case kBlockBegin:
				ns.push_back('{');
				break;
			default:
				continue;
			}
		} break;
		case '}': {
			switch (state) {
			case kClearReset:
			case kEscapeAllow:
				ns.push_back('}');
				state = kClearReset;
				break;
			case kMarkAllow:
				state = kClearReset;
				ns.push_back('$');
				ns.push_back('}');
				break;
			case kBlockBegin: {
				ks.assign(&p[pre + 1], i - pre - 1);
				std::wstring v;
				if (ResolveEnvironment(ks, v))
					ns.append(v);
				state = kClearReset;
			} break;
			default:
				continue;
			}
		} break;
		default: {
			switch (state) {
			case kClearReset:
				ns.push_back(p[i]);
				break;
			case kEscapeAllow:
				ns.push_back('`');
				ns.push_back(p[i]);
				state = kClearReset;
				break;
			case kMarkAllow:
				ns.push_back('$');
				ns.push_back(p[i]);
				state = kClearReset;
				break;
			case kBlockBegin:
			default:
				continue;
			}
		} break;
		}
	}
	va = ns;
	return true;
}

bool ClearEnvironmentVariableW() {
	//WINDIR expend like C:\WINDOW
	std::wstring path = L"${WINDIR}\\System32;${WINDIR};${WINDIR}\\System32\\Wbem;${WINDIR}\\System32\\WindowsPowerShell\\v1.0\\";
	if (EnvironmentExpend(path)) {
		SetEnvironmentVariableW(L"PATH", path.c_str());
		return true;
	}
	return false;
}


bool PutEnvironmentVariableW(const wchar_t *name, const wchar_t *va)
{
	wchar_t buffer[32767];
	auto dwSize = GetEnvironmentVariableW(name, buffer, 32767);
	if (dwSize <= 0) {
		return SetEnvironmentVariableW(name, va) ? true : false;
	}
	if (dwSize >= 32766) return false;
	if (buffer[dwSize - 1] != ';') {
		buffer[dwSize] = ';';
		dwSize++;
		buffer[dwSize] = 0;
	}
	StringCchCatW(buffer, 32767 - dwSize, va);
	return SetEnvironmentVariableW(name, buffer) ? true : false;
}

bool PutPathEnvironmentVariableW(std::vector<std::wstring> &pv) {
	std::wstring path(L";");
	for (auto &s : pv) {
		path.append(s);
		path.push_back(';');
	}
	return PutEnvironmentVariableW(L"PATH", path.c_str());
}


bool StartupMiniPosixEnv(SublimeStartupStructure &config, const std::wstring &commandline)
{
	SetCurrentDirectoryW(config.pwd.c_str());
	if (config.clearEnvironment) {
		ClearEnvironmentVariableW();
	}

	if (!config.appendPath.empty()) {
		PutPathEnvironmentVariableW(config.appendPath);
	}

	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_SHOW;
	LPCWSTR pszExecuteFile = nullptr;
	int const ArraySize = 32767;
	wchar_t cmdline[ArraySize] = { 0 };
	HRESULT hr = S_OK;
	if (PathIsRelativeW(config.executeFile.c_str())) {
		hr = StringCchCatW(cmdline, ArraySize, config.executeFile.c_str());
	}
	else {
		pszExecuteFile = config.executeFile.c_str();
	}
	if (!commandline.empty()) {
		hr = StringCchCatW(cmdline, ArraySize, commandline.c_str());
	}

	BOOL result = CreateProcessW(
		pszExecuteFile,
		cmdline,
		NULL,
		NULL,
		TRUE,
		CREATE_NEW_CONSOLE,
		NULL,
		NULL,
		&si,
		&pi
		);
	if (result) {
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}
	return true;
}

int WINAPI wWinMain(
	HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPWSTR lpCmdLine,
	int nCmdShow)
{
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;
	SublimeStartupStructure config;
	if (!SublimeStartupProfile(nullptr, config)) {
		return -1;
	}
	int Argc;
	LPCWSTR cmdline = GetCommandLineW();
	std::wstring cmd(cmdline);
	std::wstring rcmd;
	LPWSTR *Argv = CommandLineToArgvW(cmdline, &Argc);
	if (Argc >= 2) {
		auto np = cmd.find(Argv[0]);
		if (np == 0) {
			rcmd = cmd.substr(wcslen(Argv[0]) + np);
		}
		else if (np == 1) {
			rcmd = cmd.substr(wcslen(Argv[1] + np +2));
		}
	}
	LocalFree(Argv);
	return StartupMiniPosixEnv(config, cmd) ? 0 : 2;
}

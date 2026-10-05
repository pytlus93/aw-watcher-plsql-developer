#pragma once
#include "core.h"
#include <windows.h>
activity::Settings loadSettings();
activity::Settings defaultSettings();
void saveSettings(const activity::Settings& settings);
bool showSettings(HWND owner, activity::Settings& settings);
std::wstring wide(const std::string& value);
std::string utf8(const std::wstring& value);
std::string sdkText(const char* value);
extern HINSTANCE moduleInstance;

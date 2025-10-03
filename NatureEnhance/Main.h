#pragma once
#include "Gmapi.h"
#include "GameMakerMath.h"
#include "d3dx8.h"
#include "dxerr8.h"
#include <variant>
#include <string>

typedef double GMReal;
typedef const char* GMString;

#define expReal extern "C" __declspec(dllexport) GMReal _cdecl
#define expString extern "C" __declspec(dllexport) GMString _cdecl
#define innerCall void __fastcall

#define fnReal GMReal _cdecl
#define fnString GMString _cdecl

#define finish return 1.0
#define fail return 0.0
#define reterror return gm::noone

constexpr GMReal ds_type_map = 0;
constexpr GMReal ds_type_list = 1;
constexpr GMReal ds_type_stack = 2;
constexpr GMReal ds_type_grid = 3;
constexpr GMReal ds_type_queue = 4;
constexpr GMReal ds_type_priority = 5;

gm::CGMVariable GetResource(GMString res);
gm::CGMVariable GetResource(std::string res);
GMString ChangeCoding(GMString str, GMString inputCoding, GMString outputCoding);
GMString string_to_cstr(const std::string& str);
GMString STRCPY(GMString str);
void D3DCheck(HRESULT result, int pos);

extern bool show_error;
extern HWND GMWindowsHandle;
extern gm::CGMAPI* gmapi;
extern GMReal PropertyMap, NameMap;
extern IDirect3DDevice8* Device;

typedef std::variant<std::string, GMReal> dynamic;

#define simplecatch(funcname, returns) \
	catch (const wchar_t* e) \
	{ \
		if (show_error) \
		{ \
			std::wstring err = L"在执行函数 " + std::wstring(funcname) + L" 时抛出异常。\n" + std::wstring(e); \
			MessageBox(GMWindowsHandle, err.c_str(), L"NatureEnhance Error", MB_OK | MB_ICONERROR); \
		} \
		return returns; \
	} \
	catch (...) \
	{ \
		if (show_error) \
		{ \
			std::wstring err = L"在执行函数 " + std::wstring(funcname) + L" 时发生未知错误。"; \
			MessageBox(GMWindowsHandle, err.c_str(), L"NatureEnhance Error", MB_OK | MB_ICONERROR); \
		} \
		return returns; \
	}

void DEBUG(std::string str);

expReal BufferToTexture(GMReal buffer, GMReal gmtex, GMReal w, GMReal h);
expReal TimerGet();
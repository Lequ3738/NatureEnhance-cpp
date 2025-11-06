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

void ShowMessage(std::string&& str, std::string&& caption, UINT type);

#define simplecatch(funcname, returns)													\
	catch (const std::exception& e)														\
	{																					\
		if (show_error)																	\
		{																				\
			ShowMessage("在执行函数 " funcname " 出现错误：\n" + std::string(e.what()),	\
				"NatureEnhance Error", MB_OK | MB_ICONERROR);							\
		}																				\
		return returns;																	\
	}

#define msb_none 0
#define msb_error 1
#define msb_warning 2

void DEBUG(std::string str);
void console_write(const std::string& info, int mode = msb_none);

expReal BufferToTexture(GMReal buffer, GMReal gmtex, GMReal w, GMReal h);
expReal TimerGet();
std::string string_get_ext(GMString str, GMReal w, GMString lang = nullptr);
#pragma once
#include "Gmapi.h"
#include "GameMakerMath.h"

typedef double GMReal;
typedef char* GMString;

#define expReal __declspec(dllexport) GMReal _cdecl
#define expString __declspec(dllexport) GMString _cdecl

#define fnReal GMReal _cdecl
#define fnString GMString _cdecl

#define finish return 1.0
#define fail return 0.0

gm::CGMVariable GetResource(GMString res)
{
	return gm::execute_string("return " + std::string(res));
}

#pragma region Error
bool show_error = true;

#define simplecatch(funcname) \
	catch (const char* ex) \
	{ \
		if (show_error) \
		{ \
			std::string error = "在执行函数 " + std::string(funcname) + " 时抛出异常。\n" + std::string(ex); \
			std::wstring werror(error.begin(), error.end()); \
			MessageBox(NULL, werror.c_str(), L"NatureEnhance Error", MB_OK | MB_ICONERROR); \
		} \
		fail; \
	}
#pragma endregion

#pragma region Camera
GMReal CameraX, CameraY, ViewX, ViewY;
GMReal RoomWidth = 400, RoomHeight = 225, ViewWidth = 400, ViewHeight = 225;
GMReal Mode = 1, SnapDiv = 12, OffsetX = 24, OffsetY = -24, Factor = 0.16, MoveMode = 0;
GMReal LimitLeft = 0, LimitTop = 0, OldCameraX = 0, OldCameraY = 0, RegistryRoot = 0;
#pragma endregion

#pragma region Rope Calculate
double RopeA, RopeB, RopeC;
int Iterations = 15, RopeSteps = 16;
#pragma endregion

extern "C"
{
	expReal ShowErrorMessage(GMReal mode);

	expReal CameraInit(GMReal mode, GMReal playerX, GMReal playerY, GMReal playerScale,
		GMReal limitLeft, GMReal limitTop, GMReal roomWidth, GMReal roomHeight,
		GMReal viewWidth, GMReal viewHeight);
	expReal CameraMove(GMReal playerX, GMReal playerY, GMReal playerScale);
	expReal CameraSetOffset(GMReal offsetX, GMReal offsetY);
	expReal CameraSetMode(GMReal mode);
	expReal CameraSetStopFactor(GMReal factor);
	expReal CameraSetRoomSize(GMReal roomWidth, GMReal roomHeight);
	expReal CameraSetLimit(GMReal limitLeft, GMReal limitTop);
	expReal CameraGetSpeedDirection();
	expReal CameraGetSpeed();
	expReal CameraGetXSpeed();
	expReal CameraGetYSpeed();
	expReal CameraSetViewSize(GMReal viewWidth, GMReal viewHeight);
	expReal CameraExport(GMReal type);
	expReal CameraSetRelativeView(GMReal cameraX, GMReal cameraY);
	expReal CameraSetSnapLevel(GMReal level);
	expReal CameraSetView(GMReal cameraX, GMReal cameraY);

	expReal RopeSetAccuracy(GMReal iterations, GMReal steps);
	expReal DrawRope(GMReal x1, GMReal y1, GMReal x2, GMReal y2, GMReal length, GMReal back);
}
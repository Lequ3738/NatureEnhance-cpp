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
#define reterror return gm::noone

gm::CGMVariable GetResource(GMString res);

extern bool show_error;

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

	expReal JsonInit();
	expReal JsonFree();
	expReal JsonDecode(GMString jsonstr);
	expReal JsonDestroy(GMReal rootNode);
	expReal JsonGetDsType(GMReal rootNode, GMReal list);
}
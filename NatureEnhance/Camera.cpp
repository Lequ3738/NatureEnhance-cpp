#include "Main.h"

GMReal CameraX, CameraY, ViewX, ViewY;
GMReal RoomWidth = 400, RoomHeight = 225, ViewWidth = 400, ViewHeight = 225;
GMReal Mode = 1, SnapDiv = 12, OffsetX = 24, OffsetY = -24, Factor = 0.16, MoveMode = 0;
GMReal LimitLeft = 0, LimitTop = 0, OldCameraX = 0, OldCameraY = 0;

expReal CameraInit(GMReal mode, GMReal playerX, GMReal playerY, GMReal playerScale, GMReal limitLeft,
    GMReal limitTop, GMReal roomWidth, GMReal roomHeight, GMReal viewWidth, GMReal viewHeight)
{
    try
    {
        LimitLeft = limitLeft;
        LimitTop = limitTop;
        RoomWidth = roomWidth;
        RoomHeight = roomHeight;
        ViewWidth = viewWidth;
        ViewHeight = viewHeight;
        Mode = mode;

        if (mode == 1)
        {
            ViewX = clamp(floor(playerX / viewWidth) * viewWidth, limitLeft, roomWidth - 1);
            ViewY = clamp(floor(playerY / viewHeight) * viewHeight, limitTop, roomHeight - 1);

            CameraX = ViewX + (viewWidth / 2);
            CameraY = ViewY + (viewHeight / 2);
        }
        else if (mode == 2 || mode == 3)
        {
            GMReal shackPlayerX = clamp(playerX, LimitLeft + (ViewWidth / 2) - (playerScale * OffsetX),
                RoomWidth - (ViewWidth / 2) - (playerScale * OffsetX));
            GMReal shackPlayerY = clamp(playerY, LimitTop + (ViewHeight / 2) + OffsetY,
                RoomHeight - (ViewHeight / 2) + OffsetY);

            CameraX = shackPlayerX + (playerScale * OffsetX);
            CameraY = shackPlayerY + OffsetY;

            ViewX = CameraX - (viewWidth / 2);
            ViewY = CameraY - (viewHeight / 2);
        }
        else if (mode == 0)
        {
            ViewX = CameraX - (viewWidth / 2);
            ViewY = CameraY - (viewHeight / 2);
        }
        else
            throw std::runtime_error("未定义此运动模式");

        finish;
    }
	simplecatch("CameraInit", 0)
}

expReal CameraMove(GMReal playerX, GMReal playerY, GMReal playerScale)
{
    try
    {
        if (Mode == 1)  //传统的以单元格为单位移动相机
        {
            GMReal xFollow = (floor(playerX / ViewWidth) * ViewWidth) + (ViewWidth / 2);
            GMReal yFollow = (floor(playerY / ViewHeight) * ViewHeight) + (ViewHeight / 2);

            GMReal dir = point_direction(CameraX, CameraY, xFollow, yFollow);
            GMReal dis = point_distance(CameraX, CameraY, xFollow, yFollow);

            OldCameraX = CameraX;
            OldCameraY = CameraY;

            CameraX += lengthdir_x(dis, dir) / SnapDiv;
            CameraY += lengthdir_y(dis, dir) / SnapDiv;

            if (abs(CameraX - OldCameraX) < Factor)
            {
                if (round(CameraX) != round(xFollow))
                    CameraX += sign(xFollow - CameraX) / 2;
                else
                    CameraX = xFollow;
            }

            if (abs(CameraY - OldCameraY) < Factor)
            {
                if (round(CameraY) != round(yFollow))
                    CameraY += sign(yFollow - CameraY) / 2;
                else
                    CameraY = yFollow;
            }

            ViewX = clamp(CameraX - (ViewWidth / 2), LimitLeft, RoomWidth - ViewWidth);
            ViewY = clamp(CameraY - (ViewHeight / 2), LimitTop, RoomHeight - ViewHeight);
        }
        else if (Mode == 2)  //以位置偏差参数跟随玩家来平滑移动相机
        {
            GMReal shackPlayerX = clamp(playerX, LimitLeft + (ViewWidth / 2) - (playerScale * OffsetX),
                RoomWidth - (ViewWidth / 2) - (playerScale * OffsetX));
            GMReal shackPlayerY = clamp(playerY, LimitTop + (ViewHeight / 2) + OffsetY,
                RoomHeight - (ViewHeight / 2) + OffsetY);

            GMReal dir = point_direction(CameraX, CameraY, shackPlayerX + (playerScale * OffsetX),
                shackPlayerY + OffsetY);
            GMReal dis = point_distance(CameraX, CameraY, shackPlayerX + (playerScale * OffsetX),
                shackPlayerY + OffsetY);

            OldCameraX = CameraX;
            OldCameraY = CameraY;

            CameraX += lengthdir_x(dis, dir) / SnapDiv;
            CameraY += lengthdir_y(dis, dir) / SnapDiv;

            if (abs(CameraX - OldCameraX) < Factor)
                CameraX = OldCameraX;

            if (abs(CameraY - OldCameraY) < Factor)
                CameraY = OldCameraY;

            ViewX = CameraX - (ViewWidth / 2);
            ViewY = CameraY - (ViewHeight / 2);
        }
        else if (Mode == 3)  //以位置偏差参数跟随玩家来平滑移动相机
        {
            GMReal shackPlayerX = clamp(playerX, LimitLeft + (ViewWidth / 2) - (playerScale * OffsetX),
                RoomWidth - (ViewWidth / 2) - (playerScale * OffsetX));
            GMReal shackPlayerY = clamp(playerY, LimitTop + (ViewHeight / 2) + OffsetY,
                RoomHeight - (ViewHeight / 2) + OffsetY);

            OldCameraX = CameraX;
            OldCameraY = CameraY;

            CameraX += (shackPlayerX + (playerScale * OffsetX) - CameraX) / SnapDiv;
            CameraY += (shackPlayerY + OffsetY - CameraY) / SnapDiv;

            if (abs(CameraX - OldCameraX) < Factor)
                CameraX = OldCameraX;

            if (abs(CameraY - OldCameraY) < Factor)
                CameraY = OldCameraY;

            ViewX = CameraX - (ViewWidth / 2);
            ViewY = CameraY - (ViewHeight / 2);
        }
        else if (Mode == 0)
        {
            ViewX = CameraX - (ViewWidth / 2);
            ViewY = CameraY - (ViewHeight / 2);
        }
        else
            throw std::runtime_error("未定义此运动模式");

        finish;
    }
	simplecatch("CameraMove", 0)
}

expReal CameraSetOffset(GMReal offsetX, GMReal offsetY)
{
    OffsetX = offsetX;
    OffsetY = offsetY;

    finish;
}

expReal CameraSetMode(GMReal mode)
{
    Mode = mode;
    finish;
}

expReal CameraSetSnapLevel(GMReal level)
{
    SnapDiv = level;
    finish;
}

expReal CameraExport(GMReal type)
{
    if (type == 0)
        return ViewX;
    else
        return ViewY;
}

expReal CameraSetView(GMReal cameraX, GMReal cameraY)
{
    OldCameraX = CameraX;
    OldCameraY = CameraY;

    CameraX = cameraX;
    CameraY = cameraY;

    finish;
}

expReal CameraSetRelativeView(GMReal cameraX, GMReal cameraY)
{
    OldCameraX = CameraX;
    OldCameraY = CameraY;

    CameraX += cameraX;
    CameraY += cameraY;

    finish;
}

expReal CameraSetStopFactor(GMReal factor)
{
    Factor = factor;
    finish;
}

expReal CameraSetRoomSize(GMReal roomWidth, GMReal roomHeight)
{
    RoomWidth = roomWidth;
    RoomHeight = roomHeight;

    finish;
}

expReal CameraSetLimit(GMReal limitLeft, GMReal limitTop)
{
    LimitLeft = limitLeft;
    LimitTop = limitTop;

    finish;
}

expReal CameraGetSpeedDirection()
{
    return point_direction(CameraX, CameraY, OldCameraX, OldCameraY);
}

expReal CameraGetSpeed()
{
    return point_distance(CameraX, CameraY, OldCameraX, OldCameraY);
}

expReal CameraGetXSpeed()
{
    return CameraX - OldCameraX;
}

expReal CameraGetYSpeed()
{
    return CameraY - OldCameraY;
}

expReal CameraSetViewSize(GMReal viewWidth, GMReal viewHeight)
{
    ViewWidth = viewWidth;
    ViewHeight = viewHeight;

    return 1;
}
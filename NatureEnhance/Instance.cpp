#include "Main.h"
#include "DataStruct.h"
#include "Instance.h"

void GM_MoveInstance(gm::PGMINSTANCE inst, double x, double y)
{
	unsigned int funcAddress = 0x004ABF7C;

	__asm {
		lea ecx, x
		push dword ptr[ecx + 4]
		push dword ptr[ecx]

		lea ecx, y
		push dword ptr[ecx + 4]
		push dword ptr[ecx]

		mov eax, inst

		mov edx, funcAddress
		call edx
	}
}

bool GM_CollisionCheck(gm::PGMINSTANCE inst1, gm::PGMINSTANCE inst2, bool exactFlag)
{
	int result = 0;
	unsigned int funcAddress = 0x004AD1A0;

	__asm {
		mov eax, inst1
		mov edx, inst2
		movzx ecx, exactFlag
		call funcAddress
		mov result, eax
	}

	return result != 0;
}

gm::GMINSTANCE** GM_GetInstanceArray(int& size)
{
	BYTE* roomPtr = (BYTE*)gmapi->GetCurrentRoomPtr();
	size = *((DWORD*)(roomPtr + 0x68));
	return *((gm::GMINSTANCE***)(roomPtr + 0x6C));
}

int GM_GetInstanceCount(int objID)
{
	BYTE* roomPtr = (BYTE*)gmapi->GetCurrentRoomPtr();
	int* objectNumArray = *((int**)(roomPtr + 0x84));

	if (!objectNumArray)
		return 0;

	int objectNum = objectNumArray[-1];
	if (objID >= 0 && objID < objectNum)
		return objectNumArray[objID];

	return 0;
}

gm::PGMINSTANCE GM_GetInstPtr(int objID, int n)
{
	BYTE* roomPtr = (BYTE*)gmapi->GetCurrentRoomPtr();
	if (!roomPtr)
		return nullptr;

	int* objectNumArray = *((int**)(roomPtr + 0x84));
	gm::PGMINSTANCE** instTable = *((gm::PGMINSTANCE***)(roomPtr + 0x80));

	if (!objectNumArray || !instTable)
		return nullptr;

	int objectNum = objectNumArray[-1];
	if (objID < 0 || objID >= objectNum)
		return nullptr;

	int instNum = objectNumArray[objID];
	if (n < 0 || n >= instNum)
		return nullptr;

	return instTable[objID][n];
}

expReal InstancePlaceList(GMReal x, GMReal y, GMReal id, GMReal fast)
{
	int list = gm::noone;
	int ind = (int)id;
	bool prec = !(bool)fast;
	
	gm::PGMINSTANCE curInst = gmapi->GetCurrentInstancePtr();
	GMReal prevX = curInst->x, prevY = curInst->y;
	GM_MoveInstance(curInst, x, y);

	if (ind == gm::all)
	{
		int instanceArraySize = 0;
		gm::GMINSTANCE** instanceArray = GM_GetInstanceArray(instanceArraySize);

		for (int i = 0; i < instanceArraySize; ++i)
		{
			if (!instanceArray[i]->destroyed)
			{
				gm::PGMINSTANCE inst = instanceArray[i];
				if (GM_CollisionCheck(inst, curInst, prec))
				{
					if (list < 0)
						list = static_cast<int>(ne_list_create("instance_place_list() Created List."));

					gm::ds_list_add(list, inst->id);
				}
			}
		}
	}
	else if (ind >= 100000)  // instance
	{
		int instanceArraySize = 0;
		gm::GMINSTANCE** instanceArray = GM_GetInstanceArray(instanceArraySize);

		for (int i = 0; i < instanceArraySize; ++i)
		{
			if (!instanceArray[i]->destroyed && instanceArray[i]->id == ind)
			{
				gm::PGMINSTANCE inst = instanceArray[i];
				if (GM_CollisionCheck(inst, curInst, prec))
				{
					if (list < 0)
						list = static_cast<int>(ne_list_create("instance_place_list() Created List."));

					gm::ds_list_add(list, inst->id);
				}
			}
		}
	}
	else  // object
	{
		int instNum = GM_GetInstanceCount(ind);
		for (int i = 0; i < instNum; ++i)
		{
			gm::PGMINSTANCE inst = GM_GetInstPtr(ind, i);
			if (inst->destroyed) continue;

			if (GM_CollisionCheck(inst, curInst, prec))
			{
				if (list < 0)
					list = static_cast<int>(ne_list_create("instance_place_list() Created List."));

				gm::ds_list_add(list, inst->id);
			}
		}
	}

	GM_MoveInstance(curInst, prevX, prevY);
	return static_cast<GMReal>(list);
}
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
	int* objectNumArray = *(int**)(roomPtr + 0x84);

	if (!objectNumArray)
		return 0;

	int objectSize = *((int*)objectNumArray - 1);
	if (objID >= 0 && objID < objectSize)
		return objectNumArray[objID];

	return 0;
}

gm::PGMINSTANCE GM_GetInstPtr(int objID, int n)
{
	int roomPtr = (int)gmapi->GetCurrentRoomPtr();
	if (!roomPtr)
		return nullptr;

	int objectSize = *((DWORD*)(roomPtr + 0x80));
	int* objectNumArray = *((int**)(roomPtr + 0x84));

	if (!objectNumArray || objID < 0 || objID >= objectSize)
		return nullptr;

	if (n < 0 || n >= objectNumArray[objID])
		return nullptr;


}

expReal InstancePlaceList(GMReal x, GMReal y, GMReal id)
{
	int list = gm::noone;
	
	gm::PGMINSTANCE curInst = gmapi->GetCurrentInstancePtr();
	GMReal prevX = curInst->x, prevY = curInst->y;
	GM_MoveInstance(curInst, x, y);

	if (id == gm::all)
	{

	}
	else if (id >= 100000)  // instance
	{
		int instanceArraySize = 0;
		gm::GMINSTANCE** instanceArray = GM_GetInstanceArray(instanceArraySize);

		for (int i = 0; i < instanceArraySize; ++i)
		{
			if (!instanceArray[i]->destroyed && instanceArray[i]->id == id)
			{
				gm::PGMINSTANCE inst = instanceArray[i];
				if (GM_CollisionCheck(inst, curInst, true))
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
		int instNum = GM_GetInstanceCount(id);
		for (int i = 0; i < instNum; ++i)
		{

		}
	}

	GM_MoveInstance(curInst, prevX, prevY);
	return static_cast<GMReal>(list);
}
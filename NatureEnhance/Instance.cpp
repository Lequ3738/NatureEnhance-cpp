#include "Main.h"
#include "DataStruct.h"
#include "Instance.h"
#include <vector>
#include <string>

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

//------------------------------------------------------------------------------
// GM8.0 Runner 变量反射（偏移经 IDA 三方互证，2026-09-11）：
//   [0x58F134] = 变量名表：ANSI Delphi 串指针数组（dword 长度前缀在 ptr[-4]）
//   [0x58F138] = 变量名计数
//   变量 id = 名字索引 + 100000（与 gm82 inst_meta.c 同款偏置）
//   变量条目步长 = sizeof(gm::GMVARIABLE) = 40 字节（INNER_variable_global_get/local_get 查找循环同值）
// 名字缓存懒转换；DLL 生命周期内有效。
//------------------------------------------------------------------------------

static void* const GM80_varNameList = (void*)0x0058F134;
static void* const GM80_varNameCount = (void*)0x0058F138;

static std::vector<std::string> s_varNameCache;

static const char* GM80_GetVarName(int index)
{
	int count = *(int*)GM80_varNameCount;
	if (index < 0 || index >= count)
		return "";

	char** names = *(char***)GM80_varNameList;
	if ((int)s_varNameCache.size() < count)
		s_varNameCache.resize(count);

	if (s_varNameCache[index].empty())
	{
		char* str = names[index];
		int len = *(int*)(str - 4);
		s_varNameCache[index].assign(str, len);
	}

	return s_varNameCache[index].c_str();
}

static gm::PGMINSTANCE s_metaInst = nullptr;
static gm::PGMVARIABLELIST s_metaList = nullptr;
static int s_metaIndex = 0;

expReal InstmetaStart(GMReal id)
{
	s_metaInst = gmapi->GetInstancePtr(static_cast<int>(id));
	s_metaList = s_metaInst ? s_metaInst->variableListPtr : nullptr;
	s_metaIndex = 0;
	if (!s_metaList || s_metaList->count <= 0)
	{
		s_metaList = nullptr;
		return 0;
	}
	return static_cast<GMReal>(s_metaList->count);
}

expString InstmetaNext()
{
	if (!s_metaList || s_metaIndex >= s_metaList->count)
		return "";
	gm::GMVARIABLE& var = s_metaList->variables[s_metaIndex];
	s_metaIndex += 1;
	GMReturnString = GM80_GetVarName(var.symbolId - 100000);
	return GMReturnString.c_str();
}

expReal VarnameCount()
{
	return static_cast<GMReal>(*(int*)GM80_varNameCount);
}

expString VarnameGet(GMReal index)
{
	GMReturnString = GM80_GetVarName(static_cast<int>(index));
	return GMReturnString.c_str();
}
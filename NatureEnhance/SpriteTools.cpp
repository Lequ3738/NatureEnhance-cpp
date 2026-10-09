// Sprite frame-range duplication on top of the runner's own sprite internals.
// Mirrors the runner's GML sprite_duplicate (INNER_sprite_duplicate) but
// replaces the full deep copy with a subset variant: scalars are copied
// field-wise through the typed GMSPRITE view, frames and collision masks are
// duplicated only for the selected source range, and the new sprite is pushed
// onto texture pages by the runner's own uploader. Every runner interaction
// replays a proven call site's register pattern (INNER_sprite_duplicate /
// GM80_SpriteAssign); signatures are verified once before first use.
#include "Main.h"
#include "RunnerHook.h"

namespace {

std::uint8_t* g_runnerBase = nullptr;
bool g_targetsVerified = false;

bool VerifyRunnerTargets()
{
	if (g_targetsVerified)
		return true;

	g_runnerBase = gm80hook::base();

	struct Target
	{
		std::uint32_t rva;
		const unsigned char* sig;
		std::size_t len;
	};

	static const Target targets[] = {
		{ gm80hook::RVA_gm_array_grow,        gm80hook::SIG_gm_array_grow,        sizeof(gm80hook::SIG_gm_array_grow) },
		{ gm80hook::RVA_SpriteNamePrep,       gm80hook::SIG_SpriteNamePrep,       sizeof(gm80hook::SIG_SpriteNamePrep) },
		{ gm80hook::RVA_SpriteNameAssign,     gm80hook::SIG_SpriteNameAssign,     sizeof(gm80hook::SIG_SpriteNameAssign) },
		{ gm80hook::RVA_SpriteReleaseFields,  gm80hook::SIG_SpriteReleaseFields,  sizeof(gm80hook::SIG_SpriteReleaseFields) },
		{ gm80hook::RVA_SpriteObjCreate,      gm80hook::SIG_SpriteObjCreate,      sizeof(gm80hook::SIG_SpriteObjCreate) },
		{ gm80hook::RVA_SpriteFrameDuplicate, gm80hook::SIG_SpriteFrameDuplicate, sizeof(gm80hook::SIG_SpriteFrameDuplicate) },
		{ gm80hook::RVA_SpriteMaskDuplicate,  gm80hook::SIG_SpriteMaskDuplicate,  sizeof(gm80hook::SIG_SpriteMaskDuplicate) },
		{ gm80hook::RVA_SpriteUploadTextures, gm80hook::SIG_SpriteUploadTextures, sizeof(gm80hook::SIG_SpriteUploadTextures) },
	};

	for (const auto& target : targets)
	{
		if (std::memcmp(g_runnerBase + target.rva, target.sig, target.len) != 0)
			return false;
	}

	g_targetsVerified = true;
	return true;
}

// (eax=&arrayVar, edx=info, ecx=1, newCount on stack)
void ArrayGrow(void* arrayFieldAddr, const void* info, int newCount)
{
	const void* fn = g_runnerBase + gm80hook::RVA_gm_array_grow;
	__asm {
		push newCount
		mov eax, arrayFieldAddr
		mov ecx, 1
		mov edx, info
		call fn
		add esp, 4
	}
}

const void* LoadPtr(std::uint32_t rva)
{
	return *reinterpret_cast<void* const*>(g_runnerBase + rva);
}

// Delphi ctor: (eax=class VMT, dl=1) -> eax=new sprite object
void* SpriteObjCreate()
{
	const void* vmt = LoadPtr(gm80hook::RVA_SpriteClassVMT);
	const void* fn = g_runnerBase + gm80hook::RVA_SpriteObjCreate;
	void* result;
	__asm {
		mov eax, vmt
		mov dl, 1
		call fn
		mov result, eax
	}
	return result;
}

// (eax=frame VMT, dl=1, ecx=srcFrame) -> eax=new frame
void* FrameDuplicate(const void* srcFrame)
{
	const void* vmt = LoadPtr(gm80hook::RVA_FrameClassVMT);
	const void* fn = g_runnerBase + gm80hook::RVA_SpriteFrameDuplicate;
	void* result;
	__asm {
		mov ecx, srcFrame
		mov eax, vmt
		mov dl, 1
		call fn
		mov result, eax
	}
	return result;
}

// (eax=mask VMT, dl=1, ecx=srcMask) -> eax=new mask
void* MaskDuplicate(const void* srcMask)
{
	const void* vmt = LoadPtr(gm80hook::RVA_MaskClassVMT);
	const void* fn = g_runnerBase + gm80hook::RVA_SpriteMaskDuplicate;
	void* result;
	__asm {
		mov ecx, srcMask
		mov eax, vmt
		mov dl, 1
		call fn
		mov result, eax
	}
	return result;
}

// (eax=sprite)
void UploadTextures(void* sprite)
{
	const void* fn = g_runnerBase + gm80hook::RVA_SpriteUploadTextures;
	__asm {
		mov eax, sprite
		call fn
	}
}

// (eax=sprite)
void ReleaseFields(void* sprite)
{
	const void* fn = g_runnerBase + gm80hook::RVA_SpriteReleaseFields;
	__asm {
		mov eax, sprite
		call fn
	}
}

// Names the slot with the runner's own "__newsprite" literal:
// (eax=index, edx=&token) -> token, then (eax=&nameVar, edx=literal, ecx=token)
void NameAssign(void* nameVarAddr, int index)
{
	const void* prep = g_runnerBase + gm80hook::RVA_SpriteNamePrep;
	const void* assign = g_runnerBase + gm80hook::RVA_SpriteNameAssign;
	const void* lit = g_runnerBase + gm80hook::RVA_LitNewsprite;
	int token = 0;
	__asm {
		mov eax, index
		lea edx, token
		call prep
		mov ecx, token
		mov eax, nameVarAddr
		mov edx, lit
		call assign
	}
}

} // namespace

// Duplicate the frames [first, first+count) of a sprite into a new sprite.
// Scalars (origin, bounding box, separate-mask flag) carry over from the
// source; texture pages are assigned by the runner's uploader. Returns the
// new sprite id, or noone when the source or range is invalid.
expReal SpriteDuplicateFrames(GMReal spriteId, GMReal first, GMReal count)
{
	if (!VerifyRunnerTargets())
		return -1.0;

	auto* storage = reinterpret_cast<gm::PGMSPRITESTORAGE>(g_runnerBase + gm80hook::RVA_SpriteStorage);

	int src = (int)spriteId;
	if (src < 0 || src >= storage->arraySize || storage->sprites[src] == nullptr)
		return -1.0;

	gm::PGMSPRITE source = storage->sprites[src];
	int sourceCount = source->structNew.subimageCount;

	int start = (int)first;
	if (start < 0)
		start = 0;
	int frames = (int)count;
	if (frames > sourceCount - start)
		frames = sourceCount - start;
	if (frames <= 0)
		return -1.0;

	int newIndex = storage->arraySize;
	storage->arraySize = newIndex + 1;
	ArrayGrow(&storage->sprites, LoadPtr(gm80hook::RVA_SpriteArrInfo), storage->arraySize);
	ArrayGrow(&storage->names, LoadPtr(gm80hook::RVA_NameArrInfo), storage->arraySize);
	NameAssign(&storage->names[newIndex], newIndex);

	gm::PGMSPRITE dup = (gm::PGMSPRITE)SpriteObjCreate();
	if (dup == nullptr)
		return -1.0;
	storage->sprites[newIndex] = dup;

	ReleaseFields(dup);

	// Subset of GM80_SpriteAssign: scalars verbatim, per-frame data for the
	// selected range only.
	dup->structNew.originX = source->structNew.originX;
	dup->structNew.originY = source->structNew.originY;
	dup->structNew.bboxLeft = source->structNew.bboxLeft;
	dup->structNew.bboxTop = source->structNew.bboxTop;
	dup->structNew.bboxRight = source->structNew.bboxRight;
	dup->structNew.bboxBottom = source->structNew.bboxBottom;
	dup->structNew.seperateMasks = source->structNew.seperateMasks;
	dup->structNew.subimageCount = frames;

	ArrayGrow(&dup->structNew.bitmaps, LoadPtr(gm80hook::RVA_FrameArrayInfo), frames);
	for (int i = 0; i < frames; ++i)
		dup->structNew.bitmaps[i] = (gm::PGMBITMAP)FrameDuplicate(source->structNew.bitmaps[start + i]);

	void** dupMasks;
	if (dup->structNew.seperateMasks)
	{
		ArrayGrow(&dup->structNew.masks, LoadPtr(gm80hook::RVA_MaskArrayInfo), frames);
		dupMasks = static_cast<void**>(dup->structNew.masks);
		for (int i = 0; i < frames; ++i)
			dupMasks[i] = MaskDuplicate(static_cast<void**>(source->structNew.masks)[start + i]);
	}
	else
	{
		ArrayGrow(&dup->structNew.masks, LoadPtr(gm80hook::RVA_MaskArrayInfo), 1);
		dupMasks = static_cast<void**>(dup->structNew.masks);
		dupMasks[0] = MaskDuplicate(static_cast<void**>(source->structNew.masks)[0]);
	}

	UploadTextures(dup);

	return (double)newIndex;
}

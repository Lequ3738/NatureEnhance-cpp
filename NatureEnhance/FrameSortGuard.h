#pragma once

namespace framesort {

// Keeps the room instance array's in-place depth sort out of GM80_RunFrame's
// animation section. The section walks the array by index while Animation End
// handlers re-enter the whole draw pipeline, and an instance created earlier
// in the frame leaves the array unsorted, so a draw-entry sort would permute
// the array mid-walk: shifted instances are visited twice, the frame renders
// twice, and every one-shot per-frame draw gate is consumed by the first pass.
// The guard sorts once before the walk and blocks the draw-entry sort until
// the frame ends.
// No-op returning false when the runner bytes do not match the expected GM8.0
// signatures, so foreign runner versions keep their native behavior.
bool Install();

// Restores the patched call sites. Runs before this DLL is freed: a leftover
// patch would jump into the freed image.
void Uninstall();

} // namespace framesort

#pragma once

namespace errtrace {

// Installs the GM8.0 error-report enhancement:
//  - "In script <name>:" gains the call-site line number
//  - errors raised directly by builtin functions (ds_* etc.) gain the full
//    script chain and "Error in code at line" block
// No-op returning false when the runner bytes do not match the expected
// GM8.0 signatures, so foreign runner versions keep their native behavior.
bool Install();

// Restores every hooked/patched runner byte. Must run before this DLL's code
// disappears: the host frees the plugin DLLs mid-shutdown while the runner is
// still executing GML, and a leftover detour makes the very next call jump
// into the freed image (observed as the close-time hang).
void Uninstall();

} // namespace errtrace

#pragma once

namespace errtrace {

// Installs the GM8.0 error-report enhancement:
//  - "In script <name>:" gains the call-site line number
//  - errors raised directly by builtin functions (ds_* etc.) gain the full
//    script chain and "Error in code at line" block
// No-op returning false when the runner bytes do not match the expected
// GM8.0 signatures, so foreign runner versions keep their native behavior.
bool Install();

} // namespace errtrace

#pragma once

namespace app {

// Installs a process-wide unhandled-SEH filter that prints the exception code, faulting address,
// and a symbolized stack trace of the faulting thread to stderr before the process dies. Debug
// builds only carry the cost of one DbgHelp init on first crash -- zero overhead otherwise.
// Exists because a bare "Segmentation fault, exit 139" from a headless smoke run is undebuggable;
// this turns every future crash into an actionable report.
void install_crash_handler();

} // namespace app

// glide3xvk — diagnostics logging (house style learned from the [d2boot] work).
//
// Writes glide3x.log next to the game exe (same convention as d2boot.log),
// %TEMP% fallback. Unbuffered-ish: every line is flushed so a hard crash
// still leaves the last breadcrumb on disk.
#pragma once

// Called once at DLL_PROCESS_ATTACH; safe to call again (no-op).
void g3x_log_open();

// One formatted line: "[g3xvk] <fmt>\n". Flushed per call.
void g3x_log(const char* fmt, ...);

// Log-once per tag (first call logs, later calls are counted only).
// Counters surface in the periodic frame summary and at shutdown.
void g3x_log_once(const char* tag, const char* fmt, ...);

// Periodic activity summary: per-tag call counts that exceeded the log-once
// budget, frame counter, backend status. Called every N swaps and at detach.
void g3x_log_summary();

void g3x_log_close();

// (s137 smenu) Stub of the FPS Mode API so the Summon menu compiles without the FPS helper's branch (wt137-fps).
// The real runtime/sora_summon_fps.h must `#define SM_FPS_API` and define these five functions; INCLUDE IT BEFORE sora_summon_menu.h
// (sora.c: after sora_summon.h). With the stub FPS Mode is never offered.
#pragma once
#ifndef SM_FPS_API
static bool sm_fps_can(void) { return false; }          // FPS Mode (cmd 529) may be started now
static void sm_fps_request(void) {}                      // start FPS Mode
static bool sm_fps_on(void) { return false; }            // inside FPS Mode
static void sm_fps_cmd(int cmd) { (void)cmd; }           // 530 Balls / 531 Firecracker / 532 End
static bool sm_fps_cmd_ok(int cmd) { (void)cmd; return false; }
static bool sm_fps_busy(void) { return false; }          // (s137) FPS Mode intro or mode running
#endif

// sora_prompt.h (s194, DLL side; recon/s193/S193_KBD_SHRINE.md option 2): BOTW's "A" prompt (shrine, chest, talk, door,
// pick up) shows the button that really works in Sora mode. While Sora drives Link, Switch A is Sora's attack and is
// stripped from BOTW (linkpos.c link_input_filter); BOTW's A is reached with Switch X (KH2 Triangle, keyboard Space:
// pad_tap's "set tri" rule). So the exe draws BOTW's A glyph (0xE040) with Switch X's binding (runtime/nvn/glyphs.c
// glyphs_set_a_slot, found by GetProcAddress: an exe before s194 = no remap, nothing called).
//
// Slot X only while ALL hold: Sora mode on, the servo drove Link in a recent frame (sora_mod_frame_body's driving path, no
// freeze screen / chest event: those give Link the pad and A is A), "set tri" 1, "set promptx" 1, and the game not paused
// (PAUSE_STATE 0: BOTW's pause menu / map / the KH2 camp keep A = confirm). Else slot A (stock). Decided in pad_tap (every
// input poll, also while paused); the setter is called only on a change. sora_mod_unload puts A back.
// Needs (sora.c, before the include): g_on, g_tri_pickup, PAUSE_STATE, vk_frame_counter, LOG, <windows.h>.
#pragma once

#define PX_SLOT_A 0       // input_bind.h BIND_A (nn::hid button-bit order)
#define PX_SLOT_X 2       // input_bind.h BIND_X
#define PX_STALE 30       // presents: no driven frame for this long (a load, the Game Over screen) = slot A

static int g_promptx = 1;              // live "set promptx 0|1"
static volatile int g_px_drive;        // the last sora_mod_frame drove Link (written once a frame, after the body)
static volatile uint64_t g_px_frame;   // ... at this present
static int g_px_drive_now;             // (sora_mod_frame_body's driving path sets it this frame)
static volatile int g_px_sent = -1;    // the slot last given to the exe (-1: none yet; a fresh copy always sends once)

typedef void (*PxSetFn)(int slot);
static PxSetFn px_fn(void) {
    static int tried;
    static PxSetFn fn;
    if (!tried) {
        fn = (PxSetFn)(void *)GetProcAddress(GetModuleHandleA(NULL), "glyphs_set_a_slot");
        tried = 1;
        LOG("[sora] promptx: %s\n", fn ? "BOTW's A prompt follows Switch X while Sora drives (set promptx 0 = off)"
                                       : "this exe has no glyphs_set_a_slot (before s194: NEEDS EXE RELINK): the A prompt stays stock");
    }
    return fn;
}

static void px_send(int slot) {
    if (slot == g_px_sent) return;
    PxSetFn fn = px_fn();
    g_px_sent = slot;
    if (fn) fn(slot);
}

// sora_mod_frame: around the body. begin clears this frame's mark, end publishes it (one write a frame: no flicker).
static void px_frame_begin(void) { g_px_drive_now = 0; }
static void px_frame_end(uint64_t present) {
    g_px_drive = g_px_drive_now;
    if (g_px_drive_now) g_px_frame = present;
}

// pad_tap (whatever thread polls the pad)
static void px_pad(uint64_t now) {
    if (g_px_sent < 0 && !g_on) return;  // (Sora mode never on: never look the exe up)
    bool x = g_on && g_promptx && g_tri_pickup && g_px_drive && now <= g_px_frame + PX_STALE &&  // (not now - frame: the frame thread may be a present ahead)
             !*(volatile uint32_t *)(uintptr_t)PAUSE_STATE;
    px_send(x ? PX_SLOT_X : PX_SLOT_A);
}

static void px_unload(void) {
    if (g_px_sent != PX_SLOT_A) {
        PxSetFn fn = px_fn();
        if (fn) fn(PX_SLOT_A);
    }
    g_px_sent = PX_SLOT_A;
}

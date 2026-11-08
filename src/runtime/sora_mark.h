// (s111) Bug marks + live screenshots, so Claude can find "that moment" in the log and see the game itself.
//   F5 on the KEYBOARD (any time: playing, paused, menus, loads; works while the controller plays, GetAsyncKeyState is
//   system-wide) -> one log line "[MARK n] ..." (time, presented frame, world frame, pause state, body, position) + the
//   KH2 menu "decide" sound + a PNG of the next presented window image (HUD + KH2 draws included) in
//   D:/SoraBotW/play/marks/mark_<HHMMSS>_<n>.png.
//   live_test.txt "shot <name>" -> D:/SoraBotW/play/shots/<name>.png (same capture).
// F5 is free in Savage Wind (its overlay owns F1 F7 F8 F9 F11 F12 and `; keyboard play uses letters / digits / Shift /
// Space / Ctrl / Tab). The capture needs the exe export sora_shot_request (device.c, s111); an older exe: marks are
// logged without the picture (said once). Live "set mark 0" = F5 off, "set markse 0" = no sound.
#ifndef SORA_MARK_H
#define SORA_MARK_H

static int g_mark_on = 1, g_mark_se = 1, g_mark_n;

static bool mark_shot(const char *dir, const char *name, char *out, int n) {
    static void (*req)(const char *);
    static int bound;
    if (!bound) {
        bound = 1;
        req = (void (*)(const char *))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_shot_request");
        if (!req) LOG("[mark] this exe has no sora_shot_request (older build): marks / shots are logged without a picture\n");
    }
    snprintf(out, n, "%s/play/%s/%s.png", sora_root(), dir, name);
    if (!req) return false;
    char d[600];
    snprintf(d, sizeof d, "%s/play/%s", sora_root(), dir);
    CreateDirectoryA(d, NULL);
    req(out);
    return true;
}

static void mark_poll(void) {
    if (!g_mark_on) return;
    static int was;
    int down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    if (!down || was) { was = down; return; }
    was = 1;
    g_mark_n++;
    SYSTEMTIME t;
    GetLocalTime(&t);
    char name[48], path[160];
    snprintf(name, sizeof name, "mark_%02d%02d%02d_%d", t.wHour, t.wMinute, t.wSecond, g_mark_n);
    bool shot = mark_shot("marks", name, path, sizeof path);
    float p[3] = {0, 0, 0};
    uint64_t wf = linkpos_world_frame();
    bool pos = wf && linkpos_link_pos(p);
    uint32_t ps = *(volatile uint32_t *)(uintptr_t)PAUSE_STATE;
    LOG("[MARK %d] %02d:%02d:%02d presented frame %llu, world frame %llu, %s, body %s, at %s%.2f %.2f %.2f -> %s%s\n", g_mark_n,
        t.wHour, t.wMinute, t.wSecond, (unsigned long long)vk_frame_counter(), (unsigned long long)wf,
        !wf ? "no world (title / load)" : ps ? "paused / menu / map" : "playing", IN_MICKEY ? "Mickey" : IN_ROXAS ? "Roxas" : "Sora",
        pos ? "" : "(no pos) ", p[0], p[1], p[2], path, shot ? "" : " (no picture: old exe)");
    if (g_mark_se) sora_sfx_event("decide", 1);
}

static void mark_live_shot(const char *name) {
    char clean[48], path[160];
    int k = 0;
    for (const char *c = name; *c && k < 47; c++) clean[k++] = (isalnum((unsigned char)*c) || *c == '-' || *c == '_') ? *c : '_';
    clean[k] = 0;
    if (!k) snprintf(clean, sizeof clean, "shot");
    bool ok = mark_shot("shots", clean, path, sizeof path);
    LOG("[mark] live shot -> %s%s\n", path, ok ? "" : " (not saved: old exe)");
}

// (s111 user: command menu more in the bottom-left corner) live "set cmdx N" / "set cmdy N" = its offset in KH2 HUD units
// (~3.2 px each at 1440p; x - = left, y + = down; exe default -56 / 0). Needs the exe export sora_hud_menu_pos.
static int g_cmdx = -56, g_cmdy = -12;  // (s111 user: the bottom clipped under the screen at 0 -> -12, checked by a live shot)
static void cmd_pos_send(void) {
    void (*f)(float, float) = (void (*)(float, float))(void *)GetProcAddress(GetModuleHandleA(NULL), "sora_hud_menu_pos");
    if (f) f((float)g_cmdx, (float)g_cmdy);
    LOG("[mark] command menu offset %d %d%s\n", g_cmdx, g_cmdy, f ? "" : " (this exe has no sora_hud_menu_pos: restart with the s111 exe)");
}

static bool mark_live_set(const char *key, int v) {
    if (!strcmp(key, "mark")) g_mark_on = v != 0, LOG("[mark] F5 marks %s\n", g_mark_on ? "on" : "off");
    else if (!strcmp(key, "markse")) g_mark_se = v != 0, LOG("[mark] mark sound %s\n", g_mark_se ? "on" : "off");
    else if (!strcmp(key, "cmdx")) g_cmdx = v, cmd_pos_send();
    else if (!strcmp(key, "cmdy")) g_cmdy = v, cmd_pos_send();
    else return false;
    return true;
}
#endif

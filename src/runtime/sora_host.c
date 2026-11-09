// Sora-in-BotW: loads Sora's driver (runtime/sora.c, built as build/sora_mod.dll next to the .exe: `make sora`) and
// reloads it while the game runs when the file changes, so his moves can change without restarting BotW. The game
// thread (linkpos_frame -> sora_frame) checks the file's time every RELOAD_CHECK calls; a new build is copied to
// sora_mod_live<n>.dll (the linker can then replace sora_mod.dll again) and loaded; the old copy's sora_mod_unload
// lets go of the pad, the servo and the draw, and it stays loaded (its pad tap may still be running on the input
// thread). The new copy starts fresh: Sora re-enters from idle where Link stands.
//   SAVAGEWIND_SORA_RELOAD=0   load once, never reload
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "hle.h"
#include "sw_api.h"

#ifdef _WIN32
#define RELOAD_CHECK 15

static void (*g_frame)(void), (*g_unload)(void);
static char g_dir[MAX_PATH];
static FILETIME g_time;
static int g_live;
static bool g_direct;  // (s183) reload off: load sora_mod.dll without a live copy

static bool file_time(const char *path, FILETIME *t) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &a)) return false;
    *t = a.ftLastWriteTime;
    return true;
}

static void load(const char *src, const FILETIME *t) {
    char live[MAX_PATH + 32];
    snprintf(live, sizeof live, "%ssora_mod_live%lu_%d.dll", g_dir, (unsigned long)GetCurrentProcessId(), g_live++);  // (the pid: two copies of one exe never share live copies)
    // (s183) SAVAGEWIND_SORA_RELOAD=0 (the release launchers): load sora_mod.dll itself, no fresh unsigned live copy per start
    // for the antivirus to look at; the copy is for hot reload (the linker must be able to replace sora_mod.dll)
    if (g_direct) snprintf(live, sizeof live, "%s", src);
    else if (!CopyFileA(src, live, FALSE)) { LOG("[sora-host] copying %s failed (%lu)\n", src, GetLastError()); return; }
    HMODULE m = LoadLibraryA(live);
    void (*frame)(void) = m ? (void (*)(void))(void *)GetProcAddress(m, "sora_mod_frame") : NULL;
    if (!frame) { LOG("[sora-host] %s: no sora_mod_frame (%lu)\n", live, GetLastError()); return; }
    if (g_unload) g_unload();
    sw_api_dll_switch();  // (s175: every vt hook / tap of the old copy goes before the new copy's first frame)
    g_frame = frame;
    g_unload = (void (*)(void))(void *)GetProcAddress(m, "sora_mod_unload");
    g_time = *t;
    LOG("[sora-host] %s Sora's driver (%s)\n", g_live > 1 ? "reloaded" : "loaded", live);
}

void sora_frame(void) {
    static int init, reload = 1;
    static unsigned calls;
    static char path[MAX_PATH + 16];
    if (!init) {
        init = 1;
        const char *e = getenv("SAVAGEWIND_SORA_RELOAD");
        if (e && *e) reload = atoi(e);
        g_direct = !reload;
        DWORD n = GetModuleFileNameA(NULL, g_dir, MAX_PATH);
        while (n && g_dir[n - 1] != '\\' && g_dir[n - 1] != '/') n--;
        g_dir[n] = 0;
        for (int i = 0; i < 64; i++) {  // (copies of builds before the pid names, unless a game still runs them: Windows refuses to delete a loaded DLL)
            char old[MAX_PATH + 32];
            snprintf(old, sizeof old, "%ssora_mod_live%d.dll", g_dir, i);
            DeleteFileA(old);
        }
        {  // this pid's leftovers (none at start) and the copies of dead processes; a copy whose process still runs is never touched
            char pat[MAX_PATH + 32];
            WIN32_FIND_DATAA fd;
            snprintf(pat, sizeof pat, "%ssora_mod_live*_*.dll", g_dir);
            HANDLE h = FindFirstFileA(pat, &fd);
            if (h != INVALID_HANDLE_VALUE) {
                do {
                    unsigned long pid = 0;
                    int idx = 0;
                    if (sscanf(fd.cFileName, "sora_mod_live%lu_%d.dll", &pid, &idx) != 2) continue;
                    if (pid != (unsigned long)GetCurrentProcessId()) {
                        HANDLE ph = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
                        bool running = ph && WaitForSingleObject(ph, 0) == WAIT_TIMEOUT;
                        if (ph) CloseHandle(ph);
                        if (running) continue;
                    }
                    char old[MAX_PATH + 32];
                    snprintf(old, sizeof old, "%s%s", g_dir, fd.cFileName);
                    DeleteFileA(old);
                } while (FindNextFileA(h, &fd));
                FindClose(h);
            }
        }
        snprintf(path, sizeof path, "%ssora_mod.dll", g_dir);
        FILETIME t;
        if (file_time(path, &t)) load(path, &t);
        else LOG("[sora-host] no %s: no Sora\n", path);
    }
    if (reload && ++calls % RELOAD_CHECK == 0) {
        FILETIME t;
        if (file_time(path, &t) && CompareFileTime(&t, &g_time)) load(path, &t);
    }
    if (g_frame) g_frame();
}

#else  // Linux

#define RELOAD_CHECK 15

// Linux (Deck, s3 port): the same flow with dlopen. The folder is the exe's (readlink /proc/self/exe), or
// SAVAGEWIND_SORA_DIR (an AppImage's own folder is read-only). Files: sora_mod.so, live copies sora_mod_live<n>.so.
static void (*g_frame)(void), (*g_unload)(void);
static char g_dir[PATH_MAX];
static struct timespec g_time;
static int g_live;

static bool file_time(const char *path, struct timespec *t) {
    struct stat st;
    if (stat(path, &st)) return false;
    *t = st.st_mtim;
    return true;
}

static bool copy_file(const char *src, const char *dst) {
    int in = open(src, O_RDONLY), out = in < 0 ? -1 : open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    bool ok = in >= 0 && out >= 0;
    char buf[1 << 16];
    for (ssize_t n; ok && (n = read(in, buf, sizeof buf)) != 0;) ok = n > 0 && write(out, buf, (size_t)n) == n;
    if (in >= 0) close(in);
    if (out >= 0) close(out);
    return ok;
}

static void load(const char *src, const struct timespec *t) {
    char live[PATH_MAX + 32];
    snprintf(live, sizeof live, "%ssora_mod_live%d.so", g_dir, g_live++);
    if (!copy_file(src, live)) { LOG("[sora-host] copying %s to %s failed\n", src, live); return; }
    void *m = dlopen(live, RTLD_NOW | RTLD_LOCAL);
    void (*frame)(void) = m ? (void (*)(void))dlsym(m, "sora_mod_frame") : NULL;
    if (!frame) { LOG("[sora-host] %s: no sora_mod_frame (%s)\n", live, dlerror()); return; }
    if (g_unload) g_unload();
    sw_api_dll_switch();  // (s175: every vt hook / tap of the old copy goes before the new copy's first frame)
    g_frame = frame;
    g_unload = (void (*)(void))dlsym(m, "sora_mod_unload");
    g_time = *t;
    LOG("[sora-host] %s Sora's driver (%s)\n", g_live > 1 ? "reloaded" : "loaded", live);
}

void sora_frame(void) {
    static int init, reload = 1;
    static unsigned calls;
    static char path[PATH_MAX + 16];
    if (!init) {
        init = 1;
        const char *e = getenv("SAVAGEWIND_SORA_RELOAD");
        if (e && *e) reload = atoi(e);
        const char *d = getenv("SAVAGEWIND_SORA_DIR");
        if (d && *d) snprintf(g_dir, sizeof g_dir, "%s%s", d, d[strlen(d) - 1] == '/' ? "" : "/");
        else {
            ssize_t n = readlink("/proc/self/exe", g_dir, sizeof g_dir - 1);
            if (n < 0) n = 0;
            while (n && g_dir[n - 1] != '/') n--;
            g_dir[n] = 0;
        }
        for (int i = 0; i < 64; i++) {  // (the last session's copies)
            char old[PATH_MAX + 32];
            snprintf(old, sizeof old, "%ssora_mod_live%d.so", g_dir, i);
            unlink(old);
        }
        snprintf(path, sizeof path, "%ssora_mod.so", g_dir);
        struct timespec t;
        if (file_time(path, &t)) load(path, &t);
        else LOG("[sora-host] no %s: no Sora\n", path);
    }
    if (reload && ++calls % RELOAD_CHECK == 0) {
        struct timespec t;
        if (file_time(path, &t) && (t.tv_sec != g_time.tv_sec || t.tv_nsec != g_time.tv_nsec)) load(path, &t);
    }
    if (g_frame) g_frame();
}

#endif

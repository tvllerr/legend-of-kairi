// (Deck Linux port) The Win32 API our own runtime files use (sora.c and its headers, linkpos.c), so they build unchanged
// on Linux. Windows: exactly <windows.h>, nothing else (the PC build stays the same). Elsewhere: the types and constants
// we use, small inline shims, and declarations of the bigger ones in runtime/sora_win_linux.c (in the exe, exported by
// -rdynamic: sora_mod.so calls the same copies). Only what our code calls is here; Windows-only debug tools (linkpos
// watch = debug registers, sora_prof.h = SuspendThread sampling) are #ifdef _WIN32 at their sites instead.
//
// Semantics that differ on purpose:
//   VirtualQuery     in the guest window (rt.h GUEST_LO..GUEST_HI, one PROT_READ|WRITE mapping on Linux) a run of
//                    resident pages (mincore) is MEM_COMMIT and a run of untouched ones MEM_RESERVE, at most 64 MB per
//                    answer: the region scanners then read what the game touched, like committed memory on Windows,
//                    instead of 12.5 GB of zero pages. Elsewhere: /proc/self/maps. Never faults.
//   GetModuleHandleA NULL = the exe (dlopen(NULL): main program + RTLD_GLOBAL libs, the exe is -rdynamic); any named
//                    module ("swgame.dll", "kernel32.dll") = NULL, so GetProcAddress on it = NULL (callers fall back).
//   LoadLibraryA     NULL (winmm's PlaySoundA: the level-up chime logs "no PlaySoundA" and stays silent).
//   GetAsyncKeyState 0 (F5 bug marks: no keyboard on the Deck).
#pragma once

#ifdef _WIN32
#include <winsock2.h>  // (before windows.h: multiplayer sora_net_sock.h)
#include <windows.h>
#else

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/uio.h>

// ---- types
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t DWORD64;
typedef int32_t LONG;  // (32-bit as on Windows: `long` is 64 here)
typedef int64_t LONG64;
typedef uint32_t ULONG;
typedef uint64_t ULONGLONG;
typedef uintptr_t ULONG_PTR;
typedef size_t SIZE_T;
typedef int BOOL;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *PVOID;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef union {
    struct { DWORD LowPart; LONG HighPart; };
    int64_t QuadPart;
} LARGE_INTEGER;
typedef struct { DWORD dwLowDateTime, dwHighDateTime; } FILETIME;
typedef struct { WORD wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds; } SYSTEMTIME;
typedef struct {
    DWORD dwFileAttributes;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
    DWORD nFileSizeHigh, nFileSizeLow;
} WIN32_FILE_ATTRIBUTE_DATA;
typedef struct {
    PVOID BaseAddress, AllocationBase;
    DWORD AllocationProtect;
    SIZE_T RegionSize;
    DWORD State, Protect, Type;
} MEMORY_BASIC_INFORMATION;
typedef struct {
    PVOID VirtualAddress;
    union { ULONG_PTR Flags; struct { ULONG_PTR Valid : 1; ULONG_PTR Rest : 63; }; } VirtualAttributes;
} PSAPI_WORKING_SET_EX_INFORMATION;
typedef pthread_rwlock_t SRWLOCK;
typedef DWORD (*LPTHREAD_START_ROUTINE)(LPVOID);

#define WINAPI
#define CALLBACK
#define __declspec(x) __attribute__((visibility("default")))  // (only dllexport is used)
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define INFINITE 0xFFFFFFFFu
#define WAIT_OBJECT_0 0
#define SRWLOCK_INIT PTHREAD_RWLOCK_INITIALIZER
#define INVALID_FILE_ATTRIBUTES 0xFFFFFFFFu
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define FILE_ATTRIBUTE_NORMAL 0x80
#define GetFileExInfoStandard 0
#define MOVEFILE_REPLACE_EXISTING 0x1
#define MOVEFILE_WRITE_THROUGH 0x8
#define MEM_COMMIT 0x1000
#define MEM_RESERVE 0x2000
#define MEM_FREE 0x10000
#define MEM_PRIVATE 0x20000
#define MEM_MAPPED 0x40000
#define MEM_IMAGE 0x1000000
#define PAGE_NOACCESS 0x01
#define PAGE_READONLY 0x02
#define PAGE_READWRITE 0x04
#define PAGE_WRITECOPY 0x08
#define PAGE_EXECUTE 0x10
#define PAGE_EXECUTE_READ 0x20
#define PAGE_EXECUTE_READWRITE 0x40
#define PAGE_EXECUTE_WRITECOPY 0x80
#define PAGE_GUARD 0x100
#define PAGE_NOCACHE 0x200
#define PAGE_WRITECOMBINE 0x400
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define VK_F5 0x74

// ---- in runtime/sora_win_linux.c (the exe)
SIZE_T sw_VirtualQuery(const void *p, MEMORY_BASIC_INFORMATION *m, SIZE_T sz);
BOOL sw_VirtualProtect(void *p, SIZE_T n, DWORD prot, DWORD *old);
void *sw_VirtualAlloc(void *at, SIZE_T n, DWORD type, DWORD prot);
BOOL sw_QueryWorkingSetEx(HANDLE proc, PVOID ws, DWORD cb);
HANDLE sw_CreateThread(LPTHREAD_START_ROUTINE fn, LPVOID arg);
DWORD sw_WaitForSingleObject(HANDLE h, DWORD ms);
BOOL sw_CloseHandle(HANDLE h);
void *sw_self_module(void);
#define VirtualQuery sw_VirtualQuery
#define VirtualProtect sw_VirtualProtect
#define VirtualAlloc sw_VirtualAlloc
#define QueryWorkingSetEx sw_QueryWorkingSetEx
#define K32QueryWorkingSetEx sw_QueryWorkingSetEx
#define CreateThread(sec, stack, fn, arg, flags, tid) sw_CreateThread((fn), (arg))
#define WaitForSingleObject sw_WaitForSingleObject
#define CloseHandle sw_CloseHandle

// ---- inline
static inline HANDLE GetCurrentProcess(void) { return (HANDLE)(intptr_t)-1; }
static inline DWORD GetCurrentThreadId(void) { return (DWORD)syscall(SYS_gettid); }
static inline DWORD GetCurrentProcessId(void) { return (DWORD)getpid(); }
static inline unsigned long GetLastError(void) { return (unsigned long)errno; }  // (printed with %lu)
static inline BOOL SetThreadPriority(HANDLE h, int p) { (void)h, (void)p; return TRUE; }
static inline BOOL FlushInstructionCache(HANDLE h, const void *p, SIZE_T n) { (void)h, (void)p, (void)n; return TRUE; }  // (x86: coherent)
static inline short GetAsyncKeyState(int vk) { (void)vk; return 0; }

static inline uint64_t sw_mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}
static inline ULONGLONG GetTickCount64(void) { return sw_mono_ns() / 1000000u; }
static inline DWORD GetTickCount(void) { return (DWORD)(sw_mono_ns() / 1000000u); }
static inline BOOL QueryPerformanceCounter(LARGE_INTEGER *t) { t->QuadPart = (int64_t)sw_mono_ns(); return TRUE; }
static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *f) { f->QuadPart = 1000000000; return TRUE; }
static inline void Sleep(DWORD ms) {
    struct timespec ts = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
    while (nanosleep(&ts, &ts) && errno == EINTR) {}
}
static inline void GetLocalTime(SYSTEMTIME *s) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    localtime_r(&ts.tv_sec, &tm);
    s->wYear = (WORD)(tm.tm_year + 1900), s->wMonth = (WORD)(tm.tm_mon + 1), s->wDayOfWeek = (WORD)tm.tm_wday;
    s->wDay = (WORD)tm.tm_mday, s->wHour = (WORD)tm.tm_hour, s->wMinute = (WORD)tm.tm_min, s->wSecond = (WORD)tm.tm_sec;
    s->wMilliseconds = (WORD)(ts.tv_nsec / 1000000);
}

// environment (one libc environment on Linux: the exe's getenv sees the DLL's SetEnvironmentVariableA, unlike Windows)
static inline DWORD GetEnvironmentVariableA(LPCSTR name, LPSTR buf, DWORD size) {
    const char *v = getenv(name);
    if (!v) return 0;
    size_t n = strlen(v);
    if (!buf || n + 1 > size) return (DWORD)(n + 1);  // (too small: the size needed, nothing copied)
    memcpy(buf, v, n + 1);
    return (DWORD)n;
}
static inline BOOL SetEnvironmentVariableA(LPCSTR name, LPCSTR value) {
    return (value ? setenv(name, value, 1) : unsetenv(name)) == 0;
}
static inline int _putenv_s(const char *name, const char *value) {  // (CRT: "" removes it)
    return (value && *value ? setenv(name, value, 1) : unsetenv(name)) ? errno : 0;
}

// modules: the exe only (see the top)
static inline HMODULE GetModuleHandleA(LPCSTR name) { return name ? NULL : sw_self_module(); }
static inline void *GetProcAddress(HMODULE m, LPCSTR name) { return m ? dlsym(m, name) : NULL; }
static inline HMODULE LoadLibraryA(LPCSTR name) { (void)name; return NULL; }

// files
static inline DWORD GetFileAttributesA(LPCSTR p) {
    struct stat st;
    if (stat(p, &st)) return INVALID_FILE_ATTRIBUTES;
    return S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
}
static inline BOOL GetFileAttributesExA(LPCSTR p, int level, WIN32_FILE_ATTRIBUTE_DATA *a) {
    (void)level;
    struct stat st;
    if (stat(p, &st)) return FALSE;
    memset(a, 0, sizeof *a);
    a->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
    uint64_t ft = ((uint64_t)st.st_mtim.tv_sec + 11644473600ull) * 10000000ull + (uint64_t)st.st_mtim.tv_nsec / 100;  // (FILETIME units)
    a->ftLastWriteTime.dwLowDateTime = (DWORD)ft, a->ftLastWriteTime.dwHighDateTime = (DWORD)(ft >> 32);
    a->nFileSizeLow = (DWORD)st.st_size, a->nFileSizeHigh = (DWORD)((uint64_t)st.st_size >> 32);
    return TRUE;
}
static inline BOOL MoveFileExA(LPCSTR from, LPCSTR to, DWORD flags) { (void)flags; return rename(from, to) == 0; }  // (replaces)
static inline BOOL DeleteFileA(LPCSTR p) { return unlink(p) == 0; }
static inline BOOL CreateDirectoryA(LPCSTR p, void *sec) { (void)sec; return mkdir(p, 0777) == 0; }
static inline BOOL ReadProcessMemory(HANDLE proc, LPCVOID a, void *b, SIZE_T n, SIZE_T *got) {  // (ourselves only)
    (void)proc;
    struct iovec l = {b, n}, r = {(void *)a, n};
    ssize_t k = (ssize_t)syscall(SYS_process_vm_readv, (long)getpid(), &l, 1L, &r, 1L, 0L);  // (no _GNU_SOURCE needed)
    if (got) *got = k > 0 ? (SIZE_T)k : 0;
    return k == (ssize_t)n;
}

// locks + atomics
static inline void AcquireSRWLockExclusive(SRWLOCK *l) { pthread_rwlock_wrlock(l); }
static inline void ReleaseSRWLockExclusive(SRWLOCK *l) { pthread_rwlock_unlock(l); }
static inline void AcquireSRWLockShared(SRWLOCK *l) { pthread_rwlock_rdlock(l); }
static inline void ReleaseSRWLockShared(SRWLOCK *l) { pthread_rwlock_unlock(l); }
static inline int TryAcquireSRWLockExclusive(SRWLOCK *l) { return pthread_rwlock_trywrlock(l) == 0; }  // (s181 sora_esa_ai.h)
static inline int TryAcquireSRWLockShared(SRWLOCK *l) { return pthread_rwlock_tryrdlock(l) == 0; }
#define MemoryBarrier() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#define InterlockedIncrement(p) __atomic_add_fetch((p), 1, __ATOMIC_SEQ_CST)
#define InterlockedDecrement(p) __atomic_sub_fetch((p), 1, __ATOMIC_SEQ_CST)
#define InterlockedExchange(p, v) __atomic_exchange_n((p), (v), __ATOMIC_SEQ_CST)
#define InterlockedExchangePointer(p, v) __atomic_exchange_n((p), (v), __ATOMIC_SEQ_CST)
#define InterlockedOr64(p, v) __atomic_fetch_or((p), (v), __ATOMIC_SEQ_CST)
#define InterlockedCompareExchange(p, x, c) __extension__({ \
    __typeof__(*(p)) sw_cmp_ = (c); \
    __atomic_compare_exchange_n((p), &sw_cmp_, (x), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
    sw_cmp_; })

#endif  // !_WIN32

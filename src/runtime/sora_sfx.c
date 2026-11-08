// Sora-in-BotW: the KH2 sound-effect player (runtime/sora_sfx.h for the API and the env vars).
//
// Output: an SDL3 audio stream of its own on the default playback device, next to the game's (services/audio.c opens
// one for the audio renderer: F32 stereo at the renderer's rate). SDL mixes the two logical devices; nothing of the
// game's path is touched. The stream pulls through a get-callback (the device thread): the voices are mixed there, 16
// at a time, linear resampling from the sound's own rate (KH2's SEs sit at 47974..48003 Hz) to 48 kHz, per-voice gain
// and constant-power pan. Gain = the sound's KH2 volume x the caller's x SAVAGEWIND_SORA_SFX_GAIN x g_settings.volume
// x SAVAGEWIND_AUDIO_GAIN (the last two live on the stream, like the game's: a quieter menu volume quiets Sora too).
//
// Data: data/sfx/sfx.bin (tools/sfxpack.py writes it; the layout is in its docstring). Loaded whole (33 MB of PCM16)
// on the first call; without it everything is a no-op (logged once).
//
// 3D: sora_sfx_play_at pans by the listener = Link's body (linkpos_link_pos) facing the camera (linkpos_camera_fwd):
// right = (fwd.z, -fwd.x) in BotW's x/z plane, pan = dot(dir, right) scaled by distance, gain 1 within 2 u then 1/d.
// Proper HRTF / Doppler: not here; a distance low-pass would be the next cheap step (one biquad per voice).
//
// Threads: the game thread calls play/stop, the device thread mixes; one mutex around the voice table (short holds,
// no allocation in the callback). sora_sfx_motion keeps per-trigger cycle counters and the last bark's priority.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "hle.h"
#include "settings.h"
#include "sora_sfx.h"
#include "sora_path.h"  // (Deck port: D:/SoraBotW -> SORA_ROOT)

bool linkpos_link_pos(float p[3]);      // runtime/linkpos.c
bool linkpos_camera_fwd(float f[2]);

#define OUT_RATE 48000
#define NVOICE 16
#define MIX_FRAMES 512  // per callback chunk (10.7 ms at 48 kHz)

#pragma pack(push, 1)
typedef struct { char key[48]; uint32_t rate, channels; float volume; uint32_t loop, offset, frames; } Sound;
typedef struct { uint32_t slot, frame, kind, vtbl, cycle, count, first; } Trig;
typedef struct { uint32_t vtbl, priority, count, first; } VtblRow;
typedef struct { int32_t sound; uint32_t weight; } Choice;
typedef struct { char name[32]; uint32_t sound; } Name;
typedef struct { uint32_t atkp, sound; } Hit;
typedef struct {
    char magic[4];
    uint32_t version, n_sounds, n_trig, n_trig_sounds, n_vtbl, n_choices, n_names, n_hits, data_off, data_bytes, pad;
} Header;
#pragma pack(pop)

typedef struct {
    int id, serial;       // id -1 = free; serial: handles stay unique across reuse
    double pos;           // source frame position
    float step;           // source frames per output frame
    float gl, gr;         // left / right gain
    bool loop;
    int vtbl_priority;    // >= 0 for a bark (the voice table)
    uint64_t started;     // mix frame counter at start (the oldest one-shot gives way)
} Voice;

static struct {
    int state;  // 0 not tried, 1 ready, -1 off / missing / bad
    uint8_t *pack;
    const Header *h;
    const Sound *snd;
    const Trig *trig;
    const uint32_t *trig_snd;
    const VtblRow *vt;
    const Choice *choice;
    const Name *name;
    const Hit *hit;
    const int16_t *pcm;
    SDL_AudioStream *stream;
    SDL_Mutex *lock;
    Voice v[NVOICE];
    int serial;
    uint64_t frames;
    float gain;  // SAVAGEWIND_SORA_SFX_GAIN
    bool log;
    uint8_t *cycle;      // per trigger: the next footstep variant
    int last_bark_pri;   // the vtbl priority of the bark voice (-1 none)
    int last_bark;       // its handle
    struct { char name[32]; int handle; } loops[8];
} G = {0};

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

// ---- the device callback ----------------------------------------------------------------------------------------
static void mix(float *out, int n) {
    memset(out, 0, (size_t)n * 2 * sizeof(float));
    for (int vi = 0; vi < NVOICE; vi++) {
        Voice *v = &G.v[vi];
        if (v->id < 0) continue;
        const Sound *s = &G.snd[v->id];
        const int16_t *p = G.pcm + s->offset / 2;
        int ch = (int)s->channels, N = (int)s->frames;
        if (N < 2 || ch < 1) { v->id = -1; continue; }
        double pos = v->pos;
        for (int i = 0; i < n; i++) {
            int k = (int)pos;
            if (k >= N - 1) {
                if (!v->loop) { v->id = -1; break; }
                pos -= N - 1, k = (int)pos;
                if (k < 0) k = 0, pos = 0;
            }
            float f = (float)(pos - k);
            float l, r;
            if (ch == 1) {
                l = r = (p[k] + (p[k + 1] - p[k]) * f) * (1.0f / 32768.f);
            } else {
                const int16_t *a = p + (size_t)k * ch, *b = a + ch;
                l = (a[0] + (b[0] - a[0]) * f) * (1.0f / 32768.f);
                r = (a[1] + (b[1] - a[1]) * f) * (1.0f / 32768.f);
            }
            out[i * 2] += l * v->gl;
            out[i * 2 + 1] += r * v->gr;
            pos += v->step;
        }
        v->pos = pos;
    }
    for (int i = 0; i < n * 2; i++) out[i] = clampf(out[i], -1, 1);
    G.frames += (uint64_t)n;
}

static void SDLCALL get_cb(void *ud, SDL_AudioStream *st, int additional, int total) {
    (void)ud; (void)total;
    static float buf[MIX_FRAMES * 2];
    int need = additional / (int)(2 * sizeof(float));
    while (need > 0) {
        int n = need > MIX_FRAMES ? MIX_FRAMES : need;
        SDL_LockMutex(G.lock);
        mix(buf, n);
        SDL_UnlockMutex(G.lock);
        SDL_PutAudioStreamData(st, buf, n * (int)(2 * sizeof(float)));
        need -= n;
    }
}

// ---- loading ----------------------------------------------------------------------------------------------------
static bool pack_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { LOG("[sora-sfx] no %s: no KH2 sounds (tools/sfxpack.py makes it)\n", path); return false; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < (long)sizeof(Header)) { fclose(f); LOG("[sora-sfx] %s: too small\n", path); return false; }
    G.pack = malloc((size_t)sz);
    if (!G.pack || fread(G.pack, 1, (size_t)sz, f) != (size_t)sz) { fclose(f); LOG("[sora-sfx] %s: read failed\n", path); return false; }
    fclose(f);
    const Header *h = (const Header *)G.pack;
    if (memcmp(h->magic, "SKSF", 4) || h->version != 1) { LOG("[sora-sfx] %s: bad magic / version\n", path); return false; }
    size_t need = sizeof(Header) + (size_t)h->n_sounds * sizeof(Sound) + (size_t)h->n_trig * sizeof(Trig) +
                  (size_t)h->n_trig_sounds * 4 + (size_t)h->n_vtbl * sizeof(VtblRow) + (size_t)h->n_choices * sizeof(Choice) +
                  (size_t)h->n_names * sizeof(Name) + (size_t)h->n_hits * sizeof(Hit);
    if (need > h->data_off || (size_t)h->data_off + h->data_bytes > (size_t)sz) { LOG("[sora-sfx] %s: bad tables\n", path); return false; }
    const uint8_t *p = G.pack + sizeof(Header);
    G.h = h;
    G.snd = (const Sound *)p, p += (size_t)h->n_sounds * sizeof(Sound);
    G.trig = (const Trig *)p, p += (size_t)h->n_trig * sizeof(Trig);
    G.trig_snd = (const uint32_t *)p, p += (size_t)h->n_trig_sounds * 4;
    G.vt = (const VtblRow *)p, p += (size_t)h->n_vtbl * sizeof(VtblRow);
    G.choice = (const Choice *)p, p += (size_t)h->n_choices * sizeof(Choice);
    G.name = (const Name *)p, p += (size_t)h->n_names * sizeof(Name);
    G.hit = (const Hit *)p;
    G.pcm = (const int16_t *)(G.pack + h->data_off);
    for (uint32_t i = 0; i < h->n_sounds; i++)  // every sound's PCM inside the data
        if ((size_t)G.snd[i].offset + (size_t)G.snd[i].frames * G.snd[i].channels * 2 > h->data_bytes) {
            LOG("[sora-sfx] %s: sound %u out of range\n", path, i);
            return false;
        }
    G.cycle = calloc(h->n_trig ? h->n_trig : 1, 1);
    return true;
}

static bool init(void) {
    if (G.state) return G.state > 0;
    G.state = -1;
    for (int i = 0; i < NVOICE; i++) G.v[i].id = -1;
    for (int i = 0; i < 8; i++) G.loops[i].handle = -1;
    G.last_bark = G.last_bark_pri = -1;
    const char *p = getenv("SAVAGEWIND_SORA_SFX");
    if (p && !strcmp(p, "0")) { LOG("[sora-sfx] off (SAVAGEWIND_SORA_SFX=0)\n"); return false; }
    if (!p || !*p || !strcmp(p, "1")) p = "D:/SoraBotW/data/sfx/sfx.bin";
    p = sora_path(p);
    const char *e = getenv("SAVAGEWIND_SORA_SFX_GAIN");
    G.gain = e && *e ? (float)atof(e) : 1.0f;
    G.log = (e = getenv("SAVAGEWIND_SORA_SFX_LOG")) && *e && strcmp(e, "0");
    if (!pack_load(p)) return false;
    if (getenv("SAVAGEWIND_NO_AUDIO") && strcmp(getenv("SAVAGEWIND_NO_AUDIO"), "0")) { LOG("[sora-sfx] no audio device (SAVAGEWIND_NO_AUDIO)\n"); return false; }
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) { LOG("[sora-sfx] SDL audio init failed: %s\n", SDL_GetError()); return false; }
    G.lock = SDL_CreateMutex();
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, OUT_RATE};
    G.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, get_cb, NULL);
    if (!G.stream) { LOG("[sora-sfx] SDL audio open failed: %s\n", SDL_GetError()); return false; }
    SDL_ResumeAudioStreamDevice(G.stream);
    G.state = 1;
    LOG("[sora-sfx] %s: %u sounds, %u triggers, %u vtbl rows, %u names, %u hit rows; stream F32 stereo %d Hz, gain %.2f\n", p,
        G.h->n_sounds, G.h->n_trig, G.h->n_vtbl, G.h->n_names, G.h->n_hits, OUT_RATE, G.gain);
    return true;
}

// The stream's gain follows the menu volume and SAVAGEWIND_AUDIO_GAIN, as services/audio.c's does (applied per play:
// cheap, and a play is the only time it matters).
static void apply_gain(void) {
    static double env = -1;
    if (env < 0) env = getenv("SAVAGEWIND_AUDIO_GAIN") ? atof(getenv("SAVAGEWIND_AUDIO_GAIN")) : 1.0;
    float g = (float)(env * g_settings.volume);
    if (SDL_GetAudioStreamGain(G.stream) != g) SDL_SetAudioStreamGain(G.stream, g);
}

// ---- the API ----------------------------------------------------------------------------------------------------
bool sora_sfx_ready(void) { return init(); }

int sora_sfx_id(const char *name) {
    if (!name || !init()) return -1;
    for (uint32_t i = 0; i < G.h->n_names; i++)
        if (!strncmp(G.name[i].name, name, 31)) return (int)G.name[i].sound;
    for (uint32_t i = 0; i < G.h->n_sounds; i++)
        if (!strncmp(G.snd[i].key, name, 47)) return (int)i;
    static char warned[16][32];
    static int nw;
    for (int i = 0; i < nw; i++) if (!strncmp(warned[i], name, 31)) return -1;
    if (nw < 16) strncpy(warned[nw++], name, 31);
    LOG("[sora-sfx] unknown sound \"%s\"\n", name);
    return -1;
}

static int start(int id, float gain, float pan, bool loop, int pri) {
    if (id < 0 || !init() || (uint32_t)id >= G.h->n_sounds || gain <= 0) return -1;
    const Sound *s = &G.snd[id];
    if (s->frames < 2) return -1;
    apply_gain();
    pan = clampf(pan, -1, 1);
    float g = gain * s->volume * G.gain, a = (pan + 1) * 0.25f * (float)M_PI;  // constant power: 0 .. pi/2
    SDL_LockMutex(G.lock);
    int slot = -1;
    uint64_t oldest = ~0ull;
    for (int i = 0; i < NVOICE; i++) {
        if (G.v[i].id < 0) { slot = i; break; }
        if (!G.v[i].loop && G.v[i].started < oldest) oldest = G.v[i].started, slot = i;  // (a loop never gives way)
    }
    if (slot < 0) { SDL_UnlockMutex(G.lock); return -1; }
    Voice *v = &G.v[slot];
    v->id = id, v->serial = ++G.serial, v->pos = 0, v->step = (float)s->rate / OUT_RATE;
    v->gl = g * cosf(a) * (float)M_SQRT2, v->gr = g * sinf(a) * (float)M_SQRT2;
    v->loop = loop || s->loop, v->vtbl_priority = pri, v->started = G.frames;
    int handle = slot | (v->serial << 4);
    SDL_UnlockMutex(G.lock);
    if (G.log) LOG("[sora-sfx] play %d %s gain %.2f pan %.2f%s -> %d\n", id, s->key, g, pan, v->loop ? " loop" : "", handle);
    return handle;
}

static Voice *by_handle(int h) {  // (under the lock)
    if (h < 0) return NULL;
    Voice *v = &G.v[h & 15];
    return v->id >= 0 && v->serial == (h >> 4) ? v : NULL;
}

int sora_sfx_play(int id, float gain) { return start(id, gain, 0, false, -1); }
int sora_sfx_play_pan(int id, float gain, float pan, bool loop) { return start(id, gain, pan, loop, -1); }
int sora_sfx_event(const char *name, float gain) { return start(sora_sfx_id(name), gain, 0, false, -1); }

// Pan / attenuation from the listener (Link's body, the camera's forward): the stereo field is the camera's.
static void spatial(const float pos[3], float *gain, float *pan) {
    float L[3], f[2];
    *pan = 0;
    if (!pos || !linkpos_link_pos(L) || !linkpos_camera_fwd(f)) return;
    float dx = pos[0] - L[0], dz = pos[2] - L[2], dy = pos[1] - L[1];
    float d = sqrtf(dx * dx + dy * dy + dz * dz);
    float fl = sqrtf(f[0] * f[0] + f[1] * f[1]);
    if (fl > 1e-4f) {
        float rx = f[1] / fl, rz = -f[0] / fl;  // right of forward in x/z (y up)
        float side = (dx * rx + dz * rz);
        *pan = clampf(side / (d + 1.0f), -1, 1) * 0.8f;
    }
    if (d > 2.0f) *gain *= 2.0f / d;
}

int sora_sfx_play_at(int id, float gain, const float pos[3]) {
    float pan;
    spatial(pos, &gain, &pan);
    return start(id, gain, pan, false, -1);
}

void sora_sfx_stop(int h) {
    if (!init()) return;
    SDL_LockMutex(G.lock);
    Voice *v = by_handle(h);
    if (v) v->id = -1;
    SDL_UnlockMutex(G.lock);
}

bool sora_sfx_playing(int h) {
    if (!init()) return false;
    SDL_LockMutex(G.lock);
    bool on = by_handle(h) != NULL;
    SDL_UnlockMutex(G.lock);
    return on;
}

void sora_sfx_loop(const char *name, bool on) {
    if (!name || !init()) return;
    int free_i = -1;
    for (int i = 0; i < 8; i++) {
        if (G.loops[i].handle >= 0 && !strncmp(G.loops[i].name, name, 31)) {
            if (sora_sfx_playing(G.loops[i].handle)) {
                if (!on) sora_sfx_stop(G.loops[i].handle), G.loops[i].handle = -1;
                return;
            }
            G.loops[i].handle = -1;
        }
        if (G.loops[i].handle < 0 && free_i < 0) free_i = i;
    }
    if (!on || free_i < 0) return;
    int h = start(sora_sfx_id(name), 1.0f, 0, true, -1);
    if (h < 0) return;
    strncpy(G.loops[free_i].name, name, 31), G.loops[free_i].name[31] = 0, G.loops[free_i].handle = h;
}

void sora_sfx_stop_all(void) {
    if (G.state <= 0) return;
    SDL_LockMutex(G.lock);
    for (int i = 0; i < NVOICE; i++) G.v[i].id = -1;
    SDL_UnlockMutex(G.lock);
    for (int i = 0; i < 8; i++) G.loops[i].handle = -1;
    G.last_bark = -1;
}

// KH2's voice choice: equal shares when every weight is 0, else by weight (vtbl 3: -3 at weight 100 = mostly silent).
int sora_sfx_voice(int vtbl) {
    if (!init()) return -1;
    const VtblRow *r = NULL;
    for (uint32_t i = 0; i < G.h->n_vtbl; i++) if ((int)G.vt[i].vtbl == vtbl) { r = &G.vt[i]; break; }
    if (!r || !r->count) return -1;
    const Choice *c = G.choice + r->first;
    uint32_t total = 0;
    for (uint32_t i = 0; i < r->count; i++) total += c[i].weight;
    int pick = 0;
    if (total) {
        uint32_t x = (uint32_t)rand() % total, acc = 0;
        for (uint32_t i = 0; i < r->count; i++) { acc += c[i].weight; if (x < acc) { pick = (int)i; break; } }
    } else {
        pick = rand() % (int)r->count;
    }
    if (c[pick].sound < 0) return -1;
    // a bark still playing keeps the line unless the new one's priority is at least as high
    if (G.last_bark >= 0 && sora_sfx_playing(G.last_bark) && (int)r->priority < G.last_bark_pri) return -1;
    if (G.last_bark >= 0) sora_sfx_stop(G.last_bark);
    G.last_bark = start(c[pick].sound, 1.0f, 0, false, (int)r->priority);
    G.last_bark_pri = (int)r->priority;
    return G.last_bark;
}

int sora_sfx_hit(int atkp_row, float gain, const float pos[3]) {
    if (!init()) return -1;
    int id = -1;
    for (uint32_t i = 0; i < G.h->n_hits; i++) if ((int)G.hit[i].atkp == atkp_row) { id = (int)G.hit[i].sound; break; }
    if (id < 0 && G.h->n_hits) id = (int)G.hit[0].sound;  // an unknown row: the base Keyblade hit
    return sora_sfx_play_at(id, gain, pos);
}

int sora_sfx_motion(int slot, float prev, float clock, const float pos[3]) {
    if (!init() || slot < 0) return 0;
    int n = 0;
    // the triggers are sorted by slot then frame: find the slot's range (a linear scan: 172 rows)
    for (uint32_t i = 0; i < G.h->n_trig; i++) {
        const Trig *t = &G.trig[i];
        if ((int)t->slot < slot) continue;
        if ((int)t->slot > slot) break;
        float fr = (float)t->frame;
        if (!(prev < fr && fr <= clock)) continue;
        if (t->kind == 1) {
            if (sora_sfx_voice((int)t->vtbl) >= 0) n++;
        } else if (t->count) {
            uint32_t k = t->cycle ? G.cycle[i] % t->count : 0;
            if (t->cycle) G.cycle[i] = (uint8_t)((G.cycle[i] + 1) % t->count);
            int id = (int)G.trig_snd[t->first + k];
            if ((pos ? sora_sfx_play_at(id, 1.0f, pos) : sora_sfx_play(id, 1.0f)) >= 0) n++;
        }
    }
    return n;
}

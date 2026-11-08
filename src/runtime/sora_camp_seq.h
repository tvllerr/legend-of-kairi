// sora_camp_seq.h: KH2's camp (pause) menu sequence player for the KH2 pause menu (s78, slice 1).
// port of OpenKH (Apache-2.0), https://github.com/OpenKH/OpenKh
//   OpenKh.Kh2/Sequence.cs, OpenKh.Kh2/Layout.cs (data), OpenKh.Engine/Renders/SequenceRenderer.cs,
//   LayoutRenderer.cs, SpriteDrawingContext.cs, OpenKh.Game AnimatedSequenceFactory.cs (code, Tasks 3 / 4)
// Pure C, header-only: no Windows / game includes, host-tested by runtime/test/camp_test.c.
//
// camp_seq.bin ('SKCQ' v1, little-endian), baked by D:/SoraBotW/tools/hudbake (hudbake campseq) from
// kh2_fifth/original/menu/us/camp.2ld sequence 1 + msg/us/sys.bar + fontinfo.bar 'sys':
//   CsHeader, then the tables at their offsets (from the file start). Every struct below = the OpenKh
//   field order; hudbake writes the same fields in the same order with BinaryWriter.
//   msg table: n x { u16 key, u16 len, u8 bytes[len] } (sys-font bytes, keys = data/hud/camp_text.txt)
//   ascii: 128 bytes, ASCII -> sys byte (Encoders.InternationalSystem, 0 = unmapped)
//   adv: fontinfo.bar 'sys' advances (PS2 px), cell = byte - 0x20
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CS_MAGIC 0x51434B53u  // 'SKCQ'
#define CS_VERSION 1

#pragma pack(push, 1)
typedef struct { uint32_t off, count; } CsTable;
typedef struct {
    uint32_t magic, ver;
    CsTable spr, part, grp, anim, ag, l2grp, l2prop, msg, ascii, adv;
} CsHeader;
typedef struct {  // Sequence.Sprite
    int32_t left, top, right, bottom;
    float u_trans, v_trans;
    uint32_t col_left, col_top, col_right, col_bottom;
} CsSprite;
typedef struct { int32_t left, top, right, bottom, sprite; } CsPart;  // Sequence.SpritePart
typedef struct { uint16_t first, count; } CsGroup;                   // a sprite group = parts[first .. first+count)
typedef struct {  // Sequence.Animation
    int32_t flags, sprite_group, frame_start, frame_end;
    int32_t tx_start, tx_end, ty_start, ty_end;
    int32_t px_start, px_end, py_start, py_end;
    float rx_start, rx_end, ry_start, ry_end, rz_start, rz_end;
    float scale_start, scale_end, sx_start, sx_end, sy_start, sy_end;
    float curve_x_start, curve_y_start, curve_x_end, curve_y_end;
    float bounce_x_start, bounce_x_end, bounce_y_start, bounce_y_end;
    int16_t bounce_x_count, bounce_y_count;
    int32_t color_blend;
    uint32_t color_start, color_end;
} CsAnim;
typedef struct {  // Sequence.AnimationGroup = anims[first .. first+count)
    uint16_t first, count;
    int16_t do_not_loop, unk06;
    int32_t loop_start, loop_end, light_x, text_y, text_scale, ui_padding, text_x;
} CsAnimGroup;
typedef struct { uint16_t first, count; } CsL2Group;  // Layout.SequenceGroup = l2prop[first .. first+count)
typedef struct { int32_t tex, seq, ag, show_at, x, y; } CsLayoutProp;  // Layout.SequenceProperty
#pragma pack(pop)

_Static_assert(sizeof(CsHeader) == 88, "CsHeader");
_Static_assert(sizeof(CsSprite) == 40, "CsSprite");
_Static_assert(sizeof(CsAnim) == 144, "CsAnim");
_Static_assert(sizeof(CsAnimGroup) == 36, "CsAnimGroup");

#include <math.h>
#include <string.h>

// SoraHudQuad = the exe's quad (sora_hud.h, Task 6 of the slice); defined here too for the host test
#ifndef SORA_HUD_QUAD_DEFINED
#define SORA_HUD_QUAD_DEFINED
typedef struct {
    float x[4], y[4];   // virtual px (KH2's 512 x 416 frame, 16:9 = x -85..597), corners TL TR BL BR (OpenKH Vec0..3)
    float u[4], v[4];   // texels of the logical sheet (camp 256 x 256, font 512 x 256)
    uint32_t col[4];    // 0xAABBGGRR, 0x80 = 1.0 (multiply, then clamp)
    float wrap[4];      // UV repeat region, texels: l r t b (l == r: none in u, t == b: none in v)
    uint8_t tex;        // 0 camp sheet, 1 sys font, 2 solid colour (no texture)
    uint8_t blend;      // 0 normal, 1 add, 2 subtract (VK_BLEND_OP_REVERSE_SUBTRACT: dst - src)
} SoraHudQuad;
#endif

// ---- Task 3: SequenceRenderer + LayoutRenderer ----

// Sequence.Animation flags (OpenKh.Kh2/Sequence.cs)
#define CS_F_LINEAR      0x00000001  // DisableCurveFlag
#define CS_F_ACTIVE      0x00000002  // IsActiveFlag (feeds the child context)
#define CS_F_NO_BOUNCE   0x00000010
#define CS_F_NO_ROTATE   0x00000020
#define CS_F_NO_SCALE    0x00000040
#define CS_F_COLOR_START 0x00000080  // ColorInterpolationFlag: ColorStart only
#define CS_F_NO_COLOR    0x00000400  // ColorMaskFlag
#define CS_F_TRANS_START 0x00001000  // TranslationInterpolationFlag: TranslateStart only
#define CS_F_NO_PIVOT    0x00004000

typedef struct {
    const CsHeader *h;
    const CsSprite *spr; const CsPart *part; const CsGroup *grp; const CsAnim *anim; const CsAnimGroup *ag;
    const CsL2Group *l2g; const CsLayoutProp *l2;
    int n_spr, n_part, n_grp, n_anim, n_ag, n_l2g, n_l2;
    const uint8_t *msg; int n_msg, msg_bytes;  // { u16 key, u16 len, bytes }
    const uint8_t *ascii;                      // 128
    const uint8_t *adv; int n_adv;
} CampSeq;
typedef struct { SoraHudQuad *q; int n, cap; } CsOut;
typedef struct { float x, y, text_x, text_y, text_scale, ui_size, ui_pad; float col[4]; } CsChild;  // OpenKH ChildContext

static bool cs_table_(const CsTable *t, size_t elem, size_t n) {
    return t->off >= sizeof(CsHeader) && t->off <= n && (uint64_t)t->count * elem <= n - t->off;
}

static bool cs_load(CampSeq *s, const uint8_t *buf, size_t n) {
    memset(s, 0, sizeof *s);
    if (!buf || n < sizeof(CsHeader)) return false;
    const CsHeader *h = (const CsHeader *)buf;
    if (h->magic != CS_MAGIC || h->ver != CS_VERSION) return false;
    if (!cs_table_(&h->spr, sizeof(CsSprite), n) || !cs_table_(&h->part, sizeof(CsPart), n) ||
        !cs_table_(&h->grp, sizeof(CsGroup), n) || !cs_table_(&h->anim, sizeof(CsAnim), n) ||
        !cs_table_(&h->ag, sizeof(CsAnimGroup), n) || !cs_table_(&h->l2grp, sizeof(CsL2Group), n) ||
        !cs_table_(&h->l2prop, sizeof(CsLayoutProp), n) || !cs_table_(&h->msg, 4, n) ||
        !cs_table_(&h->ascii, 1, n) || h->ascii.count != 128 || !cs_table_(&h->adv, 1, n))
        return false;
    s->h = h;
    s->spr = (const CsSprite *)(buf + h->spr.off);        s->n_spr = (int)h->spr.count;
    s->part = (const CsPart *)(buf + h->part.off);        s->n_part = (int)h->part.count;
    s->grp = (const CsGroup *)(buf + h->grp.off);         s->n_grp = (int)h->grp.count;
    s->anim = (const CsAnim *)(buf + h->anim.off);        s->n_anim = (int)h->anim.count;
    s->ag = (const CsAnimGroup *)(buf + h->ag.off);       s->n_ag = (int)h->ag.count;
    s->l2g = (const CsL2Group *)(buf + h->l2grp.off);     s->n_l2g = (int)h->l2grp.count;
    s->l2 = (const CsLayoutProp *)(buf + h->l2prop.off);  s->n_l2 = (int)h->l2prop.count;
    s->ascii = buf + h->ascii.off;
    s->adv = buf + h->adv.off;                            s->n_adv = (int)h->adv.count;
    // every index in range, so the player never checks again
    for (int i = 0; i < s->n_part; i++) if (s->part[i].sprite < 0 || s->part[i].sprite >= s->n_spr) return false;
    for (int i = 0; i < s->n_grp; i++) if (s->grp[i].first + s->grp[i].count > s->n_part) return false;
    for (int i = 0; i < s->n_anim; i++) if (s->anim[i].sprite_group < 0 || s->anim[i].sprite_group >= s->n_grp) return false;
    for (int i = 0; i < s->n_ag; i++) if (s->ag[i].first + s->ag[i].count > s->n_anim) return false;
    for (int i = 0; i < s->n_l2g; i++) if (s->l2g[i].first + s->l2g[i].count > s->n_l2) return false;
    for (int i = 0; i < s->n_l2; i++) if (s->l2[i].ag < 0 || s->l2[i].ag >= s->n_ag || s->l2[i].show_at < 0) return false;
    // msg: walk the variable-length entries
    size_t p = h->msg.off;
    for (uint32_t i = 0; i < h->msg.count; i++) {
        if (n - p < 4) return false;
        uint16_t len; memcpy(&len, buf + p + 2, 2);
        if (n - p - 4 < len) return false;
        p += 4u + len;
    }
    s->msg = buf + h->msg.off; s->n_msg = (int)h->msg.count; s->msg_bytes = (int)(p - h->msg.off);
    return true;
}

static const uint8_t *cs_msg(const CampSeq *s, int key, int *len) {
    const uint8_t *p = s->msg;
    for (int i = 0; i < s->n_msg; i++) {
        uint16_t k, l; memcpy(&k, p, 2); memcpy(&l, p + 2, 2);
        if (k == key) { if (len) *len = l; return p + 4; }
        p += 4u + l;
    }
    if (len) *len = 0;
    return NULL;
}
static uint8_t cs_ascii(const CampSeq *s, char c) { return (unsigned char)c < 128 ? s->ascii[(unsigned char)c] : 0; }

static int cs_group_end(const CampSeq *s, int ag) {  // SequenceExtensions.GetFrameLength
    if (ag < 0 || ag >= s->n_ag) return 0;
    int m = 0;
    for (int i = 0; i < s->ag[ag].count; i++) { int e = s->anim[s->ag[ag].first + i].frame_end; if (e > m) m = e; }
    return m;
}

typedef struct {  // SequenceRenderer.Context
    int gframe, frame;
    float px, py, pivx, pivy, sx, sy, rx, ry, rz;
    float col[4];
    int blend;
} CsCtx_;

static void cs_col_(uint32_t c, float o[4]) {  // ConvertColor: 0x80 = 1.0
    for (int i = 0; i < 4; i++) o[i] = (float)((c >> (8 * i)) & 0xFF) / 128.0f;
}
static float cs_lerp_(float m, float a, float b) { return a * (1.0f - m) + b * m; }
static double cs_lerpd_(double m, double a, double b) { return a * (1.0 - m) + b * m; }
static uint32_t cs_pack_(const float c[4]) {
    uint32_t r = 0;
    for (int i = 0; i < 4; i++) {
        double v = round((double)c[i] * 128.0);
        r |= (uint32_t)(v < 0 ? 0 : v > 255 ? 255 : v) << (8 * i);
    }
    return r;
}

static void cs_draw_frame_(const CampSeq *s, CsOut *o, const CsCtx_ *c, const CsPart *pt) {
    if (!o || o->n >= o->cap) return;
    const CsSprite *f = &s->spr[pt->sprite];
    SoraHudQuad *q = &o->q[o->n++];
    memset(q, 0, sizeof *q);
    // SpriteDrawingContext: Position / DestinationSize / Traslate(pivot) / ScaleSize / RotateX,Y,Z(-r) / Traslate(position)
    float L = (float)pt->left, T = (float)pt->top, W = (float)(pt->right - pt->left), H = (float)(pt->bottom - pt->top);
    float vx[4] = { L, L + W, L, L + W }, vy[4] = { T, T, T + H, T + H };
    float cx = cosf(-c->rx), cy = cosf(-c->ry), cz = cosf(-c->rz), sz = sinf(-c->rz);
    for (int i = 0; i < 4; i++) {
        float x = (vx[i] + c->pivx) * c->sx, y = (vy[i] + c->pivy) * c->sy;
        y *= cx;  // CreateRotationX on (x, y, 0)
        x *= cy;  // CreateRotationY
        float x2 = x * cz - y * sz, y2 = x * sz + y * cz;  // CreateRotationZ (row vector)
        q->x[i] = x2 + c->px; q->y[i] = y2 + c->py;
    }
    float hs = f->u_trans != 0 ? f->u_trans * (float)c->gframe : 0, vs = f->v_trans != 0 ? f->v_trans * (float)c->gframe : 0;
    float l = (float)f->left + hs, r = (float)f->right + hs, t = (float)f->top + vs, b = (float)f->bottom + vs;
    q->u[0] = l; q->v[0] = t; q->u[1] = r; q->v[1] = t; q->u[2] = l; q->v[2] = b; q->u[3] = r; q->v[3] = b;
    uint32_t sc[4] = { f->col_left, f->col_top, f->col_right, f->col_bottom };
    for (int i = 0; i < 4; i++) {
        float k[4]; cs_col_(sc[i], k);
        for (int j = 0; j < 4; j++) k[j] *= c->col[j];
        q->col[i] = cs_pack_(k);
    }
    if (f->u_trans != 0) { q->wrap[0] = (float)(f->left < f->right ? f->left : f->right); q->wrap[1] = (float)(f->left < f->right ? f->right : f->left); }
    if (f->v_trans != 0) { q->wrap[2] = (float)(f->top < f->bottom ? f->top : f->bottom); q->wrap[3] = (float)(f->top < f->bottom ? f->bottom : f->top); }
    q->tex = 0;
    q->blend = (uint8_t)c->blend;
}

static void cs_draw_anim_(const CampSeq *s, CsOut *o, const CsCtx_ *parent, const CsAnim *a, CsChild *child) {
    if (parent->frame < a->frame_start || parent->frame > a->frame_end) return;
    CsCtx_ c = *parent;
    double delta = (double)(c.frame - a->frame_start) / (a->frame_end - a->frame_start);  // a 1-frame anim = NaN, as OpenKH
    float t = (a->flags & CS_F_LINEAR) ? (float)delta : (float)((sin(delta * M_PI - M_PI / 2.0) + 1.0) / 2.0);
    c.blend = a->color_blend;
    float tx = cs_lerp_(t, (float)a->tx_start, (float)a->tx_end), ty = cs_lerp_(t, (float)a->ty_start, (float)a->ty_end);
    if (!(a->flags & CS_F_TRANS_START)) { c.px += tx; c.py += ty; }
    else { c.px += (float)a->tx_start; c.py += (float)a->ty_start; }
    if (!(a->flags & CS_F_NO_SCALE)) {
        float sc = cs_lerp_(t, a->scale_start, a->scale_end);
        c.sx = sc * cs_lerp_(t, a->sx_start, a->sx_end);
        c.sy = sc * cs_lerp_(t, a->sy_start, a->sy_end);
    } else c.sx = c.sy = 1.0f;
    if (!(a->flags & CS_F_NO_COLOR)) {
        float k[4], k0[4], k1[4];
        cs_col_(a->color_start, k0); cs_col_(a->color_end, k1);
        for (int j = 0; j < 4; j++) k[j] = (a->flags & CS_F_COLOR_START) ? k0[j] : (float)cs_lerpd_(t, k0[j], k1[j]);
        for (int j = 0; j < 4; j++) c.col[j] *= k[j];
    }
    if (!(a->flags & CS_F_NO_ROTATE)) {
        c.rx = cs_lerp_(t, a->rx_start, a->rx_end);
        c.ry = cs_lerp_(t, a->ry_start, a->ry_end);
        c.rz = cs_lerp_(t, a->rz_start, a->rz_end);
    }
    if (!(a->flags & CS_F_NO_PIVOT)) {
        c.pivx += cs_lerp_(t, (float)a->px_start, (float)a->px_end);
        c.pivy += cs_lerp_(t, (float)a->py_start, (float)a->py_end);
    }
    if (!(a->flags & CS_F_NO_BOUNCE)) {
        float bx = (float)sin(cs_lerpd_(delta * a->bounce_x_count, 0, M_PI));
        float by = (float)sin(cs_lerpd_(delta * a->bounce_y_count, 0, M_PI));
        c.px += bx * cs_lerp_(t, a->bounce_x_start, a->bounce_x_end);
        c.py += by * cs_lerp_(t, a->bounce_y_start, a->bounce_y_end);
    }
    if ((a->flags & CS_F_ACTIVE) && child) {
        child->x = c.px + c.pivx; child->y = c.py + c.pivy;
        memcpy(child->col, c.col, sizeof c.col);
        if (a->flags & CS_F_TRANS_START) { child->x += tx - (float)a->tx_start; child->y += ty - (float)a->ty_start; }
    }
    const CsGroup *g = &s->grp[a->sprite_group];
    for (int i = 0; i < g->count; i++) cs_draw_frame_(s, o, &c, &s->part[g->first + i]);
}

static int cs_loop_(int min, int max, int val) {
    if (val < max) return val;
    if (max <= min) return min;
    int mod = (val - min) % (max - min);
    if (mod < 0) mod += max - min;
    return min + mod;
}

// = SequenceRenderer.Draw; child (may be NULL) = its CurrentChildContext
static bool cs_draw_group(const CampSeq *s, CsOut *o, int agi, int frame, float x, float y, float alpha, CsChild *child) {
    if (agi < 0 || agi >= s->n_ag) return false;
    const CsAnimGroup *g = &s->ag[agi];
    CsCtx_ c; memset(&c, 0, sizeof c);
    c.gframe = c.frame = frame; c.px = x; c.py = y;
    c.col[0] = c.col[1] = c.col[2] = 1.0f; c.col[3] = alpha;
    if (child) {  // GetActualFrame
        child->text_x = (float)g->text_x; child->text_y = (float)g->text_y; child->text_scale = (float)g->text_scale;
        child->ui_size = (float)g->light_x; child->ui_pad = (float)g->ui_padding;
    }
    int end = cs_group_end(s, agi);
    if (!g->do_not_loop) c.frame = cs_loop_(g->loop_start, g->loop_end ? g->loop_end : end, frame);
    for (int i = 0; i < g->count; i++) cs_draw_anim_(s, o, &c, &s->anim[g->first + i], child);
    if (g->count == 0) return false;
    return g->do_not_loop == 0 || c.frame < end;
}

// = LayoutRenderer.Draw (every prop on texture 0, sequence 1: hudbake checks that)
static void cs_draw_layout(const CampSeq *s, CsOut *o, int l2, int frame) {
    if (l2 < 0 || l2 >= s->n_l2g) return;
    const CsL2Group *g = &s->l2g[l2];
    for (int i = 0; i < g->count; i++) {
        const CsLayoutProp *p = &s->l2[g->first + i];
        int f = frame - p->show_at;
        if (f < 0) continue;
        cs_draw_group(s, o, p->ag, f, (float)p->x, (float)p->y, 1.0f, NULL);
    }
}
static bool cs_layout_last(const CampSeq *s, int l2, int frame) {  // = LayoutRenderer.IsLastFrame
    if (l2 < 0 || l2 >= s->n_l2g) return true;
    const CsL2Group *g = &s->l2g[l2];
    int len = 0;
    for (int i = 0; i < g->count; i++) {
        const CsLayoutProp *p = &s->l2[g->first + i];
        int e = cs_group_end(s, p->ag) + p->show_at;
        if (e > len) len = e;
    }
    return frame > len;
}

// ---- Task 4: AnimatedSequence tree (OpenKh.Game AnimatedSequenceFactory.cs) + KH2 sys-font text (campbake SysText) ----

#define CS_DEFAULT_STACK (-2147483648.0f)  // AnimatedSequenceDesc.DefaultStacking (int.MinValue)
enum { CS_BOTTOM_LEFT, CS_BOTTOM_CENTER, CS_BOTTOM_RIGHT, CS_CENTER, CS_TOP_CENTER, CS_TOP_LEFT };  // TextAnchor
// AnimationFlags (OpenKH.Game's own, not the sequence's)
#define CS_A_TEXT_TX           0x01  // TextTranslateX: the text skips the child context's TextPositionX
#define CS_A_NO_CHILD_TX       0x02  // NoChildTranslationX: children keep the parent context's x
#define CS_A_CHILD_STACK_H     0x04  // ChildStackHorizontally: x += own UiSize after each child
#define CS_A_STACK_NEXT_H      0x08  // StackNextChildHorizontally: x += the child's UiSize
#define CS_A_STACK_NEXT_V      0x10  // StackNextChildVertically: y += the child's UiSize
#define CS_A_TEXT_IGNORE_COLOR 0x20
#define CS_A_TEXT_IGNORE_SCALE 0x40

typedef struct CsDesc {  // = AnimatedSequenceDesc; start / loop / end = -1 none; text (literal) wins over msg_key (> 0)
    float x, y;
    int16_t start, loop, end;
    int16_t stack_index;
    float stack_w, stack_h;
    int msg_key;
    char text[24];
    uint8_t anchor, nkids;
    uint32_t flags;
    const struct CsDesc *kids;
} CsDesc;

#define CS_MAX_NODES 256
typedef struct {
    CsDesc d;          // d.kids unused here: kid0 / nkids below
    int kid0, nkids;   // children = n[kid0 .. kid0+nkids)
    bool root;         // RootAnimatedSequence (no sequence of its own)
    int anim, frame;
    bool running, is_end;
    CsChild ctx;       // the node's SequenceRenderer.CurrentChildContext: lives across frames, starts all 0
} CsNode;
typedef struct { CsNode n[CS_MAX_NODES]; int used; } CsTree;

static void cs_tree_clear(CsTree *t) { t->used = 0; }

static int cs_fill_(CsTree *t, int id, const CsDesc *d) {
    CsNode *n = &t->n[id];
    memset(n, 0, sizeof *n);
    n->d = *d; n->d.kids = NULL;
    n->anim = -1;
    if (!d->nkids || !d->kids) return 0;
    if (t->used + d->nkids > CS_MAX_NODES) return -1;
    n->kid0 = t->used; n->nkids = d->nkids; t->used += d->nkids;
    for (int i = 0; i < d->nkids; i++) if (cs_fill_(t, n->kid0 + i, &d->kids[i]) < 0) return -1;
    return 0;
}
// = AnimatedSequenceFactory.Create(descs): a root over n descs (children copied in); returns its id, -1 = pool full
static int cs_tree_add(CsTree *t, const CsDesc *d, int n) {
    if (n < 0 || t->used + 1 + n > CS_MAX_NODES) return -1;
    int id = t->used++;
    CsNode *r = &t->n[id];
    memset(r, 0, sizeof *r);
    r->root = true; r->anim = -1; r->kid0 = t->used; r->nkids = n; t->used += n;
    for (int i = 0; i < n; i++) if (cs_fill_(t, r->kid0 + i, &d[i]) < 0) return -1;
    return id;
}

static void cs_begin(CsTree *t, int id) {
    CsNode *n = &t->n[id];
    for (int i = 0; i < n->nkids; i++) cs_begin(t, n->kid0 + i);
    if (n->root) return;
    n->anim = n->d.start >= 0 ? n->d.start : n->d.loop;
    n->frame = 0;
    if (n->anim < 0) { n->anim = n->d.end; n->running = false; n->is_end = n->anim < 0; }
    else { n->running = true; n->is_end = false; }
}
static void cs_end(CsTree *t, int id) {
    CsNode *n = &t->n[id];
    for (int i = 0; i < n->nkids; i++) cs_end(t, n->kid0 + i);
    if (n->root) return;
    n->running = false;
    if (n->d.end >= 0) { n->anim = n->d.end; n->is_end = false; n->frame = 0; }
    else n->is_end = true;
}
static inline void cs_skip(CsTree *t, int id) {
    CsNode *n = &t->n[id];
    for (int i = 0; i < n->nkids; i++) cs_skip(t, n->kid0 + i);
    if (n->root) return;
    if (n->running) { if (n->anim == n->d.start) { n->anim = n->d.loop; n->frame = 0; } }
    else n->is_end = true;
}
static void cs_update(CsTree *t, int id, double dt) {
    (void)dt;
    CsNode *n = &t->n[id];
    if (!n->root && !n->is_end) n->frame++;
    for (int i = 0; i < n->nkids; i++) cs_update(t, n->kid0 + i, dt);
}
static bool cs_is_end(const CsTree *t, int id) {
    const CsNode *n = &t->n[id];
    if (!n->root) return n->is_end;
    for (int i = 0; i < n->nkids; i++) if (!cs_is_end(t, n->kid0 + i)) return false;
    return true;
}

static int cs_ctl_args_(int b) {  // control code argument bytes (OpenKh Msg.cs, as campbake SysText.Args)
    switch (b) {
    case 0x04: case 0x06: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0e: case 0x16: case 0x19:
    case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f: return 1;
    case 0x12: case 0x14: case 0x15: case 0x17: case 0x18: return 2;
    case 0x08: return 3;
    case 0x07: case 0x11: case 0x13: return 4;
    case 0x0f: return 5;
    case 0x05: return 6;
    default: return 0;
    }
}
// = campbake SysText.Draw: sys-font cells 18 x 24 (28 per row), x scale = scale * 0.85 (DrawContext.GlobalScale), space
// 0x01 = 6 px, 0x02 newline 24; o NULL = measure only. w / h = DrawContext Width / Height (NULL ok)
static void cs_text(const CampSeq *s, CsOut *o, const uint8_t *msg, int len, float x, float y, float scale,
                    const float col[4], float *w, float *h) {
    double cx = x, cy = y, sx = (float)(1.0 * scale * 0.85), sy = scale, wm = 0;
    uint32_t pc = (o && col) ? cs_pack_(col) : 0;
    for (int i = 0; i < len; i++) {
        int b = msg[i];
        if (b == 0) break;
        if (b == 0x01) { cx += 6 * sx; continue; }
        if (b == 0x02) { cx = x; cy += 24 * sy; continue; }
        if (b < 0x20) { i += cs_ctl_args_(b); continue; }
        int cell = b - 0x20;
        if (o && o->n < o->cap) {
            SoraHudQuad *q = &o->q[o->n++];
            memset(q, 0, sizeof *q);
            float l = (float)(cell % 28 * 18), t = (float)(cell / 28 * 24);
            float X0 = (float)cx, Y0 = (float)cy, X1 = (float)(cx + 18 * sx), Y1 = (float)(cy + 24 * sy);
            q->x[0] = X0; q->y[0] = Y0; q->x[1] = X1; q->y[1] = Y0; q->x[2] = X0; q->y[2] = Y1; q->x[3] = X1; q->y[3] = Y1;
            q->u[0] = l; q->v[0] = t; q->u[1] = l + 18; q->v[1] = t; q->u[2] = l; q->v[2] = t + 24; q->u[3] = l + 18; q->v[3] = t + 24;
            for (int k = 0; k < 4; k++) q->col[k] = pc;
            q->tex = 1;
        }
        cx += (cell < s->n_adv ? s->adv[cell] : 18) * sx;
        if (cx - x > wm) wm = cx - x;
    }
    if (w) *w = (float)wm;
    if (h) *h = (float)(cy - y + 24 * sy);
}

static int cs_node_msg_(const CampSeq *s, const CsNode *n, uint8_t *buf, int cap, const uint8_t **out) {
    if (n->d.text[0]) {  // MessageText: plain text through Encoders.InternationalSystem
        int len = 0;
        for (int i = 0; i < (int)sizeof n->d.text && n->d.text[i] && len < cap; i++) {
            uint8_t b = cs_ascii(s, n->d.text[i]);
            if (b) buf[len++] = b;
        }
        *out = buf;
        return len;
    }
    if (n->d.msg_key > 0) { int len = 0; *out = cs_msg(s, n->d.msg_key, &len); return *out ? len : 0; }
    *out = NULL;
    return 0;
}
static float cs_ui_size_(const CampSeq *s, const CsNode *n) {  // AnimatedSequence.UiSize = its group's UiPadding
    return n->anim >= 0 && n->anim < s->n_ag ? (float)s->ag[n->anim].ui_padding : 0;
}

// = AnimatedSequence.Draw(ChildContext): pc = the parent's (or a fresh root) child context
static void cs_draw_node_(CsTree *t, const CampSeq *s, CsOut *o, int id, const CsChild *pc) {
    CsNode *n = &t->n[id];
    if (n->anim < 0 || n->anim >= s->n_ag) return;
    const CsAnimGroup *g = &s->ag[n->anim];
    float ax = (n->d.stack_w != CS_DEFAULT_STACK ? n->d.stack_w : (float)g->text_x) * n->d.stack_index;
    float ay = (n->d.stack_h != CS_DEFAULT_STACK ? n->d.stack_h : (float)g->text_x) * n->d.stack_index;  // OpenKH quirk: TextPositionX
    if (!cs_draw_group(s, o, n->anim, n->frame, n->d.x + ax + pc->x, n->d.y + ay + pc->y, pc->col[3], &n->ctx)) {
        if (n->running) { n->anim = n->d.loop; n->frame = 0; }
        else n->is_end = true;
    }
    CsChild *cc = &n->ctx;
    uint8_t buf[sizeof n->d.text]; const uint8_t *m;
    int len = cs_node_msg_(s, n, buf, (int)sizeof buf, &m);
    if (len > 0 && o) {
        float ts = (pc->text_scale == 0 || (n->d.flags & CS_A_TEXT_IGNORE_SCALE)) ? 0.7f : pc->text_scale / 22.0f;
        float w, h;
        cs_text(s, NULL, m, len, 0, 0, ts, NULL, &w, &h);
        float xp = 0, yp = 0;
        switch (n->d.anchor) {
        default: case CS_BOTTOM_LEFT: break;
        case CS_BOTTOM_CENTER: xp = -w / 2; break;
        case CS_BOTTOM_RIGHT: xp = pc->ui_size - w; break;
        case CS_CENTER: xp = -w / 2; yp = -h / 2; break;
        case CS_TOP_CENTER: xp = -w / 2; yp = -h; break;
        case CS_TOP_LEFT: yp = -h; break;
        }
        if (!(n->d.flags & CS_A_TEXT_TX)) xp += cc->text_x;
        float tc[4] = { cc->col[0], cc->col[1], cc->col[2], cc->col[3] };
        if (n->d.flags & CS_A_TEXT_IGNORE_COLOR) tc[0] = tc[1] = tc[2] = 1.0f;
        const float sh[4] = { 0, 0, 0, tc[3] * 0.75f };
        cs_text(s, o, m, len, cc->x + xp + 1.5f, cc->y + cc->text_y + yp + 1.5f, ts, sh, NULL, NULL);
        cs_text(s, o, m, len, cc->x + xp, cc->y + cc->text_y + yp, ts, tc, NULL, NULL);
    }
    if (n->d.flags & CS_A_NO_CHILD_TX) cc->x = pc->x;
    float oy = cc->y;
    for (int i = 0; i < n->nkids; i++) {
        const CsNode *k = &t->n[n->kid0 + i];
        cc->y = oy + cc->ui_pad * k->d.stack_index;
        cs_draw_node_(t, s, o, n->kid0 + i, cc);
        if (n->d.flags & CS_A_CHILD_STACK_H) cc->x += cc->ui_size;
        if (n->d.flags & CS_A_STACK_NEXT_H) cc->x += cs_ui_size_(s, k);
        if (n->d.flags & CS_A_STACK_NEXT_V) oy += cs_ui_size_(s, k);
    }
}
// = IAnimatedSequence.Draw(x, y). Not const: Draw moves the state on (loop / IsEnd), as OpenKH. o NULL = state only.
static void cs_draw(CsTree *t, const CampSeq *s, CsOut *o, int id, float x, float y) {
    CsNode *n = &t->n[id];
    CsChild c = { .x = x, .y = y, .col = { 1, 1, 1, 1 } };
    if (!n->root) { cs_draw_node_(t, s, o, id, &c); return; }
    for (int i = 0; i < n->nkids; i++) { CsChild k = c; cs_draw_node_(t, s, o, n->kid0 + i, &k); }
}

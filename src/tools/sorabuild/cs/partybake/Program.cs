// partybake (a parametrized copy of sorabake for Donald / Goofy; sorabake itself is untouched): the offline bake of KH2 Sora's motion data for runtime/sora.c (SORA_DRIVER_RECON.md 5.3).
//   sorabake info <slot>...          header, root curves, triggers of mset slots
//   sorabake emu <slot> [frames]     Kh2AnimEmu bone 0 vs our root curve evaluation (Hermite convention check)
//   sorabake bake <motion.bin> <params.h>
//   sorabake model <model.bin>        mesh + skin + textures of P_EX100 + W_EX010 for the host's draw pass (format: Model())
//   sorabake pose <pose.bin> [slot..] Kh2AnimEmu bone matrices per GFR frame, root move taken out (format: Pose())
//   sorabake obj <model.bin> <pose.bin> <slot> <frame> <out.obj>   skins like the runtime does (checks both files)
//   sorabake bind [sora_bind.txt]     Sora's bind (rest) pose per bone: parent, depth, kids, verts, world pos, idle pos
//   sorabake ragmap [sora_ragmap.h]   Sora skeleton (parents, bind pos) + Sora -> Link joint map for the ragdoll retarget
//   sorabake view <model.bin> <pose.bin> <slot> <frame|-1=bind> <out.png>   software render + ragmap segments overlay
// Inputs: KH2 extract P_EX100.mdlx/.mset (kh2_sixth), 00battle.bin + 03system.bin (kh2_first).
// sora_motion.bin (little-endian), magic 'SKRM' v1:
//   header  u32 magic, u32 version, u32 n_slots, u32 reserved
//   dir[n]  u16 slot, u16 frames (GFR, 60 fps), u16 loop (GFR, from FrameReturn), u16 flags (1 loop, 2 has_root,
//           4 has_root_yaw), u32 off_root, u32 off_bits, u32 off_ftrig, u16 n_ftrig, u16 start (GFR, FrameStart*2)
//   root    (frames + 1) x {f32 x, y, z (cm, entity frame: +x left, +y up, +z forward), f32 yaw (rad)}: the root's
//           position at GFR frame f of the motion relative to frame 0 (cumulative, not deltas; sample between frames
//           linearly). Only when flags & 2.
//   bits    frames x u32: range triggers active at frame f (bit map: RangeBit below)
//   ftrig   n x {u16 frame (motion-relative GFR), u8 id, u8 nparam, i16 p1, i16 p2}
using OpenKh.Kh2;
using System; using System.Collections.Generic; using System.IO; using System.Linq; using System.Text;

string Kh2 = Environment.GetEnvironmentVariable("SORABAKE_KH2");   // KH2 extract root; required
if (string.IsNullOrEmpty(Kh2)) { Console.Error.WriteLine("partybake: set SORABAKE_KH2 to the KH2 extract root"); Environment.Exit(2); }
string Env(string k, string d) { var v = Environment.GetEnvironmentVariable(k); return string.IsNullOrEmpty(v) ? d : v; }
// partybake: PARTYBAKE_MODEL = P_EX100 (Sora) | P_EX020 (Donald) | P_EX030 (Goofy); PARTYBAKE_WEAPON = W_EX010 | W_EX020 | W_EX030;
// PARTYBAKE_WJOINT = the bone the weapon hangs on (objentry WeaponJoint: Sora 178, Donald 131, Goofy 101); PARTYBAKE_PRTY = prty NeoMoveset index (Sora 1, Donald 17, Goofy 20)
string actorName = Env("PARTYBAKE_MODEL", "P_EX100"), weaponName = Env("PARTYBAKE_WEAPON", "W_EX010");
string msetPath = Env("PARTYBAKE_MSET", $"{Kh2}/kh2_sixth/original/obj/{actorName}.mset"), mdlxPath = $"{Kh2}/kh2_sixth/original/obj/{actorName}.mdlx";
// The Keyblade (Kingdom Key): its own skeleton + mset (same slot layout as Sora's), hung on Sora's bone 178 (PHH's bake:
// weapons [{bone 178, bones 12}]); baked into the same files as bones 228.. so the runtime draws it with Sora.
string kbMdlxPath = File.Exists($"{Kh2}/kh2_first/original/obj/{weaponName}.mdlx") ? $"{Kh2}/kh2_first/original/obj/{weaponName}.mdlx" : $"{Kh2}/kh2_sixth/original/obj/{weaponName}.mdlx";
string kbMsetPath = Env("PARTYBAKE_KBMSET", $"{Kh2}/kh2_sixth/original/obj/{weaponName}.mset");  // (s133 limits: PARTYBAKE_MSET / _KBMSET = a carved Limit mset, tools_s133l/mkmset.py)
int KbParent = int.Parse(Env("PARTYBAKE_WJOINT", "178"));
int PrtyRecord = int.Parse(Env("PARTYBAKE_PRTY", "1"));
bool Seek = Env("PARTYBAKE_SEEK", "1") == "1";  // (s58/s59: see KbStart; default ON since s59, PARTYBAKE_SEEK=0 = the old start-at-0 bake)
string battlePath = $"{Kh2}/kh2_first/original/00battle.bin", systemPath = $"{Kh2}/kh2_first/original/03system.bin";
var mset = Bar.Read(new MemoryStream(File.ReadAllBytes(msetPath))).ToList();

switch (args.Length > 0 ? args[0] : "") {
case "info":
    foreach (var a in args.Skip(1)) Info(int.Parse(a));
    break;
case "emu":
    Emu(int.Parse(args[1]), args.Length > 2 ? int.Parse(args[2]) : -1);
    break;
case "bake":
    Bake(args[1], args[2]);
    break;
case "model":
    Model(args[1]);
    break;
case "pose":
    Pose(args[1], args.Skip(2).Select(int.Parse).ToList());
    break;
case "wpose":  // (s58 helper GOOFYSHIELD) the weapon object's own motion: wpose <out.bin> <slot>...
    WPose(args[1], args.Skip(2).Select(int.Parse).ToList());
    break;
case "kbinfo":
    KbInfo();
    break;
case "obj":
    Obj(args[1], args[2], int.Parse(args[3]), int.Parse(args[4]), args[5]);
    break;
case "hits":
    HitsInfo(args.Skip(1).Select(int.Parse).ToList());
    break;
case "hitbake":
    HitBake(args[1]);
    break;
case "bind":
    Bind(args.Length > 1 ? args[1] : "sora_bind.txt");
    break;
case "ragmap":
    RagMap(args.Length > 1 ? args[1] : "sora_ragmap.h");
    break;
case "view":
    View(args[1], args[2], int.Parse(args[3]), int.Parse(args[4]), args[5]);
    break;
default:
    Console.WriteLine($"partybake [{actorName} + {weaponName} on bone {KbParent}, prty {PrtyRecord}] info <slot>... | emu <slot> [frames] | bake <motion.bin> <params.h> | model | pose | obj | hits | hitbake | bind [txt] | ragmap [h] | view <model> <pose> <slot> <frame|-1> <png>");
    break;
}

Slot Load(int slot) {
    var e = mset[slot];
    if (e.Stream.Length == 0) return null;
    var anb = Bar.Read(e.Stream);
    var me = anb.FirstOrDefault(x => x.Type == Bar.EntryType.Motion);
    if (me == null) return null;
    me.Stream.Position = 0;
    if (!Motion.isInterpolated(me.Stream)) return null;
    me.Stream.Position = 0;
    var m = new Motion.InterpolatedMotion(me.Stream);
    var te = anb.FirstOrDefault(x => x.Type == Bar.EntryType.MotionTriggers);
    MotionTrigger trig = null;
    if (te != null && te.Stream.Length > 0) { te.Stream.Position = 0; trig = new MotionTrigger(te.Stream); }
    return new Slot(slot, e.Name, m, trig);
}

void Info(int slot) {
    var s = Load(slot);
    if (s == null) { Console.WriteLine($"slot {slot}: empty / not interpolated"); return; }
    var fd = s.M.InterpolatedMotionHeader.FrameData;
    Console.WriteLine($"slot {slot} {s.Name}: start {fd.FrameStart} end {fd.FrameEnd} return {fd.FrameReturn} fps {fd.FramesPerSecond}" +
                      $" -> GFR start {s.Start} frames {s.Frames} loop {s.Loop}");
    var ids = s.M.RootPosition.FCurveId;
    Console.WriteLine($"  root static T ({s.M.RootPosition.TranslateX:F1} {s.M.RootPosition.TranslateY:F1} {s.M.RootPosition.TranslateZ:F1}) curves [{string.Join(" ", ids)}]");
    for (int c = 0; c < 9; c++) {
        var keys = s.Keys(c);
        if (keys == null) continue;
        Console.WriteLine($"  ch{c}: " + string.Join(" ", keys.Select(k => $"({k.t:0.##} {k.v:0.##} {k.type} l{k.l:0.###} r{k.r:0.###})")));
        Console.WriteLine($"    end value {s.Eval(c, s.Start + s.Frames):F2}");
    }
    if (s.T != null) {
        foreach (var r in s.T.RangeTriggerList)
            Console.WriteLine($"  range {r.Trigger} {r.StartFrame}..{r.EndFrame} n{r.ParamSize} {r.Param1} {r.Param2}");
        foreach (var f in s.T.FrameTriggerList)
            Console.WriteLine($"  frame {f.Trigger} @{f.Frame} n{f.ParamSize} {f.Param1} {f.Param2}");
    }
}

// Kh2AnimEmu runs KH2's own PS2 motion code: if its bone 0 carries the root move, it checks our curve evaluation.
void Emu(int slot, int frames) {
    var s = Load(slot);
    var anb = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(File.ReadAllBytes(msetPath))).ToList()).GetAnb(slot);   // (fresh streams: Load read them)
    var p = anb.GetAnimProvider(new MemoryStream(File.ReadAllBytes(mdlxPath)));
    if (frames < 0) frames = s.Frames;
    Console.WriteLine($"emu frames {p.FrameCount} loop {p.FrameLoop} end {p.FrameEnd} fps {p.FramePerSecond}");
    var M = p.ProvideMatrices(0);
    var b0 = M[0].Translation;
    var hip = s.JointKeys(0, 8);
    Console.WriteLine($"bone 0 translate z curve: {(hip == null ? "none" : string.Join(" ", hip.Select(k => $"({k.t:0.#} {k.v:0.##} {k.type})")))}");
    for (int f = 0; f <= frames; f++) {
        if (f > 0) M = p.ProvideMatrices(1.0);   // (60 Hz ticks: one GFR frame)
        var t = M[0].Translation - b0;
        float h = s.Eval(8, s.Start + f) - s.Eval(8, s.Start), l = s.EvalLinear(8, s.Start + f) - s.EvalLinear(8, s.Start);
        float j = Slot.EvalKeys(hip, s.Start + f) - Slot.EvalKeys(hip, s.Start), jl = Slot.EvalKeys(hip, s.Start + f, true) - Slot.EvalKeys(hip, s.Start, true);
        Console.WriteLine($"  f{f,3} emu bone0 dz {t.Z,8:F2}  root+hip z hermite {h + j,8:F2} linear {l + jl,8:F2}  (root {h,7:F2} hip {j,6:F2})");
    }
}

// Range trigger id -> runtime bit (SORA_DRIVER_RECON.md 5.3)
static int RangeBit(int id) => id switch {
    10 or 33 => 23, <= 16 => id, 22 => 22, 24 => 24, 27 => 27, 28 => 28, 29 => 29,
    41 => 17, 42 => 18, 47 => 19, 49 => 20, 50 => 21, _ => -1 };

void Bake(string binPath, string hPath) {
    var slots = new List<Slot>();
    for (int i = 0; i < mset.Count; i++) { var s = Load(i); if (s != null) slots.Add(s); }
    var dir = new MemoryStream(); var data = new MemoryStream();
    var dw = new BinaryWriter(dir); var w = new BinaryWriter(data);
    int dataBase = 16 + slots.Count * 24;
    int unmapped = 0;
    foreach (var s in slots) {
        int flags = (s.Loops ? 1 : 0) | (s.HasRoot ? 2 : 0) | (s.M.RootPosition.FCurveId[4] >= 0 ? 4 : 0);
        uint offRoot = 0;
        if (s.HasRoot) {
            offRoot = (uint)(dataBase + data.Position);
            float[] o = { s.Eval(6, s.Start), s.Eval(7, s.Start), s.Eval(8, s.Start), s.Eval(4, s.Start) };
            for (int f = 0; f <= s.Frames; f++) {
                w.Write(s.Eval(6, s.Start + f) - o[0]); w.Write(s.Eval(7, s.Start + f) - o[1]);
                w.Write(s.Eval(8, s.Start + f) - o[2]); w.Write(s.Eval(4, s.Start + f) - o[3]);
            }
        }
        uint offBits = (uint)(dataBase + data.Position);
        for (int f = 0; f < s.Frames; f++) {
            uint b = 0;
            if (s.T != null) foreach (var r in s.T.RangeTriggerList) {
                int bit = RangeBit(r.Trigger);
                if (bit < 0) { if (f == 0) unmapped++; continue; }
                int end = r.EndFrame < 0 ? int.MaxValue : r.EndFrame;   // (-1 = to the end)
                if (f >= r.StartFrame && f <= end) b |= 1u << bit;
            }
            w.Write(b);
        }
        uint offF = (uint)(dataBase + data.Position);
        var ft = s.T?.FrameTriggerList ?? new List<MotionTrigger.FrameTrigger>();
        foreach (var f in ft) { w.Write((ushort)f.Frame); w.Write(f.Trigger); w.Write(f.ParamSize); w.Write(f.Param1); w.Write(f.Param2); }
        dw.Write((ushort)s.Index); dw.Write((ushort)s.Frames); dw.Write((ushort)s.Loop); dw.Write((ushort)flags);
        dw.Write(offRoot); dw.Write(offBits); dw.Write(offF); dw.Write((ushort)ft.Count); dw.Write((ushort)s.Start);
    }
    using (var o = new BinaryWriter(File.Create(binPath))) {
        o.Write(0x4D524B53u); o.Write(1u); o.Write((uint)slots.Count); o.Write(0u);
        o.Write(dir.ToArray()); o.Write(data.ToArray());
    }
    Console.WriteLine($"{binPath}: {slots.Count} slots, {16 + dir.Length + data.Length} bytes ({unmapped} unmapped range triggers)");
    File.WriteAllText(hPath, Params(slots));
    Console.WriteLine($"{hPath} written");
}

// ------------------------------------------------------------------------------------------- draw data (step 4)
// sora_model.bin (little-endian), magic 'SKRG' v1. KH2 model space (cm, +x left, +y up, +z forward):
//   header  u32 magic, version, n_bones, n_groups, n_tex, n_verts, n_infl, n_idx
//   tex[]   u32 w, h, off (w x h RGBA8, alpha already 0..255), addr (TextureAddressMode as stored)
//   group[] u32 tex, attrib (KH2 group bits: 0 alpha phase), first_idx, n_idx
//   vert[]  u32 first_infl, n_infl, f32 u, v (texture units, 0..1 over the image)
//   infl[]  u32 bone, f32 x, y, z, w (position in the bone's frame premultiplied by the weight: v = sum infl * M[bone])
//   idx[]   u32 vertex index (triangle list)
void Model(string outPath) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(mdlxPath)));
    var model = OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream);
    var tex = ModelTexture.Read(bar.First(e => e.Type == Bar.EntryType.ModelTexture).Stream);
    var kbBar = Bar.Read(new MemoryStream(File.ReadAllBytes(kbMdlxPath)));
    var kb = OpenKh.Kh2.Models.ModelSkeletal.Read(kbBar.First(e => e.Type == Bar.EntryType.Model).Stream);
    var kbTex = ModelTexture.Read(kbBar.First(e => e.Type == Bar.EntryType.ModelTexture).Stream);
    var verts = new List<(int fi, int ni, float u, float v)>(); var infl = new List<(int b, System.Numerics.Vector4 p)>();
    var idx = new List<int>(); var groups = new List<(uint tex, int attr, int fi, int ni)>();
    // group attrib bit 8 (not a KH2 bit) = Keyblade group: the runtime can hide it on its own
    void Add(OpenKh.Kh2.Models.ModelSkeletal md, int boneBase, int texBase, int attrOr) {
        foreach (var g in md.Groups) {
            int fi = idx.Count, vb = verts.Count;
            foreach (var v in g.Mesh.Vertices) {
                verts.Add((infl.Count, v.BPositions.Count, v.U / 4096f, v.V / 4096f));
                foreach (var b in v.BPositions) {
                    var p = b.Position;
                    if (p.W == 0) p = new System.Numerics.Vector4(p.X, p.Y, p.Z, 1);   // (one bone: stored without a weight)
                    infl.Add((boneBase + b.BoneIndex, p));
                }
            }
            foreach (var t in g.Mesh.Triangles) foreach (var i in t) idx.Add(vb + i);
            groups.Add((g.Header.TextureIndex + (uint)texBase, g.Header.AttributesBitfield | attrOr, fi, idx.Count - fi));
        }
    }
    Add(model, 0, 0, 0);
    Add(kb, model.Bones.Count, tex.Images.Count, 0x100);
    var images = tex.Images.Concat(kbTex.Images).ToList();
    int nBones = model.Bones.Count + kb.Bones.Count;
    var texData = new List<byte[]>();
    foreach (var im in images) {
        var px = OpenKh.Imaging.ImageExtensions.ToBgra32(im);
        int amax = 0;
        for (int i = 3; i < px.Length; i += 4) amax = Math.Max(amax, px[i]);
        if (amax <= 0x80) for (int i = 3; i < px.Length; i += 4) px[i] = (byte)Math.Min(255, px[i] * 2);   // (PS2 alpha: 0x80 = 1)
        texData.Add(px);
        Console.WriteLine($"tex {texData.Count - 1}: {im.Size.Width}x{im.Size.Height} {im.PixelFormat} alpha max {amax} addr {im.TextureAddressMode}");
    }
    int head = 32, texDir = head, grpDir = texDir + 16 * texData.Count, vOff = grpDir + 16 * groups.Count;
    int iOff = vOff + 16 * verts.Count, xOff = iOff + 20 * infl.Count, dOff = xOff + 4 * idx.Count;
    using var w = new BinaryWriter(File.Create(outPath));
    w.Write(0x47524B53u); w.Write(1u); w.Write((uint)nBones); w.Write((uint)groups.Count); w.Write((uint)texData.Count);
    w.Write((uint)verts.Count); w.Write((uint)infl.Count); w.Write((uint)idx.Count);
    int o = dOff;
    for (int i = 0; i < texData.Count; i++) {
        var im = images[i];
        w.Write((uint)im.Size.Width); w.Write((uint)im.Size.Height); w.Write((uint)o); w.Write((uint)im.TextureAddressMode.GetHashCode());
        o += texData[i].Length;
    }
    foreach (var g in groups) { w.Write(g.tex); w.Write((uint)g.attr); w.Write((uint)g.fi); w.Write((uint)g.ni); }
    foreach (var v in verts) { w.Write((uint)v.fi); w.Write((uint)v.ni); w.Write(v.u); w.Write(v.v); }
    foreach (var x in infl) { w.Write((uint)x.b); w.Write(x.p.X); w.Write(x.p.Y); w.Write(x.p.Z); w.Write(x.p.W); }
    foreach (var i in idx) w.Write((uint)i);
    foreach (var t in texData) w.Write(t);
    Console.WriteLine($"{outPath}: {nBones} bones ({kb.Bones.Count} Keyblade), {groups.Count} groups ({string.Join(" ", groups.Select(g => $"t{g.tex}/a{g.attr:X}/{g.ni / 3}"))}), " +
                      $"{verts.Count} verts, {infl.Count} influences, {idx.Count / 3} tris, {texData.Count} textures, {o} bytes");
}

// sora_pose.bin, magic 'SKRP' v1:
//   header  u32 magic, version, n_slots, n_bones
//   dir[n]  u16 slot, u16 poses (= GFR frames + 1, pose f at GFR frame f of the motion), u32 off
//   pose    poses x bones x 12 f32: rows r0 r1 r2 r3 of the bone's model matrix (v' = x r0 + y r1 + z r2 + w r3), with
//           the root move of sora_motion.bin (translation + yaw relative to frame 0) taken out: drawn at the entity,
//           which the runtime moves by that same root delta, it lands where KH2 draws it.
System.Numerics.Matrix4x4 RootAt(Slot s, int f) =>
    System.Numerics.Matrix4x4.CreateRotationY(s.Eval(4, s.Start + f)) *
    System.Numerics.Matrix4x4.CreateTranslation(s.Eval(6, s.Start + f), s.Eval(7, s.Start + f), s.Eval(8, s.Start + f));

// (s58 helper GOOFYSHIELD) PARTYBAKE_SEEK=1: start each slot's emu at its FrameStart (Slot.Start, GFR) instead of 0.
// Without it a motion whose keys begin late (Goofy A402 slot 772: FrameStart 65) bakes as the pre-start pose, frozen.
// (s59 helper PARTYBAKE) default ON for pose, hitbake and wpose. Verified (recon/S59_HELPER_PARTYBAKE.md): every
// start-0 slot bakes byte-identical; a start-N slot is the old bake shifted by N (exact for slots without root
// motion); a negative start (Goofy A201 slot 46: FrameStart -1 = GFR -2) is a true 2-frame backwards shift, the emu
// has keys there. Sora's root curves (RootAt: s.Start + f) were already on this timeline, now the bones are too.
int KbStart(int slot) {
    var e = Bar.Read(new MemoryStream(File.ReadAllBytes(kbMsetPath))).ToList()[slot];
    var me = Bar.Read(e.Stream).FirstOrDefault(x => x.Type == Bar.EntryType.Motion);
    if (me == null) return 0;
    me.Stream.Position = 0;
    if (!Motion.isInterpolated(me.Stream)) return 0;
    me.Stream.Position = 0;
    return (int)Math.Round(new Motion.InterpolatedMotion(me.Stream).InterpolatedMotionHeader.FrameData.FrameStart * 2);
}
void Pose(string outPath, List<int> only) {
    var slots = new List<Slot>();
    for (int i = 0; i < mset.Count; i++) { if (only.Count > 0 && !only.Contains(i)) continue; var s = Load(i); if (s != null) slots.Add(s); }
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(mdlxPath)));
    int nbS = OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream).Bones.Count;
    var msetBytes = File.ReadAllBytes(msetPath); var mdlxBytes = File.ReadAllBytes(mdlxPath);
    var kbMsetBytes = File.ReadAllBytes(kbMsetPath); var kbMdlxBytes = File.ReadAllBytes(kbMdlxPath);
    var kbEntries = Bar.Read(new MemoryStream(kbMsetBytes)).ToList();
    var kbAt = (int slot) => new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(kbMsetBytes)).ToList()).GetAnb(slot).GetAnimProvider(new MemoryStream(kbMdlxBytes));
    var kbIdle = kbAt(0).ProvideMatrices(0);   // slots without a Keyblade motion: A000's first pose (the grip at rest)
    int nbK = OpenKh.Kh2.Models.ModelSkeletal.Read(Bar.Read(new MemoryStream(kbMdlxBytes)).First(e => e.Type == Bar.EntryType.Model).Stream).Bones.Count;
    int nb = nbS + nbK;
    var noKb = new List<string>();
    var data = new MemoryStream(); var dw = new BinaryWriter(data); var dir = new List<(int slot, int n, long off)>();
    int head = 16 + 8 * slots.Count;
    var sw = System.Diagnostics.Stopwatch.StartNew();
    foreach (var s in slots) {
        var anb = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(msetBytes)).ToList()).GetAnb(s.Index);
        var p = anb.GetAnimProvider(new MemoryStream(mdlxBytes));
        bool hasKb = s.Index < kbEntries.Count && kbEntries[s.Index].Stream.Length > 0;
        var kp = hasKb ? kbAt(s.Index) : null;
        if (!hasKb) noKb.Add($"{s.Index} {s.Name}");
        dir.Add((s.Index, s.Frames + 1, head + data.Position));
        for (int f = 0; f <= s.Frames; f++) {
            var M = p.ProvideMatrices(f == 0 ? (Seek ? s.Start : 0) : 1.0);
            var K = hasKb ? kp.ProvideMatrices(f == 0 ? (Seek ? KbStart(s.Index) : 0) : 1.0) : kbIdle;
            if (M.Length < nbS || K.Length < nbK) throw new Exception($"slot {s.Index}: {M.Length} + {K.Length} matrices for {nbS} + {nbK} bones");
            System.Numerics.Matrix4x4.Invert(RootAt(s, f), out var rinv);
            var strip = s.HasRoot ? rinv * RootAt(s, 0) : System.Numerics.Matrix4x4.Identity;
            for (int b = 0; b < nb; b++) {
                var m = b < nbS ? M[b] * strip : K[b - nbS] * M[KbParent] * strip;
                dw.Write(m.M11); dw.Write(m.M12); dw.Write(m.M13); dw.Write(m.M21); dw.Write(m.M22); dw.Write(m.M23);
                dw.Write(m.M31); dw.Write(m.M32); dw.Write(m.M33); dw.Write(m.M41); dw.Write(m.M42); dw.Write(m.M43);
            }
            if (s.HasRoot && (f == 0 || f == s.Frames || f % 12 == 0)) {
                var t0 = M[0].Translation; var t1 = (M[0] * strip).Translation;
                Console.WriteLine($"  slot {s.Index} {s.Name} f{f}: bone0 ({t0.X:F1} {t0.Y:F1} {t0.Z:F1}) stripped ({t1.X:F1} {t1.Y:F1} {t1.Z:F1})");
            }
        }
    }
    using (var w = new BinaryWriter(File.Create(outPath))) {
        w.Write(0x50524B53u); w.Write(1u); w.Write((uint)slots.Count); w.Write((uint)nb);
        foreach (var d in dir) { w.Write((ushort)d.slot); w.Write((ushort)d.n); w.Write((uint)d.off); }
        w.Write(data.ToArray());
    }
    Console.WriteLine($"{outPath}: {slots.Count} slots, {nb} bones, {head + data.Length} bytes, {sw.ElapsedMilliseconds} ms");
    Console.WriteLine($"slots without a Keyblade motion ({noKb.Count}, idle grip used): {string.Join(", ", noKb)}");
}

// Keyblade recon: W_EX010 skeleton, its mset vs Sora's, P_EX100's objentry weapon joint, Sora's matrix count.
void KbInfo() {
    var ents = Objentry.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_first/original/00objentry.bin")));
    foreach (var e in ents.Where(e => e.ModelName == actorName || e.ModelName == weaponName))
        Console.WriteLine($"objentry {e.ObjectId}: {e.ModelName} type {e.ObjectType} weaponJoint {e.WeaponJoint} anim {e.AnimationName}");
    var kbBar = Bar.Read(new MemoryStream(File.ReadAllBytes(kbMdlxPath)));
    foreach (var e in kbBar) Console.WriteLine($"kb bar {e.Name} {e.Type} {e.Stream.Length}");
    var kb = OpenKh.Kh2.Models.ModelSkeletal.Read(kbBar.First(e => e.Type == Bar.EntryType.Model).Stream);
    foreach (var b in kb.Bones) Console.WriteLine($"kb bone {b.Index} parent {b.ParentIndex} T ({b.TranslationX:F1} {b.TranslationY:F1} {b.TranslationZ:F1}) R ({b.RotationX:F2} {b.RotationY:F2} {b.RotationZ:F2})");
    Console.WriteLine($"kb groups {kb.Groups.Count} verts {kb.Groups.Sum(g => g.Mesh.Vertices.Count)}");
    var kbm = Bar.Read(new MemoryStream(File.ReadAllBytes(kbMsetPath))).ToList();
    int nonEmpty = kbm.Count(e => e.Stream.Length > 0);
    Console.WriteLine($"kb mset {kbm.Count} entries ({nonEmpty} non-empty), sora mset {mset.Count}");
    foreach (int i in new[] { 0, 11, 604, 612, 620, 838 }) if (i < kbm.Count) Console.WriteLine($"  kb slot {i} {kbm[i].Name} len {kbm[i].Stream.Length}  sora {mset[i].Name} len {mset[i].Stream.Length}");
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(mdlxPath)));
    var sora = OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream);
    var anb = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(File.ReadAllBytes(msetPath))).ToList()).GetAnb(0);
    var M = anb.GetAnimProvider(new MemoryStream(File.ReadAllBytes(mdlxPath))).ProvideMatrices(0);
    Console.WriteLine($"sora bones {sora.Bones.Count}, matrices {M.Length}");
    System.Numerics.Matrix4x4 Loc(OpenKh.Kh2.Models.ModelCommon.Bone b) =>
        System.Numerics.Matrix4x4.CreateScale(b.ScaleX, b.ScaleY, b.ScaleZ) * System.Numerics.Matrix4x4.CreateRotationX(b.RotationX) *
        System.Numerics.Matrix4x4.CreateRotationY(b.RotationY) * System.Numerics.Matrix4x4.CreateRotationZ(b.RotationZ) *
        System.Numerics.Matrix4x4.CreateTranslation(b.TranslationX, b.TranslationY, b.TranslationZ);
    var W = new System.Numerics.Matrix4x4[sora.Bones.Count];
    for (int b = 0; b < W.Length; b++) W[b] = Loc(sora.Bones[b]) * (sora.Bones[b].ParentIndex >= 0 ? W[sora.Bones[b].ParentIndex] : System.Numerics.Matrix4x4.Identity);
    var kw = Loc(kb.Bones[0]);
    var near = Enumerable.Range(0, W.Length).OrderBy(b => (W[b].Translation - kw.Translation).Length()).Take(6);
    foreach (var b in near) {
        System.Numerics.Matrix4x4.Invert(W[b], out var wi); var rel = kw * wi;
        Console.WriteLine($"bind sora bone {b} (parent {sora.Bones[b].ParentIndex}) at ({W[b].Translation.X:F1} {W[b].Translation.Y:F1} {W[b].Translation.Z:F1}) d {(W[b].Translation - kw.Translation).Length():F2}; kb0 rel T ({rel.M41:F1} {rel.M42:F1} {rel.M43:F1})");
    }
    if (W.Length > 25) Console.WriteLine($"bind bone 25 at ({W[25].Translation.X:F1} {W[25].Translation.Y:F1} {W[25].Translation.Z:F1})");
    var kbBytes = File.ReadAllBytes(kbMdlxPath);
    foreach (int slot in new[] { 0, 604, 612 }) {
        var sa = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(File.ReadAllBytes(msetPath))).ToList()).GetAnb(slot).GetAnimProvider(new MemoryStream(File.ReadAllBytes(mdlxPath)));
        var ka = new OpenKh.Kh2Anim.Mset.MsetIndir(kbm.Select(e => { e.Stream.Position = 0; return e; }).ToList()).GetAnb(slot).GetAnimProvider(new MemoryStream(kbBytes));
        for (int f = 0; f < 30; f++) {
            var S = sa.ProvideMatrices(f == 0 ? 0 : 1.0); var K = ka.ProvideMatrices(f == 0 ? 0 : 1.0);
            if (f % 10 != 0) continue;
            var k0 = K[0].Translation; var k11 = K[11].Translation;
            int best = -1; float bd = 1e9f;
            for (int b = 0; b < S.Length; b++) { float d = (S[b].Translation - k0).Length(); if (d < bd) { bd = d; best = b; } }
            Console.WriteLine($"slot {slot} f{f}: kb0 ({k0.X:F1} {k0.Y:F1} {k0.Z:F1}) kb11 ({k11.X:F1} {k11.Y:F1} {k11.Z:F1}) nearest sora bone {best} d {bd:F2}; sora b1 ({S[1].Translation.X:F1} {S[1].Translation.Y:F1} {S[1].Translation.Z:F1})");
        }
    }
}

// Hit recon: the Keyblade mset's triggers on Sora's slots (and Sora's own), the atkp rows they name.
void HitsInfo(List<int> only) {
    var kbm = Bar.Read(new MemoryStream(File.ReadAllBytes(kbMsetPath))).ToList();
    var ids = new HashSet<int>();
    foreach (int slot in only) {
        var e = kbm[slot];
        Console.WriteLine($"slot {slot} {e.Name} (sora {mset[slot].Name}) kb len {e.Stream.Length}");
        if (e.Stream.Length == 0) continue;
        e.Stream.Position = 0;
        foreach (var x in Bar.Read(e.Stream)) {
            Console.WriteLine($"  kb anb entry {x.Name} {x.Type} {x.Stream.Length}");
            if (x.Type != Bar.EntryType.MotionTriggers || x.Stream.Length == 0) continue;
            x.Stream.Position = 0;
            var t = new MotionTrigger(x.Stream);
            foreach (var r in t.RangeTriggerList) {
                Console.WriteLine($"  kb range {r.Trigger} {r.StartFrame}..{r.EndFrame} n{r.ParamSize} {r.Param1} {r.Param2} {r.Param3} {r.Param4}");
                if (r.ParamSize > 0) ids.Add(r.Param1);
            }
            foreach (var f in t.FrameTriggerList) Console.WriteLine($"  kb frame {f.Trigger} @{f.Frame} n{f.ParamSize} {f.Param1} {f.Param2}");
        }
        var s = Load(slot);
        if (s?.T != null) foreach (var r in s.T.RangeTriggerList)
            Console.WriteLine($"  sora range {r.Trigger} {r.StartFrame}..{r.EndFrame} n{r.ParamSize} {r.Param1} {r.Param2}");
    }
    var bat = Bar.Read(new MemoryStream(File.ReadAllBytes(battlePath)));
    var atkpE = bat.First(e => e.Name == "atkp").Stream; atkpE.Position = 0;
    var rows = OpenKh.Kh2.Battle.Atkp.Read(atkpE);
    var props = typeof(OpenKh.Kh2.Battle.Atkp).GetProperties();
    foreach (var a in rows) {
        int id = Convert.ToInt32(typeof(OpenKh.Kh2.Battle.Atkp).GetProperty("Id").GetValue(a));
        if (!ids.Contains(id)) continue;
        Console.WriteLine("atkp " + string.Join(" ", props.Select(p => $"{p.Name}={p.GetValue(a)}")));
    }
}

// sora_hits.bin (little-endian), magic 'SKRH' v1: the Keyblade's hit windows + blade segment (step 5).
// The windows are W_EX010.mset range trigger 10 (attack_hitbox, params atkp id, group) on Sora's slot, frames
// motion-relative GFR, end inclusive (-1 = to the end); Power / Kind / EnemyReaction from 00battle.bin atkp.
//   header  u32 magic, version, n_slots, reserved
//   dir[n]  u16 slot, u16 n_win, u16 n_seg (= GFR frames + 1), u16 reserved, u32 off_win, u32 off_seg
//   win[]   u16 start, end, atkp, group, power, kind; u8 reaction, effect, pad[2]
//   seg[]   n_seg x {f32 base x, y, z, tip x, y, z}: grip (Keyblade bone 0) and blade tip (its vertex farthest from the
//           grip) in Sora's entity frame (cm, +x left, +y up, +z forward) with the root move taken out, like the pose bake
void HitBake(string outPath) {
    var kbMsetBytes = File.ReadAllBytes(kbMsetPath); var kbMdlxBytes = File.ReadAllBytes(kbMdlxPath);
    var msetBytes = File.ReadAllBytes(msetPath); var mdlxBytes = File.ReadAllBytes(mdlxPath);
    var kbm = Bar.Read(new MemoryStream(kbMsetBytes)).ToList();
    var kb = OpenKh.Kh2.Models.ModelSkeletal.Read(Bar.Read(new MemoryStream(kbMdlxBytes)).First(e => e.Type == Bar.EntryType.Model).Stream);
    var kverts = kb.Groups.SelectMany(g => g.Mesh.Vertices).Select(v => v.BPositions[0]).ToList();
    var bat = Bar.Read(new MemoryStream(File.ReadAllBytes(battlePath)));
    var atkpE = bat.First(e => e.Name == "atkp").Stream; atkpE.Position = 0;
    var atkp = OpenKh.Kh2.Battle.Atkp.Read(atkpE).GroupBy(a => (int)a.Id).ToDictionary(g => g.Key, g => g.First());
    var kbIdle = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(kbMsetBytes)).ToList()).GetAnb(0).GetAnimProvider(new MemoryStream(kbMdlxBytes)).ProvideMatrices(0);
    // the tip vertex: farthest from the grip in the Keyblade's own rest pose (one vertex for every frame)
    System.Numerics.Vector3 VertAt(System.Numerics.Matrix4x4[] K, int i) {
        var b = kverts[i]; var p = b.Position;
        return System.Numerics.Vector3.Transform(new System.Numerics.Vector3(p.X, p.Y, p.Z), K[b.BoneIndex]);
    }
    int tip = Enumerable.Range(0, kverts.Count).OrderByDescending(i => (VertAt(kbIdle, i) - kbIdle[0].Translation).Length()).First();
    Console.WriteLine($"tip vertex {tip} (bone {kverts[tip].BoneIndex}), {(VertAt(kbIdle, tip) - kbIdle[0].Translation).Length():F1} cm from the grip");
    var dir = new List<(int slot, int nw, int ns, long ow, long os)>();
    var data = new MemoryStream(); var w = new BinaryWriter(data);
    var todo = new List<(Slot s, List<MotionTrigger.RangeTrigger> wins)>();
    for (int i = 0; i < mset.Count && i < kbm.Count; i++) {
        if (kbm[i].Stream.Length == 0) continue;
        var s = Load(i);
        if (s == null) continue;
        kbm[i].Stream.Position = 0;
        var te = Bar.Read(kbm[i].Stream).FirstOrDefault(x => x.Type == Bar.EntryType.MotionTriggers);
        if (te == null || te.Stream.Length == 0) continue;
        te.Stream.Position = 0;
        var wins = new MotionTrigger(te.Stream).RangeTriggerList.Where(r => r.Trigger == 10 && r.ParamSize >= 1).ToList();
        if (wins.Count > 0) todo.Add((s, wins));
    }
    int head = 16 + 16 * todo.Count;
    foreach (var (s, wins) in todo) {
        long ow = head + data.Position;
        foreach (var r in wins) {
            atkp.TryGetValue(r.Param1, out var a);
            w.Write((ushort)r.StartFrame); w.Write((ushort)(r.EndFrame < 0 ? s.Frames : r.EndFrame));
            w.Write((ushort)r.Param1); w.Write((ushort)(r.ParamSize > 1 ? r.Param2 : 0));
            w.Write((ushort)(a?.Power ?? 0)); w.Write((ushort)(a == null ? 0 : (int)a.Kind));
            w.Write((byte)(a?.EnemyReaction ?? 0)); w.Write((byte)(a?.EffectOnHit ?? 0)); w.Write((ushort)0);
        }
        long os = head + data.Position;
        var p = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(msetBytes)).ToList()).GetAnb(s.Index).GetAnimProvider(new MemoryStream(mdlxBytes));
        var kp = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(kbMsetBytes)).ToList()).GetAnb(s.Index).GetAnimProvider(new MemoryStream(kbMdlxBytes));
        var line = new StringBuilder();
        for (int f = 0; f <= s.Frames; f++) {
            var M = p.ProvideMatrices(f == 0 ? (Seek ? s.Start : 0) : 1.0); var K = kp.ProvideMatrices(f == 0 ? (Seek ? KbStart(s.Index) : 0) : 1.0);  // (s59: seek like Pose)
            System.Numerics.Matrix4x4.Invert(RootAt(s, f), out var rinv);
            var strip = s.HasRoot ? rinv * RootAt(s, 0) : System.Numerics.Matrix4x4.Identity;
            var toEnt = M[KbParent] * strip;
            var b = System.Numerics.Vector3.Transform(K[0].Translation, toEnt);
            var t = System.Numerics.Vector3.Transform(VertAt(K, tip), toEnt);
            w.Write(b.X); w.Write(b.Y); w.Write(b.Z); w.Write(t.X); w.Write(t.Y); w.Write(t.Z);
            if (wins.Any(r => f == r.StartFrame || f == (r.EndFrame < 0 ? s.Frames : r.EndFrame)))
                line.Append($" f{f} tip ({t.X:F0} {t.Y:F0} {t.Z:F0}) len {(t - b).Length():F0}");
        }
        dir.Add((s.Index, wins.Count, s.Frames + 1, ow, os));
        Console.WriteLine($"slot {s.Index} {s.Name}: " + string.Join(" ", wins.Select(r => $"[{r.StartFrame}..{r.EndFrame} atkp {r.Param1} " +
            $"power {(atkp.TryGetValue(r.Param1, out var a) ? a.Power : -1)}]")) + line);
    }
    using var o = new BinaryWriter(File.Create(outPath));
    o.Write(0x48524B53u); o.Write(1u); o.Write((uint)dir.Count); o.Write(0u);
    foreach (var d in dir) { o.Write((ushort)d.slot); o.Write((ushort)d.nw); o.Write((ushort)d.ns); o.Write((ushort)0); o.Write((uint)d.ow); o.Write((uint)d.os); }
    o.Write(data.ToArray());
    Console.WriteLine($"{outPath}: {dir.Count} slots, {head + data.Length} bytes");
}

// The runtime's skinning in C#, from the two files: an obj of slot / frame (model space, cm).
void Obj(string modelPath, string posePath, int slot, int frame, string outPath) {
    var m = File.ReadAllBytes(modelPath); var q = File.ReadAllBytes(posePath);
    uint U(byte[] a, int o) => BitConverter.ToUInt32(a, o);
    float F(byte[] a, int o) => BitConverter.ToSingle(a, o);
    int nb = (int)U(m, 8), ng = (int)U(m, 12), nt = (int)U(m, 16), nv = (int)U(m, 20), ni = (int)U(m, 24);
    int grp = 32 + 16 * nt, vo = grp + 16 * ng, io = vo + 16 * nv, xo = io + 20 * ni;
    int ns = (int)U(q, 8), pose = -1;
    for (int k = 0; k < ns; k++) if (BitConverter.ToUInt16(q, 16 + 8 * k) == slot) {
        int n = BitConverter.ToUInt16(q, 18 + 8 * k);
        pose = (int)U(q, 20 + 8 * k) + Math.Min(frame, n - 1) * nb * 48;
    }
    if (pose < 0) throw new Exception($"slot {slot} not in {posePath}");
    using var w = new StreamWriter(outPath);
    for (int v = 0; v < nv; v++) {
        int fi = (int)U(m, vo + 16 * v), n = (int)U(m, vo + 16 * v + 4);
        float x = 0, y = 0, z = 0;
        for (int k = fi; k < fi + n; k++) {
            int o = io + 20 * k, b = (int)U(m, o), r = pose + b * 48;
            float px = F(m, o + 4), py = F(m, o + 8), pz = F(m, o + 12), pw = F(m, o + 16);
            x += px * F(q, r) + py * F(q, r + 12) + pz * F(q, r + 24) + pw * F(q, r + 36);
            y += px * F(q, r + 4) + py * F(q, r + 16) + pz * F(q, r + 28) + pw * F(q, r + 40);
            z += px * F(q, r + 8) + py * F(q, r + 20) + pz * F(q, r + 32) + pw * F(q, r + 44);
        }
        w.WriteLine(FormattableString.Invariant($"v {x:F3} {y:F3} {z:F3}"));
        w.WriteLine(FormattableString.Invariant($"vt {F(m, vo + 16 * v + 8):F5} {1 - F(m, vo + 16 * v + 12):F5}"));
    }
    for (int g = 0; g < ng; g++) {
        int fi = (int)U(m, grp + 16 * g + 8), n = (int)U(m, grp + 16 * g + 12);
        w.WriteLine($"g group{g}_tex{U(m, grp + 16 * g)}");
        for (int k = fi; k + 2 < fi + n; k += 3) {
            uint a = U(m, xo + 4 * k) + 1, b = U(m, xo + 4 * k + 4) + 1, c = U(m, xo + 4 * k + 8) + 1;
            w.WriteLine($"f {a}/{a} {b}/{b} {c}/{c}");
        }
    }
    Console.WriteLine($"{outPath}: slot {slot} frame {frame}, {nv} verts");
}

// ------------------------------------------------------------------------------------------- bind pose + ragdoll map
// KH2 bone local matrix (row vectors): S * Rx * Ry * Rz * T, as Kh2AnimEmu composes the skeleton.
System.Numerics.Matrix4x4 BoneLocal(OpenKh.Kh2.Models.ModelCommon.Bone b) =>
    System.Numerics.Matrix4x4.CreateScale(b.ScaleX, b.ScaleY, b.ScaleZ) * System.Numerics.Matrix4x4.CreateRotationX(b.RotationX) *
    System.Numerics.Matrix4x4.CreateRotationY(b.RotationY) * System.Numerics.Matrix4x4.CreateRotationZ(b.RotationZ) *
    System.Numerics.Matrix4x4.CreateTranslation(b.TranslationX, b.TranslationY, b.TranslationZ);

List<OpenKh.Kh2.Models.ModelCommon.Bone> SoraBones() {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(mdlxPath)));
    return OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream).Bones;
}

System.Numerics.Matrix4x4[] BindWorld(List<OpenKh.Kh2.Models.ModelCommon.Bone> bones) {
    var W = new System.Numerics.Matrix4x4[bones.Count];
    for (int b = 0; b < W.Length; b++)
        W[b] = BoneLocal(bones[b]) * (bones[b].ParentIndex >= 0 ? W[bones[b].ParentIndex] : System.Numerics.Matrix4x4.Identity);
    return W;
}

// Kh2AnimEmu's matrices of a slot at GFR frame f (model space, root move NOT stripped)
System.Numerics.Matrix4x4[] SlotPose(int slot, int frame) {
    var anb = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(File.ReadAllBytes(msetPath))).ToList()).GetAnb(slot);
    var p = anb.GetAnimProvider(new MemoryStream(File.ReadAllBytes(mdlxPath)));
    var M = p.ProvideMatrices(0);
    for (int f = 1; f <= frame; f++) M = p.ProvideMatrices(1.0);
    return M;
}

// sora_bind.txt: Sora's rest pose from the mdlx skeleton (no motion), plus the idle pose (MSLOT_IDLE f0) for the eye.
// KH2 bones have no names: the hierarchy + the positions (+x left, +y up, +z forward, cm) identify the joints.
void Bind(string outPath) {
    var bones = SoraBones();
    var W = BindWorld(bones);
    var I = SlotPose(2, 0);   // A000 field idle, frame 0 (sora_params.h MSLOT_IDLE)
    int n = bones.Count;
    var kids = new int[n]; var depth = new int[n];
    for (int b = 0; b < n; b++) { int p = bones[b].ParentIndex; if (p >= 0) { kids[p]++; depth[b] = depth[p] + 1; } }
    // vertex count per bone (rigid skin: how much mesh each bone carries)
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(mdlxPath)));
    var model = OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream);
    var nv = new int[n];
    foreach (var g in model.Groups) foreach (var v in g.Mesh.Vertices) foreach (var bp in v.BPositions) nv[bp.BoneIndex]++;
    var sb = new StringBuilder();
    sb.AppendLine("# Sora (P_EX100.mdlx) bind pose, generated by sorabake bind. KH2 model space: +x LEFT, +y up, +z forward, cm.");
    sb.AppendLine($"# {n} bones. bind = skeleton rest (S*Rx*Ry*Rz*T per bone, chained); idle = A000 slot 2 frame 0 (Kh2AnimEmu).");
    sb.AppendLine("# idx parent depth kids verts | bind world x y z | local T x y z | local R x y z (rad) | idle world x y z | len to parent (bind)");
    for (int b = 0; b < n; b++) {
        var p = W[b].Translation; var bn = bones[b]; var q = I[b].Translation;
        float len = bn.ParentIndex >= 0 ? (p - W[bn.ParentIndex].Translation).Length() : 0;
        sb.AppendLine(FormattableString.Invariant(
            $"{b,3} {bn.ParentIndex,3} {depth[b],2} {kids[b],2} {nv[b],4} | {p.X,8:F2} {p.Y,8:F2} {p.Z,8:F2} | {bn.TranslationX,8:F2} {bn.TranslationY,8:F2} {bn.TranslationZ,8:F2} | {bn.RotationX,6:F3} {bn.RotationY,6:F3} {bn.RotationZ,6:F3} | {q.X,8:F2} {q.Y,8:F2} {q.Z,8:F2} | {len,6:F2}"));
    }
    File.WriteAllText(outPath, sb.ToString());
    Console.Write(sb.ToString());
    Console.WriteLine($"{outPath} written ({n} bones)");
}

// The joint ids + the map, shared by ragmap (header) and view (render). Sora joints = the first bone at each joint
// position (coincident helper bones sit under it and keep their relative pose).
((string name, int id, string note)[] J, (string role, string sora, string link, string soraTo, string linkTo, string sl, string sr, string ll, string lr, string name)[] map) RagDef() {
    var J = new (string name, int id, string note)[] {
        ("ROOT",        0, "model root at hip height; everything hangs off it = the pelvis to place"),
        ("HIP_HUB",     3, "hip hub 14 cm below the root, 7 kids: both hip joints + 5 strap/pouch bones"),
        ("SPINE_1",    65, "first spine bone (coincident with the root), waist band mesh"),
        ("SPINE_2",    66, "y 107.5"),
        ("SPINE_3",    67, "y 112.9, the jacket body (359 verts) = the chest bone to rotate"),
        ("CHEST_HUB",  68, "y 132.4, 20 kids: clavicles, neck, hood, jacket hem flaps, necklace"),
        ("NECK",      136, "coincident with the chest hub; 137 carries the neck mesh"),
        ("HEAD",      138, "y 141.2, head joint (140 carries the 906-vert head mesh)"),
        ("HEAD_TOP",  141, "y 156.6, 11 kids = hair spikes / face bits"),
        ("CLAV_L",     74, "x +5.0"), ("SHOULDER_L", 76, "x +12.2"), ("ELBOW_L", 79, "x +35.9"), ("WRIST_L", 80, "x +57.1"),
        ("PALM_L",     86, "x +62.7, 5 finger chains"), ("FINGER_MID_L", 97, "middle finger base (longest finger, z -3.4)"),
        ("CLAV_R",    166, ""), ("SHOULDER_R", 168, ""), ("ELBOW_R", 171, ""), ("WRIST_R", 172, ""),
        ("PALM_R",    178, "= the Keyblade's parent (PHH kh2_bake.json weapons bone 178)"), ("FINGER_MID_R", 189, ""),
        ("HIP_L",      10, "x +7.7, y 88.5"), ("KNEE_L", 12, "y 58.2, shin mesh"), ("ANKLE_L", 13, "y 15.4"),
        ("HEEL_L",     16, "y 0 (sole contact)"), ("TOE_L", 17, "y 8.4, z +13.7 (Sora's shoe tip curls up)"),
        ("HIP_R",      39, ""), ("KNEE_R", 41, ""), ("ANKLE_R", 42, ""), ("HEEL_R", 45, ""), ("TOE_R", 46, ""),
    };
    // the map: (role, sora, link, sora_to, link_to, sora_l, sora_r, link_l, link_r, name)
    // role: PELVIS / CHEST = full frame (up = sora->sora_to vs link->link_to, left = sora_l - sora_r vs link_l - link_r);
    //       LIMB = swing-only (direction sora->sora_to vs link->link_to); HEAD = orientation delta of Link's Head (no positions)
    var map = new (string role, string sora, string link, string soraTo, string linkTo, string sl, string sr, string ll, string lr, string name)[] {
        ("PELVIS", "ROOT",       "SKL_ROOT",   "SPINE_3",      "SPINE_1",     "HIP_L",      "HIP_R",      "LEG_1_L", "LEG_1_R", "pelvis"),
        ("CHEST",  "SPINE_3",    "SPINE_1",    "NECK",         "NECK",        "SHOULDER_L", "SHOULDER_R", "ARM_1_L", "ARM_1_R", "chest (jacket bone; arms + head hang off it)"),
        ("LIMB",   "NECK",       "NECK",       "HEAD",         "HEAD",        null, null, null, null, "neck"),
        ("HEAD",   "HEAD",       "HEAD",       "HEAD_TOP",     "HAIR_ROOT",   null, null, null, null, "head (Link Head orientation delta; the position pair is a weak fallback)"),
        ("LIMB",   "CLAV_L",     "CLAVICLE_L", "SHOULDER_L",   "ARM_1_L",     null, null, null, null, "clavicle L"),
        ("LIMB",   "SHOULDER_L", "ARM_1_L",    "ELBOW_L",      "ARM_2_L",     null, null, null, null, "upper arm L"),
        ("LIMB",   "ELBOW_L",    "ARM_2_L",    "WRIST_L",      "WRIST_L",     null, null, null, null, "forearm L"),
        ("LIMB",   "WRIST_L",    "WRIST_L",    "FINGER_MID_L", "FINGER_C_1_L", null, null, null, null, "hand L (optional: Finger_C = middle finger assumed)"),
        ("LIMB",   "CLAV_R",     "CLAVICLE_R", "SHOULDER_R",   "ARM_1_R",     null, null, null, null, "clavicle R"),
        ("LIMB",   "SHOULDER_R", "ARM_1_R",    "ELBOW_R",      "ARM_2_R",     null, null, null, null, "upper arm R"),
        ("LIMB",   "ELBOW_R",    "ARM_2_R",    "WRIST_R",      "WRIST_R",     null, null, null, null, "forearm R"),
        ("LIMB",   "WRIST_R",    "WRIST_R",    "FINGER_MID_R", "FINGER_C_1_R", null, null, null, null, "hand R (optional; the Keyblade rides on 178 under it)"),
        ("LIMB",   "HIP_L",      "LEG_1_L",    "KNEE_L",       "LEG_2_L",     null, null, null, null, "thigh L"),
        ("LIMB",   "KNEE_L",     "LEG_2_L",    "ANKLE_L",      "ANKLE_L",     null, null, null, null, "shin L"),
        ("LIMB",   "ANKLE_L",    "ANKLE_L",    "TOE_L",        "TOE_L",       null, null, null, null, "foot L (ankle -> shoe tip)"),
        ("LIMB",   "HIP_R",      "LEG_1_R",    "KNEE_R",       "LEG_2_R",     null, null, null, null, "thigh R"),
        ("LIMB",   "KNEE_R",     "LEG_2_R",    "ANKLE_R",      "ANKLE_R",     null, null, null, null, "shin R"),
        ("LIMB",   "ANKLE_R",    "ANKLE_R",    "TOE_R",        "TOE_R",       null, null, null, null, "foot R"),
    };
    return (J, map);
}

// sora_ragmap.h: Sora's skeleton (parents, bind positions) + the Sora -> Link joint map for the ragdoll retarget
// (SAVAGE_WIND_RECON.md session 22 "aim chain"). KH2 bones have no names; the ids below were read off sora_bind.txt
// (hierarchy + bind positions, +x = LEFT) and are checked here against the model every bake.
void RagMap(string outPath) {
    var bones = SoraBones();
    var W = BindWorld(bones);
    var kbBar = Bar.Read(new MemoryStream(File.ReadAllBytes(kbMdlxPath)));
    var kb = OpenKh.Kh2.Models.ModelSkeletal.Read(kbBar.First(e => e.Type == Bar.EntryType.Model).Stream).Bones;
    int nS = bones.Count, nAll = nS + kb.Count;
    System.Numerics.Vector3 P(int b) => W[b].Translation;
    void Check(bool ok, string what) { if (!ok) throw new Exception("ragmap: skeleton check failed: " + what); }
    bool ChainUp(int b, int anc) { for (int i = 0; i < 64 && b >= 0; i++) { if (b == anc) return true; b = bones[b].ParentIndex; } return false; }

    var (J, map) = RagDef();
    int Jid(string n) => J.First(j => j.name == n).id;
    // hierarchy checks (a wrong guess fails the bake instead of writing junk)
    foreach (var (a, b) in new[] { ("ROOT", "HIP_HUB"), ("ROOT", "SPINE_1"), ("SPINE_1", "SPINE_2"), ("SPINE_2", "SPINE_3"), ("SPINE_3", "CHEST_HUB"),
        ("CHEST_HUB", "NECK"), ("NECK", "HEAD"), ("HEAD", "HEAD_TOP"), ("CHEST_HUB", "CLAV_L"), ("CLAV_L", "SHOULDER_L"), ("SHOULDER_L", "ELBOW_L"),
        ("ELBOW_L", "WRIST_L"), ("WRIST_L", "PALM_L"), ("PALM_L", "FINGER_MID_L"), ("CHEST_HUB", "CLAV_R"), ("CLAV_R", "SHOULDER_R"),
        ("SHOULDER_R", "ELBOW_R"), ("ELBOW_R", "WRIST_R"), ("WRIST_R", "PALM_R"), ("PALM_R", "FINGER_MID_R"), ("HIP_HUB", "HIP_L"), ("HIP_L", "KNEE_L"),
        ("KNEE_L", "ANKLE_L"), ("ANKLE_L", "HEEL_L"), ("HEEL_L", "TOE_L"), ("HIP_HUB", "HIP_R"), ("HIP_R", "KNEE_R"), ("KNEE_R", "ANKLE_R"),
        ("ANKLE_R", "HEEL_R"), ("HEEL_R", "TOE_R") })
        Check(ChainUp(Jid(b), Jid(a)), $"{b} ({Jid(b)}) is not under {a} ({Jid(a)})");
    Check(bones[KbParent].ParentIndex >= 0 && ChainUp(KbParent, Jid("WRIST_R")), "Keyblade parent 178 not under WRIST_R");
    // left/right mirror checks: +x = left, same y/z, same segment lengths
    foreach (var (l, r) in new[] { ("CLAV_L", "CLAV_R"), ("SHOULDER_L", "SHOULDER_R"), ("ELBOW_L", "ELBOW_R"), ("WRIST_L", "WRIST_R"),
        ("PALM_L", "PALM_R"), ("FINGER_MID_L", "FINGER_MID_R"), ("HIP_L", "HIP_R"), ("KNEE_L", "KNEE_R"), ("ANKLE_L", "ANKLE_R"), ("HEEL_L", "HEEL_R"), ("TOE_L", "TOE_R") }) {
        var a = P(Jid(l)); var b = P(Jid(r));
        Check(a.X > 0 && b.X < 0 && Math.Abs(a.X + b.X) < 0.1f && Math.Abs(a.Y - b.Y) < 0.1f && Math.Abs(a.Z - b.Z) < 0.1f, $"{l}/{r} not mirrored in x: ({a.X:F2} {a.Y:F2} {a.Z:F2}) vs ({b.X:F2} {b.Y:F2} {b.Z:F2})");
    }
    foreach (var (n, y) in new[] { ("ROOT", 102.62f), ("HIP_HUB", 88.49f), ("CHEST_HUB", 132.42f), ("HEAD", 141.19f), ("HEEL_L", 0f), ("KNEE_L", 58.17f), ("ELBOW_L", 128.80f) })
        Check(Math.Abs(P(Jid(n)).Y - y) < 0.05f, $"{n} y {P(Jid(n)).Y:F2} != {y}");

    // Link joints (recon/link_bones.txt)
    var L = new (string name, int id)[] { ("ROOT", 0), ("SKL_ROOT", 1), ("SPINE_1", 2), ("SPINE_2", 3), ("CLAVICLE_L", 4), ("ARM_1_L", 5), ("ARM_2_L", 7),
        ("WRIST_L", 10), ("FINGER_C_1_L", 17), ("CLAVICLE_R", 28), ("ARM_1_R", 29), ("ARM_2_R", 31), ("WRIST_R", 34), ("FINGER_C_1_R", 41), ("NECK", 52),
        ("HEAD", 53), ("HAIR_ROOT", 90), ("WAIST", 93), ("LEG_1_L", 106), ("LEG_2_L", 108), ("ANKLE_L", 110), ("TOE_L", 111), ("LEG_1_R", 112),
        ("LEG_2_R", 114), ("ANKLE_R", 116), ("TOE_R", 117) };
    var linkNames = File.Exists("link_bones.txt") ? File.ReadAllLines("link_bones.txt")
        .Select(ln => ln.Trim().Split(' ', StringSplitOptions.RemoveEmptyEntries)).Where(t => t.Length > 2 && int.TryParse(t[0], out _)).ToDictionary(t => int.Parse(t[0]), t => t[1]) : null;
    if (linkNames != null) foreach (var (n, id) in L) {
        string want = n == "ROOT" ? "Root" : n == "SKL_ROOT" ? "Skl_Root" : n;
        Check(string.Equals(linkNames[id].Replace("_", ""), want.Replace("_", ""), StringComparison.OrdinalIgnoreCase), $"Link bone {id} is {linkNames[id]}, expected {want}");
    }

    int Lid(string n) => n == null ? -1 : L.First(l => l.name == n).id;
    int Sid(string n) => n == null ? -1 : Jid(n);
    string F(float v) => v.ToString("0.0###", System.Globalization.CultureInfo.InvariantCulture) + "f";

    var sb = new StringBuilder();
    sb.AppendLine("// GENERATED by tools/sorabake (sorabake ragmap): KH2 Sora's skeleton + the Sora -> Link joint map. Don't edit.");
    sb.AppendLine("// Sora bone ids = sora_model.bin / sora_pose.bin order (P_EX100.mdlx bones 0..227, Keyblade W_EX010 bones 228..239 under 178).");
    sb.AppendLine("// Link bone ids = Link.sbfres skeleton order (recon/link_bones.txt). Units: KH2 cm, model space +x LEFT, +y up, +z forward.");
    sb.AppendLine("// KH2 bones carry no names: the joints were identified from the hierarchy + bind positions (data/sora_bind.txt) and are");
    sb.AppendLine("// re-checked by the bake (parent chains, left/right mirror in x, joint heights). Retarget design: SAVAGE_WIND_RECON.md session 22.");
    sb.AppendLine("#pragma once\n#include <stdint.h>\n");
    sb.AppendLine($"#define SORA_RAG_BONES {nAll}      // Sora {nS} + Keyblade {kb.Count}");
    sb.AppendLine($"#define SORA_RAG_SORA_BONES {nS}");
    sb.AppendLine($"#define SORA_RAG_KB_PARENT {KbParent}   // Keyblade bone 228's parent (right palm)\n");
    sb.AppendLine("// Sora joints (the first bone at each joint; helper bones coincident with it sit below it in the hierarchy)");
    foreach (var j in J) sb.AppendLine($"#define SORA_J_{j.name,-13} {j.id,3}   // bind ({P(j.id).X:F1} {P(j.id).Y:F1} {P(j.id).Z:F1}){(j.note.Length > 0 ? " " + j.note : "")}");
    sb.AppendLine("\n// Link joints (Link.sbfres)");
    foreach (var l in L) sb.AppendLine($"#define LINK_J_{l.name,-13} {l.id,3}");
    sb.AppendLine("\n// parent of every bone (-1 = root); unmapped bones keep the motion pose's transform relative to their nearest mapped ancestor");
    sb.Append($"static const int16_t sora_rag_parent[SORA_RAG_BONES] = {{");
    for (int b = 0; b < nAll; b++) {
        if (b % 20 == 0) sb.Append("\n    ");
        int p = b < nS ? bones[b].ParentIndex : (kb[b - nS].ParentIndex < 0 ? KbParent : nS + kb[b - nS].ParentIndex);
        sb.Append($"{p}, ");
    }
    sb.AppendLine("\n};\n");
    sb.AppendLine("// bind (rest) pose world position of every bone, cm (the Keyblade's in its own weapon space, hung on 178 at runtime)");
    sb.Append("static const float sora_rag_bind_pos[SORA_RAG_BONES][3] = {");
    var kbW = new System.Numerics.Matrix4x4[kb.Count];
    for (int b = 0; b < kb.Count; b++) kbW[b] = BoneLocal(kb[b]) * (kb[b].ParentIndex >= 0 ? kbW[kb[b].ParentIndex] : System.Numerics.Matrix4x4.Identity);
    for (int b = 0; b < nAll; b++) {
        if (b % 4 == 0) sb.Append("\n    ");
        var p = b < nS ? P(b) : kbW[b - nS].Translation;
        sb.Append($"{{{F(p.X)}, {F(p.Y)}, {F(p.Z)}}}, ");
    }
    sb.AppendLine("\n};\n");
    sb.AppendLine("enum { SORA_RAG_PELVIS = 0, SORA_RAG_CHEST = 1, SORA_RAG_LIMB = 2, SORA_RAG_HEAD = 3 };");
    sb.AppendLine("typedef struct {");
    sb.AppendLine("    uint8_t role;                 // SORA_RAG_*: PELVIS / CHEST = full frame, LIMB = swing-only, HEAD = orientation delta");
    sb.AppendLine("    int16_t sora, link;           // the Sora bone to rotate (and place, for the pelvis) and Link's joint at the same spot");
    sb.AppendLine("    int16_t sora_to, link_to;     // segment end: direction sora->sora_to is aimed along link->link_to (frames: the 'up' axis)");
    sb.AppendLine("    int16_t sora_l, sora_r;       // frames only: 'left' axis = pos(sora_l) - pos(sora_r) vs pos(link_l) - pos(link_r); -1 for limbs");
    sb.AppendLine("    int16_t link_l, link_r;");
    sb.AppendLine("    float bind_dir[3], bind_len;  // sora->sora_to in the bind pose: unit direction (KH axes) and length, cm");
    sb.AppendLine("    const char *name;");
    sb.AppendLine("} SoraRagBone;\n");
    sb.AppendLine("// Order = top-down (parents before children), so a single pass can apply frames then limbs.");
    sb.AppendLine("static const SoraRagBone sora_ragmap[] = {");
    var table = new StringBuilder();
    var I = SlotPose(2, 0);   // idle, frame 0: rigid segments keep their bind length; sides must stay +x left / -x right
    table.AppendLine("role    name                                       sora -> to   link -> to   bind dir (x y z)        len cm  idle len  idle x   side");
    foreach (var m in map) {
        int s = Sid(m.sora), t = Sid(m.soraTo), l = Lid(m.link), lt = Lid(m.linkTo);
        var d = P(t) - P(s); float len = d.Length(); var u = len > 1e-4f ? d / len : System.Numerics.Vector3.Zero;
        float ilen = (I[t].Translation - I[s].Translation).Length(); float ix = I[s].Translation.X;
        bool isL = m.name.EndsWith(" L") || m.name.Contains(" L "), isR = m.name.EndsWith(" R") || m.name.Contains(" R ");
        Check(Math.Abs(ilen - len) < 0.05f, $"{m.name}: segment length changes between bind ({len:F2}) and idle ({ilen:F2}) - not a rigid bone pair");
        Check(!(isL && ix < 0) && !(isR && ix > 0), $"{m.name}: wrong side in the idle pose (x {ix:F1})");
        sb.AppendLine($"    {{SORA_RAG_{m.role}, SORA_J_{m.sora}, LINK_J_{m.link}, SORA_J_{m.soraTo}, LINK_J_{m.linkTo}, " +
            $"{(m.sl == null ? "-1, -1, -1, -1" : $"SORA_J_{m.sl}, SORA_J_{m.sr}, LINK_J_{m.ll}, LINK_J_{m.lr}")}, " +
            $"{{{F(u.X)}, {F(u.Y)}, {F(u.Z)}}}, {F(len)}, \"{m.name}\"}},");
        string side = m.name.EndsWith(" L") || m.name.Contains(" L ") ? (P(s).X > 0 ? "left  (+x) OK" : "left  WRONG") :
                      m.name.EndsWith(" R") || m.name.Contains(" R ") ? (P(s).X < 0 ? "right (-x) OK" : "right WRONG") : "centre";
        table.AppendLine($"{m.role,-7} {m.name,-42} {s,3} -> {t,3}   {l,3} -> {lt,3}   ({u.X,5:F2} {u.Y,5:F2} {u.Z,5:F2})   {len,6:F2}  {ilen,7:F2}  {ix,6:F1}   {side}");
    }
    sb.AppendLine("};");
    sb.AppendLine($"#define SORA_RAGMAP_N {map.Length}");
    File.WriteAllText(outPath, sb.ToString());
    Console.Write(table.ToString());
    Console.WriteLine($"stature: heel y {P(Jid("HEEL_L")).Y:F1}, hip joint y {P(Jid("HIP_L")).Y:F1}, root y {P(0).Y:F1}, shoulder y {P(Jid("SHOULDER_L")).Y:F1}, head joint y {P(Jid("HEAD")).Y:F1}, head top hub y {P(Jid("HEAD_TOP")).Y:F1}");
    Console.WriteLine($"spans: hips {P(Jid("HIP_L")).X - P(Jid("HIP_R")).X:F1} cm, shoulders {P(Jid("SHOULDER_L")).X - P(Jid("SHOULDER_R")).X:F1} cm, fingertip to fingertip {2 * P(100).X:F1} cm (T-pose)");
    Console.WriteLine($"{outPath} written ({map.Length} entries, {nAll} bones)");
}

// sorabake view <model.bin> <pose.bin> <slot> <frame> <out.png>: software render of the skinned mesh (front view +
// left-side view, flat shaded, z-buffered) with the ragmap segments on top: left side green, right side red, centre
// blue, segment start = small square. frame -1 = the bind pose (no motion; pose.bin unused). Checks the map by eye.
void View(string modelPath, string posePath, int slot, int frame, string outPath) {
    var m = File.ReadAllBytes(modelPath);
    uint U(byte[] a, int o) => BitConverter.ToUInt32(a, o);
    float F(byte[] a, int o) => BitConverter.ToSingle(a, o);
    int nb = (int)U(m, 8), ng = (int)U(m, 12), nt = (int)U(m, 16), nv = (int)U(m, 20), ni = (int)U(m, 24);
    int grp = 32 + 16 * nt, vo = grp + 16 * ng, io = vo + 16 * nv, xo = io + 20 * ni;
    var B = new System.Numerics.Matrix4x4[nb];
    if (frame < 0) {
        var bones = SoraBones(); var W = BindWorld(bones);
        var kbBar = Bar.Read(new MemoryStream(File.ReadAllBytes(kbMdlxPath)));
        var kb = OpenKh.Kh2.Models.ModelSkeletal.Read(kbBar.First(e => e.Type == Bar.EntryType.Model).Stream).Bones;
        var kbW = new System.Numerics.Matrix4x4[kb.Count];
        for (int b = 0; b < kb.Count; b++) kbW[b] = BoneLocal(kb[b]) * (kb[b].ParentIndex >= 0 ? kbW[kb[b].ParentIndex] : System.Numerics.Matrix4x4.Identity);
        for (int b = 0; b < nb; b++) B[b] = b < W.Length ? W[b] : kbW[b - W.Length] * W[KbParent];
    } else {
        var q = File.ReadAllBytes(posePath);
        int ns = (int)U(q, 8), pose = -1;
        for (int k = 0; k < ns; k++) if (BitConverter.ToUInt16(q, 16 + 8 * k) == slot) {
            int n = BitConverter.ToUInt16(q, 18 + 8 * k);
            pose = (int)U(q, 20 + 8 * k) + Math.Min(frame, n - 1) * nb * 48;
        }
        if (pose < 0) throw new Exception($"slot {slot} not in {posePath}");
        for (int b = 0; b < nb; b++) {
            int r = pose + b * 48;
            B[b] = new System.Numerics.Matrix4x4(F(q, r), F(q, r + 4), F(q, r + 8), 0, F(q, r + 12), F(q, r + 16), F(q, r + 20), 0,
                                                 F(q, r + 24), F(q, r + 28), F(q, r + 32), 0, F(q, r + 36), F(q, r + 40), F(q, r + 44), 1);
        }
    }
    var pos = new System.Numerics.Vector3[nv];
    for (int v = 0; v < nv; v++) {
        int fi = (int)U(m, vo + 16 * v), n = (int)U(m, vo + 16 * v + 4);
        var acc = System.Numerics.Vector4.Zero;
        for (int k = fi; k < fi + n; k++) {
            int o = io + 20 * k, b = (int)U(m, o);
            acc += System.Numerics.Vector4.Transform(new System.Numerics.Vector4(F(m, o + 4), F(m, o + 8), F(m, o + 12), F(m, o + 16)), B[b]);
        }
        pos[v] = new System.Numerics.Vector3(acc.X, acc.Y, acc.Z);
    }
    // canvas: two panels, 190 cm tall each at 4 px/cm; x range -60..60 cm
    const int PW = 480, PH = 800; const float S = 4f; int Wd = PW * 2, Ht = PH;
    var rgb = new byte[Wd * Ht * 3]; var zb = new float[Wd * Ht];
    for (int i = 0; i < Wd * Ht; i++) { zb[i] = float.NegativeInfinity; rgb[3 * i] = 24; rgb[3 * i + 1] = 24; rgb[3 * i + 2] = 28; }
    // panel 0 = front view (camera at +z looking back: Sora's left (+x) is on the viewer's RIGHT); panel 1 = his left side (camera at +x; +z forward to the right)
    (float x, float y, float z) Proj(System.Numerics.Vector3 p, int panel) => panel == 0
        ? (PW * 0.5f + p.X * S, PH - 20 - p.Y * S, p.Z) : (PW + PW * 0.5f + p.Z * S, PH - 20 - p.Y * S, p.X);
    var light = System.Numerics.Vector3.Normalize(new System.Numerics.Vector3(0.3f, 0.8f, 0.6f));
    void Tri((float x, float y, float z) a, (float x, float y, float z) b, (float x, float y, float z) c, (double r, double g, double bl) col) {
        int x0 = Math.Max(0, (int)MathF.Floor(Math.Min(a.x, Math.Min(b.x, c.x)))), x1 = Math.Min(Wd - 1, (int)MathF.Ceiling(Math.Max(a.x, Math.Max(b.x, c.x))));
        int y0 = Math.Max(0, (int)MathF.Floor(Math.Min(a.y, Math.Min(b.y, c.y)))), y1 = Math.Min(Ht - 1, (int)MathF.Ceiling(Math.Max(a.y, Math.Max(b.y, c.y))));
        float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (Math.Abs(area) < 1e-6f) return;
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float w0 = ((b.x - px) * (c.y - py) - (b.y - py) * (c.x - px)) / area;
            float w1 = ((c.x - px) * (a.y - py) - (c.y - py) * (a.x - px)) / area;
            float w2 = 1 - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            float z = w0 * a.z + w1 * b.z + w2 * c.z; int i = y * Wd + x;
            if (z <= zb[i]) continue;
            zb[i] = z; rgb[3 * i] = (byte)col.r; rgb[3 * i + 1] = (byte)col.g; rgb[3 * i + 2] = (byte)col.bl;
        }
    }
    for (int g = 0; g < ng; g++) {
        int fi = (int)U(m, grp + 16 * g + 8), n = (int)U(m, grp + 16 * g + 12);
        bool keyblade = (U(m, grp + 16 * g + 4) & 0x100) != 0;
        for (int k = fi; k + 2 < fi + n; k += 3) {
            var a = pos[U(m, xo + 4 * k)]; var b = pos[U(m, xo + 4 * k + 4)]; var c = pos[U(m, xo + 4 * k + 8)];
            var nrm = System.Numerics.Vector3.Cross(b - a, c - a);
            if (nrm.LengthSquared() < 1e-6f) continue;
            float sh = 0.35f + 0.65f * Math.Abs(System.Numerics.Vector3.Dot(System.Numerics.Vector3.Normalize(nrm), light));
            var col = keyblade ? (200 * sh, 170 * sh, 90 * sh) : (150 * sh, 150 * sh, 160 * sh);
            for (int panel = 0; panel < 2; panel++) Tri(Proj(a, panel), Proj(b, panel), Proj(c, panel), col);
        }
    }
    void Px(int x, int y, (int r, int g, int b) c) { if (x < 0 || y < 0 || x >= Wd || y >= Ht) return; int i = 3 * (y * Wd + x); rgb[i] = (byte)c.r; rgb[i + 1] = (byte)c.g; rgb[i + 2] = (byte)c.b; }
    void Line((float x, float y, float z) a, (float x, float y, float z) b, (int r, int g, int b) c) {
        int n = (int)Math.Max(Math.Abs(b.x - a.x), Math.Abs(b.y - a.y)) + 1;
        for (int i = 0; i <= n; i++) { float t = (float)i / n; int x = (int)(a.x + (b.x - a.x) * t), y = (int)(a.y + (b.y - a.y) * t); Px(x, y, c); Px(x + 1, y, c); Px(x, y + 1, c); }
    }
    var (J, map) = RagDef();
    int Jid(string n) => J.First(j => j.name == n).id;
    foreach (var e in map) {
        int s = Jid(e.sora), t = Jid(e.soraTo);
        var col = e.name.EndsWith(" L") || e.name.Contains(" L ") ? (40, 230, 60) : e.name.EndsWith(" R") || e.name.Contains(" R ") ? (240, 50, 50) : (70, 130, 255);
        var ps = B[s].Translation; var pt = B[t].Translation;
        for (int panel = 0; panel < 2; panel++) {
            var a = Proj(ps, panel); var b = Proj(pt, panel);
            Line(a, b, col);
            for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) Px((int)a.x + dx, (int)a.y + dy, (255, 255, 255));
        }
    }
    // ground line at y = 0 (dashed), vertical marks at x / z = -50, 0, +50 cm
    for (int x = 0; x < Wd; x++) if ((x / 8) % 2 == 0) Px(x, PH - 20, (90, 90, 90));
    for (int panel = 0; panel < 2; panel++) for (int cm = -50; cm <= 50; cm += 50) { var p = Proj(new System.Numerics.Vector3(cm, 0, cm), panel); for (int y = 0; y < Ht; y += 4) Px((int)p.x, y, cm == 0 ? (120, 120, 60) : (60, 60, 60)); }
    WritePng(outPath, Wd, Ht, rgb);
    Console.WriteLine($"{outPath}: {Wd}x{Ht}, slot {slot} frame {frame}{(frame < 0 ? " (bind pose)" : "")}; left panel = front view (Sora's left on the viewer's right), right panel = his left side (forward = right)");
}

void WritePng(string path, int w, int h, byte[] rgb) {
    var raw = new byte[h * (1 + w * 3)];
    for (int y = 0; y < h; y++) { raw[y * (1 + w * 3)] = 0; Buffer.BlockCopy(rgb, y * w * 3, raw, y * (1 + w * 3) + 1, w * 3); }
    byte[] z;
    using (var zs = new MemoryStream()) { using (var d = new System.IO.Compression.ZLibStream(zs, System.IO.Compression.CompressionLevel.Optimal, true)) d.Write(raw, 0, raw.Length); z = zs.ToArray(); }
    var crcT = new uint[256];
    for (uint n = 0; n < 256; n++) { uint c = n; for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crcT[n] = c; }
    uint Crc(byte[] d) { uint c = 0xFFFFFFFFu; foreach (var b in d) c = crcT[(c ^ b) & 0xFF] ^ (c >> 8); return c ^ 0xFFFFFFFFu; }
    void BE(Stream s, uint v) { s.WriteByte((byte)(v >> 24)); s.WriteByte((byte)(v >> 16)); s.WriteByte((byte)(v >> 8)); s.WriteByte((byte)v); }
    void Chunk(Stream s, string type, byte[] data) {
        var td = Encoding.ASCII.GetBytes(type).Concat(data).ToArray();
        BE(s, (uint)data.Length); s.Write(td, 0, td.Length); BE(s, Crc(td));
    }
    using var f = File.Create(path);
    f.Write(new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 }, 0, 8);
    var ihdr = new MemoryStream(); BE(ihdr, (uint)w); BE(ihdr, (uint)h); ihdr.Write(new byte[] { 8, 2, 0, 0, 0 }, 0, 5);
    Chunk(f, "IHDR", ihdr.ToArray()); Chunk(f, "IDAT", z); Chunk(f, "IEND", Array.Empty<byte>());
}

// ------------------------------------------------------------------------------------------- sora_params.h
string Params(List<Slot> slots) {
    var sb = new StringBuilder();
    sb.AppendLine("// GENERATED by tools/sorabake (sorabake bake): KH2 Sora's numbers from the game files. Don't edit.");
    sb.AppendLine("// prty row 1 (NeoMoveset 1 = base Sora) of 03system.bin pref, ptya set of NeoMoveset 1 from 00battle.bin.");
    sb.AppendLine("// Units: KH2 cm, 60 fps frames, radians. Constants not in a table carry their KH2 PC exe source (re:0x...).");
    sb.AppendLine("#pragma once\n");
    var sys = Bar.Read(new MemoryStream(File.ReadAllBytes(systemPath)));
    var pref = Bar.Read(sys.First(e => e.Name == "pref").Stream);
    var prtyFile = OpenKh.Kh2.SystemData.PrtyFile.Read(pref.First(e => e.Name == "prty").Stream);
    var prty = prtyFile.UniqueEntries[prtyFile.Offsets[PrtyRecord]];
    string F(float v) => v.ToString("0.0#####", System.Globalization.CultureInfo.InvariantCulture) + "f";
    sb.AppendLine($"#define KH_WALK {F(prty.WalkSpeed)}     // prty WalkSpeed, cm/frame");
    sb.AppendLine($"#define KH_RUN {F(prty.RunSpeed)}      // prty RunSpeed");
    sb.AppendLine($"#define KH_JUMP_H {F(prty.JumpHeight)}  // prty JumpHeight, cm");
    sb.AppendLine($"#define KH_TURN {F(prty.TurnSpeed)}   // prty TurnSpeed, rad/frame");
    var sstmE = pref.First(e => e.Name == "sstm").Stream; sstmE.Position = 0;
    var sstm = Xe.BinaryMapper.BinaryMapping.ReadObject<OpenKh.Kh2.SystemData.Sstm>(sstmE);
    sb.AppendLine($"#define KH_FALL_MAX {F(sstm.FallMax)}  // sstm FallMax, cm/frame");
    sb.AppendLine($"#define KH_CEILING_STOP {F(sstm.CeilingStop)}  // sstm CeilingStop");
    sb.AppendLine($"#define KH_ATTACK_DIST_TO_SPEED {F(sstm.AttackDistanceToSpeed)}  // sstm AttackDistanceToSpeed");
    sb.AppendLine(@"#define KH_G 0.40816f         // jump gravity, cm/frame^2 (re:0x5CE184)
#define KH_AIR_KEEP 0.9f      // air control with the stick held (re:0x3B7AD0)
#define KH_AIR_BRAKE 0.95f    // and without
#define KH_PHYS_STEP 2.0f     // KH frames per ACTION physics step (re:0x3B89A0)
#define KH_APEX_EARLY 5.0f    // Fall plays this many frames before the apex (re:0x3CB680)
#define KH_HOP_MIN 6.0f       // short hop window start (re:0x3CB680)
#define KH_WALKOFF_MAX 8.0f   // walk-off carry cap, cm/frame (re:0x3B9090)
#define KH_STICK_DZ 60.0f     // per-axis stick dead zone of 127.5 (re:0x39C180)
#define KH_RUN_STICK 0.8f     // run at stick magnitude >= this (re:0x3A8FA0)
#define KH_LOOP_BLEND 8.0f    // idle/walk/run change blend (re:0x3A8FA0)
#define KH_COMBO_GROUND 3     // default combo limits: ground, air, finishers per combo (re:0x4016B0)
#define KH_COMBO_AIR 3
#define KH_COMBO_FINISHERS 1
");
    sb.AppendLine("// range trigger bits in sora_motion.bin (OpenKH ids 0..16, 22, 24, 27..29 keep their number)");
    foreach (var (bn, b) in new[] { ("GROUNDED", 0), ("AIR", 1), ("BLEND_TO_IDLE", 2), ("NO_GRAVITY", 3), ("ALLOW_COMBO", 11),
        ("ALLOW_RC", 14), ("ALLOW_MOVEMENT", 17), ("KEEP_MOMENTUM", 18), ("ROTATE", 19), ("KEEP_MOTION", 20),
        ("ALLOW_FINISHER", 21), ("TURN_TO_TARGET", 22), ("ATTACK", 23), ("KEEP_KINETICS", 24), ("NO_HIT", 27),
        ("TURN_TO_LOCK", 28), ("NO_FALL_OFF", 29) })
        sb.AppendLine($"#define RB_{bn} (1u << {b})");
    sb.AppendLine("#define RB_FREE (RB_GROUNDED | RB_AIR | RB_BLEND_TO_IDLE)\n");

    // ptya: pointers[NeoMoveset] -> set; entry 0x44 bytes (OpenKH docs/kh2/file/type/00battle.md)
    var bat = Bar.Read(new MemoryStream(File.ReadAllBytes(battlePath)));
    var ptya = bat.First(e => e.Name == "ptya").Stream; ptya.Position = 0;
    var r = new BinaryReader(ptya);
    r.ReadUInt32(); int np = r.ReadInt32();
    var ptrs = Enumerable.Range(0, np).Select(_ => r.ReadInt32()).ToArray();
    ptya.Position = ptrs[1];
    int n = r.ReadInt32();
    sb.AppendLine($"// ptya set at 0x{ptrs[1]:X} (pointer 1 = NeoMoveset 1), {n} entries in table order (KH2 picks the first eligible)");
    sb.AppendLine("typedef struct { uint8_t id, type; int8_t sub, combo_offset; uint32_t flag; uint16_t motion, next;");
    sb.AppendLine("    float jump, jump_max, jump_min, speed_min, speed_max, near_, far_, low, high, inner_min, inner_max, blend, dist_adjust;");
    sb.AppendLine("    uint16_t ability, score; } KhPtya;");
    sb.AppendLine("#define PTYA_AIR (1u << 0)\n#define PTYA_JUMP_HIGH (1u << 1)\n#define PTYA_FINISHER (1u << 2)");
    sb.AppendLine("#define PTYA_LOCK 1\n#define PTYA_ENEMY 2\n#define PTYA_ALWAYS 3\n#define PTYA_ENTRY_WALL 1\n#define PTYA_ENTRY_GROUND 5\n#define PTYA_ENTRY_AIR 9");
    sb.AppendLine("static const KhPtya kh_ptya[] = {");
    for (int i = 0; i < n; i++) {
        byte id = r.ReadByte(), type = r.ReadByte(); sbyte sub = r.ReadSByte(), co = r.ReadSByte(); uint flag = r.ReadUInt32();
        ushort mot = r.ReadUInt16(), next = r.ReadUInt16();
        var fl = Enumerable.Range(0, 13).Select(_ => r.ReadSingle()).ToArray();
        ushort ab = r.ReadUInt16(), score = r.ReadUInt16();
        string nm = mset.Count > mot * 4 ? mset[mot * 4 + 2].Name : "?";
        foreach (var v in new[] { 0, 1, 2, 3 }) if (mset[mot * 4 + v].Stream.Length > 0) { nm = mset[mot * 4 + v].Name; break; }
        sb.AppendLine($"    {{{id}, {type}, {sub}, {co}, {flag}, {mot}, {next}, {string.Join(", ", fl.Select(F))}, {ab}, {score}}},  // {nm}");
    }
    sb.AppendLine("};\n");
    // range 28 turn_to_lock keeps only its bit in sora_motion.bin; its degree param (rate = deg * pi/180 * 2 per frame,
    // re:0x3B9F70) goes here, keyed by slot (TARGET_SELECT_RECON.md 7)
    sb.AppendLine("// range 28 turn_to_lock windows: mset slot, frames start..end (inclusive, as the bits), degrees param");
    sb.AppendLine("typedef struct { uint16_t slot, start, end, deg; } KhTurnLock;");
    sb.AppendLine("static const KhTurnLock kh_turn_lock[] = {");
    int nturn = 0;
    foreach (var s in slots) {
        if (s.T == null) continue;
        foreach (var rt in s.T.RangeTriggerList.Where(rt => rt.Trigger == 28)) {
            int end = rt.EndFrame < 0 ? s.Frames : rt.EndFrame;
            sb.AppendLine($"    {{{s.Index}, {rt.StartFrame}, {end}, {(rt.ParamSize > 0 ? rt.Param1 : 0)}}},  // {s.Name}");
            nturn++;
        }
    }
    if (nturn == 0) sb.AppendLine("    {0, 0, 0, 0},");
    sb.AppendLine("};\n");
    sb.AppendLine("// motion id -> mset slot (= id * 4 + variant, first non-empty of field+weapon 3, field 2, battle 0, battle-no-weapon 1)");
    sb.AppendLine("// for the motions the driver plays; the full slot list is in sora_motion.bin");
    foreach (var (nm, id) in new[] { ("IDLE", 0), ("WALK", 1), ("RUN", 2), ("JUMP", 3), ("FALL", 4), ("LAND", 5),
        ("COMBO1", 151), ("COMBO2", 152), ("COMBO3", 153), ("FINISH_V", 154), ("FINISH_H", 155), ("AIR1", 181),
        ("AIR2", 182), ("AIR_FINISH1", 183), ("AIR_FINISH2", 184), ("ROLL", 209) }) {
        int slot = new[] { 3, 2, 0, 1 }.Select(v => id * 4 + v).FirstOrDefault(sl => sl < mset.Count && slots.Any(s => s.Index == sl), -1);
        var s = slots.FirstOrDefault(x => x.Index == slot);
        sb.AppendLine($"#define MSLOT_{nm} {slot}  // {s?.Name} {s?.Frames} frames{(s != null && s.HasRoot ? $", root z {s.Eval(8, s.Start + s.Frames) - s.Eval(8, s.Start):F1} cm" : "")}");
    }
    return sb.ToString();
}

// (s58 helper GOOFYSHIELD) 'SKRP' v1 of the WEAPON alone (its bones in its own object space, KH2 cm; no hand): the
// motion KH2 plays on the detached weapon object (Goofy Bash's flight: W_EX030 motion 201 = slot 804, a 20-frame loop).
void WPose(string outPath, List<int> slots) {
    var kbMsetBytes = File.ReadAllBytes(kbMsetPath); var kbMdlxBytes = File.ReadAllBytes(kbMdlxPath);
    var kbm = Bar.Read(new MemoryStream(kbMsetBytes)).ToList();
    int nbK = OpenKh.Kh2.Models.ModelSkeletal.Read(Bar.Read(new MemoryStream(kbMdlxBytes)).First(e => e.Type == Bar.EntryType.Model).Stream).Bones.Count;
    var data = new MemoryStream(); var dw = new BinaryWriter(data); var dir = new List<(int slot, int n, long off)>();
    int head = 16 + 8 * slots.Count;
    foreach (int slot in slots) {
        var me = Bar.Read(kbm[slot].Stream).First(x => x.Type == Bar.EntryType.Motion);
        me.Stream.Position = 0;
        var fd = new Motion.InterpolatedMotion(me.Stream).InterpolatedMotionHeader.FrameData;
        kbm[slot].Stream.Position = 0;
        int start = (int)Math.Round(fd.FrameStart * 2), frames = (int)Math.Round((fd.FrameEnd - fd.FrameStart) * 2);
        var kp = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(kbMsetBytes)).ToList()).GetAnb(slot).GetAnimProvider(new MemoryStream(kbMdlxBytes));
        dir.Add((slot, frames + 1, head + data.Position));
        for (int f = 0; f <= frames; f++) {
            var K = kp.ProvideMatrices(f == 0 ? (Seek ? start : 0) : 1.0);
            for (int b = 0; b < nbK; b++) {
                var m = K[b];
                dw.Write(m.M11); dw.Write(m.M12); dw.Write(m.M13); dw.Write(m.M21); dw.Write(m.M22); dw.Write(m.M23);
                dw.Write(m.M31); dw.Write(m.M32); dw.Write(m.M33); dw.Write(m.M41); dw.Write(m.M42); dw.Write(m.M43);
            }
        }
        Console.WriteLine($"weapon slot {slot}: start {start} frames {frames} loop {(int)Math.Round((fd.FrameReturn - fd.FrameStart) * 2)}, {nbK} bones");
    }
    using var w = new BinaryWriter(File.Create(outPath));
    w.Write(0x50524B53u); w.Write(1u); w.Write((uint)slots.Count); w.Write((uint)nbK);
    foreach (var d in dir) { w.Write((ushort)d.slot); w.Write((ushort)d.n); w.Write((uint)d.off); }
    w.Write(data.ToArray());
    Console.WriteLine($"{outPath}: {slots.Count} slots, {nbK} bones, {head + data.Length} bytes");
}


record Key(float t, float v, int type, float l, float r);

class Slot {
    public readonly int Index; public readonly string Name; public readonly Motion.InterpolatedMotion M; public readonly MotionTrigger T;
    public readonly int Start, Frames, Loop; public readonly bool Loops, HasRoot;
    readonly List<Key>[] curves = new List<Key>[9];
    public Slot(int index, string name, Motion.InterpolatedMotion m, MotionTrigger t) {
        Index = index; Name = name; M = m; T = t;
        var fd = m.InterpolatedMotionHeader.FrameData;
        Start = (int)Math.Round(fd.FrameStart * 2); Frames = (int)Math.Round((fd.FrameEnd - fd.FrameStart) * 2);
        Loop = (int)Math.Round((fd.FrameReturn - fd.FrameStart) * 2); Loops = fd.FrameReturn < fd.FrameEnd;
        int nf = m.FCurvesForward.Count;
        for (int c = 0; c < 9; c++) {
            int id = m.RootPosition.FCurveId[c];
            if (id < 0) continue;
            var fc = id < nf ? m.FCurvesForward[id] : m.FCurvesInverse[id - nf];
            curves[c] = KeysOf(fc);
        }
        HasRoot = curves[6] != null || curves[7] != null || curves[8] != null;
    }
    public List<Key> Keys(int c) => curves[c];
    List<Key> KeysOf(Motion.FCurve fc) => Enumerable.Range(fc.KeyStartId, fc.KeyCount).Select(k => M.FCurveKeys[k]).Select(k => new Key(
        M.KeyTimes[(k.Type_Time >> 2) & 0x3FFF], M.KeyValues[k.ValueId], k.Type_Time & 3,
        M.KeyTangents[k.LeftTangentId], M.KeyTangents[k.RightTangentId])).ToList();
    // a bone's own curve (forward F-curves; channel low 4 bits: 0-2 scale, 3-5 rotate, 6-8 translate)
    public List<Key> JointKeys(int joint, int ch) =>
        M.FCurvesForward.Where(fc => fc.JointId == joint && (fc.Channel & 0xF) == ch).Select(KeysOf).FirstOrDefault();
    public float Eval(int c, float f, bool linear = false) => EvalKeys(curves[c], f, linear);
    // Maya-style F-curve: tangents are slopes per frame (Hermite with the span length), pre/post clamp
    public static float EvalKeys(List<Key> k, float f, bool linear = false) {
        if (k == null) return 0;
        if (f <= k[0].t) return k[0].v;
        if (f >= k[^1].t) return k[^1].v;
        int i = 0;
        while (i + 1 < k.Count && k[i + 1].t <= f) i++;
        var a = k[i]; var b = k[i + 1];
        float dt = b.t - a.t, s = (f - a.t) / dt;
        if (a.type == 0) return a.v;
        if (a.type == 1 || linear) return a.v + (b.v - a.v) * s;
        float s2 = s * s, s3 = s2 * s;
        return (2 * s3 - 3 * s2 + 1) * a.v + (s3 - 2 * s2 + s) * dt * a.r + (-2 * s3 + 3 * s2) * b.v + (s3 - s2) * dt * b.l;
    }
    public float EvalLinear(int c, float f) => Eval(c, f, true);
}

// gobake (s59 helper GAMEOVER): KH2's Game Over float motion for runtime/sora.c's KH2 Game Over screen.
//   gobake info                     what GAMEOVER.mset holds (sets, sub-motions, bone counts) + the pose check
//   gobake pose <out.bin> [set]     the Sora sub-motion of GAMEOVER.mset slot <set> (default 252 'AK10') as a pose bake
// Input: <KH2>/kh2_fifth/original/obj/GAMEOVER.mset (a Motionset BAR: slots 252 'AK10', 253 'AK12' are
// Motionsets themselves, 4 entries per motion like an mset; sub-entry 2 '0010' = 228 bones = P_EX100), P_EX100.mdlx
// (kh2_sixth), the Keyblade W_EX010 (its A000 idle grip, hung on bone 178 like tools/sorabake Pose(): the same 240-bone
// layout as data/sora_pose.bin, so actor 0 = data/sora_model.bin draws it).
// Output = tools/sorabake's SKRP v1 pose format: u32 'SKRP', u32 1, u32 n_slots (1), u32 n_bones (240); dir {u16 slot,
// u16 poses, u32 off}; poses x bones x 12 f32 (rows r0 r1 r2 r3 of a row-vector matrix, model space, KH cm). One pose
// per 60 Hz tick (KH2 GFR) from tick 0 to the motion's end (FrameEnd 150 at 30 fps = 300 ticks: 301 poses, pose 300 =
// pose 0, a seamless loop). The motion's root curves are NOT stripped (the float's bob is bone 0's own translation).
using OpenKh.Kh2;
using System; using System.IO; using System.Linq; using System.Numerics;

string Kh2 = Environment.GetEnvironmentVariable("SORABAKE_KH2");   // KH2 extract root; required
if (string.IsNullOrEmpty(Kh2)) { Console.Error.WriteLine("gobake: set SORABAKE_KH2"); Environment.Exit(2); }
string goPath = $"{Kh2}/kh2_fifth/original/obj/GAMEOVER.mset", mdlxPath = $"{Kh2}/kh2_sixth/original/obj/P_EX100.mdlx";
string kbMdlxPath = $"{Kh2}/kh2_first/original/obj/W_EX010.mdlx", kbMsetPath = $"{Kh2}/kh2_sixth/original/obj/W_EX010.mset";
const int KbParent = 178, OutSlot = 252;
var goBytes = File.ReadAllBytes(goPath); var mdlx = File.ReadAllBytes(mdlxPath);
var sora = OpenKh.Kh2.Models.ModelSkeletal.Read(Bar.Read(new MemoryStream(mdlx)).First(e => e.Type == Bar.EntryType.Model).Stream);
int nbS = sora.Bones.Count;

System.Collections.Generic.List<Bar.Entry> Sub(int set) => Bar.Read(Bar.Read(new MemoryStream(goBytes)).ToList()[set].Stream).ToList();

switch (args.Length > 0 ? args[0] : "") {
case "info": Info(); break;
case "pose": Pose(args[1], args.Length > 2 ? int.Parse(args[2]) : 252); break;
default: Console.WriteLine("gobake info | pose <out.bin> [set 252]"); break;
}

void Info() {
    var outer = Bar.Read(new MemoryStream(goBytes)).ToList();
    for (int s = 0; s < outer.Count; s++) {
        if (outer[s].Stream.Length == 0) continue;
        Console.WriteLine($"GAMEOVER.mset slot {s} '{outer[s].Name}' {outer[s].Type} {outer[s].Stream.Length} bytes");
        var inner = Sub(s);
        for (int i = 0; i < inner.Count; i++) {
            if (inner[i].Stream.Length == 0) continue;
            inner[i].Stream.Position = 0;
            var anb = Bar.Read(inner[i].Stream).ToList();
            var me = anb.First(a => a.Type == Bar.EntryType.Motion); me.Stream.Position = 0;
            var m = new Motion.InterpolatedMotion(me.Stream); var h = m.InterpolatedMotionHeader; var fd = h.FrameData;
            Console.WriteLine($"  sub {i} '{inner[i].Name}': {h.BoneCount} bones (total {h.TotalBoneCount}), frames {fd.FrameStart}..{fd.FrameEnd} return {fd.FrameReturn} at {fd.FramesPerSecond} fps");
        }
    }
    foreach (var v in new[] { "P_EX100", "P_EX100_NM", "P_EX100_TR", "P_EX100_WI", "P_EX100_LK", "P_EX100_BTLF", "P_EX110", "P_EX020", "P_EX030" }) {
        var f = $"{Kh2}/kh2_sixth/original/obj/{v}.mdlx";
        if (!File.Exists(f)) continue;
        var mm = OpenKh.Kh2.Models.ModelSkeletal.Read(Bar.Read(new MemoryStream(File.ReadAllBytes(f))).First(e => e.Type == Bar.EntryType.Model).Stream);
        Console.WriteLine($"  {v}.mdlx: {mm.Bones.Count} bones");
    }
}

void Pose(string outPath, int set) {
    var inner = Sub(set);
    int sub = Enumerable.Range(0, inner.Count).First(i => inner[i].Stream.Length > 0 && inner[i].Name == "0010");  // (228 bones = Sora)
    var p = new OpenKh.Kh2Anim.Mset.MsetIndir(inner).GetAnb(sub).GetAnimProvider(new MemoryStream(mdlx));
    var kbBytes = File.ReadAllBytes(kbMdlxPath);
    var kbIdle = new OpenKh.Kh2Anim.Mset.MsetIndir(Bar.Read(new MemoryStream(File.ReadAllBytes(kbMsetPath))).ToList()).GetAnb(0)
        .GetAnimProvider(new MemoryStream(kbBytes)).ProvideMatrices(0);
    int nbK = OpenKh.Kh2.Models.ModelSkeletal.Read(Bar.Read(new MemoryStream(kbBytes)).First(e => e.Type == Bar.EntryType.Model).Stream).Bones.Count;
    int nb = nbS + nbK, ticks = (int)Math.Round(p.FrameEnd * 60 / p.FramePerSecond);
    Console.WriteLine($"set {set} sub {sub} '{inner[sub].Name}': end {p.FrameEnd} at {p.FramePerSecond} fps -> {ticks} ticks, {nbS} + {nbK} bones");
    var data = new MemoryStream(); var dw = new BinaryWriter(data);
    Vector3 h0 = default, a0 = default;
    for (int f = 0; f <= ticks; f++) {
        var M = p.ProvideMatrices(f == 0 ? 0 : 1.0);
        if (M.Length < nbS) throw new Exception($"{M.Length} matrices for {nbS} bones");
        for (int b = 0; b < nb; b++) {
            var m = b < nbS ? M[b] : kbIdle[b - nbS] * M[KbParent];
            dw.Write(m.M11); dw.Write(m.M12); dw.Write(m.M13); dw.Write(m.M21); dw.Write(m.M22); dw.Write(m.M23);
            dw.Write(m.M31); dw.Write(m.M32); dw.Write(m.M33); dw.Write(m.M41); dw.Write(m.M42); dw.Write(m.M43);
        }
        if (f == 0) { h0 = M[140].Translation; a0 = M[16].Translation; }
        if (f % 60 == 0 || f == ticks)
            Console.WriteLine($"  tick {f,3}: hips {Fmt(M[2].Translation)} head {Fmt(M[140].Translation)} ankle {Fmt(M[16].Translation)}");
    }
    using (var w = new BinaryWriter(File.Create(outPath))) {
        w.Write(0x50524B53u); w.Write(1u); w.Write(1u); w.Write((uint)nb);
        w.Write((ushort)OutSlot); w.Write((ushort)(ticks + 1)); w.Write((uint)(16 + 8));
        w.Write(data.ToArray());
    }
    Console.WriteLine($"{outPath}: 1 slot ({OutSlot}), {ticks + 1} poses, {nb} bones, {24 + data.Length} bytes; head-ankle span {Vector3.Distance(h0, a0):F1} cm");
}
static string Fmt(Vector3 v) => $"({v.X,6:F1} {v.Y,6:F1} {v.Z,6:F1})";

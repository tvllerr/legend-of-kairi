// skatebake (s110s): KH2 skateboard recon + bake. Commands:
//   skatebake dump <mdlx>         bones, groups, per-group bind-pose bbox + dominant bones
//   skatebake mset <mset>         list the motion entries
using OpenKh.Kh2;
using System; using System.Collections.Generic; using System.IO; using System.Linq; using System.Numerics;
System.Numerics.Matrix4x4 BoneLocal(OpenKh.Kh2.Models.ModelCommon.Bone b) =>
    Matrix4x4.CreateScale(b.ScaleX, b.ScaleY, b.ScaleZ) * Matrix4x4.CreateRotationX(b.RotationX) *
    Matrix4x4.CreateRotationY(b.RotationY) * Matrix4x4.CreateRotationZ(b.RotationZ) *
    Matrix4x4.CreateTranslation(b.TranslationX, b.TranslationY, b.TranslationZ);
switch (args.Length > 0 ? args[0] : "") {
case "dump": {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(args[1])));
    var md = OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream);
    var W = new Matrix4x4[md.Bones.Count];
    for (int b = 0; b < W.Length; b++) W[b] = BoneLocal(md.Bones[b]) * (md.Bones[b].ParentIndex >= 0 ? W[md.Bones[b].ParentIndex] : Matrix4x4.Identity);
    Console.WriteLine($"{md.Bones.Count} bones, {md.Groups.Count} groups");
    int gi = 0;
    foreach (var g in md.Groups) {
        var mn = new Vector3(1e9f); var mx = new Vector3(-1e9f); var bc = new Dictionary<int, int>();
        foreach (var v in g.Mesh.Vertices) {
            var p = Vector3.Zero;
            foreach (var bp in v.BPositions) {
                var q = bp.Position; if (q.W == 0) q = new Vector4(q.X, q.Y, q.Z, 1);
                p += Vector3.Transform(new Vector3(q.X, q.Y, q.Z), W[bp.BoneIndex]) * q.W;
                bc[bp.BoneIndex] = bc.GetValueOrDefault(bp.BoneIndex) + 1;
            }
            mn = Vector3.Min(mn, p); mx = Vector3.Max(mx, p);
        }
        Console.WriteLine($"g{gi++} tex{g.Header.TextureIndex} attr{g.Header.AttributesBitfield:X} v{g.Mesh.Vertices.Count} t{g.Mesh.Triangles.Count} bbox ({mn.X:F1} {mn.Y:F1} {mn.Z:F1})-({mx.X:F1} {mx.Y:F1} {mx.Z:F1}) bones {string.Join(",", bc.OrderByDescending(k => k.Value).Take(6).Select(k => $"{k.Key}x{k.Value}"))}");
    }
    break; }
case "obj": {   // bind-pose OBJ (one object per group) for a look in Blender / matplotlib
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(args[1])));
    var md = OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream);
    var W = new Matrix4x4[md.Bones.Count];
    for (int b = 0; b < W.Length; b++) W[b] = BoneLocal(md.Bones[b]) * (md.Bones[b].ParentIndex >= 0 ? W[md.Bones[b].ParentIndex] : Matrix4x4.Identity);
    using var w = new StreamWriter(args[2]); int vb = 0, gi = 0;
    foreach (var g in md.Groups) {
        w.WriteLine($"o g{gi++}_tex{g.Header.TextureIndex}");
        foreach (var v in g.Mesh.Vertices) {
            var p = Vector3.Zero;
            foreach (var bp in v.BPositions) { var q = bp.Position; if (q.W == 0) q = new Vector4(q.X, q.Y, q.Z, 1); p += Vector3.Transform(new Vector3(q.X, q.Y, q.Z), W[bp.BoneIndex]) * q.W; }
            w.WriteLine(FormattableString.Invariant($"v {p.X:F3} {p.Y:F3} {p.Z:F3}"));
        }
        foreach (var t in g.Mesh.Triangles) w.WriteLine($"f {vb + t[0] + 1} {vb + t[1] + 1} {vb + t[2] + 1}");
        vb += g.Mesh.Vertices.Count;
    }
    break; }
case "scan": {   // flat, board-sized models (bind-pose bbox) in a directory
    foreach (var f in Directory.GetFiles(args[1], "*.mdlx")) {
        try {
            var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(f)));
            var me = bar.FirstOrDefault(e => e.Type == Bar.EntryType.Model); if (me == null) continue;
            var md = OpenKh.Kh2.Models.ModelSkeletal.Read(me.Stream);
            var W = new Matrix4x4[md.Bones.Count];
            for (int b = 0; b < W.Length; b++) W[b] = BoneLocal(md.Bones[b]) * (md.Bones[b].ParentIndex >= 0 ? W[md.Bones[b].ParentIndex] : Matrix4x4.Identity);
            var mn = new Vector3(1e9f); var mx = new Vector3(-1e9f);
            foreach (var g in md.Groups) foreach (var v in g.Mesh.Vertices) {
                var p = Vector3.Zero;
                foreach (var bp in v.BPositions) { var q = bp.Position; if (q.W == 0) q = new Vector4(q.X, q.Y, q.Z, 1); p += Vector3.Transform(new Vector3(q.X, q.Y, q.Z), W[bp.BoneIndex]) * q.W; }
                mn = Vector3.Min(mn, p); mx = Vector3.Max(mx, p);
            }
            var d = mx - mn; var dims = new[] { d.X, d.Y, d.Z }.OrderBy(x => x).ToArray();
            if (md.Groups.Count > 0 && dims[2] > 50 && dims[2] < 130 && dims[1] > 12 && dims[1] < 40 && dims[0] < 18)
                Console.WriteLine($"{Path.GetFileName(f)} bones {md.Bones.Count} groups {md.Groups.Count} dims {d.X:F0} {d.Y:F0} {d.Z:F0}");
        } catch { }
    }
    break; }
case "mot": {   // every interpolated motion of an mset: slot name frames fps joints + trigger summary
    var ms = Bar.Read(new MemoryStream(File.ReadAllBytes(args[1]))).ToList();
    for (int i = 0; i < ms.Count; i++) {
        if (ms[i].Name == "DUMM" || ms[i].Stream.Length == 0) continue;
        var anb = Bar.Read(ms[i].Stream);
        var me = anb.FirstOrDefault(x => x.Type == Bar.EntryType.Motion);
        string line = $"{i} {ms[i].Name}: entries [{string.Join(",", anb.Select(x => x.Type.ToString()))}]";
        if (me != null) {
            me.Stream.Position = 0;
            if (Motion.isInterpolated(me.Stream)) {
                me.Stream.Position = 0;
                var m = new Motion.InterpolatedMotion(me.Stream);
                var fd = m.InterpolatedMotionHeader.FrameData;
                line += $" start {fd.FrameStart} end {fd.FrameEnd} ret {fd.FrameReturn} fps {fd.FramesPerSecond} joints {m.FCurvesForward.Select(f => f.JointId).Distinct().Count()} maxjoint {m.FCurvesForward.Select(f => (int)f.JointId).DefaultIfEmpty(-1).Max()} bonecount {m.InterpolatedMotionHeader.BoneCount}";
            } else line += " (not interpolated)";
        }
        var te = anb.FirstOrDefault(x => x.Type == Bar.EntryType.MotionTriggers);
        if (te != null && te.Stream.Length > 0 && args.Length > 2) {
            te.Stream.Position = 0; var t = new MotionTrigger(te.Stream);
            line += " | " + string.Join(" ", t.RangeTriggerList.Select(r => $"R{r.Trigger}:{r.StartFrame}-{r.EndFrame}({r.Param1},{r.Param2})")) + " " + string.Join(" ", t.FrameTriggerList.Select(f => $"F{f.Trigger}@{f.Frame}({f.Param1},{f.Param2})"));
        }
        Console.WriteLine(line);
    }
    break; }
case "as": {   // nested Anb entries of a slot: mot <mset> <slot>
    var ms = Bar.Read(new MemoryStream(File.ReadAllBytes(args[1]))).ToList();
    var outer = Bar.Read(ms[int.Parse(args[2])].Stream).ToList();
    for (int k = 0; k < outer.Count; k++) {
        string line = $"  [{k}] {outer[k].Type} {outer[k].Name} len {outer[k].Stream.Length}";
        try {
            var inner = Bar.Read(outer[k].Stream).ToList();
            line += " -> " + string.Join(",", inner.Select(x => x.Type + ":" + x.Name + ":" + x.Stream.Length));
            var me = inner.FirstOrDefault(x => x.Type == Bar.EntryType.Motion);
            if (me != null) { me.Stream.Position = 0; if (Motion.isInterpolated(me.Stream)) { me.Stream.Position = 0; var m = new Motion.InterpolatedMotion(me.Stream); var fd = m.InterpolatedMotionHeader.FrameData; line += $" | start {fd.FrameStart} end {fd.FrameEnd} ret {fd.FrameReturn} fps {fd.FramesPerSecond} bones {m.InterpolatedMotionHeader.BoneCount}"; } }
        } catch (Exception ex) { line += " (not a bar: " + ex.Message + ")"; }
        Console.WriteLine(line);
    }
    break; }
case "probe": {   // probe <mset> <mdlx> <slot> <frames>: bone count + bone 0/1 + a few world positions through MsetIndir.GetAnb
    var ent = Bar.Read(new MemoryStream(File.ReadAllBytes(args[1]))).ToList();
    int slotArg = int.Parse(args[3]);
    if (args.Length > 5) {   // a nested rider slot (AS..): entry [int args[5]] of that slot's outer bar stands in as a plain entry 0
        var inner = Bar.Read(ent[slotArg].Stream).ToList()[int.Parse(args[5])];
        inner.Stream.Position = 0;
        ent = new List<Bar.Entry> { inner }; slotArg = 0;
    }
    var anb = new OpenKh.Kh2Anim.Mset.MsetIndir(ent).GetAnb(slotArg);
    var p = anb.GetAnimProvider(new MemoryStream(File.ReadAllBytes(args[2])));
    int nf = int.Parse(args[4]);
    for (int f = 0; f <= nf; f += Math.Max(1, nf / 6)) {
        var M = p.ProvideMatrices(f == 0 ? 0 : Math.Max(1, nf / 6));
        Console.WriteLine($"f{f}: {M.Length} mats; b0 {M[0].Translation.X:F1} {M[0].Translation.Y:F1} {M[0].Translation.Z:F1}; b1 {M[1].Translation.X:F1} {M[1].Translation.Y:F1} {M[1].Translation.Z:F1}; b{M.Length / 2} {M[M.Length / 2].Translation.X:F1} {M[M.Length / 2].Translation.Y:F1} {M[M.Length / 2].Translation.Z:F1}");
    }
    break; }
case "riders": {   // riders <mset>: slot 252..270 entry [2] sizes + frames
    var ms = Bar.Read(new MemoryStream(File.ReadAllBytes(args[1]))).ToList();
    for (int i = 252; i < ms.Count; i++) {
        if (ms[i].Name == "DUMM") continue;
        var outer = Bar.Read(ms[i].Stream).ToList();
        for (int k = 0; k < outer.Count; k++) {
            if (outer[k].Stream.Length == 0) continue;
            var inner = Bar.Read(outer[k].Stream).ToList();
            var me = inner.FirstOrDefault(x => x.Type == Bar.EntryType.Motion);
            string d = "-";
            if (me != null && me.Stream.Length > 0) { me.Stream.Position = 0; if (Motion.isInterpolated(me.Stream)) { me.Stream.Position = 0; var m = new Motion.InterpolatedMotion(me.Stream); var fd = m.InterpolatedMotionHeader.FrameData; d = $"start {fd.FrameStart} end {fd.FrameEnd} ret {fd.FrameReturn} bones {m.InterpolatedMotionHeader.BoneCount}"; } }
            Console.WriteLine($"slot {i} {ms[i].Name} [{k}] {outer[k].Name}: {d}");
        }
    }
    break; }
case "rider":
case "board":
    Rider.Run(args);
    break;
case "mset": {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(args[1]))).ToList();
    for (int i = 0; i < bar.Count; i++) if (bar[i].Name != "DUMM") Console.WriteLine($"{i} {bar[i].Name} len {bar[i].Stream.Length}");
    break; }
}

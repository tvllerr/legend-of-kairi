// (s110 skate 1b) bakes the REAL KH2 skateboard F_TT010 (+ the rider's own skate motions) into 'SKRG' / 'SKRP' files for sora_draw.c.
//   rider <body.mdlx> <ridersMset> <outModel> <outPose> <outHeader> [PREFIX]   body mesh + the board's 8 bones as one actor
//   board <outModel> <outPose> <outHeader> [PREFIX]                              the board alone (the parked one)
// Board motions = F_TT010.mset slots 0, 151..169; the rider's motion of board slot S = slot S+101 of the riders mset
// (F_TT010.mset: Roxas, 229 bones; F_TT010_SORA.mset: Sora, 228 bones), nested entry [2].
using OpenKh.Kh2;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Numerics;
using System.Text;

static class Rider {
    public static void Run(string[] args) {
        string K1 = (Environment.GetEnvironmentVariable("SORABAKE_KH2") ?? throw new Exception("skatebake: set SORABAKE_KH2")) + "/kh2_first/original/obj/";
        bool rid = args[0] == "rider";
        int a = rid ? 3 : 1;
        string bodyMdlx = rid ? args[1] : null, ridersMset = rid ? args[2] : null;
        string outModel = args[a], outPose = args[a + 1], outHdr = args[a + 2], prefix = args.Length > a + 3 ? args[a + 3] : (rid ? "SKR" : "SKB");
        var boardBytes = File.ReadAllBytes(K1 + "F_TT010.mdlx");
        var boardMsetBytes = File.ReadAllBytes(K1 + "F_TT010.mset");
        var bBar = Bar.Read(new MemoryStream(boardBytes));
        var bModel = OpenKh.Kh2.Models.ModelSkeletal.Read(bBar.First(e => e.Type == Bar.EntryType.Model).Stream);
        var bTex = ModelTexture.Read(bBar.First(e => e.Type == Bar.EntryType.ModelTexture).Stream);
        OpenKh.Kh2.Models.ModelSkeletal body = null; ModelTexture bodyTex = null; byte[] bodyBytes = null;
        if (rid) {
            bodyBytes = File.ReadAllBytes(bodyMdlx);
            var yBar = Bar.Read(new MemoryStream(bodyBytes));
            body = OpenKh.Kh2.Models.ModelSkeletal.Read(yBar.First(e => e.Type == Bar.EntryType.Model).Stream);
            bodyTex = ModelTexture.Read(yBar.First(e => e.Type == Bar.EntryType.ModelTexture).Stream);
        }
        var verts = new List<(int fi, int ni, float u, float v)>(); var infl = new List<(int b, Vector4 p)>();
        var idx = new List<int>(); var groups = new List<(uint tex, int attr, int fi, int ni)>();
        void Add(OpenKh.Kh2.Models.ModelSkeletal md, int boneBase, int texBase) {
            foreach (var g in md.Groups) {
                int fi = idx.Count, vb = verts.Count;
                foreach (var v in g.Mesh.Vertices) {
                    verts.Add((infl.Count, v.BPositions.Count, v.U / 4096f, v.V / 4096f));
                    foreach (var b in v.BPositions) {
                        var p = b.Position; if (p.W == 0) p = new Vector4(p.X, p.Y, p.Z, 1);
                        infl.Add((boneBase + b.BoneIndex, p));
                    }
                }
                foreach (var t in g.Mesh.Triangles) foreach (var i in t) idx.Add(vb + i);
                groups.Add((g.Header.TextureIndex + (uint)texBase, g.Header.AttributesBitfield & ~1, fi, idx.Count - fi));
            }
        }
        int nbBody = rid ? body.Bones.Count : 0, nBones = nbBody + bModel.Bones.Count;
        var images = new List<OpenKh.Imaging.IImageRead>();
        if (rid) { Add(body, 0, 0); images.AddRange(bodyTex.Images); }
        Add(bModel, nbBody, images.Count); images.AddRange(bTex.Images);
        var texData = new List<byte[]>();
        foreach (var im in images) {
            var px = OpenKh.Imaging.ImageExtensions.ToBgra32(im); int amax = 0;
            for (int i = 3; i < px.Length; i += 4) amax = Math.Max(amax, px[i]);
            if (amax <= 0x80) for (int i = 3; i < px.Length; i += 4) px[i] = (byte)Math.Min(255, px[i] * 2);
            texData.Add(px);
        }
        int head = 32, texDir = head, grpDir = texDir + 16 * texData.Count, vOff = grpDir + 16 * groups.Count, iOff = vOff + 16 * verts.Count;
        int xOff = iOff + 20 * infl.Count, dOff = xOff + 4 * idx.Count;
        using (var w = new BinaryWriter(File.Create(outModel))) {
            w.Write(0x47524B53u); w.Write(1u); w.Write((uint)nBones); w.Write((uint)groups.Count); w.Write((uint)texData.Count);
            w.Write((uint)verts.Count); w.Write((uint)infl.Count); w.Write((uint)idx.Count);
            int o = dOff;
            for (int i = 0; i < texData.Count; i++) { w.Write((uint)images[i].Size.Width); w.Write((uint)images[i].Size.Height); w.Write((uint)o); w.Write(0u); o += texData[i].Length; }
            foreach (var g in groups) { w.Write(g.tex); w.Write((uint)g.attr); w.Write((uint)g.fi); w.Write((uint)g.ni); }
            foreach (var v in verts) { w.Write((uint)v.fi); w.Write((uint)v.ni); w.Write(v.u); w.Write(v.v); }
            foreach (var x in infl) { w.Write((uint)x.b); w.Write(x.p.X); w.Write(x.p.Y); w.Write(x.p.Z); w.Write(x.p.W); }
            foreach (var i in idx) w.Write((uint)i);
            foreach (var t in texData) w.Write(t);
            Console.WriteLine($"{outModel}: {nBones} bones ({nbBody} body + {bModel.Bones.Count} board), {groups.Count} groups, {verts.Count} verts, {idx.Count / 3} tris, {texData.Count} textures");
        }
        var bEnt = Bar.Read(new MemoryStream(boardMsetBytes)).ToList();
        var rEnt = rid ? Bar.Read(new MemoryStream(File.ReadAllBytes(ridersMset))).ToList() : null;
        var data = new MemoryStream(); var dw = new BinaryWriter(data);
        var dir = new List<(int slot, int n, long off)>(); var hdr = new StringBuilder(); var present = new List<int>();
        var slotList = new List<int> { 0 }; if (rid) for (int s2 = 151; s2 <= 169; s2++) slotList.Add(s2);   // (the parked board only needs its rest pose)
        foreach (var sl in slotList) {
            if (bEnt[sl].Name == "DUMM" || bEnt[sl].Stream.Length == 0) continue;
            var bp = new OpenKh.Kh2Anim.Mset.MsetIndir(bEnt).GetAnb(sl).GetAnimProvider(new MemoryStream(boardBytes));
            bEnt[sl].Stream.Position = 0; var bInner = Bar.Read(bEnt[sl].Stream).First(x => x.Type == Bar.EntryType.Motion); bInner.Stream.Position = 0;
            if (!Motion.isInterpolated(bInner.Stream)) { Console.WriteLine($"slot {sl}: board motion not interpolated, skipped"); continue; }
            bInner.Stream.Position = 0;
            var fd = new Motion.InterpolatedMotion(bInner.Stream).InterpolatedMotionHeader.FrameData;
            Func<double, Matrix4x4[]> rpf = null;
            if (rid) {
                if (sl == 0) continue;
                rEnt[sl + 101].Stream.Position = 0; var outer = Bar.Read(rEnt[sl + 101].Stream).ToList();
                if (outer.Count < 3 || outer[2].Stream.Length == 0) { Console.WriteLine($"slot {sl}: no rider motion"); continue; }
                var inner = Bar.Read(outer[2].Stream).ToList();
                var me = inner.FirstOrDefault(x => x.Type == Bar.EntryType.Motion);
                if (me == null || me.Stream.Length == 0) { Console.WriteLine($"slot {sl}: rider motion empty"); continue; }
                outer[2].Stream.Position = 0;
                var prov = new OpenKh.Kh2Anim.Mset.MsetIndir(new List<Bar.Entry> { outer[2] }).GetAnb(0).GetAnimProvider(new MemoryStream(bodyBytes));
                rpf = tt => prov.ProvideMatrices(tt);
            }
            int start = (int)Math.Round(fd.FrameStart * 2), frames = (int)Math.Round((fd.FrameEnd - fd.FrameStart) * 2);
            int loop = fd.FrameReturn < fd.FrameEnd ? (int)Math.Round((fd.FrameReturn - fd.FrameStart) * 2) : -1;
            dir.Add((sl, frames + 1, data.Position)); present.Add(sl);
            hdr.AppendLine($"    {{{sl}, {frames}, {loop}, {start}}},  // board {bEnt[sl].Name}{(rid ? " / rider " + rEnt[sl + 101].Name : "")}: start {fd.FrameStart} end {fd.FrameEnd} ret {fd.FrameReturn} (30 fps)");
            for (int f = 0; f <= frames; f++) {
                double t = f == 0 ? start : 1.0;
                var B = bp.ProvideMatrices(t);
                Matrix4x4[] R = rid ? rpf(t) : null;
                if (rid && R.Length < nbBody) throw new Exception($"slot {sl}: {R.Length} rider matrices for {nbBody} bones");
                for (int b = 0; b < nBones; b++) {
                    var m = b < nbBody ? R[b] : B[b - nbBody];
                    dw.Write(m.M11); dw.Write(m.M12); dw.Write(m.M13); dw.Write(m.M21); dw.Write(m.M22); dw.Write(m.M23);
                    dw.Write(m.M31); dw.Write(m.M32); dw.Write(m.M33); dw.Write(m.M41); dw.Write(m.M42); dw.Write(m.M43);
                }
            }
        }
        int phead = 16 + 8 * dir.Count;
        using (var w = new BinaryWriter(File.Create(outPose))) {
            w.Write(0x50524B53u); w.Write(1u); w.Write((uint)dir.Count); w.Write((uint)nBones);
            foreach (var d in dir) { w.Write((ushort)d.slot); w.Write((ushort)d.n); w.Write((uint)(phead + d.off)); }
            w.Write(data.ToArray());
        }
        File.WriteAllText(outHdr, "// GENERATED by tools_s110s/skatebake `" + args[0] + "` (KH2 F_TT010 board + " + (rid ? Path.GetFileName(bodyMdlx) + " rider motions" : "alone") + "). Don't edit.\n#pragma once\n" +
            $"#define {prefix}_BONES {nBones}\n#define {prefix}_BODY_BONES {nbBody}\n// {{board mset slot, frames (60 Hz), loop frame (-1 none), start (60 Hz)}}\nstatic const struct {{ int slot, frames, loop, start; }} {prefix}_SLOTS[] = {{\n" + hdr + "};\n");
        Console.WriteLine($"{outPose}: {dir.Count} slots, {nBones} bones, {phead + data.Length} bytes; slots {string.Join(" ", present)}");
    }
}

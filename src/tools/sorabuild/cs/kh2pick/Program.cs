// kh2pick <Image/dt dir> <list.json> <out dir>
// list.json = {"files": [["kh2_first/original/obj/P_EX100.mdlx", size, "sha256"], ...]} (our names + hashes only, no game bytes)
// Writes <out>/<kh2_x>/original/<name> and <out>/<kh2_x>/remastered/<name>/<asset> exactly like
// `OpenKh.Command.IdxImg hed extract`, only for the listed paths, then checks every sha256. Exit 1 on any miss/mismatch.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text.Json;
using OpenKh.Egs;

static class P
{
    static int Main(string[] a)
    {
        if (a.Length < 3) { Console.Error.WriteLine("usage: kh2pick <Image/dt dir> <list.json> <out dir>"); return 2; }
        string dt = a[0], outd = a[2];
        var doc = JsonDocument.Parse(File.ReadAllText(a[1]));
        var want = new Dictionary<string, (long size, string sha)>();
        foreach (var e in doc.RootElement.GetProperty("files").EnumerateArray())
            want[e[0].GetString()] = (e[1].GetInt64(), e[2].GetString());
        // per package: name -> wanted parts ("original" or remastered asset name)
        var byPkg = new Dictionary<string, Dictionary<string, List<string>>>();
        foreach (var rel in want.Keys)
        {
            var p = rel.Split('/');
            string pkg = p[0], kind = p[1], rest = string.Join("/", p.Skip(2));
            string name, part;
            if (kind == "original") { name = rest; part = "\0orig"; }
            else if (kind != "remastered") { Console.Error.WriteLine("bad path " + rel); return 2; }
            if (!byPkg.TryGetValue(pkg, out var m)) byPkg[pkg] = m = new Dictionary<string, List<string>>();
            if (kind == "original") { Add(m, rest, "\0orig"); continue; }
            // remastered/<name>/<asset>: asset names can hold '/' too (B_CA040.a.us + se/obj/se296), so register every split;
            // only real hed names ever match, and the final check below catches any listed path not written
            for (int i = rest.IndexOf('/'); i > 0; i = rest.IndexOf('/', i + 1))
                Add(m, rest.Substring(0, i), rest.Substring(i + 1));
        }
        int bad = 0, done = 0;
        var left = new HashSet<string>(want.Keys);
        foreach (var (pkg, names) in byPkg.OrderBy(x => x.Key))
        {
            string hed = Path.Combine(dt, pkg + ".hed"), pkgf = Path.Combine(dt, pkg + ".pkg");
            if (!File.Exists(hed) || !File.Exists(pkgf)) { Console.Error.WriteLine("missing " + hed + " / .pkg"); return 1; }
            using var hs = File.OpenRead(hed);
            using var ps = File.OpenRead(pkgf);
            foreach (var en in Hed.Read(hs))
            {
                var md5 = Helpers.ToString(en.MD5);
                if (!EgsTools.Names.TryGetValue(md5, out var name) || !names.TryGetValue(name, out var parts)) continue;
                ps.Position = en.Offset;
                var asset = new EgsHdAsset(ps);
                foreach (var part in parts)
                {
                    byte[] data; string rel;
                    if (part == "\0orig") { data = asset.OriginalData; rel = pkg + "/original/" + name; }
                    else
                    {
                        rel = pkg + "/remastered/" + name + "/" + part;
                        if (!asset.RemasteredAssetsDecompressedData.TryGetValue(part, out data)) continue;  // another split of the path
                    }
                    if (!left.Remove(rel)) continue;  // written already
                    var dst = Path.Combine(outd, rel);
                    Directory.CreateDirectory(Path.GetDirectoryName(dst));
                    File.WriteAllBytes(dst, data);
                    var (size, sha) = want[rel];
                    var got = Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
                    if (data.LongLength != size || got != sha) { Console.Error.WriteLine("DIFF " + rel + " size " + data.LongLength + " sha " + got); bad++; }
                    done++;
                }
            }
        }
        foreach (var n in left.OrderBy(x => x)) { Console.Error.WriteLine("NOT FOUND " + n); bad++; }
        Console.WriteLine("kh2pick: " + done + " files, " + bad + " problems");
        return bad == 0 ? 0 : 1;
    }

    static void Add(Dictionary<string, List<string>> m, string name, string part)
    {
        if (!m.TryGetValue(name, out var l)) m[name] = l = new List<string>();
        l.Add(part);
    }
}

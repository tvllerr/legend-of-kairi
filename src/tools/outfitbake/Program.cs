// outfitbake bonecmp <a.mdlx> <b.mdlx>: do two KH2 models share one skeleton (bone count, parent, local transform)?
using OpenKh.Kh2; using System; using System.IO; using System.Linq; using System.Reflection;
if (args.Length < 3 || args[0] != "bonecmp") { Console.WriteLine("usage: outfitbake bonecmp a.mdlx b.mdlx"); return 2; }
OpenKh.Kh2.Models.ModelSkeletal Load(string p) { var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(p))); return OpenKh.Kh2.Models.ModelSkeletal.Read(bar.First(e => e.Type == Bar.EntryType.Model).Stream); }
var A = Load(args[1]); var B = Load(args[2]);
Console.WriteLine($"{Path.GetFileName(args[1])}: {A.Bones.Count} bones; {Path.GetFileName(args[2])}: {B.Bones.Count} bones");
if (A.Bones.Count != B.Bones.Count) { Console.WriteLine("DIFFERENT bone count"); return 1; }
int bad = 0; double maxd = 0;
var props = A.Bones[0].GetType().GetProperties().Where(p => p.PropertyType == typeof(float) || p.PropertyType == typeof(int) || p.PropertyType == typeof(short) || p.PropertyType == typeof(ushort) || p.PropertyType == typeof(uint)).ToList();
for (int i = 0; i < A.Bones.Count; i++)
    foreach (var p in props) {
        double a = Convert.ToDouble(p.GetValue(A.Bones[i])), b = Convert.ToDouble(p.GetValue(B.Bones[i]));
        double d = Math.Abs(a - b); if (d > maxd) maxd = d;
        if (d > 1e-4) { if (bad++ < 12) Console.WriteLine($"  bone {i} {p.Name}: {a} vs {b}"); }
    }
Console.WriteLine($"fields compared: {string.Join(",", props.Select(p => p.Name))}");
Console.WriteLine(bad == 0 ? "SAME skeleton (all fields equal)" : $"{bad} field differences, max {maxd}");
return bad == 0 ? 0 : 1;

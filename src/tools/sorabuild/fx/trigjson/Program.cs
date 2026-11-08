// trigjson <kh2 root> <mset path or obj name> <out.json>: every mset entry's frame + range triggers as JSON.
// Verbatim decode of tools_s110/sorabake `trigjson` (s110 roxsfx); only the KH2 root is an argument.
using OpenKh.Kh2;
using System; using System.Collections.Generic; using System.IO; using System.Linq; using System.Text;

string Kh2 = args[0];
string ObjFile(string name, string ext) {
    if (name.Contains('/') || name.Contains((char)92)) return name;
    var six = $"{Kh2}/kh2_sixth/original/obj/{name}.{ext}";
    return File.Exists(six) ? six : $"{Kh2}/kh2_first/original/obj/{name}.{ext}";
}
var mp = ObjFile(args[1], "mset");
var ms = Bar.Read(new MemoryStream(File.ReadAllBytes(mp))).ToList();
var sb = new StringBuilder("{\n");
bool firstE = true;
for (int i = 0; i < ms.Count; i++) {
    var e = ms[i];
    if (e.Stream.Length == 0) continue;
    e.Stream.Position = 0;
    List<Bar.Entry> inner;
    try { inner = Bar.Read(e.Stream).ToList(); } catch { continue; }
    var te = inner.FirstOrDefault(x => x.Type == Bar.EntryType.MotionTriggers);
    if (te == null || te.Stream.Length == 0) continue;
    te.Stream.Position = 0;
    var t = new MotionTrigger(te.Stream);
    var fr = string.Join(",", t.FrameTriggerList.Select(f => $"[{f.Trigger},{f.Frame},{f.ParamSize},{f.Param1},{f.Param2}]"));
    var rg = string.Join(",", t.RangeTriggerList.Select(r => $"[{r.Trigger},{r.StartFrame},{r.EndFrame},{r.ParamSize},{r.Param1},{r.Param2}]"));
    if (!firstE) sb.Append(",\n");
    firstE = false;
    sb.Append($"\"{i}\":{{\"name\":\"{e.Name}\",\"frame\":[{fr}],\"range\":[{rg}]}}");
}
sb.Append("\n}\n");
File.WriteAllText(args[2], sb.ToString());
Console.WriteLine($"{mp}: {ms.Count} entries -> {args[2]}");

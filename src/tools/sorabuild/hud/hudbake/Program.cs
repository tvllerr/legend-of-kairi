// hudbake: KH2 HUD art (HP/MP gauge, Sora's portrait) for runtime/sora_hud.c. Recon: D:/SoraBotW/recon/SORA_HUD_RECON.md
//   hudbake bar <file>                         BAR entries
//   hudbake seq <2dd> <name>                   dump a Seqd (sprites, sprite groups, animation groups)
//   hudbake imgd <2dd> <name> <out.png>        an Imgd as PNG
//   hudbake render <2dd> <name> <hd.png|-> <out.png> [at x y] [sg N [0xAABBGGRR]]... [ag N [frame]]   mini sequence renderer
//                                              (ag = an animation group as KH2's code names them, at its steady frame by default)
//   hudbake hud <out.bin>                      the runtime's hud.bin (HD gauge sheet + Sora's face)
//   hudbake cmd <out.bin> [world] [atlas.png]  the runtime's cmd.bin: the command menu (world = tt0 default, hb0, ...)
//   hudbake cmdtest <cmd.bin> <out.png>        draws the menu pages from a cmd.bin as runtime/sora_hud.c lays them out
// Build (sorabuild step hud_build): dotnet build -c Release -o <dir> -p:OpenKh=<OpenKH release DLL dir>; run with SORABUILD_KH2=<KH2 extract>
using OpenKh.Kh2;
using System; using System.Collections.Generic; using System.IO; using System.Linq; using System.Reflection; using System.Text;

string Kh2 = Environment.GetEnvironmentVariable("SORABUILD_KH2") ?? throw new Exception("set SORABUILD_KH2 = the KH2 extract root");
// the command menu bake (CmdBake): KH2 animation group ids, the labels' sys-font glyph codes (msg/us/sys.bar messages
// 480-483, 1288, 1294, 1291, 1297), the font's cell (PS2 px; the HD font PNG is 2x)
int[] CmdGroups = { 6, 7, 9, 11, 16, 17, 18, 19, 20, 37, 39, 41, 44, 45, 57, 58, 59, 60, 65, 66, 67, 68, 69,
    2, 3, 4, 5 };  // (s134 rcslot) 2..5 = comw's Reaction Command slot (2 offered, 3 cmd flag, 4 flash, 5 offered state 7)
// (s68 helper VALOR BAKE) labels 8.. = the Drive page (sys.bar 485 Valor Form, 484 Revert, 486 Wisdom Form, 20095
// Limit Form (= cmd 0x2A1 msg 52863), 487 Master Form, 488 Final Form, 489 Antiform; bytes read from sys.bar with
// tools/kh2_dream_text.py's reader, 0x01 = space). cmd.bin v1 keeps its fixed table of 8 (what sora_hud.c's N_LABEL
// reads); labels 8.. live in the 'SKLX' trailer (CmdBake). SORA_CMD_LABELS=8 = the pre-s68 bake (byte-identical cmd.bin).
string[] LabelNames = { "Attack", "Magic", "Items", "Drive", "Fire", "Blizzard", "Thunder", "Cure",
    "Valor Form", "Revert", "Wisdom Form", "Limit Form", "Master Form", "Final Form", "Antiform",
    // (s88) 15.. = the spell tiers (sys.bar 1289 Fira, 1290 Firaga, 1295 Blizzara, 1296 Blizzaga, 1292 Thundara, 1293 Thundaga,
    // 1298 Cura, 1299 Curaga); SORA_CMD_LABELS=15 = the s71 bake
    "Fira", "Firaga", "Blizzara", "Blizzaga", "Thundara", "Thundaga", "Cura", "Curaga",
    // (s89 limit) 23.. = Limit Form's command page: cmd 707 "Limit" (sys.bar 20235, the top slot in Limit Form) and its
    // Limit commands cmd 698 Sonic Rave (20222), 704 Strike Raid (20228), 701 Last Arcanum (20225), 683 Infinity (20214);
    // bytes read from sys.bar (kh2_dream_text.py's reader). SORA_CMD_LABELS=23 = the s88 bake
    "Limit", "Sonic Rave", "Strike Raid", "Last Arcanum", "Infinity",
    // (s119 mkmenu) 28 Jump, 29 Pearl = KH2's Mickey menu labels (sys.bar 1203 / 15252, bytes read with tools_s116/kmsg.py);
    // 30..35 = the button icons (codes >= 1000 = cell (code-1000) of fontimage.bar's 'icon' sheet, 21 cols of 24 PS2 px cells):
    // 30 Circle, 31 Square, 32 Cross (PlayStation art, idx 70 / 69 / 68), 33 B, 34 X, 35 A (the Xbox art on the same buttons, idx 37 / 36 / 35).
    // SORA_CMD_LABELS=28 = the s89 bake (byte-identical cmd.bin)
    "Jump", "Pearl", "Btn Circle", "Btn Square", "Btn Cross", "Btn B", "Btn X", "Btn A",
    // (s133 magic) 36.. = Magnet, Magnera, Magnega, Reflect, Reflera, Reflega (sys.bar 1300-1305, bytes read with tools_s116/kmsg.py; sora.c MG_LABEL_BASE 36).
    // SORA_CMD_LABELS=36 = the s119 bake (byte-identical cmd.bin)
    "Magnet", "Magnera", "Magnega", "Reflect", "Reflera", "Reflega",
    // (s134 rcslot) 42 Btn Triangle (icon code 1067 = sheet cell 67), 43 Btn Y (1034 = cell 34): the Reaction Command slot's button icon.
    // SORA_CMD_LABELS=42 = the s133 bake
    "Btn Triangle", "Btn Y",
    // (s137 smenu) 44.. = the Summon page: sys.bar 1284 Summon, 15225 Chicken Little, 17051 FPS Mode, 17052 Balls, 17053 Firecracker, 17054 End, 10434 Dismiss
    // (ids read from the cmd rows of 03system.bin, name = u16 at +8 & 0x7FFF). SORA_CMD_LABELS=44 = the s134 bake
    "Summon", "Chicken Little", "FPS Mode", "Balls", "Firecracker", "End", "Dismiss",
    // (s137 stitch) 51..55 = Stitch's names: sys.bar 1308 Stitch (cmd 88), 16862 Ohana! (cmd 519, the Limit), 17033 Shoot (525), 17034 Ukulele (526), 17047 Blast (527)
    // (ids read from the cmd rows of 03system.bin; bytes from sys.bar, '!' = 72). 56 = a placeholder (s137 review M3: b137-sumdata reserves 51..56, Peter Pan
    // starts at 57). SORA_CMD_LABELS=51 = the s137 smenu bake. NOTE the exe takes at most 128 labels since the s137 review (64 before: runtime/sora_hud.c).
    "Stitch", "Ohana!", "Shoot", "Ukulele", "Blast", "-",
    // (s137 sumdata) 57.. = Peter Pan / Genie (sys.bar ids read from the cmd rows of 03system.bin, name = u16 at +8 & 0x7FFF; bytes decoded with tools_s116/kmsg.py):
    //   57 Peter Pan (cmd 306, msg 10330), 58 Genie (cmd 89, 1309), 59 Never Land (cmd 468, 15598), 60 The Flying Boy (470, 15600), 61 Tiny Fairy (471, 15601),
    //   62 Journey's End (479, 15609; glyph 236 = the apostrophe), 63 Stop (482, 14439; Stitch's cmd 521 has the same text: keep ONE of them at merge),
    //   64..67 the Genie versions "{ed}Valor{ed} Genie" .. Final (cmds 535..538, msgs 17078..17081; glyph 237 = the star-like mark KH2 puts round the form name),
    //   68 Sonic (542, 17165), 69 Rave (543, 17166), 70 Strike (544, 17167), 71 Judgment (545, 17168), 72 Arcana (546, 17169), 73 Bash (547, 17170),
    //   74 Impact (549, 17172).  Cmd 548 "Infinity" (17171) = the existing label 27 (same text).
    //   SORA_CMD_LABELS=57 = the s137 smenu bake + Stitch's rows (the first 57 entries); 51 = the s137 smenu bake
    "Peter Pan", "Genie", "Never Land", "The Flying Boy", "Tiny Fairy", "Journey's End", "Stop",
    "Valor Genie", "Wisdom Genie", "Master Genie", "Final Genie",
    "Sonic", "Rave", "Strike", "Judgment", "Arcana", "Bash", "Impact" };
int[][] LabelCodes = {
    new[] { 46, 173, 173, 154, 156, 164 }, new[] { 58, 154, 160, 162, 156 }, new[] { 54, 173, 158, 166, 172 },
    new[] { 49, 171, 162, 175, 158 }, new[] { 51, 162, 171, 158 }, new[] { 47, 165, 162, 179, 179, 154, 171, 157 },
    new[] { 65, 161, 174, 167, 157, 158, 171 }, new[] { 48, 174, 171, 158 },
    new[] { 67, 154, 165, 168, 171, 1, 51, 168, 171, 166 }, new[] { 63, 158, 175, 158, 171, 173 },
    new[] { 68, 162, 172, 157, 168, 166, 1, 51, 168, 171, 166 }, new[] { 57, 162, 166, 162, 173, 1, 51, 168, 171, 166 },
    new[] { 58, 154, 172, 173, 158, 171, 1, 51, 168, 171, 166 }, new[] { 51, 162, 167, 154, 165, 1, 51, 168, 171, 166 },
    new[] { 46, 167, 173, 162, 159, 168, 171, 166 },
    new[] { 51, 162, 171, 154 }, new[] { 51, 162, 171, 154, 160, 154 },
    new[] { 47, 165, 162, 179, 179, 154, 171, 154 }, new[] { 47, 165, 162, 179, 179, 154, 160, 154 },
    new[] { 65, 161, 174, 167, 157, 154, 171, 154 }, new[] { 65, 161, 174, 167, 157, 154, 160, 154 },
    new[] { 48, 174, 171, 154 }, new[] { 48, 174, 171, 154, 160, 154 },
    new[] { 57, 162, 166, 162, 173 }, new[] { 64, 168, 167, 162, 156, 1, 63, 154, 175, 158 },
    new[] { 64, 173, 171, 162, 164, 158, 1, 63, 154, 162, 157 }, new[] { 57, 154, 172, 173, 1, 46, 171, 156, 154, 167, 174, 166 },
    new[] { 54, 167, 159, 162, 167, 162, 173, 178 },
    new[] { 55, 174, 166, 169 }, new[] { 61, 158, 154, 171, 165 },
    new[] { 1070 }, new[] { 1069 }, new[] { 1068 }, new[] { 1037 }, new[] { 1036 }, new[] { 1035 },
    new[] { 58, 154, 160, 167, 158, 173 }, new[] { 58, 154, 160, 167, 158, 171, 154 }, new[] { 58, 154, 160, 167, 158, 160, 154 },
    new[] { 63, 158, 159, 165, 158, 156, 173 }, new[] { 63, 158, 159, 165, 158, 171, 154 }, new[] { 63, 158, 159, 165, 158, 160, 154 },
    new[] { 1067 }, new[] { 1034 },
    new[] { 64, 174, 166, 166, 168, 167 }, new[] { 48, 161, 162, 156, 164, 158, 167, 1, 57, 162, 173, 173, 165, 158 },
    new[] { 51, 61, 64, 1, 58, 168, 157, 158 }, new[] { 47, 154, 165, 165, 172 },
    new[] { 51, 162, 171, 158, 156, 171, 154, 156, 164, 158, 171 }, new[] { 50, 167, 157 }, new[] { 49, 162, 172, 166, 162, 172, 172 },
    new[] { 64, 173, 162, 173, 156, 161 }, new[] { 60, 161, 154, 167, 154, 72 }, new[] { 64, 161, 168, 168, 173 },
    new[] { 66, 164, 174, 165, 158, 165, 158 }, new[] { 47, 165, 154, 172, 173 },  // (s137 review S5: Ukulele = sys.bar 17034 bytes, VERIFIED)
    new[] { 1 },  // 56 placeholder
    new[] { 61, 158, 173, 158, 171, 1, 61, 154, 167 }, new[] { 52, 158, 167, 162, 158 },
    new[] { 59, 158, 175, 158, 171, 1, 57, 154, 167, 157 }, new[] { 65, 161, 158, 1, 51, 165, 178, 162, 167, 160, 1, 47, 168, 178 },
    new[] { 65, 162, 167, 178, 1, 51, 154, 162, 171, 178 }, new[] { 55, 168, 174, 171, 167, 158, 178, 236, 172, 1, 50, 167, 157 }, new[] { 64, 173, 168, 169 },
    new[] { 237, 67, 154, 165, 168, 171, 237, 1, 52, 158, 167, 162, 158 }, new[] { 237, 68, 162, 172, 157, 168, 166, 237, 1, 52, 158, 167, 162, 158 },
    new[] { 237, 58, 154, 172, 173, 158, 171, 237, 1, 52, 158, 167, 162, 158 }, new[] { 237, 51, 162, 167, 154, 165, 237, 1, 52, 158, 167, 162, 158 },
    new[] { 64, 168, 167, 162, 156 }, new[] { 63, 154, 175, 158 }, new[] { 64, 173, 171, 162, 164, 158 }, new[] { 55, 174, 157, 160, 166, 158, 167, 173 },
    new[] { 46, 171, 156, 154, 167, 154 }, new[] { 47, 154, 172, 161 }, new[] { 54, 166, 169, 154, 156, 173 } };
{   int nl = int.TryParse(Environment.GetEnvironmentVariable("SORA_CMD_LABELS"), out int nlv) ? Math.Clamp(nlv, 8, LabelCodes.Length) : LabelCodes.Length;
    LabelCodes = LabelCodes.Take(nl).ToArray(); LabelNames = LabelNames.Take(nl).ToArray();
    if (nl <= 43) CmdGroups = CmdGroups.Where(g => g > 5).ToArray(); }  // (s134 review: SORA_CMD_LABELS<=42 = the pre-s134 bakes byte-identical, no RC slot groups 2..5)
const int FontCols = 28, FontFirst = 32, CellW = 18, CellH = 24;
switch (args.Length > 0 ? args[0] : "") {
case "types":
    foreach (var n in args.Skip(1)) DumpType(n);
    break;
case "bar":
    DumpBar(args[1]);
    break;
case "seq":   // seq <2dd> <entry name> [group filter]: sprites, sprite groups, animation groups of a Seqd
    DumpSeq(args[1], args[2]);
    break;
case "render":  // render <2dd> <seqd name> <hd.png|-> <out.png> sg <group>... | ag <animgroup> <frame>: draws sprite groups at the centre of a 512x256 canvas (x2 with HD)
    Render(args[1], args[2], args[3], args[4], args.Skip(5).ToList());
    break;
case "hud":    // hud <out.bin>: D:/SoraBotW/data/hud/hud.bin for runtime/sora_hud.c (format: HudBake())
    HudBake(args[1]);
    break;
case "imgd":  // imgd <2dd> <entry name> <out.png>: the Imgd as RGBA PNG
    ImgdPng(args[1], args[2], args[3]);
    break;
case "cmd":   // cmd <out.bin> [world] [atlas.png]: D:/SoraBotW/data/hud/cmd.bin for runtime/sora_hud.c (format: CmdBake())
    CmdBake(args[1], args.Length > 2 ? args[2] : "tt0", args.Length > 3 ? args[3] : null);
    break;
case "lock":  // lock <out.bin>: D:/SoraBotW/data/hud/lock.bin, KH2's lock-on reticle for runtime/sora_hud.c (format: LockBake())
    LockBake(args[1]);
    break;
case "locktest":  // locktest <lock.bin> <out.png> <icon> <frame>...: frames of the bake side by side (as sora_hud.c draws them, 4x)
    LockTest(args[1], args[2], int.Parse(args[3]), args.Skip(4).Select(int.Parse).ToList());
    break;
case "cmdtest":  // cmdtest <cmd.bin> <out.png>: the three menu pages composed from the bake (the layout of sora_hud.c)
    CmdTest(args[1], args[2]);
    break;
case "dream":  // (s54 dream) dream <out.bin> [labels.txt]: D:/SoraBotW/data/hud/dream.bin, KH2's Station of Awakening lines (format: DreamBake())
    DreamBake(args[1], args.Length > 2 ? args[2] : throw new Exception("dream: give the labels file"));
    break;
case "lvup":  // (s55) lvup <out.bin> [labels.txt]: KH2's level-up panel (gaug anim groups 104 / 105 + its text) for sora_hud.c (format: LvupBake())
    LvupBake(args[1], args.Length > 2 ? args[2] : throw new Exception("lvup: give the labels file"));
    break;
case "lvupshot":  // (s55) lvupshot <lvup.bin> <hud.bin> <shot.png> <out.png> [frame]: the panel over a game screenshot at the HUD's scale
    LvupShot(args[1], args[2], args[3], args[4], args.Length > 5 ? int.Parse(args[5]) : 60);
    break;
case "lvuptest":  // (s55) lvuptest <lvup.bin> <hud.bin> <out.png>: the panel as the wip sora_hud.c lays it out, at several frames
    LvupTest(args[1], args[2], args[3]);
    break;
case "party":  // (s57 party) party <out.bin>: KH2's party gauges (gaug anim groups 64-74 + Donald's / Goofy's faces) for sora_hud.c (format: PartyBake())
    PartyBake(args[1]);
    break;
case "partytest":  // (s57 party) partytest <party.bin> <hud.bin> <out.png>: the plates in several states as the wip sora_hud.c draws them
    PartyTest(args[1], args[2], args[3]);
    break;
case "partyshot":  // (s57 party) partyshot <party.bin> <hud.bin> <shot.png> <out.png> [x y]: ring + plates over a screenshot (x, y = ring centre, HUD units from the bottom-right; default 60 177)
    PartyShot(args[1], args[2], args[3], args[4], args.Length > 6 ? float.Parse(args[5]) : 60, args.Length > 6 ? float.Parse(args[6]) : 177);
    break;
case "drive":  // (s67 drive) drive <out.bin>: KH2's Drive gauge (gaug anim groups 20-37) for sora_hud.c (format: DriveBake())
    DriveBake(args[1]);
    break;
case "drivetest":  // (s67 drive) drivetest <drive.bin> <hud.bin> <out.png> [party.bin]: the gauge in several states beside Sora's ring (+ the party plates)
    DriveTest(args[1], args[2], args[3], args.Length > 4 ? args[4] : null);
    break;
case "dreamtest":  // (s54 dream) dreamtest <cmd.bin> <dream.bin> <out.png>: the picker's pages as the wip sora_hud.c lays them out
    DreamTest(args[1], args[2], args[3], args.Length > 4 && args[4] == "titles");
    break;
case "camp":  // (s78 camp) camp <out.bin> [variant 0-3]: D:/SoraBotW/data/hud/camp.bin, the camp sheet + sys font (format: CampBake())
    CampBake(args[1], args.Length > 2 ? int.Parse(args[2]) : 0);
    break;
case "campseq":  // (s78 camp) campseq <out.bin> <camp_text.txt>: D:/SoraBotW/data/hud/camp_seq.bin (format: runtime/sora_camp_seq.h)
    CampSeqBake(args[1], args[2]);
    break;
default:
    Console.WriteLine("hudbake types <OpenKh.Kh2.TypeName>... | bar <file>");
    break;
}

void DumpType(string n) {
    var t = typeof(Bar).Assembly.GetType(n) ?? typeof(OpenKh.Imaging.ImageExtensions).Assembly.GetType(n);
    if (t == null) { Console.WriteLine("MISSING " + n); return; }
    Console.WriteLine("=== " + t.FullName);
    foreach (var m in t.GetMethods(BindingFlags.Public | BindingFlags.Static | BindingFlags.Instance | BindingFlags.DeclaredOnly)) Console.WriteLine("  M " + m);
    foreach (var p in t.GetProperties()) Console.WriteLine("  P " + p.PropertyType.Name + " " + p.Name);
    foreach (var f in t.GetFields()) Console.WriteLine("  F " + f.FieldType.Name + " " + f.Name);
    foreach (var nt in t.GetNestedTypes()) {
        Console.WriteLine("  --- nested " + nt.Name);
        foreach (var p in nt.GetProperties()) Console.WriteLine("     P " + p.PropertyType.Name + " " + p.Name);
        foreach (var f in nt.GetFields()) Console.WriteLine("     F " + f.FieldType.Name + " " + f.Name);
    }
}

void DumpSeq(string path, string name) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(path)));
    var e = bar.First(x => x.Type == Bar.EntryType.Seqd && x.Name == name);
    e.Stream.Position = 0;
    var s = Sequence.Read(e.Stream);
    Console.WriteLine($"Seqd '{name}': unk04 {s.Unknown04} uv {s.UVEnableFlag}, {s.Sprites.Count} sprites, {s.SpriteGroups.Count} sprite groups, {s.AnimationGroups.Count} animation groups");
    for (int i = 0; i < s.Sprites.Count; i++) {
        var p = s.Sprites[i];
        Console.WriteLine($"  sprite {i}: uv {p.Left},{p.Top} - {p.Right},{p.Bottom} ({p.Right - p.Left}x{p.Bottom - p.Top}) col L {p.ColorLeft:X8} T {p.ColorTop:X8} R {p.ColorRight:X8} B {p.ColorBottom:X8} scroll {p.UTranslation},{p.VTranslation}");
    }
    for (int i = 0; i < s.SpriteGroups.Count; i++) {
        var g = s.SpriteGroups[i];
        Console.WriteLine($"  group {i}: {g.Count} parts");
        foreach (var q in g) Console.WriteLine($"     part: sprite {q.SpriteIndex} at {q.Left},{q.Top} - {q.Right},{q.Bottom} ({q.Right - q.Left}x{q.Bottom - q.Top})");
    }
    for (int i = 0; i < s.AnimationGroups.Count; i++) {
        var g = s.AnimationGroups[i];
        Console.WriteLine($"  anim group {i}: {g.Animations.Count} anims, noloop {g.DoNotLoop} unk06 {g.Unknown06} loop {g.LoopStart}-{g.LoopEnd} light {g.LightPositionX} text {g.TextPositionX},{g.TextPositionY} scale {g.TextScale} pad {g.UiPadding}");
        foreach (var a in g.Animations)
            Console.WriteLine($"     anim: flags {a.Flags:X} sg {a.SpriteGroupIndex} f {a.FrameStart}-{a.FrameEnd} tr {a.TranslateXStart},{a.TranslateYStart}->{a.TranslateXEnd},{a.TranslateYEnd} piv {a.PivotXStart},{a.PivotYStart}->{a.PivotXEnd},{a.PivotYEnd} rot {a.RotationXStart},{a.RotationYStart},{a.RotationZStart}->{a.RotationXEnd},{a.RotationYEnd},{a.RotationZEnd} sc {a.ScaleStart}->{a.ScaleEnd} sx {a.ScaleXStart}->{a.ScaleXEnd} sy {a.ScaleYStart}->{a.ScaleYEnd} curve {a.CurveXStart},{a.CurveYStart}->{a.CurveXEnd},{a.CurveYEnd} bounce {a.BounceXStart},{a.BounceYStart}->{a.BounceXEnd},{a.BounceYEnd} x{a.BounceXCount},{a.BounceYCount} colblend {a.ColorBlend} col {a.ColorStart:X8}->{a.ColorEnd:X8}");
    }
}

void Render(string path, string name, string hdPng, string outPath, List<string> what) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(path)));
    var se = bar.First(x => x.Type == Bar.EntryType.Seqd && x.Name == name); se.Stream.Position = 0;
    var s = Sequence.Read(se.Stream);
    var im = FindImgd(path, name);
    int tw = im.Size.Width, th = im.Size.Height, scale = 1;
    byte[] tex;
    if (hdPng != "-") { tex = Png.Read(hdPng, out int hw, out int hh); scale = hw / tw; tw = hw; th = hh; }
    else { var bgra = OpenKh.Imaging.ImageExtensions.ToBgra32(im); tex = new byte[tw * th * 4]; for (int i = 0; i < tw * th; i++) { tex[i * 4] = bgra[i * 4 + 2]; tex[i * 4 + 1] = bgra[i * 4 + 1]; tex[i * 4 + 2] = bgra[i * 4]; tex[i * 4 + 3] = bgra[i * 4 + 3]; } }
    int cw = 512 * scale, ch = 512 * scale;
    var canvas = new float[cw * ch * 4];
    for (int i = 0; i < cw * ch; i++) { canvas[i * 4] = 0.35f; canvas[i * 4 + 1] = 0.35f; canvas[i * 4 + 2] = 0.35f; canvas[i * 4 + 3] = 1; }
    float ox = 256, oy = 256;
    void Part(Sequence.SpritePart q, uint[] corners, float dx = 0, float dy = 0) {  // corners: L T R B, sprite x tint
        var sp = s.Sprites[q.SpriteIndex];
        float l = q.Left + dx, t = q.Top + dy, r = q.Right + dx, b = q.Bottom + dy;
        int x0 = (int)MathF.Floor(Math.Min(l, r)), x1 = (int)MathF.Ceiling(Math.Max(l, r)), y0 = (int)MathF.Floor(Math.Min(t, b)), y1 = (int)MathF.Ceiling(Math.Max(t, b));
        for (int yy = y0 * scale; yy < y1 * scale; yy++) for (int xx = x0 * scale; xx < x1 * scale; xx++) {
            float fx = (xx + 0.5f) / scale, fy = (yy + 0.5f) / scale;
            float u = (fx - l) / (r - l), v = (fy - t) / (b - t);   // 0..1 across the part (mirrors when r<l)
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            float su = sp.Left + u * (sp.Right - sp.Left), sv = sp.Top + v * (sp.Bottom - sp.Top);
            if (sp.Right == sp.Left) su = sp.Left; if (sp.Bottom == sp.Top) sv = sp.Top;
            int tx = Math.Clamp((int)(su * scale), 0, tw - 1), ty = Math.Clamp((int)(sv * scale), 0, th - 1);
            int ti = (ty * tw + tx) * 4;
            // corner colours (bilinear), 0x80 = 1.0, format AABBGGRR
            var cl = Seq.C(corners[0]); var ct = Seq.C(corners[1]); var cr = Seq.C(corners[2]); var cb = Seq.C(corners[3]);
            float a = tex[ti + 3] / 255f;
            int px = (int)(ox * scale) + xx, py = (int)(oy * scale) + yy;
            if (px < 0 || py < 0 || px >= cw || py >= ch) continue;
            float[] col = new float[4];
            for (int k = 0; k < 4; k++) col[k] = (cl[k] * (1 - u) + cr[k] * u) * 0.5f + (ct[k] * (1 - v) + cb[k] * v) * 0.5f;
            float alpha = a * Math.Min(col[3], 1);
            int ci = (py * cw + px) * 4;
            for (int k = 0; k < 3; k++) canvas[ci + k] = canvas[ci + k] * (1 - alpha) + Math.Min(tex[ti + k] / 255f * col[k], 1) * alpha;
        }
    }
    for (int i = 0; i < what.Count; i++) {
        if (what[i] == "sg") {
            int g = int.Parse(what[++i]); uint tint = 0x80808080; if (i + 1 < what.Count && what[i + 1].StartsWith("0x")) tint = Convert.ToUInt32(what[++i], 16);
            foreach (var q in s.SpriteGroups[g]) Part(q, Seq.Corners(s.Sprites[q.SpriteIndex], tint));
        }
        else if (what[i] == "at") { ox = float.Parse(what[++i]); oy = float.Parse(what[++i]); }
        else if (what[i] == "ag") {  // ag <animation group> [frame]: the group's look at that frame (default: its steady frame)
            int g = int.Parse(what[++i]);
            int f = Seq.SteadyFrame(s.AnimationGroups[g]);
            if (i + 1 < what.Count && int.TryParse(what[i + 1], out int ff)) { f = ff; i++; }
            foreach (var p in Seq.Compose(s, g, f)) Part(p.q, p.col, p.dx, p.dy);
        }
    }
    var outb = new byte[cw * ch * 4];
    for (int i = 0; i < cw * ch * 4; i++) outb[i] = (byte)Math.Clamp(canvas[i] * 255, 0, 255);
    Png.Write(outPath, cw, ch, outb);
    Console.WriteLine($"wrote {outPath} ({cw}x{ch}, scale {scale})");
}

// hud.bin, little-endian, magic 'SKHD' v1: header u32 magic, u32 version, u32 n_images, u32 reserved;
// dir[n] {u32 w, u32 h, u32 off, u32 reserved}; images RGBA8 rows top-down. Image 0 = the KH2 PC HD gauge sheet
// (field2d/us/es0field.2dd 'gaug', 1024x512 = 2x the PS2 512x256 sheet whose pixel coordinates the sprites use),
// image 1 = Sora's portrait (menu/shopface/p_ex100.bin HD DDS, 384x384, BGRA8 -> RGBA8, background alpha 0).
void HudBake(string outPath) {
    var sheet = Png.Read($"{Kh2}/kh2_first/remastered/field2d/us/es0field.2dd/US_es0field_2dd1.png", out int sw, out int sh);
    var dds = File.ReadAllBytes($"{Kh2}/kh2_fifth/remastered/menu/shopface/p_ex100.bin/-0.dds");
    int fw = BitConverter.ToInt32(dds, 16), fh = BitConverter.ToInt32(dds, 12);
    var face = new byte[fw * fh * 4];
    for (int i = 0; i < fw * fh; i++) { face[i * 4] = dds[128 + i * 4 + 2]; face[i * 4 + 1] = dds[128 + i * 4 + 1]; face[i * 4 + 2] = dds[128 + i * 4]; face[i * 4 + 3] = dds[128 + i * 4 + 3]; }
    using var w = new BinaryWriter(File.Create(outPath));
    w.Write(Encoding.ASCII.GetBytes("SKHD")); w.Write(1); w.Write(2); w.Write(0);
    uint off = 16 + 2 * 16;
    w.Write(sw); w.Write(sh); w.Write(off); w.Write(0); off += (uint)sheet.Length;
    w.Write(fw); w.Write(fh); w.Write(off); w.Write(0);
    w.Write(sheet); w.Write(face);
    Console.WriteLine($"wrote {outPath}: sheet {sw}x{sh}, face {fw}x{fh}");
}

// cmd.bin, little-endian, magic 'SKCM' v1 (runtime/sora_hud.c): KH2's command menu art, one world's theme.
//   header   u32 'SKCM', u32 version 1, u32 n_img, u32 n_groups
//   img[n]   u32 w, h, off, reserved                       RGBA8 rows top-down
//   label[8] u16 x, y, w, h                                rects in image 1: 0 Attack 1 Magic 2 Items 3 Drive 4 Fire
//                                                          5 Blizzard 6 Thunder 7 Cure (HD sys font, cell 48 px tall)
//   grp[n]   u16 id, u16 n_parts, u32 off                  -> n_parts x part
//   part     f32 l, t, r, b (uv, PS2 px of the 512x256 sheet; r < l / b < t mirror); u32 col L, T, R, B (0xAABBGGRR,
//            0x80 = 1.0: the sprite's corner colours x the animation's colour); f32 x0, y0, x1, y1 (local, PS2 px);
//            u32 blend (the Seqd ColorBlend: 0 normal, 1 additive)
// Image 0 = the world's HD 'comw' sheet (1024x512 = 2x the PS2 sheet), image 1 = the label atlas.
// The group ids are KH2's *animation group* ids (what its menu tables name: 6 background, 9 selected slot, 16 slot,
// 11 last slot, 18 title, 45 icon frame, 37/41/39/44 Attack/Magic/Items/Drive icons, 20 cursor, 60/57/59/58 the
// Magic page, 65 + 66..69 the shortcut page), each composed at its steady frame (Seq.Compose) into a flat quad list.
// Every world has its own 'comw' (different sprites and sprite groups, the same 99 animation groups): world = tt0
// (Twilight Town, PHH's reference), hb0, al0, ... (kh2_first / kh2_fifth field2d/us/<world>command.2dd).
void CmdBake(string outPath, string world, string atlasPng) {
    var (barPath, hdPath) = FindCmd(world);
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(barPath)));
    var se = bar.First(x => x.Type == Bar.EntryType.Seqd && x.Name == "comw"); se.Stream.Position = 0;
    var s = Sequence.Read(se.Stream);
    var sheet = LoadHd(hdPath, out int sw, out int sh);
    Console.WriteLine($"{world}: {barPath} ({s.Sprites.Count} sprites, {s.SpriteGroups.Count} sprite groups, {s.AnimationGroups.Count} anim groups), HD {hdPath} {sw}x{sh}");
    // the label atlas: each label's glyph cells side by side at the font's per-glyph advances, one row per label.
    // (s40, user wish) font 'evt' by default = KH2's outlined, rounded font (fontimage.bar 1: 21 cols of 24 x 32 PS2 px,
    // the same glyph order as 'sys'); SORA_CMD_FONT=sys = the squarish 'sys' font of s34 (28 cols of 18 x 24).
    // Both have their baseline at 7/8 of the cell, so sora_hud.c's label height (the whole cell) needs no change.
    // (s40b: the user's vanilla screenshot has the sys glyphs, bigger and outlined -> default back to sys + outline)
    bool evt = (Environment.GetEnvironmentVariable("SORA_CMD_FONT") ?? "sys") == "evt";
    int fcols = evt ? 21 : FontCols, fcw = evt ? 24 : CellW, fch = evt ? 32 : CellH;
    var widths = FontInfo(evt ? "evt" : "sys");
    var font = Png.Read($"{Kh2}/kh2_first/remastered/msg/us/fontimage.bar/US_fontimage_bar{(evt ? 1 : 0)}.png", out int fw, out int fh);
    int fs = fw / (fcols * fcw) > 0 ? fw / 512 : 1, cw = fcw * fs, chh = fch * fs;   // HD scale (2)
    Console.WriteLine($"label font {(evt ? "evt" : "sys")}: {fw}x{fh}, cell {cw}x{chh}");
    // (s68) 0x01 = space: no glyph, SORA_CMD_SPACE PS2 px (default 6 = the GUESS dream.bin uses too)
    int space = int.TryParse(Environment.GetEnvironmentVariable("SORA_CMD_SPACE"), out int spv) ? spv : 6;
    int Adv(int c) => c == 1 ? space : c >= 1000 ? 24 : widths[c - FontFirst];
    var icons = Png.Read($"{Kh2}/kh2_first/remastered/msg/us/fontimage.bar/US_fontimage_bar2.png", out int iw, out int ih);  // (s119) the icon sheet, 48 HD px cells
    int aw = 0; foreach (var codes in LabelCodes) aw = Math.Max(aw, codes.Sum(c => Adv(c)) * fs + cw);
    aw = (aw + 15) & ~15;
    int ah = LabelCodes.Length * chh;
    var atlas = new byte[aw * ah * 4];
    var labels = new (int x, int y, int w, int h)[LabelCodes.Length];
    for (int li = 0; li < LabelCodes.Length; li++) {
        int pen = 0, y0 = li * chh;
        foreach (int code in LabelCodes[li]) {
            if (code == 1) { pen += space * fs; continue; }
            if (code >= 1000) {  // (s119) a button icon: its whole 48 x 48 cell, no rim (the art has its own shadow)
                int ic = code - 1000, ix = ic % 21 * 48, iy = ic / 21 * 48;
                for (int y = 0; y < 48 && y < chh; y++) for (int x = 0; x < 48; x++) {
                    int si = ((iy + y) * iw + ix + x) * 4, di = ((y0 + y) * aw + pen + x) * 4;
                    if (pen + x >= aw) continue;
                    for (int k = 0; k < 4; k++) atlas[di + k] = icons[si + k];
                }
                pen += 48; continue;
            }
            int cell = code - FontFirst, cx = cell % fcols * cw, cy = cell / fcols * chh;
            for (int y = 0; y < chh; y++) for (int x = 0; x < cw; x++) {
                int si = ((cy + y) * fw + cx + x) * 4, di = ((y0 + y) * aw + pen + x) * 4;
                if (pen + x >= aw) continue;
                float a = font[si + 3] / 255f;   // "over" (cells overlap their neighbours' advance)
                for (int k = 0; k < 3; k++) atlas[di + k] = (byte)(font[si + k] * a + atlas[di + k] * (1 - a));
                atlas[di + 3] = (byte)Math.Min(255, font[si + 3] + atlas[di + 3] * (1 - a));
            }
            pen += widths[cell] * fs;
        }
        labels[li] = (0, y0, pen, chh);
        Console.WriteLine($"  label {li} {LabelNames[li]}: {pen}x{chh} at 0,{y0}");
    }
    // KH2's command-menu text has a dark rim (the sys sheet has none): the glyphs over their alpha dilated by
    // SORA_CMD_OUTLINE HD px (default 2 = 1 PS2 px; 0 = none) in near-black
    int ol = int.TryParse(Environment.GetEnvironmentVariable("SORA_CMD_OUTLINE"), out int olv) ? olv : (evt ? 0 : 2);
    if (ol > 0) {
        var src = (byte[])atlas.Clone();
        for (int y = 0; y < ah; y++) for (int x = 0; x < aw; x++) {
            if (LabelCodes[y / chh][0] >= 1000) continue;  // (s119: the icon rows keep their own art)
            int m = 0;
            for (int dy = -ol; dy <= ol; dy++) for (int dx = -ol; dx <= ol; dx++) {
                int sx = x + dx, sy = y + dy;
                if (dx * dx + dy * dy > ol * ol + ol || sx < 0 || sy < 0 || sx >= aw || sy >= ah) continue;
                m = Math.Max(m, src[(sy * aw + sx) * 4 + 3]);
            }
            int di = (y * aw + x) * 4; float a = src[di + 3] / 255f;
            for (int k = 0; k < 3; k++) atlas[di + k] = (byte)(src[di + k] * a + 20 * (1 - a));
            atlas[di + 3] = (byte)Math.Max(src[di + 3], m);
        }
        Console.WriteLine($"label outline {ol} HD px");
    }
    if (atlasPng != null) { Png.Write(atlasPng, aw, ah, atlas); Console.WriteLine("wrote " + atlasPng); }
    // the sprite groups
    var parts = new MemoryStream(); var pw = new BinaryWriter(parts);
    var grp = new List<(int id, int n, int off)>();
    foreach (int id in CmdGroups) {
        int f = Seq.SteadyFrame(s.AnimationGroups[id]);
        var pieces = Seq.Compose(s, id, f);
        grp.Add((id, pieces.Count, (int)parts.Position));
        Console.Write($"  group {id} @ frame {f}: {pieces.Count} parts:");
        foreach (var p in pieces) {
            var sp = s.Sprites[p.q.SpriteIndex];
            pw.Write((float)sp.Left); pw.Write((float)sp.Top); pw.Write((float)sp.Right); pw.Write((float)sp.Bottom);
            foreach (uint c in p.col) pw.Write(c);
            pw.Write(p.q.Left + p.dx); pw.Write(p.q.Top + p.dy); pw.Write(p.q.Right + p.dx); pw.Write(p.q.Bottom + p.dy);
            pw.Write((uint)p.blend);
            Console.Write($" s{p.q.SpriteIndex}@{p.q.Left + p.dx},{p.q.Top + p.dy}{(p.blend != 0 ? "+" : "")}");
        }
        Console.WriteLine();
    }
    using var w = new BinaryWriter(File.Create(outPath));
    w.Write(Encoding.ASCII.GetBytes("SKCM")); w.Write(1); w.Write(2); w.Write(grp.Count);
    uint off = (uint)(16 + 2 * 16 + 8 * 8 + grp.Count * 8 + parts.Length);
    w.Write(sw); w.Write(sh); w.Write(off); w.Write(0); off += (uint)sheet.Length;
    // (s68) more than 8 labels: the atlas image's reserved word = the file offset of the 'SKLX' trailer {u32 'SKLX',
    // u32 n, n x {u16 x, y, w, h}} = ALL labels (0..n-1; 0..7 = the fixed table again). An exe that knows it reads ids
    // 0..n-1 from there; the s67 exe (N_LABEL 8) ignores the word and the trailer, so the file loads there unchanged.
    uint ext = labels.Length > 8 ? off + (uint)atlas.Length : 0;
    w.Write(aw); w.Write(ah); w.Write(off); w.Write(ext);
    foreach (var l in labels.Take(8)) { w.Write((ushort)l.x); w.Write((ushort)l.y); w.Write((ushort)l.w); w.Write((ushort)l.h); }
    foreach (var g in grp) { w.Write((ushort)g.id); w.Write((ushort)g.n); w.Write((uint)g.off); }
    w.Write(parts.ToArray()); w.Write(sheet); w.Write(atlas);
    if (ext != 0) {
        if (w.BaseStream.Position != ext) throw new Exception($"SKLX offset {ext} != {w.BaseStream.Position}");
        w.Write(Encoding.ASCII.GetBytes("SKLX")); w.Write(labels.Length);
        foreach (var l in labels) { w.Write((ushort)l.x); w.Write((ushort)l.y); w.Write((ushort)l.w); w.Write((ushort)l.h); }
        Console.WriteLine($"SKLX trailer at {ext}: {labels.Length} labels ({string.Join(", ", LabelNames.Select((nm, i) => $"{i} {nm}"))})");
    }
    Console.WriteLine($"wrote {outPath}: sheet {sw}x{sh}, atlas {aw}x{ah}, {grp.Count} groups, {parts.Length / 52} parts");
}

// (s54 dream) dream.bin, little-endian, magic 'SKDM' v1 (recon/wip_s54_dream/sora_hud.c): the Dream Weapon picker's
// labels = KH2's own Station of Awakening lines (tools/kh2_dream_text.py -> data/dream/dream_labels.txt: "<id> <key>
// <glyph bytes hex>", the message bytes themselves) rendered like cmd.bin's labels: the HD sys font (fontimage.bar 0,
// cells of 18 x 24 PS2 px, x2), fontinfo.bar 'sys' advances, the same dark rim (SORA_CMD_OUTLINE, default 2 HD px).
//   header   u32 'SKDM', u32 version 1, u32 w, u32 h, u32 n_label
//   label[n] u16 x, y, w, h        rects in the atlas (one row of 48 HD px per label, in the labels.txt order = id)
//   atlas    RGBA8 w x h, rows top-down
// Space (byte 01) has no glyph: SORA_DREAM_SPACE PS2 px (default 6 = GUESS, KH2's own space width not found yet).
void DreamBake(string outPath, string labelsTxt) {
    var lines = File.ReadAllLines(labelsTxt).Where(l => l.Trim().Length > 0).Select(l => l.Split(' ')).ToList();
    var codes = new List<byte[]>();
    for (int i = 0; i < lines.Count; i++) {
        if (int.Parse(lines[i][0]) != i) throw new Exception($"{labelsTxt}: label {i} out of order");
        codes.Add(Convert.FromHexString(lines[i][2]));
    }
    var widths = FontInfo("sys");
    var font = Png.Read($"{Kh2}/kh2_first/remastered/msg/us/fontimage.bar/US_fontimage_bar0.png", out int fw, out int fh);
    int fs = fw / 512, cw = CellW * fs, chh = CellH * fs;
    int space = int.TryParse(Environment.GetEnvironmentVariable("SORA_DREAM_SPACE"), out int spv) ? spv : 6;
    int Adv(byte c) => c == 0x01 ? space : widths[c - FontFirst];
    int aw = 0; foreach (var c in codes) aw = Math.Max(aw, c.Sum(x => Adv(x)) * fs + cw);
    aw = (aw + 15) & ~15;
    int ah = codes.Count * chh;
    var atlas = new byte[aw * ah * 4];
    var labels = new (int x, int y, int w, int h)[codes.Count];
    for (int li = 0; li < codes.Count; li++) {
        int pen = 0, y0 = li * chh;
        foreach (byte code in codes[li]) {
            if (code < FontFirst) { if (code != 0x01) throw new Exception($"label {li}: byte {code:x2} is no glyph"); pen += space * fs; continue; }
            int cell = code - FontFirst, cx = cell % FontCols * cw, cy = cell / FontCols * chh;
            for (int y = 0; y < chh; y++) for (int x = 0; x < cw; x++) {
                int si = ((cy + y) * fw + cx + x) * 4, di = ((y0 + y) * aw + pen + x) * 4;
                if (pen + x >= aw) continue;
                float a = font[si + 3] / 255f;
                for (int k = 0; k < 3; k++) atlas[di + k] = (byte)(font[si + k] * a + atlas[di + k] * (1 - a));
                atlas[di + 3] = (byte)Math.Min(255, font[si + 3] + atlas[di + 3] * (1 - a));
            }
            pen += widths[cell] * fs;
        }
        labels[li] = (0, y0, pen + 2 * fs, chh);  // (+1 PS2 px: the rim of the last glyph)
        Console.WriteLine($"  label {li} {lines[li][1]}: {labels[li].w}x{chh} at 0,{y0}");
    }
    int ol = int.TryParse(Environment.GetEnvironmentVariable("SORA_CMD_OUTLINE"), out int olv) ? olv : 2;
    if (ol > 0) {  // (as CmdBake: the glyphs over their alpha dilated by ol HD px, in near-black)
        var src = (byte[])atlas.Clone();
        for (int y = 0; y < ah; y++) for (int x = 0; x < aw; x++) {
            int m = 0;
            for (int dy = -ol; dy <= ol; dy++) for (int dx = -ol; dx <= ol; dx++) {
                int sx = x + dx, sy = y + dy;
                if (dx * dx + dy * dy > ol * ol + ol || sx < 0 || sy < 0 || sx >= aw || sy >= ah) continue;
                m = Math.Max(m, src[(sy * aw + sx) * 4 + 3]);
            }
            int di = (y * aw + x) * 4; float a = src[di + 3] / 255f;
            for (int k = 0; k < 3; k++) atlas[di + k] = (byte)(src[di + k] * a + 20 * (1 - a));
            atlas[di + 3] = (byte)Math.Max(src[di + 3], m);
        }
    }
    using var w = new BinaryWriter(File.Create(outPath));
    w.Write(Encoding.ASCII.GetBytes("SKDM")); w.Write(1); w.Write(aw); w.Write(ah); w.Write(codes.Count);
    foreach (var l in labels) { w.Write((ushort)l.x); w.Write((ushort)l.y); w.Write((ushort)l.w); w.Write((ushort)l.h); }
    w.Write(atlas);
    Console.WriteLine($"wrote {outPath}: atlas {aw}x{ah}, {codes.Count} labels, space {space} px, outline {ol}");
}

// (s54 dream) dreamtest: the picker's three kinds of page as recon/wip_s54_dream/sora_hud.c dream_draw lays them out
// (KH2's 512 x 416 frame, 2x, no widescreen shift): the list (cursor on Shield: its examine lines), the question
// (cursor on "You bet it is!"), the result line; plus groups 7 / 17 / 19 of cmd.bin at the right (title candidates).
// Layout constants = sora_hud.c's DREAM_* / P.dream_* defaults (keep in step).
void DreamTest(string cmdPath, string dreamPath, string outPath, bool titles = false) {
    var b = File.ReadAllBytes(cmdPath); var d = File.ReadAllBytes(dreamPath);
    if (Encoding.ASCII.GetString(b, 0, 4) != "SKCM" || Encoding.ASCII.GetString(d, 0, 4) != "SKDM") throw new Exception("need cmd.bin + dream.bin");
    int nimg = BitConverter.ToInt32(b, 8), ngrp = BitConverter.ToInt32(b, 12);
    var img = new (int w, int h, int off, byte[] px)[nimg + 1];
    for (int i = 0; i < nimg; i++) img[i] = (BitConverter.ToInt32(b, 16 + i * 16), BitConverter.ToInt32(b, 20 + i * 16), BitConverter.ToInt32(b, 24 + i * 16), b);
    int dn = BitConverter.ToInt32(d, 16);
    img[nimg] = (BitConverter.ToInt32(d, 8), BitConverter.ToInt32(d, 12), 20 + dn * 8, d);
    int DIM = nimg;
    (int x, int y, int w, int h) DL(int i) => (BitConverter.ToUInt16(d, 20 + i * 8), BitConverter.ToUInt16(d, 22 + i * 8), BitConverter.ToUInt16(d, 24 + i * 8), BitConverter.ToUInt16(d, 26 + i * 8));
    int grp0 = 16 + nimg * 16 + 64;
    const int S = 2, FW = 512 * 3, FH = 416;
    var canvas = new float[FW * S * FH * S * 4];
    for (int i = 0; i < FW * S * FH * S; i++) { canvas[i * 4] = 0.42f; canvas[i * 4 + 1] = 0.5f; canvas[i * 4 + 2] = 0.38f; canvas[i * 4 + 3] = 1; }
    void Quad(int im, float u0, float v0, float u1, float v1, float x0, float y0, float x1, float y1, uint[] col) {
        int ix0 = (int)MathF.Floor(Math.Min(x0, x1) * S), ix1 = (int)MathF.Ceiling(Math.Max(x0, x1) * S), iy0 = (int)MathF.Floor(Math.Min(y0, y1) * S), iy1 = (int)MathF.Ceiling(Math.Max(y0, y1) * S);
        var cl = Seq.C(col[0]); var ct = Seq.C(col[1]); var cr = Seq.C(col[2]); var cb = Seq.C(col[3]);
        var I = img[im];
        for (int py = iy0; py < iy1; py++) for (int px = ix0; px < ix1; px++) {
            if (px < 0 || py < 0 || px >= FW * S || py >= FH * S) continue;
            float u = ((px + 0.5f) / S - x0) / (x1 - x0), v = ((py + 0.5f) / S - y0) / (y1 - y0);
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            int tx = Math.Clamp((int)(u0 + u * (u1 - u0)), 0, I.w - 1), ty = Math.Clamp((int)(v0 + v * (v1 - v0)), 0, I.h - 1);
            int ti = I.off + (ty * I.w + tx) * 4, ci = (py * FW * S + px) * 4;
            float[] c = new float[4];
            for (int k = 0; k < 4; k++) c[k] = (cl[k] * (1 - u) + cr[k] * u) * 0.5f + (ct[k] * (1 - v) + cb[k] * v) * 0.5f;
            float a = (im < 0 ? 1 : I.px[ti + 3] / 255f) * Math.Min(c[3], 1);
            for (int k = 0; k < 3; k++) canvas[ci + k] = canvas[ci + k] * (1 - a) + Math.Min(I.px[ti + k] / 255f * c[k], 1) * a;
        }
    }
    void Rect(float x0, float y0, float x1, float y1, float r, float g, float bl, float a) {
        for (int py = (int)(y0 * S); py < (int)(y1 * S); py++) for (int px = (int)(x0 * S); px < (int)(x1 * S); px++) {
            if (px < 0 || py < 0 || px >= FW * S || py >= FH * S) continue;
            int ci = (py * FW * S + px) * 4;
            canvas[ci] = canvas[ci] * (1 - a) + r * a; canvas[ci + 1] = canvas[ci + 1] * (1 - a) + g * a; canvas[ci + 2] = canvas[ci + 2] * (1 - a) + bl * a;
        }
    }
    float sc = img[0].w / 512f;
    void Group(int id, float x, float y) {
        for (int g = 0; g < ngrp; g++) {
            int gid = BitConverter.ToUInt16(b, grp0 + g * 8), n = BitConverter.ToUInt16(b, grp0 + g * 8 + 2), off = grp0 + ngrp * 8 + BitConverter.ToInt32(b, grp0 + g * 8 + 4);
            if (gid != id) continue;
            for (int i = 0; i < n; i++, off += 52) {
                float l = BitConverter.ToSingle(b, off), t = BitConverter.ToSingle(b, off + 4), r = BitConverter.ToSingle(b, off + 8), bt = BitConverter.ToSingle(b, off + 12);
                var col = new[] { BitConverter.ToUInt32(b, off + 16), BitConverter.ToUInt32(b, off + 20), BitConverter.ToUInt32(b, off + 24), BitConverter.ToUInt32(b, off + 28) };
                float x0 = BitConverter.ToSingle(b, off + 32), y0 = BitConverter.ToSingle(b, off + 36), x1 = BitConverter.ToSingle(b, off + 40), y1 = BitConverter.ToSingle(b, off + 44);
                if (r == l) r = l + 0.5f; if (bt == t) bt = t + 0.5f;
                Quad(0, l * sc, t * sc, r * sc, bt * sc, x + x0, y + y0, x + x1, y + y1, col);
            }
            return;
        }
        Console.WriteLine("  (no group " + id + ")");
    }
    const float FONT = 21, TFONT = 21, TX = 256, TY = 280, PITCH = 1.15f, PADX = 10, PADY = 5;  // (= sora_hud.c P.menu_font, P.dream_*)
    void Label(int li, float x, float y, uint rgba) {  // a row label: at the row + (10, -0.583 font), as sora_hud.c labels_flush
        var l = DL(li); float k = FONT / l.h;
        Quad(DIM, l.x, l.y, l.x + l.w, l.y + l.h, x + 10, y - FONT * 0.583f, x + 10 + l.w * k, y - FONT * 0.583f + l.h * k, new[] { rgba, rgba, rgba, rgba });
    }
    void Text(float ox, int[] ids) {  // the message lines: centred on (TX, TY), TFONT tall, PITCH apart, on a dark box
        float k = TFONT / DL(0).h, top = TY - ids.Length * TFONT * PITCH / 2, maxw = ids.Max(i => DL(i).w * k);
        Rect(ox + TX - maxw / 2 - PADX, top - PADY, ox + TX + maxw / 2 + PADX, top + ids.Length * TFONT * PITCH + PADY, 0.06f, 0.05f, 0.05f, 0.70f);
        for (int j = 0; j < ids.Length; j++) {
            var l = DL(ids[j]); float w = l.w * k, y0 = top + j * TFONT * PITCH + (TFONT * PITCH - TFONT) / 2;
            Quad(DIM, l.x, l.y, l.x + l.w, l.y + l.h, ox + TX - w / 2, y0, ox + TX + w / 2, y0 + l.h * k, new uint[] { 0x80E6E6E6, 0x80E6E6E6, 0x80E6E6E6, 0x80E6E6E6 });
        }
    }
    void Menu(float ox, int[] rows, int cur) {  // sora_hud.c dream_draw's rows: background 6, slots 9 / 16 / 11, cursor 20, no title
        float X = ox + 28; int n = rows.Length;
        Group(6, X, 322);
        for (int i = 0; i < n; i++) {
            float y = 410 - 22 * n + 22 * i;
            if (i == cur) { Group(9, X + 8, y); Group(20, X + 8, y); Label(rows[i], X + 8, y, 0x80E6E6E6); }
            else { Group(i == n - 1 ? 11 : 16, X, y); Label(rows[i], X, y, 0x80B4B4B4); }
        }
    }
    // page 1: the list, cursor on Shield (row 1) -> its examine lines (labels 5, 6)
    Menu(0, new[] { 0, 1, 2 }, 1); Text(0, new[] { 5, 6 });
    // page 2: the question for the Shield, cursor on "You bet it is!"
    Menu(512, new[] { 12, 13 }, 1); Text(512, new[] { 10 });
    // page 3: the result line, no rows (as sora_hud.c dream_draw: n_rows 0 = the message alone, no slots)
    // (s55: the s54 preview also drew cmd.bin's title candidates 7 / 17 / 19 here, at x 1324 - over this page's box:
    // those were the 3 stray "COMMANDS" headers, a preview-only artefact; `dreamtest ... titles` still shows them, apart)
    Text(1024, new[] { 15 });
    if (titles) { Group(7, 1024 + 400, 60); Group(17, 1024 + 400, 110); Group(19, 1024 + 400, 160); }
    var outb = new byte[canvas.Length];
    for (int i = 0; i < canvas.Length; i++) outb[i] = (byte)Math.Clamp(canvas[i] * 255, 0, 255);
    Png.Write(outPath, FW * S, FH * S, outb);
    Console.WriteLine($"wrote {outPath} ({FW * S}x{FH * S})");
}

// The lock-on icons (s41 / s42): field2d/us/tt0field.2dd 'fiel'. Icon 0 = the AUTO target (anim group 5: yellow 3-arc
// mark, show f0-59 loop from 11; group 4: hide, 10 frames). Icon 1 = the R1 LOCK (group 27: blue-white 3 arcs + arrow
// tips, show f0-69 loop from 12, inner arcs sg 41-43 rotate 0 -> 120 deg over f10-69; group 26: hide, 10 frames).
// Every frame pre-composed with the full transform (translation + pivot, bounce, scale / sx / sy, rotation Z about the
// animation's origin): a part's 4 corners = R(rot) * (q * scale) + (dx, dy), PS2 px around the target. Additive parts
// (colblend 1: sg 48's glow) are kept, blend = 1.
// lock.bin 'SKLK' v2: {magic, version 2, w, h, ps2_w, n_frames} + 2 x {show_first, loop_start, release_first, end}
// (absolute frame indices: show = show_first..release_first-1 looping from loop_start, hide = release_first..end-1);
// then n_frames x {u32 off, u32 n} (off in parts, into the parts block), the parts (sora_hud.c LPart, 68 bytes:
// {l, t, r, b, col[4] L T R B, x[4], y[4] corners TL TR BR BL, blend}), the HD sheet RGBA8.
// Bounce (GUESS, OpenKh-like): d += amplitude * sin(2 pi count t) over the animation's span.
void LockBake(string outPath) {
    string barPath = $"{Kh2}/kh2_first/original/field2d/us/tt0field.2dd", hdPath = $"{Kh2}/kh2_first/remastered/field2d/us/tt0field.2dd/US_tt0field_2dd0.png";
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(barPath)));
    var se = bar.First(x => x.Type == Bar.EntryType.Seqd && x.Name == "fiel"); se.Stream.Position = 0;
    var s = Sequence.Read(se.Stream);
    var sheet = LoadHd(hdPath, out int sw, out int sh);
    var parts = new MemoryStream(); var pw = new BinaryWriter(parts);
    var frames = new List<(int off, int n)>();
    int nadd = 0;
    void Frame(int g, int f) {
        int n = 0, off = (int)(parts.Position / 68);
        foreach (var a in s.AnimationGroups[g].Animations) {
            int f0 = (int)a.FrameStart, f1 = (int)a.FrameEnd;
            if (f < f0 || f > f1) continue;
            int fl = a.Flags;
            float d = f1 > f0 ? (float)(f - f0) / (f1 - f0) : 0;
            float t = (fl & 0x1) != 0 ? d : (MathF.Sin(d * MathF.PI - MathF.PI / 2) + 1) / 2;
            float L(float x0, float x1) => x0 + (x1 - x0) * t;
            float dx = (fl & 0x1000) != 0 ? a.TranslateXStart : L(a.TranslateXStart, a.TranslateXEnd);
            float dy = (fl & 0x1000) != 0 ? a.TranslateYStart : L(a.TranslateYStart, a.TranslateYEnd);
            if ((fl & 0x4000) == 0) { dx += L(a.PivotXStart, a.PivotXEnd); dy += L(a.PivotYStart, a.PivotYEnd); }
            if ((fl & 0x10) == 0) {
                dx += a.BounceXStart * MathF.Sin(2 * MathF.PI * a.BounceXCount * d);
                dy += a.BounceYStart * MathF.Sin(2 * MathF.PI * a.BounceYCount * d);
            }
            float sc = (fl & 0x40) != 0 ? 1 : L(a.ScaleStart, a.ScaleEnd);
            float sx = sc * ((fl & 0x40) != 0 ? 1 : L(a.ScaleXStart, a.ScaleXEnd)), sy = sc * ((fl & 0x40) != 0 ? 1 : L(a.ScaleYStart, a.ScaleYEnd));
            float rot = (fl & 0x20) != 0 ? 0 : L(a.RotationZStart, a.RotationZEnd);
            float cs = MathF.Cos(rot), sn = MathF.Sin(rot);
            uint ac = (fl & 0x400) != 0 ? 0x80808080u : (fl & 0x80) != 0 ? (uint)a.ColorStart : Seq.Lerp((uint)a.ColorStart, (uint)a.ColorEnd, t);
            foreach (var q in s.SpriteGroups[a.SpriteGroupIndex]) {
                var sp = s.Sprites[q.SpriteIndex];
                if (sp.Right == sp.Left) continue;  // (sprite 4: the empty one)
                pw.Write((float)sp.Left); pw.Write((float)sp.Top); pw.Write((float)sp.Right); pw.Write((float)sp.Bottom);
                foreach (uint c in Seq.Corners(sp, ac)) pw.Write(c);
                var cx = new[] { q.Left * sx, q.Right * sx, q.Right * sx, q.Left * sx };
                var cy = new[] { q.Top * sy, q.Top * sy, q.Bottom * sy, q.Bottom * sy };
                for (int k = 0; k < 4; k++) pw.Write(dx + cx[k] * cs - cy[k] * sn);
                for (int k = 0; k < 4; k++) pw.Write(dy + cx[k] * sn + cy[k] * cs);
                pw.Write(a.ColorBlend != 0 ? 1u : 0u);
                if (a.ColorBlend != 0) nadd++;
                n++;
            }
        }
        frames.Add((off, n));
    }
    var icons = new List<int[]>();
    foreach (var (show, hide) in new[] { (5, 4), (27, 26) }) {
        var gs = s.AnimationGroups[show]; var gh = s.AnimationGroups[hide];
        int sEnd = gs.Animations.Max(a => (int)a.FrameEnd), hEnd = gh.Animations.Max(a => (int)a.FrameEnd);
        int first = frames.Count;
        for (int f = 0; f <= sEnd; f++) Frame(show, f);
        int rel = frames.Count;
        for (int f = 0; f <= hEnd; f++) Frame(hide, f);
        icons.Add(new[] { first, first + (int)gs.LoopStart, rel, frames.Count });
    }
    using var w = new BinaryWriter(File.Create(outPath));
    w.Write(Encoding.ASCII.GetBytes("SKLK")); w.Write(2); w.Write(sw); w.Write(sh); w.Write(256); w.Write(frames.Count);
    foreach (var ic in icons) foreach (int v in ic) w.Write(v);
    foreach (var fr in frames) { w.Write((uint)fr.off); w.Write((uint)fr.n); }
    w.Write(parts.ToArray()); w.Write(sheet);
    Console.WriteLine($"wrote {outPath}: sheet {sw}x{sh}, {frames.Count} frames, {parts.Length / 68} parts ({nadd} additive); " +
        string.Join(", ", icons.Select((ic, i) => $"icon {i} show {ic[0]} loop {ic[1]} hide {ic[2]}..{ic[3] - 1}")));
}

// locktest: frames of a lock.bin side by side, 4x, each as sora_hud.c draws it (premultiplied blend: alpha parts over,
// additive parts added), the target at each cell's centre; dark grey background.
void LockTest(string binPath, string outPath, int icon, List<int> fs) {
    var b = File.ReadAllBytes(binPath);
    if (Encoding.ASCII.GetString(b, 0, 4) != "SKLK" || BitConverter.ToInt32(b, 4) != 2) throw new Exception("not a v2 lock.bin");
    int sw = BitConverter.ToInt32(b, 8), sh = BitConverter.ToInt32(b, 12), ps2 = BitConverter.ToInt32(b, 16), nf = BitConverter.ToInt32(b, 20);
    int ic0 = 24 + icon * 16, show = BitConverter.ToInt32(b, ic0);
    int fr0 = 24 + 32, parts0 = fr0 + nf * 8, sheet0 = b.Length - sw * sh * 4;
    float tsc = sw / (float)ps2;
    const int S = 4, CW = 96;
    int W = CW * S * fs.Count, H = CW * S;
    var canvas = new float[W * H * 4];
    for (int i = 0; i < W * H; i++) { canvas[i * 4] = 0.25f; canvas[i * 4 + 1] = 0.25f; canvas[i * 4 + 2] = 0.25f; canvas[i * 4 + 3] = 1; }
    for (int c = 0; c < fs.Count; c++) {
        int fi = show + fs[c];
        uint off = BitConverter.ToUInt32(b, fr0 + fi * 8), n = BitConverter.ToUInt32(b, fr0 + fi * 8 + 4);
        float ox = (c + 0.5f) * CW * S, oy = 0.5f * CW * S;
        for (uint i = 0; i < n; i++) {
            int p = parts0 + (int)(off + i) * 68;
            float l = BitConverter.ToSingle(b, p), t = BitConverter.ToSingle(b, p + 4), r = BitConverter.ToSingle(b, p + 8), bt = BitConverter.ToSingle(b, p + 12);
            var cl = Seq.C(BitConverter.ToUInt32(b, p + 16)); var ct = Seq.C(BitConverter.ToUInt32(b, p + 20)); var cr = Seq.C(BitConverter.ToUInt32(b, p + 24)); var cb = Seq.C(BitConverter.ToUInt32(b, p + 28));
            var X = new float[4]; var Y = new float[4];
            for (int k = 0; k < 4; k++) { X[k] = ox + BitConverter.ToSingle(b, p + 32 + k * 4) * S; Y[k] = oy + BitConverter.ToSingle(b, p + 48 + k * 4) * S; }
            bool add = BitConverter.ToUInt32(b, p + 64) != 0;
            // affine: P = C0 + u (C1 - C0) + v (C3 - C0)
            float ax = X[1] - X[0], ay = Y[1] - Y[0], bx = X[3] - X[0], by = Y[3] - Y[0], det = ax * by - ay * bx;
            if (Math.Abs(det) < 1e-6f) continue;
            int x0 = (int)MathF.Floor(X.Min()), x1 = (int)MathF.Ceiling(X.Max()), y0 = (int)MathF.Floor(Y.Min()), y1 = (int)MathF.Ceiling(Y.Max());
            for (int py = Math.Max(y0, 0); py < Math.Min(y1, H); py++) for (int px = Math.Max(x0, 0); px < Math.Min(x1, W); px++) {
                float qx = px + 0.5f - X[0], qy = py + 0.5f - Y[0];
                float u = (qx * by - qy * bx) / det, v = (ax * qy - ay * qx) / det;
                if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
                float su = r == l ? l : l + u * (r - l), sv = bt == t ? t : t + v * (bt - t);
                int tx = Math.Clamp((int)(su * tsc), 0, sw - 1), ty = Math.Clamp((int)(sv * tsc), 0, sh - 1);
                int ti = sheet0 + (ty * sw + tx) * 4, ci = (py * W + px) * 4;
                float[] col = new float[4];
                for (int k = 0; k < 4; k++) col[k] = (cl[k] * (1 - u) + cr[k] * u) * 0.5f + (ct[k] * (1 - v) + cb[k] * v) * 0.5f;
                float a = b[ti + 3] / 255f * Math.Min(col[3], 1);
                for (int k = 0; k < 3; k++) {
                    float src = Math.Min(b[ti + k] / 255f * col[k], 1) * a;
                    canvas[ci + k] = add ? Math.Min(canvas[ci + k] + src, 1) : canvas[ci + k] * (1 - a) + src;
                }
            }
        }
    }
    var o = new byte[W * H * 4];
    for (int i = 0; i < o.Length; i++) o[i] = (byte)Math.Clamp((int)(canvas[i] * 255 + 0.5f), 0, 255);
    Png.Write(outPath, W, H, o);
    Console.WriteLine($"wrote {outPath}: icon {icon} frames {string.Join(",", fs)} ({W}x{H})");
}

(string bar, string hd) FindCmd(string world) {
    foreach (var pkg in new[] { "kh2_first", "kh2_fifth" }) {
        string bar = $"{Kh2}/{pkg}/original/field2d/us/{world}command.2dd";
        if (!File.Exists(bar)) continue;
        string dir = $"{Kh2}/{pkg}/remastered/field2d/us/{world}command.2dd";
        string png = $"{dir}/US_{world}command_2dd0.png", dds = $"{dir}/US_{world}command_2dd0.dds";
        return (bar, File.Exists(png) ? png : File.Exists(dds) ? dds : throw new Exception("no HD art in " + dir));
    }
    throw new Exception($"no {world}command.2dd under {Kh2}");
}

// an HD sheet: PNG, or the uncompressed BGRA8 DDS the KH2 PC pkgs use (128-byte header, height at 12, width at 16)
static byte[] LoadHd(string path, out int w, out int h) {
    if (path.EndsWith(".png")) return Png.Read(path, out w, out h);
    var dds = File.ReadAllBytes(path);
    w = BitConverter.ToInt32(dds, 16); h = BitConverter.ToInt32(dds, 12);
    if (dds.Length < 128 + w * h * 4) throw new Exception("dds: not uncompressed BGRA8: " + path);
    var o = new byte[w * h * 4];
    for (int i = 0; i < w * h; i++) { o[i * 4] = dds[128 + i * 4 + 2]; o[i * 4 + 1] = dds[128 + i * 4 + 1]; o[i * 4 + 2] = dds[128 + i * 4]; o[i * 4 + 3] = dds[128 + i * 4 + 3]; }
    return o;
}

// per-glyph advances (PS2 px, one byte per cell from code 32) of a font: msg/us/fontinfo.bar entry 'sys' (280) / 'evt' (336)
int[] FontInfo(string name) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_first/original/msg/us/fontinfo.bar")));
    var e = bar.First(x => x.Name == name); e.Stream.Position = 0;
    var ms = new MemoryStream(); e.Stream.CopyTo(ms);
    Console.WriteLine($"font widths: fontinfo.bar '{name}' ({ms.Length})");
    return ms.ToArray().Select(b => (int)b).ToArray();
}


// cmdtest: the three pages as runtime/sora_hud.c lays them out (KH2's 512 x 416 frame, 2x), from the bake itself:
// the top page (cursor on Attack) with the Magic page open (cursor on Blizzard), and the shortcut page 230 px right.
void CmdTest(string binPath, string outPath) {
    var b = File.ReadAllBytes(binPath);
    if (Encoding.ASCII.GetString(b, 0, 4) != "SKCM") throw new Exception("not a cmd.bin");
    int nimg = BitConverter.ToInt32(b, 8), ngrp = BitConverter.ToInt32(b, 12);
    var img = new (int w, int h, int off)[nimg];
    for (int i = 0; i < nimg; i++) img[i] = (BitConverter.ToInt32(b, 16 + i * 16), BitConverter.ToInt32(b, 20 + i * 16), BitConverter.ToInt32(b, 24 + i * 16));
    int lab0 = 16 + nimg * 16, grp0 = lab0 + 64;
    // (s68) labels 8.. from the 'SKLX' trailer (atlas image reserved word = its offset)
    int ext = BitConverter.ToInt32(b, 16 + 16 + 12), next = ext > 0 && Encoding.ASCII.GetString(b, ext, 4) == "SKLX" ? BitConverter.ToInt32(b, ext + 4) : 8;
    (int x, int y, int w, int h) Label(int i) { int o = i < 8 ? lab0 + i * 8 : ext + 8 + i * 8; if (i >= next) throw new Exception($"label {i}: the bake has {next}");
        return (BitConverter.ToUInt16(b, o), BitConverter.ToUInt16(b, o + 2), BitConverter.ToUInt16(b, o + 4), BitConverter.ToUInt16(b, o + 6)); }
    Console.WriteLine($"{binPath}: {next} labels{(ext > 0 ? $" (SKLX at {ext})" : "")}");
    bool drive = Environment.GetEnvironmentVariable("CMDTEST_PAGE") == "drive";
    bool limitp = Environment.GetEnvironmentVariable("CMDTEST_PAGE") == "limit";   // (s89 limit) labels 23 Limit + 24..27   // the Drive page (labels 8 Valor Form, 9 Revert, ...)
    const int S = 2, FW = 512, FH = 416;
    var canvas = new float[FW * S * FH * S * 4];
    for (int i = 0; i < FW * S * FH * S; i++) { canvas[i * 4] = 0.35f; canvas[i * 4 + 1] = 0.35f; canvas[i * 4 + 2] = 0.35f; canvas[i * 4 + 3] = 1; }
    // a textured quad (frame px) sampling image im at (u0,v0)-(u1,v1) (image px), corner colours L T R B (0x80 = 1.0)
    void Quad(int im, float u0, float v0, float u1, float v1, float x0, float y0, float x1, float y1, uint[] col) {
        int ix0 = (int)MathF.Floor(Math.Min(x0, x1) * S), ix1 = (int)MathF.Ceiling(Math.Max(x0, x1) * S), iy0 = (int)MathF.Floor(Math.Min(y0, y1) * S), iy1 = (int)MathF.Ceiling(Math.Max(y0, y1) * S);
        var cl = Seq.C(col[0]); var ct = Seq.C(col[1]); var cr = Seq.C(col[2]); var cb = Seq.C(col[3]);
        for (int py = iy0; py < iy1; py++) for (int px = ix0; px < ix1; px++) {
            if (px < 0 || py < 0 || px >= FW * S || py >= FH * S) continue;
            float u = ((px + 0.5f) / S - x0) / (x1 - x0), v = ((py + 0.5f) / S - y0) / (y1 - y0);
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            int tx = Math.Clamp((int)(u0 + u * (u1 - u0)), 0, img[im].w - 1), ty = Math.Clamp((int)(v0 + v * (v1 - v0)), 0, img[im].h - 1);
            int ti = img[im].off + (ty * img[im].w + tx) * 4, ci = (py * FW * S + px) * 4;
            float[] c = new float[4];
            for (int k = 0; k < 4; k++) c[k] = (cl[k] * (1 - u) + cr[k] * u) * 0.5f + (ct[k] * (1 - v) + cb[k] * v) * 0.5f;
            float a = b[ti + 3] / 255f * Math.Min(c[3], 1);
            for (int k = 0; k < 3; k++) canvas[ci + k] = canvas[ci + k] * (1 - a) + Math.Min(b[ti + k] / 255f * c[k], 1) * a;
        }
    }
    float sc = img[0].w / 512f;
    void Group(int id, float x, float y) {
        for (int g = 0; g < ngrp; g++) {
            int gid = BitConverter.ToUInt16(b, grp0 + g * 8), n = BitConverter.ToUInt16(b, grp0 + g * 8 + 2), off = grp0 + ngrp * 8 + BitConverter.ToInt32(b, grp0 + g * 8 + 4);
            if (gid != id) continue;
            for (int i = 0; i < n; i++, off += 52) {
                float l = BitConverter.ToSingle(b, off), t = BitConverter.ToSingle(b, off + 4), r = BitConverter.ToSingle(b, off + 8), bt = BitConverter.ToSingle(b, off + 12);
                var col = new[] { BitConverter.ToUInt32(b, off + 16), BitConverter.ToUInt32(b, off + 20), BitConverter.ToUInt32(b, off + 24), BitConverter.ToUInt32(b, off + 28) };
                float x0 = BitConverter.ToSingle(b, off + 32), y0 = BitConverter.ToSingle(b, off + 36), x1 = BitConverter.ToSingle(b, off + 40), y1 = BitConverter.ToSingle(b, off + 44);
                if (r == l) r = l + 0.5f; if (bt == t) bt = t + 0.5f;
                Quad(0, l * sc, t * sc, r * sc, bt * sc, x + x0, y + y0, x + x1, y + y1, col);
            }
            return;
        }
        Console.WriteLine("  (no group " + id + ")");
    }
    const float FONT = 21;   // the drawn cell height (PS2 px): s40b, matched to the user's vanilla screenshot (was 14)
    void Text(int li, float x, float y, uint rgba) {
        var l = Label(li); float k = FONT / l.h;   // atlas px -> PS2 px (the cell is FONT tall, as in sora_hud.c)
        Quad(1, l.x, l.y, l.x + l.w, l.y + l.h, x + 10, y - FONT * 0.583f, x + 10 + l.w * k, y - FONT * 0.583f + l.h * k, new[] { rgba, rgba, rgba, rgba });
    }
    const float PAD_GX = -8, PAD_TX = 14;   // (s119) = sora_hud.c PAD_GX / PAD_TX
    uint[] COL = { 0x80B4B4B4, 0, 0x800079B7, 0, 0, 0x80646464 }, SEL = { 0x80E6E6E6, 0, 0x8000A7FF, 0, 0, 0x80969696 };
    int[] ICON = { 37, 41, 39, 44 };
    if (Environment.GetEnvironmentVariable("CMDTEST_PAGE") == "mickey") {   // (s119 mkmenu) KH2's Mickey pane, as sora_hud.c menu_draw's pad pane lays it out
        float MX = 28; int mn = 3; int[] ml = { 28, 29, 0 }, mg = Environment.GetEnvironmentVariable("CMDTEST_PAD") == "xbox" ? new[] { 35, 34, 33 } : new[] { 32, 31, 30 }, ms = { 0, Environment.GetEnvironmentVariable("CMDTEST_POOR") != null ? 2 : 0, 0 };
        // rows: Jump (Cross / A), Pearl (Square / X), Attack (Circle / B) = the buttons by position (sora.c header; review)
        Group(6, MX, 322); Group(18, MX, 388 - 22 * mn);
        for (int i = 0; i < mn; i++) {
            float y = 410 - 22 * mn + 22 * i;
            Group(i == mn - 1 ? 11 : 16, MX, y);
            Text(mg[i], MX + PAD_GX, y, 0x80808080); Text(ml[i], MX + PAD_TX, y, COL[ms[i]]);
        }
        var ob = new byte[canvas.Length];
        for (int i = 0; i < canvas.Length; i++) ob[i] = (byte)Math.Clamp(canvas[i] * 255, 0, 255);
        Png.Write(outPath, FW * S, FH * S, ob);
        Console.WriteLine($"wrote {outPath} ({FW * S}x{FH * S})");
        return;
    }
    bool rcp = Environment.GetEnvironmentVariable("CMDTEST_PAGE") == "rc";   // (s134 rcslot) the Reaction Command slot above the top page (group 2 / 4, Triangle icon 42 / Y 43)
    // top page: 4 rows, cursor 0, states normal / normal / greyed / greyed
    float X = 28; int n = 4, cur = drive ? 3 : 0; int[] st = drive ? new[] { 0, 0, 0, 0 } : new[] { 0, 0, 5, 5 };
    Group(6, X, 322);
    Group(18, X, 388 - 22 * n);
    for (int i = 0; i < n; i++) {
        float y = 410 - 22 * n + 22 * i;
        if (i == cur) { Group(9, X + 8, y); Group(45, X + 8, y); Group(ICON[i], X + 8, y); Group(20, X + 8, y); Text(i, X + 8, y, SEL[st[i]]); }
        else { Group(i == n - 1 ? 11 : 16, X, y); Text(limitp && i == 1 ? 23 : i, X, y, COL[st[i]]); }
    }
    if (rcp) {   // = sora_hud.c rc_draw: slot at (36, 377 - 22 n), icon label x 15 (+10 = 25, inside the slot's circle), name text at slot + (12, -8)
        float RY = 377 - 22 * n;
        Group(int.TryParse(Environment.GetEnvironmentVariable("CMDTEST_RCG"), out int rg) ? rg : 2, 36, RY);
        Text(Environment.GetEnvironmentVariable("CMDTEST_PAD") == "xbox" ? 43 : 42, 15, RY, 0x80808080);
        Text(39, 36 + 2, RY, 0x80E6E6E6);
        var ob2 = new byte[canvas.Length];
        for (int i = 0; i < canvas.Length; i++) ob2[i] = (byte)Math.Clamp(canvas[i] * 255, 0, 255);
        Png.Write(outPath, FW * S, FH * S, ob2);
        Console.WriteLine($"wrote {outPath} ({FW * S}x{FH * S})");
        return;
    }
    // the Magic page: Fire, Blizzard (selected), Thunder (too costly: state 2), Cure
    float XS = 44; int ns = 4, scur = 1; int[] sl = { 4, 5, 6, 7 }, ss = { 0, 0, 2, 0 };
    if (drive) { ns = Math.Min(next - 8, 7); scur = 0; sl = Enumerable.Range(8, ns).ToArray(); ss = sl.Select(l => l == 9 ? 5 : l > 9 ? 5 : 0).ToArray(); }
    if (limitp) { ns = 4; scur = 0; sl = new[] { 24, 26, 25, 27 }; ss = new[] { 0, 0, 2, 2 }; }   // (s89 limit: the DLL's row order)
    Group(60, XS, 388 - 22 * ns);
    for (int i = 0; i < ns; i++) {
        float y = 410 - 22 * ns + 22 * i; bool sel = i == scur;
        Group(sel ? 57 : i == ns - 1 ? 58 : 59, XS + (sel ? 8 : 0), y);
        Text(sl[i], XS + (sel ? 8 : 0), y, (sel ? SEL : COL)[ss[i]]);
    }
    // the shortcut page, drawn 230 px to the right: title 65, rows 67 69 68 66 (Fire / Blizzard / Thunder / Cure)
    float XC = 28 + 230; int[] SCG = { 67, 69, 68, 66 }, scl = { 4, 5, 6, 7 };
    Group(6, XC, 322);
    Group(65, XC, 388 - 22 * 4);
    for (int i = 0; i < 4; i++) { float y = 410 - 22 * 4 + 22 * i; Group(SCG[i], XC, y); Text(scl[i], XC, y, COL[0]); }
    var outb = new byte[canvas.Length];
    for (int i = 0; i < canvas.Length; i++) outb[i] = (byte)Math.Clamp(canvas[i] * 255, 0, 255);
    Png.Write(outPath, FW * S, FH * S, outb);
    Console.WriteLine($"wrote {outPath} ({FW * S}x{FH * S})");
}

static Imgd FindImgd(string path, string name) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(path)));
    var e = bar.First(x => x.Type == Bar.EntryType.Imgd && x.Name == name);
    e.Stream.Position = 0;
    return Imgd.Read(e.Stream);
}

void ImgdPng(string path, string name, string outPath) {
    var im = FindImgd(path, name);
    var bgra = OpenKh.Imaging.ImageExtensions.ToBgra32(im);
    int w = im.Size.Width, h = im.Size.Height;
    Console.WriteLine($"Imgd '{name}': {w}x{h} {im.PixelFormat} swizzled {im.IsSwizzled} data {im.Data.Length} clut {(im.Clut == null ? 0 : im.Clut.Length)}");
    var rgba = new byte[w * h * 4];
    for (int i = 0; i < w * h; i++) { rgba[i * 4] = bgra[i * 4 + 2]; rgba[i * 4 + 1] = bgra[i * 4 + 1]; rgba[i * 4 + 2] = bgra[i * 4]; rgba[i * 4 + 3] = bgra[i * 4 + 3]; }
    Png.Write(outPath, w, h, rgba);
    Console.WriteLine("wrote " + outPath);
}

void DumpBar(string path) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes(path)));
    int i = 0;
    foreach (var e in bar) Console.WriteLine($"  [{i++}] {e.Type} '{e.Name}' index {e.Index} len {e.Stream.Length}");
}

// ===================================================================== (s55 HUD helper) KH2's level-up panel
// dk::LEVEL_UP (PHH hud_kh2.py LevelUp / LevelUpInfo, every rule tagged re:0x... there; PHH = architecture reference, no
// code copied). The panel = es0field.2dd 'gaug' animation group 0x68 (104, one line) / 0x69 (105, two lines) for Sora
// (re:0x71AD38, character type 0), played once (DoNotLoop, frames 0-200 at 60 Hz) at the HUD's widescreen x offset +0x55;
// its text in KH2's system font at size 14: the name (font 1 = drop shadow) right-aligned at the anchor + 0x55, "%2d"
// level at the anchor + TextPositionX (+0x55), line 1 at the anchor + (LightPositionX + 0x55, TextPositionY), line 2 at
// + (LightPositionX + 0x55, 0x11 + TextPositionY) (font 0 = no shadow), all 0x80E5E5E5 with the anchor animation's alpha.
// lvup.bin, little-endian, magic 'SKLV' v1 (recon/wip_s55_hud/sora_hud.c lvup_load):
//   header   u32 'SKLV', version 1, n_grp, n_frames, atlas_w, atlas_h, n_label, atlas px per frame px
//   grp[n]   u16 id, u16 ui_padding, i16 light_x, i16 text_x, i16 text_y, u16 n_frames, u32 first_frame      (16 B)
//   frame[n] u32 part_off, u32 n_parts (LPart units), i32 anchor_x, anchor_y, u32 anchor colour 0xAABBGGRR     (20 B)
//   label[n] u16 x, y, w, h (atlas px); f32 ox, oy (frame px: the rect's top-left from the text's top-left); f32 adv
//            (KH2 text_width, frame px: right alignment); u32 key (0 name, 1..99 the level "%2d", 0x101..0x104
//            Strength / Magic / Defense / Maximum AP Increased!, 0x10000 + item id = "<icon><ability>!", (s76) 0x200 +
//            Drive Form = the form's name "Valor Form" / "Wisdom Form" in the name slot)                          (24 B)
//   parts    LPart (sora_hud.c, 68 B): uv l t r b (PS2 px of the gaug sheet = hud.bin image 0), 4 corner colours,
//            corners x[4] y[4] TL TR BR BL (frame px of KH2's 512 x 416 frame, translation included, x offset not),
//            blend (ColorBlend: 0 normal, 1 additive)
//   atlas    RGBA8 (the text colour 0x80E5E5E5 baked in: draw the labels with (1, 1, 1, alpha))
// Animation rules = PHH kh2_2d (re:0x192250 / re:0x193880): eased t = the Bezier with CurveYStart / CurveYEnd (flag 0x1
// = linear), integer lerps truncated, bounce += trunc(sin(count pi t) x lerp(BounceStart, BounceEnd)) (0x8 cos, 0x10 off),
// colour = ColorStart (0x80) or the per-byte integer lerp, 0x8000 = FrameEnd not shown, anchor = the first active 0x2 anim.
// Text = PHH kh2_2d._text (re:0x3AED80 / 0x3B0280 / 0x3AFF50 / 0x4070F0 / 0x406ED0), English build, size 14: glyph =
// code - 0x20 (0x66 alone = 0x34), cell 18 x 24 x scale, its bottom at pen y + 2 px, advance (int)(w x scale + 0.5);
// space (int)(18 / 3 x scale + 0.5); code 9 = an icon (24 x 24 cells, 21 per row) as a 17 x 18 px quad; font 1 = drop
// shadow (+2, +1 px, black, half alpha).

void LvupBake(string outPath, string labelsTxt) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_first/original/field2d/us/es0field.2dd")));
    var se = bar.First(x => x.Type == Bar.EntryType.Seqd && x.Name == "gaug"); se.Stream.Position = 0;
    var s = Sequence.Read(se.Stream);
    var parts = new MemoryStream(); var pw = new BinaryWriter(parts);
    var frames = new List<(int off, int n, int ax, int ay, uint ac)>();
    var grps = new List<(int id, int pad, int lx, int tx, int ty, int n, int first)>();
    foreach (int g in new[] { 104, 105 }) {
        var ag = s.AnimationGroups[g];
        int fend = ag.Animations.Max(a => (int)a.FrameEnd), first = frames.Count;
        int ax = 0, ay = 0; uint ac = 0;
        for (int f = 0; f <= fend; f++) {
            int off = (int)(parts.Position / 68), n = 0;
            bool anc = false;
            foreach (var a in ag.Animations) {
                if (!Kh2A.Active(a, f)) continue;
                float t = Kh2A.T(a, f);
                var (x, y) = Kh2A.Pos(a, t);
                uint c = Kh2A.Col(a, t);
                if (!anc && (a.Flags & 2) != 0) { anc = true; ax = x; ay = y; ac = c; }
                if ((a.Flags & 0x400) != 0) c = 0x80808080u;
                foreach (var q in s.SpriteGroups[a.SpriteGroupIndex]) {
                    var sp = s.Sprites[q.SpriteIndex];
                    pw.Write((float)sp.Left); pw.Write((float)sp.Top); pw.Write((float)sp.Right); pw.Write((float)sp.Bottom);
                    foreach (uint cc in Seq.Corners(sp, c)) pw.Write(cc);
                    pw.Write((float)(x + q.Left)); pw.Write((float)(x + q.Right)); pw.Write((float)(x + q.Right)); pw.Write((float)(x + q.Left));
                    pw.Write((float)(y + q.Top)); pw.Write((float)(y + q.Top)); pw.Write((float)(y + q.Bottom)); pw.Write((float)(y + q.Bottom));
                    pw.Write(a.ColorBlend != 0 ? 1u : 0u);
                    n++;
                }
            }
            frames.Add((off, n, ax, ay, ac));
        }
        grps.Add((g, ag.UiPadding, ag.LightPositionX, ag.TextPositionX, ag.TextPositionY, frames.Count - first, first));
        Console.WriteLine($"  group {g}: {frames.Count - first} frames, pad {ag.UiPadding}, light {ag.LightPositionX}, text {ag.TextPositionX},{ag.TextPositionY}; anchor f0 {frames[first].ax},{frames[first].ay} {frames[first].ac:X8}, f5 {frames[first + 5].ax},{frames[first + 5].ay} {frames[first + 5].ac:X8}, f60 {frames[first + 60].ax},{frames[first + 60].ay} {frames[first + 60].ac:X8}, f195 {frames[first + 195].ax},{frames[first + 195].ay} {frames[first + 195].ac:X8}");
    }

    // ---- the text
    const int AS = 3;               // atlas px per frame px
    const float SIZE = 14, BASE = 18, SUB = 16;
    const uint TEXT_COL = 0x80E5E5E5;  // KH: re:0x16AA50
    var widths = FontInfo("sys");
    var font = Png.Read($"{Kh2}/kh2_first/remastered/msg/us/fontimage.bar/US_fontimage_bar0.png", out int fw, out int fh);
    var icon = Png.Read($"{Kh2}/kh2_first/remastered/msg/us/fontimage.bar/US_fontimage_bar2.png", out int iw, out int ih);
    float fsc = fw / 512f, isc = iw / 512f;
    // icon-font advances (PS2 px) of the first 256 icons: mined from the player's own KH2 exe by the install (sounds_mine -> recipe
    // "hud".icon_widths -> sounds_build writes kh2_hud.json {"icon_widths": {"widths": [...]}}); no table is shipped in this source
    string hudJson = Environment.GetEnvironmentVariable("SORABUILD_HUDJSON") ?? throw new Exception("set SORABUILD_HUDJSON = work/sounds/kh2_hud.json (sorabuild sounds_build)");
    if (!File.Exists(hudJson)) throw new Exception("icon widths: " + hudJson + " is missing (run the sorabuild sounds_build step)");
    int[] iconW;
    try { using var hj = System.Text.Json.JsonDocument.Parse(File.ReadAllBytes(hudJson)); iconW = hj.RootElement.GetProperty("icon_widths").GetProperty("widths").EnumerateArray().Select(x => x.GetInt32()).ToArray(); }
    catch (Exception ex) when (ex is KeyNotFoundException || ex is InvalidOperationException || ex is System.Text.Json.JsonException) { throw new Exception("icon widths: " + hudJson + " has no icon_widths.widths list (" + ex.Message + ")"); }
    if (iconW.Length < 256) throw new Exception("icon widths: " + hudJson + " has only " + iconW.Length + " entries, need 256");
    Console.WriteLine($"icon widths: kh2_hud.json ({iconW.Length})");
    float tr = (TEXT_COL & 0xFF) / 128f, tg = ((TEXT_COL >> 8) & 0xFF) / 128f, tb = ((TEXT_COL >> 16) & 0xFF) / 128f;
    List<GQuad> Layout(byte[] codes, bool shadow, out float width) {
        var o = new List<GQuad>();
        float scale = SIZE / BASE, sr = scale;
        int lineH = (int)(BASE * scale + 0.5f), gx = 0, gy = lineH << 4, prev = -1;
        void Glyph(int x, int y, int g, float r, float gg, float bb, float a) {
            int cw = CellW, ch = CellH, per = 512 / cw;
            float u0 = g % per * cw, v0 = g / per * ch;
            int x1 = (int)(cw * SUB * scale) + x, yb = y + 0x20, yt = yb - (int)(ch * SUB * scale);
            o.Add(new GQuad(0, u0, v0, u0 + cw, v0 + ch, x / SUB, yt / SUB, x1 / SUB, yb / SUB, r, gg, bb, a));
        }
        for (int i = 0; i < codes.Length;) {
            int c = codes[i];
            if (c == 0 || c == 0x10 || c == 2) break;
            if (c >= 0x19) {
                int g = c > 0x1F ? c - 0x20 : 0xD;
                if (g == 0x46) { int nx = i + 1 < codes.Length ? codes[i + 1] : 0, pv = i > 0 ? codes[i - 1] : 0; if (nx != 0x66 && pv != 0x66) g = 0x34; }
                int keep = g;
                if (g == 0x46 && prev == 0x46) { gx += (int)(sr * -32f); g = 0x47; keep = prev; }
                prev = keep;
                if (shadow) Glyph(gx + 0x20, gy + 0x10, g, 0, 0, 0, 0.5f);
                Glyph(gx, gy, g, tr, tg, tb, 1);
                gx += (int)(sr * widths[g] + 0.5f) * (int)SUB;
                i += c > 0x1F ? 1 : 2; continue;
            }
            if (c == 1) gx += (int)(sr * (int)(BASE / 3) + 0.5f) * (int)SUB;
            else if (c == 9 && i + 1 < codes.Length) {
                int k = codes[i + 1];
                int bs = (int)(sr * 64f + gy), y1 = bs - 0x10, y0 = bs + (3 - (int)(sr * 28f)) * (int)SUB, x0 = gx + 0x20, x1 = x0 + ((int)(sr * -28f) + 4) * -(int)SUB;
                float u0 = k % 21 * 24, v0 = k / 21 * 24;
                o.Add(new GQuad(1, u0, v0, u0 + 24, v0 + 24, x0 / SUB, y0 / SUB, x1 / SUB, y1 / SUB, 1, 1, 1, 1));
                gx = (int)((iconW[k] << 4) * sr + gx);
            }
            i += c == 9 ? 2 : 1;  // (the panel's codes: glyphs, 01 space, 09 icon only)
        }
        // text_width (re:0x3AEAA0): glyphs (int)(w x scale + 0.5), spaces (int)(base / 3 x scale + 0.5), icons their
        // table width; rounded up to even
        int w = 0;
        for (int i = 0; i < codes.Length;) {
            int c = codes[i];
            if (c == 0 || c == 0x10 || c == 2) break;
            if (c >= 0x20) { w += (int)(widths[c - 0x20] * scale + 0.5f); i++; }
            else if (c == 1) { w += (int)(BASE / 3 * scale + 0.5f); i++; }
            else if (c == 9) { w += iconW[codes[i + 1]]; i += 2; }
            else i++;
        }
        width = (w + 1) & ~1;
        return o;
    }
    var lines = File.ReadAllLines(labelsTxt).Where(l => l.Trim().Length > 0).Select(l => l.Split(' ')).ToList();
    var labs = new List<(uint key, List<GQuad> q, float x0, float y0, int w, int h, float adv)>();
    foreach (var l in lines) {
        string k = l[1]; uint key = k == "name" ? 0u : k.StartsWith("lv") ? uint.Parse(k[2..]) : k == "str" ? 0x101u : k == "mag" ? 0x102u :
            k == "def" ? 0x103u : k == "ap" ? 0x104u : k.StartsWith("fn") ? 0x200u + uint.Parse(k[2..]) : k.StartsWith("ab") ? 0x10000u + uint.Parse(k[2..]) : throw new Exception("label key " + k);
        var q = Layout(Convert.FromHexString(l[3]), l[2] == "1", out float adv);
        float x0 = MathF.Floor(q.Min(z => Math.Min(z.x0, z.x1))) - 1, y0 = MathF.Floor(q.Min(z => Math.Min(z.y0, z.y1))) - 1;
        float x1 = MathF.Ceiling(q.Max(z => Math.Max(z.x0, z.x1))) + 1, y1 = MathF.Ceiling(q.Max(z => Math.Max(z.y0, z.y1))) + 1;
        labs.Add((key, q, x0, y0, (int)((x1 - x0) * AS), (int)((y1 - y0) * AS), adv));
    }
    // shelf packing, 1024 wide
    int aw = 1024, ax0 = 0, ay0 = 0, rowh = 0;
    var at = new (int x, int y)[labs.Count];
    for (int i = 0; i < labs.Count; i++) {
        if (ax0 + labs[i].w > aw) { ax0 = 0; ay0 += rowh; rowh = 0; }
        at[i] = (ax0, ay0); ax0 += labs[i].w; rowh = Math.Max(rowh, labs[i].h);
    }
    int ah = (ay0 + rowh + 3) & ~3;
    var atlas = new float[aw * ah * 4];
    float[] Samp(byte[] img, int w, int h, float x, float y, float cx0, float cy0, float cx1, float cy1) {  // bilinear, clamped to the cell
        x = Math.Clamp(x, cx0 + 0.5f, cx1 - 0.5f) - 0.5f; y = Math.Clamp(y, cy0 + 0.5f, cy1 - 0.5f) - 0.5f;
        int xi = (int)MathF.Floor(x), yi = (int)MathF.Floor(y); float fx = x - xi, fy = y - yi;
        var o = new float[4];
        for (int j = 0; j < 4; j++) {
            int sx = Math.Clamp(xi + (j & 1), 0, w - 1), sy = Math.Clamp(yi + (j >> 1), 0, h - 1);
            float wt = ((j & 1) != 0 ? fx : 1 - fx) * ((j >> 1) != 0 ? fy : 1 - fy);
            int si = (sy * w + sx) * 4; float a = img[si + 3] / 255f;
            for (int k = 0; k < 3; k++) o[k] += img[si + k] / 255f * a * wt;  // (premultiplied while filtering)
            o[3] += a * wt;
        }
        if (o[3] > 0) for (int k = 0; k < 3; k++) o[k] /= o[3];
        return o;
    }
    for (int i = 0; i < labs.Count; i++) {
        var L = labs[i];
        for (int py = 0; py < L.h; py++) for (int px = 0; px < L.w; px++) {
            float fx = L.x0 + (px + 0.5f) / AS, fy = L.y0 + (py + 0.5f) / AS;
            int di = ((at[i].y + py) * aw + at[i].x + px) * 4;
            foreach (var q in L.q) {
                if (fx < q.x0 || fx >= q.x1 || fy < q.y0 || fy >= q.y1) continue;
                float u = q.u0 + (fx - q.x0) / (q.x1 - q.x0) * (q.u1 - q.u0), v = q.v0 + (fy - q.y0) / (q.y1 - q.y0) * (q.v1 - q.v0);
                var c = q.img == 0 ? Samp(font, fw, fh, u * fsc, v * fsc, q.u0 * fsc, q.v0 * fsc, q.u1 * fsc, q.v1 * fsc)
                                   : Samp(icon, iw, ih, u * isc, v * isc, q.u0 * isc, q.v0 * isc, q.u1 * isc, q.v1 * isc);
                float sa = c[3] * q.a; if (sa <= 0) continue;
                float da = atlas[di + 3], oa = sa + da * (1 - sa);
                float[] sc = { Math.Min(c[0] * q.r, 1), Math.Min(c[1] * q.g, 1), Math.Min(c[2] * q.b, 1) };
                for (int k = 0; k < 3; k++) atlas[di + k] = (sc[k] * sa + atlas[di + k] * da * (1 - sa)) / oa;
                atlas[di + 3] = oa;
            }
        }
    }
    var ab = new byte[aw * ah * 4];
    for (int i = 0; i < ab.Length; i++) ab[i] = (byte)Math.Clamp(atlas[i] * 255 + 0.5f, 0, 255);
    if (Environment.GetEnvironmentVariable("SORA_LVUP_ATLAS_PNG") is string ap && ap.Length > 0) { Png.Write(ap, aw, ah, ab); Console.WriteLine("wrote " + ap); }
    using var w = new BinaryWriter(File.Create(outPath));
    w.Write(Encoding.ASCII.GetBytes("SKLV")); w.Write(1); w.Write(grps.Count); w.Write(frames.Count); w.Write(aw); w.Write(ah); w.Write(labs.Count); w.Write(AS);
    foreach (var g in grps) { w.Write((ushort)g.id); w.Write((ushort)g.pad); w.Write((short)g.lx); w.Write((short)g.tx); w.Write((short)g.ty); w.Write((ushort)g.n); w.Write((uint)g.first); }
    foreach (var f in frames) { w.Write((uint)f.off); w.Write((uint)f.n); w.Write(f.ax); w.Write(f.ay); w.Write(f.ac); }
    for (int i = 0; i < labs.Count; i++) {
        var L = labs[i];
        w.Write((ushort)at[i].x); w.Write((ushort)at[i].y); w.Write((ushort)L.w); w.Write((ushort)L.h);
        w.Write(L.x0); w.Write(L.y0); w.Write(L.adv); w.Write(L.key);
    }
    w.Write(parts.ToArray()); w.Write(ab);
    Console.WriteLine($"wrote {outPath}: {grps.Count} groups, {frames.Count} frames, {parts.Length / 68} parts, {labs.Count} labels, atlas {aw}x{ah} ({AS} px / frame px)");
}

// lvuptest: what the wip sora_hud.c lvup_draw puts on screen, in frame px of KH2's frame with the widescreen +0x55 (the
// image's right edge = x 597, its top = y 0), the top-right 380 x 170 of it per cell, 3x. Same rules as the C.
void LvupTest(string binPath, string hudPath, string outPath) {
    var b = File.ReadAllBytes(binPath); var hb = File.ReadAllBytes(hudPath);
    if (Encoding.ASCII.GetString(b, 0, 4) != "SKLV" || Encoding.ASCII.GetString(hb, 0, 4) != "SKHD") throw new Exception("need lvup.bin + hud.bin");
    int ng = BitConverter.ToInt32(b, 8), nf = BitConverter.ToInt32(b, 12), aw = BitConverter.ToInt32(b, 16), ah = BitConverter.ToInt32(b, 20), nl = BitConverter.ToInt32(b, 24), AS = BitConverter.ToInt32(b, 28);
    int g0 = 32, f0 = g0 + ng * 16, l0 = f0 + nf * 20, p0 = l0 + nl * 24, a0 = b.Length - aw * ah * 4;
    int sw = BitConverter.ToInt32(hb, 16), sh = BitConverter.ToInt32(hb, 20), soff = BitConverter.ToInt32(hb, 24);
    float ssc = sw / 512f;
    const int S = 3, CW = 380, CH = 170, XOFF = 0x55;
    var cells = new (string what, int frame, int[][] panels)[] {  // panels: {level, line1, line2} (0 = none)
        ("LV 7 (sword): Defense + Combo Boost, f60", 60, new[] { new[] { 7, 0x103, 0x10000 + 390 } }),
        ("LV 2: Defense only (one-line group 104), f60", 60, new[] { new[] { 2, 0x103, 0 } }),
        ("LV 3 + LV 4 at once (stacked), f60", 60, new[] { new[] { 3, 0x101, 0x104 }, new[] { 4, 0x103, 0 } }),
        ("LV 12: Magic + Magic Lock-On, f5 (sliding in)", 5, new[] { new[] { 12, 0x102, 0x10000 + 403 } }),
        ("LV 12, f190 (leaving)", 190, new[] { new[] { 12, 0x102, 0x10000 + 403 } }),
        ("LV 99 two lines, f120", 120, new[] { new[] { 99, 0x101, 0x102 } }),
    };
    int cols = 2, rows = (cells.Length + 1) / 2, W = CW * S * cols, H = CH * S * rows;
    var cv = new float[W * H * 4];
    for (int i = 0; i < W * H; i++) { cv[i * 4] = 0.45f; cv[i * 4 + 1] = 0.52f; cv[i * 4 + 2] = 0.40f; cv[i * 4 + 3] = 1; }
    for (int i = 0; i < W * H; i++) if ((i % W) % (CW * S) == 0 || (i / W) % (CH * S) == 0) { cv[i * 4] = cv[i * 4 + 1] = cv[i * 4 + 2] = 0.2f; }
    void Blit(int cx, int cy, byte[] img, int ioff, int iw, int ih, float u0, float v0, float u1, float v1, float x0, float y0, float x1, float y1, float[] col) {
        float bx = cx * CW * S - (597 - CW) * S, by = cy * CH * S;  // frame (x, y) -> canvas
        int ix0 = (int)MathF.Floor(Math.Min(x0, x1) * S + bx), ix1 = (int)MathF.Ceiling(Math.Max(x0, x1) * S + bx), iy0 = (int)MathF.Floor(Math.Min(y0, y1) * S + by), iy1 = (int)MathF.Ceiling(Math.Max(y0, y1) * S + by);
        for (int py = Math.Max(iy0, cy * CH * S); py < Math.Min(iy1, (cy + 1) * CH * S); py++) for (int px = Math.Max(ix0, cx * CW * S); px < Math.Min(ix1, (cx + 1) * CW * S); px++) {
            float u = ((px + 0.5f - bx) / S - x0) / (x1 - x0), v = ((py + 0.5f - by) / S - y0) / (y1 - y0);
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            int tx = Math.Clamp((int)(u0 + u * (u1 - u0)), 0, iw - 1), ty = Math.Clamp((int)(v0 + v * (v1 - v0)), 0, ih - 1);
            int ti = ioff + (ty * iw + tx) * 4, ci = (py * W + px) * 4;
            float a = img[ti + 3] / 255f * Math.Min(col[3], 1);
            for (int k = 0; k < 3; k++) cv[ci + k] = cv[ci + k] * (1 - a) + Math.Min(img[ti + k] / 255f * col[k], 1) * a;
        }
    }
    int LabelOf(uint key) { for (int i = 0; i < nl; i++) if (BitConverter.ToUInt32(b, l0 + i * 24 + 20) == key) return i; return -1; }
    void Label(int cx, int cy, uint key, float x, float y, float alpha, bool right) {
        int i = LabelOf(key); if (i < 0) return;
        int o = l0 + i * 24;
        float lx = BitConverter.ToUInt16(b, o), ly = BitConverter.ToUInt16(b, o + 2), lw = BitConverter.ToUInt16(b, o + 4), lh = BitConverter.ToUInt16(b, o + 6);
        float ox = BitConverter.ToSingle(b, o + 8), oy = BitConverter.ToSingle(b, o + 12), adv = BitConverter.ToSingle(b, o + 16);
        if (right) x -= adv;
        Blit(cx, cy, b, a0, aw, ah, lx, ly, lx + lw, ly + lh, x + ox, y + oy, x + ox + lw / AS, y + oy + lh / AS, new[] { 1f, 1f, 1f, alpha });
    }
    for (int c = 0; c < cells.Length; c++) {
        int cx = c % cols, cy = c / cols, ystack = 0;
        foreach (var pn in cells[c].panels) {
            int nlines = (pn[1] != 0 ? 1 : 0) + (pn[2] != 0 ? 1 : 0);
            int gi = nlines >= 2 ? 1 : 0, go = g0 + gi * 16;
            int pad = BitConverter.ToUInt16(b, go + 2), lxp = BitConverter.ToInt16(b, go + 4), txp = BitConverter.ToInt16(b, go + 6), typ = BitConverter.ToInt16(b, go + 8), gn = BitConverter.ToUInt16(b, go + 10);
            int first = BitConverter.ToInt32(b, go + 12), f = Math.Min(cells[c].frame, gn - 1), fo = f0 + (first + f) * 20;
            uint poff = BitConverter.ToUInt32(b, fo), pcount = BitConverter.ToUInt32(b, fo + 4);
            int ax = BitConverter.ToInt32(b, fo + 8), ay = BitConverter.ToInt32(b, fo + 12); uint ac = BitConverter.ToUInt32(b, fo + 16);
            for (uint k = 0; k < pcount; k++) {
                int p = p0 + (int)(poff + k) * 68;
                float l = BitConverter.ToSingle(b, p), t = BitConverter.ToSingle(b, p + 4), r = BitConverter.ToSingle(b, p + 8), bt = BitConverter.ToSingle(b, p + 12);
                var cl = Seq.C(BitConverter.ToUInt32(b, p + 16)); var ct = Seq.C(BitConverter.ToUInt32(b, p + 20)); var cr = Seq.C(BitConverter.ToUInt32(b, p + 24)); var cb = Seq.C(BitConverter.ToUInt32(b, p + 28));
                float x0 = BitConverter.ToSingle(b, p + 32), x1 = BitConverter.ToSingle(b, p + 36), y0 = BitConverter.ToSingle(b, p + 48), y1 = BitConverter.ToSingle(b, p + 56);
                var col = new float[4]; for (int j = 0; j < 4; j++) col[j] = (cl[j] + ct[j] + cr[j] + cb[j]) * 0.25f;  // (flat: the mean corner)
                if (r == l) r = l + 0.5f; if (bt == t) bt = t + 0.5f;
                Blit(cx, cy, hb, soff, sw, sh, l * ssc, t * ssc, r * ssc, bt * ssc, XOFF + x0, ystack + y0, XOFF + x1, ystack + y1, col);
            }
            float al = Math.Min(((ac >> 24) & 0xFF) / 128f, 1);
            int side(int x) => x >= 0x100 ? XOFF : -XOFF;
            if (pn[1] != 0) Label(cx, cy, (uint)pn[1], XOFF + ax + lxp, ystack + ay + typ, al, false);
            Label(cx, cy, 0, ax + side(ax), ystack + ay, al, true);
            Label(cx, cy, (uint)pn[0], ax + txp + side(ax + txp), ystack + ay, al, false);
            if (pn[2] != 0) Label(cx, cy, (uint)pn[2], ax + lxp + side(ax + lxp), ystack + ay + 0x11 + typ, al, false);
            ystack += pad;
        }
        Console.WriteLine($"  cell {c}: {cells[c].what}");
    }
    var ob = new byte[cv.Length];
    for (int i = 0; i < cv.Length; i++) ob[i] = (byte)Math.Clamp(cv[i] * 255, 0, 255);
    Png.Write(outPath, W, H, ob);
    Console.WriteLine($"wrote {outPath} ({W}x{H})");
}

// lvupshot: the panel over a game screenshot at the HUD's real scale (sora_hud.c: unit = image height / 448, KH2 x 597
// on the right edge, y 0 on the top): LV 7 (Defense + Combo Boost) and LV 8 (Magic) stacked, at frame `f`.
void LvupShot(string binPath, string hudPath, string shotPath, string outPath, int fr) {
    var b = File.ReadAllBytes(binPath); var hb = File.ReadAllBytes(hudPath);
    int ng = BitConverter.ToInt32(b, 8), nf = BitConverter.ToInt32(b, 12), aw = BitConverter.ToInt32(b, 16), ah = BitConverter.ToInt32(b, 20), nl = BitConverter.ToInt32(b, 24), AS = BitConverter.ToInt32(b, 28);
    int g0 = 32, f0 = g0 + ng * 16, l0 = f0 + nf * 20, p0 = l0 + nl * 24, a0 = b.Length - aw * ah * 4;
    int sw = BitConverter.ToInt32(hb, 16), sh = BitConverter.ToInt32(hb, 20), soff = BitConverter.ToInt32(hb, 24);
    float ssc = sw / 512f;
    var shot = Png.Read(shotPath, out int W, out int H);
    float unit = H / 448f, ox = W - 597 * unit;
    const int XOFF = 0x55;
    var cv = new float[W * H * 4];
    for (int i = 0; i < cv.Length; i++) cv[i] = shot[i] / 255f;
    void Blit(byte[] img, int ioff, int iw, int ih, float u0, float v0, float u1, float v1, float x0, float y0, float x1, float y1, float[] col) {
        float X0 = ox + x0 * unit, X1 = ox + x1 * unit, Y0 = y0 * unit, Y1 = y1 * unit;
        for (int py = Math.Max((int)MathF.Floor(Math.Min(Y0, Y1)), 0); py < Math.Min((int)MathF.Ceiling(Math.Max(Y0, Y1)), H); py++)
            for (int px = Math.Max((int)MathF.Floor(Math.Min(X0, X1)), 0); px < Math.Min((int)MathF.Ceiling(Math.Max(X0, X1)), W); px++) {
                float u = (px + 0.5f - X0) / (X1 - X0), v = (py + 0.5f - Y0) / (Y1 - Y0);
                if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
                int tx = Math.Clamp((int)(u0 + u * (u1 - u0)), 0, iw - 1), ty = Math.Clamp((int)(v0 + v * (v1 - v0)), 0, ih - 1);
                int ti = ioff + (ty * iw + tx) * 4, ci = (py * W + px) * 4;
                float a = img[ti + 3] / 255f * Math.Min(col[3], 1);
                for (int k = 0; k < 3; k++) cv[ci + k] = cv[ci + k] * (1 - a) + Math.Min(img[ti + k] / 255f * col[k], 1) * a;
            }
    }
    int LabelOf(uint key) { for (int i = 0; i < nl; i++) if (BitConverter.ToUInt32(b, l0 + i * 24 + 20) == key) return i; return -1; }
    void Label(uint key, float x, float y, float alpha, bool right) {
        int i = LabelOf(key); if (i < 0) return;
        int o = l0 + i * 24;
        float lx = BitConverter.ToUInt16(b, o), ly = BitConverter.ToUInt16(b, o + 2), lw = BitConverter.ToUInt16(b, o + 4), lh = BitConverter.ToUInt16(b, o + 6);
        float lox = BitConverter.ToSingle(b, o + 8), loy = BitConverter.ToSingle(b, o + 12), adv = BitConverter.ToSingle(b, o + 16);
        if (right) x -= adv;
        Blit(b, a0, aw, ah, lx, ly, lx + lw, ly + lh, x + lox, y + loy, x + lox + lw / AS, y + loy + lh / AS, new[] { 1f, 1f, 1f, alpha });
    }
    int ystack = 0;
    foreach (var pn in new[] { new[] { 7, 0x103, 0x10000 + 390 }, new[] { 8, 0x102, 0 } }) {
        int nlines = (pn[1] != 0 ? 1 : 0) + (pn[2] != 0 ? 1 : 0), go = g0 + (nlines >= 2 ? 1 : 0) * 16;
        int pad = BitConverter.ToUInt16(b, go + 2), lxp = BitConverter.ToInt16(b, go + 4), txp = BitConverter.ToInt16(b, go + 6), typ = BitConverter.ToInt16(b, go + 8), gn = BitConverter.ToUInt16(b, go + 10);
        int first = BitConverter.ToInt32(b, go + 12), fo = f0 + (first + Math.Min(fr, gn - 1)) * 20;
        uint poff = BitConverter.ToUInt32(b, fo), pcount = BitConverter.ToUInt32(b, fo + 4);
        int ax = BitConverter.ToInt32(b, fo + 8), ay = BitConverter.ToInt32(b, fo + 12); uint ac = BitConverter.ToUInt32(b, fo + 16);
        for (uint k = 0; k < pcount; k++) {
            int p = p0 + (int)(poff + k) * 68;
            float l = BitConverter.ToSingle(b, p), t = BitConverter.ToSingle(b, p + 4), r = BitConverter.ToSingle(b, p + 8), bt = BitConverter.ToSingle(b, p + 12);
            var cl = Seq.C(BitConverter.ToUInt32(b, p + 16)); var ct = Seq.C(BitConverter.ToUInt32(b, p + 20)); var cr = Seq.C(BitConverter.ToUInt32(b, p + 24)); var cb = Seq.C(BitConverter.ToUInt32(b, p + 28));
            float x0 = BitConverter.ToSingle(b, p + 32), x1 = BitConverter.ToSingle(b, p + 36), y0 = BitConverter.ToSingle(b, p + 48), y1 = BitConverter.ToSingle(b, p + 56);
            var col = new float[4]; for (int j = 0; j < 4; j++) col[j] = (cl[j] + ct[j] + cr[j] + cb[j]) * 0.25f;
            if (r == l) r = l + 0.5f; if (bt == t) bt = t + 0.5f;
            Blit(hb, soff, sw, sh, l * ssc, t * ssc, r * ssc, bt * ssc, XOFF + x0, ystack + y0, XOFF + x1, ystack + y1, col);
        }
        float al = Math.Min(((ac >> 24) & 0xFF) / 128f, 1);
        int side(int x) => x >= 0x100 ? XOFF : -XOFF;
        if (pn[1] != 0) Label((uint)pn[1], XOFF + ax + lxp, ystack + ay + typ, al, false);
        Label(0, ax + side(ax), ystack + ay, al, true);
        Label((uint)pn[0], ax + txp + side(ax + txp), ystack + ay, al, false);
        if (pn[2] != 0) Label((uint)pn[2], ax + lxp + side(ax + lxp), ystack + ay + 0x11 + typ, al, false);
        ystack += pad;
    }
    var ob = new byte[cv.Length];
    for (int i = 0; i < cv.Length; i++) ob[i] = (byte)Math.Clamp(cv[i] * 255, 0, 255);
    Png.Write(outPath, W, H, ob);
    Console.WriteLine($"wrote {outPath} ({W}x{H}, unit {unit:F3} px)");
}

// ===================================================================== (s57 party HUD helper) KH2's party gauges
// dk::GAUGE_FRIEND (PHH hud_kh2.py GaugeFriend: setup re:0x181D20, tick re:0x182730; architecture reference only, no
// code copied). Each party member's plate = es0field.2dd 'gaug' animation groups 0x48 (72 normal), 0x43 (67 low HP),
// 0x46 (70 hit), 0x47 (71 intro), 0x49 / 0x4A (73 / 74: member 2 sliding into / back out of member 1's place); at the
// plate's anchor three children: 0x40 (64 the lost-HP track), 0x41 (65 HP, left half), 0x42 (66 MP, right half), each
// cut by KH2's CIRCLE_MASK (re:0x182180 / re:0x182730: sora_hud.c draws it as an angle clip from 6 o'clock, filling up
// to ratio x 180 degrees). The face = the member's own obj/P_EX020.a.us (Donald) / P_EX030.a.us (Goofy) 'face' Imgd +
// Seqd (anim groups 3 normal, 0 low HP, 2 KO, 1 hurt: re:0x17EBA0 / 0x17EBE0), HD art = the remastered .a.us folder's
// -6.dds / -2.dds (192 x 192 BGRA8, 4x the PS2 48 x 48).
// party.bin, little-endian, magic 'SKPT' v1 (runtime/sora_hud.c party_load):
//   header   u32 'SKPT', version 1, n_grp, n_frames, n_face, face_w, face_h, face_ps2 (the PS2 face Imgd's width);
//            i32 orb_x, orb_y (Sora's orb = anim group 137's anchor, KH2 frame px), slot_dx, slot_dy (anim group 72's
//            anchor - the orb), pitch (anim group 71's TextPositionX: member 2's plate y offset), reserved     (56 B)
//   grp[n]   u16 id (KH2 anim group id; 0x100 + n = face group n), u16 tex (0 gaug sheet, 1 face), u16 n_frames,
//            u16 loop (the frame a looping group goes back to; 0xFFFF = one-shot), u32 first frame, u32 0     (16 B)
//   frame[n] u32 part_off, u32 n_parts (LPart units), f32 anchor_x, anchor_y, u32 anchor colour 0xAABBGGRR      (20 B)
//   parts    LPart (sora_hud.c, 68 B): uv l t r b (PS2 px of the gaug sheet 512 x 256 / of the face 48 x 48), 4 corner
//            colours, corners x[4] y[4] TL TR BR BL (scale / rotation applied: group 67's red ring turns and grows),
//            blend. Plate groups (67 70 71 72 73 74): KH2 frame px minus the orb; children (64 65 66) and the faces:
//            relative to the plate's anchor (where KH2 attaches them, re:0x16ABF0)
//   faces    n_face x RGBA8 face_w x face_h (0 Donald, 1 Goofy)
// Animation = the Kh2A rules LvupBake uses (PHH kh2_2d re:0x192250 / re:0x193880), plus the part scale / rotation.
void PartyBake(string outPath) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_first/original/field2d/us/es0field.2dd")));
    var se = bar.First(x => x.Type == Bar.EntryType.Seqd && x.Name == "gaug"); se.Stream.Position = 0;
    var s = Sequence.Read(se.Stream);
    Sequence FaceSeq(string obj) {
        var fb = Bar.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_sixth/original/obj/{obj}.a.us")));
        var fe = fb.First(x => x.Type == Bar.EntryType.Seqd && x.Name == "face"); fe.Stream.Position = 0;
        return Sequence.Read(fe.Stream);
    }
    string Sig(Sequence q) {  // the face sequences must match (one set of face groups serves both members)
        var sb = new StringBuilder();
        foreach (var sp in q.Sprites) sb.Append($"{sp.Left},{sp.Top},{sp.Right},{sp.Bottom},{sp.ColorLeft:X};");
        foreach (var g in q.SpriteGroups) foreach (var p in g) sb.Append($"{p.SpriteIndex}:{p.Left},{p.Top},{p.Right},{p.Bottom};");
        foreach (var g in q.AnimationGroups) { sb.Append($"|{g.DoNotLoop},{g.LoopStart}"); foreach (var a in g.Animations) sb.Append($"{a.Flags:X},{a.SpriteGroupIndex},{a.FrameStart},{a.FrameEnd},{a.ColorStart:X},{a.ColorEnd:X};"); }
        return sb.ToString();
    }
    var fd = FaceSeq("P_EX020"); var fg = FaceSeq("P_EX030");
    if (Sig(fd) != Sig(fg)) throw new Exception("Donald's and Goofy's face sequences differ: bake them per member");
    Console.WriteLine($"face seqs: Donald == Goofy ({fd.Sprites.Count} sprites, {fd.AnimationGroups.Count} anim groups)");
    (float x, float y, uint c) Anchor(Sequence q, int g, int f, (float, float, uint) keep) {
        foreach (var a in q.AnimationGroups[g].Animations) {
            if ((a.Flags & 2) == 0 || !Kh2A.Active(a, f)) continue;
            float t = Kh2A.T(a, f); var (x, y) = Kh2A.Pos(a, t);
            return (x, y, Kh2A.Col(a, t));
        }
        return keep;
    }
    var orb = Anchor(s, 137, 0, (0, 0, 0));
    var slot = Anchor(s, 72, 0, (0, 0, 0));
    int pitch = s.AnimationGroups[71].TextPositionX;
    Console.WriteLine($"orb (ag 137) {orb.x},{orb.y}; plate (ag 72) {slot.x},{slot.y} = orb + {slot.x - orb.x},{slot.y - orb.y}; pitch (ag 71 TextPositionX) {pitch}");
    var parts = new MemoryStream(); var pw = new BinaryWriter(parts);
    var frames = new List<(int off, int n, float ax, float ay, uint ac)>();
    var grps = new List<(int id, int tex, int n, int loop, int first)>();
    var plate = new HashSet<int> { 67, 70, 71, 72, 73, 74 };
    void Group(Sequence q, int g, int id, int tex, bool rel) {
        var ag = q.AnimationGroups[g];
        int n = ag.Animations.Max(a => (int)a.FrameEnd) + 1, first = frames.Count;
        float bx = rel ? orb.x : 0, by = rel ? orb.y : 0;
        (float, float, uint) anc = (0, 0, 0x80808080u);
        for (int f = 0; f < n; f++) {
            int off = (int)(parts.Position / 68), cnt = 0;
            anc = Anchor(q, g, f, anc);
            foreach (var a in ag.Animations) {
                if (!Kh2A.Active(a, f)) continue;
                float t = Kh2A.T(a, f);
                var (x, y) = Kh2A.Pos(a, t);
                uint c = (a.Flags & 0x400) != 0 ? 0x80808080u : Kh2A.Col(a, t);
                float sc = (a.Flags & 0x40) != 0 ? 1 : Kh2A.FL(t, a.ScaleStart, a.ScaleEnd);
                float sx = sc * ((a.Flags & 0x40) != 0 ? 1 : Kh2A.FL(t, a.ScaleXStart, a.ScaleXEnd)), sy = sc * ((a.Flags & 0x40) != 0 ? 1 : Kh2A.FL(t, a.ScaleYStart, a.ScaleYEnd));
                float rot = (a.Flags & 0x20) != 0 ? 0 : Kh2A.FL(t, a.RotationZStart, a.RotationZEnd), cs = MathF.Cos(rot), sn = MathF.Sin(rot);
                foreach (var p in q.SpriteGroups[a.SpriteGroupIndex]) {
                    var sp = q.Sprites[p.SpriteIndex];
                    pw.Write((float)sp.Left); pw.Write((float)sp.Top); pw.Write((float)sp.Right); pw.Write((float)sp.Bottom);
                    foreach (uint cc in Seq.Corners(sp, c)) pw.Write(cc);
                    var cx = new[] { p.Left * sx, p.Right * sx, p.Right * sx, p.Left * sx };
                    var cy = new[] { p.Top * sy, p.Top * sy, p.Bottom * sy, p.Bottom * sy };
                    for (int k = 0; k < 4; k++) pw.Write(x - bx + cx[k] * cs - cy[k] * sn);
                    for (int k = 0; k < 4; k++) pw.Write(y - by + cx[k] * sn + cy[k] * cs);
                    pw.Write(a.ColorBlend != 0 ? 1u : 0u);
                    cnt++;
                }
            }
            frames.Add((off, cnt, anc.Item1 - bx, anc.Item2 - by, anc.Item3));
        }
        int loop = ag.DoNotLoop != 0 ? 0xFFFF : Math.Min((int)ag.LoopStart, n - 1);
        grps.Add((id, tex, n, loop, first));
        Console.WriteLine($"  {(tex == 1 ? "face" : "gaug")} anim group {g}: {n} frames, {(loop == 0xFFFF ? "one-shot" : $"loops from {loop}")}, f0 {frames[first].n} parts, anchor {frames[first].ax},{frames[first].ay} {frames[first].ac:X8}; last anchor {frames[^1].ax},{frames[^1].ay} {frames[^1].ac:X8}");
    }
    foreach (int g in new[] { 64, 65, 66, 67, 70, 71, 72, 73, 74 }) Group(s, g, g, 0, plate.Contains(g));
    for (int g = 0; g < 4; g++) Group(fd, g, 0x100 + g, 1, false);
    var faces = new List<byte[]>(); int fw = 0, fh = 0;
    foreach (var dds in new[] { $"{Kh2}/kh2_sixth/remastered/obj/P_EX020.a.us/-6.dds", $"{Kh2}/kh2_sixth/remastered/obj/P_EX030.a.us/-2.dds" }) {
        var img = LoadHd(dds, out int w, out int h);
        if (faces.Count > 0 && (w != fw || h != fh)) throw new Exception("faces differ in size");
        fw = w; fh = h; faces.Add(img);
        Console.WriteLine($"face {dds}: {w}x{h}");
    }
    using var o = new BinaryWriter(File.Create(outPath));
    o.Write(Encoding.ASCII.GetBytes("SKPT")); o.Write(1); o.Write(grps.Count); o.Write(frames.Count); o.Write(faces.Count); o.Write(fw); o.Write(fh); o.Write(48);
    o.Write((int)orb.x); o.Write((int)orb.y); o.Write((int)(slot.x - orb.x)); o.Write((int)(slot.y - orb.y)); o.Write(pitch); o.Write(0);
    foreach (var g in grps) { o.Write((ushort)g.id); o.Write((ushort)g.tex); o.Write((ushort)g.n); o.Write((ushort)g.loop); o.Write((uint)g.first); o.Write(0u); }
    foreach (var f in frames) { o.Write((uint)f.off); o.Write((uint)f.n); o.Write(f.ax); o.Write(f.ay); o.Write(f.ac); }
    o.Write(parts.ToArray());
    foreach (var f in faces) o.Write(f);
    Console.WriteLine($"wrote {outPath}: {grps.Count} groups, {frames.Count} frames, {parts.Length / 68} parts, {faces.Count} faces {fw}x{fh}");
}

// ---- (s57 party) the previews: a software copy of sora_hud.c's party_draw (+ Sora's ring, simplified), same rules.
// partytest <party.bin> <hud.bin> <out.png>: states side by side at 4 px per HUD unit; partyshot <party.bin> <hud.bin>
// <shot.png> <out.png> [x y]: over a game screenshot at the runtime's scale (unit = height / 448), the ring centre x, y
// HUD units from the bottom-right (default = sora_hud.c's new default 60, 177).
// Sora's ring as sora_hud.c draws it (full HP: no clip), the MP bar full, his face in its circle: just enough to judge
// where the plates sit. hud.bin: image 0 = the gauge sheet, 1 = the face.
void PtSoraRing(PtCanvas c, byte[] hb, float hpmax) {
    int sw = BitConverter.ToInt32(hb, 16), sh = BitConverter.ToInt32(hb, 20), soff = BitConverter.ToInt32(hb, 24);
    int fw = BitConverter.ToInt32(hb, 32), fh = BitConverter.ToInt32(hb, 36), foff = BitConverter.ToInt32(hb, 40);
    float ssc = sw / 512f;
    void Sp(float l, float t, float r, float b, uint tint, float x0, float y0, float x1, float y1) {
        if (r == l) r = l + 0.5f; if (b == t) b = t + 0.5f;
        c.Quad(hb, soff, sw, sh, new[] { x0, x1, x1, x0 }, new[] { y0, y0, y1, y1 }, l * ssc, t * ssc, r * ssc, b * ssc, Seq.C(tint));
    }
    void Ring(float l, float t, float r, float b, uint tint) { Sp(l, t, r, b, tint, -45, -45, 0, 0); Sp(l, t, r, b, tint, 45, -45, 0, 0); Sp(l, t, r, b, tint, 45, 45, 0, 0); }
    foreach (var (x0, y0) in new[] { (-37f, -37f), (37f, -37f), (37f, 37f), (-37f, 37f) }) Sp(119, 146, 156, 183, 0x40808080, x0, y0, 0, 0);
    c.Quad(hb, foff, fw, fh, new[] { -35f, 35f, 35f, -35f }, new[] { -35f, -35f, 35f, 35f }, 0, 0, fw, fh, new[] { 1f, 1f, 1f, 1f }, (x, y) => x * x + y * y <= 34 * 34);
    foreach (var (x0, y0) in new[] { (-37f, -37f), (37f, -37f), (37f, 37f), (-37f, 37f) }) Sp(217, 0, 254, 37, 0x80808080, x0, y0, 0, 0);
    float L = hpmax * 2;
    Ring(1, 60, 46, 105, 0x80808080); Sp(45, 60, 46, 105, 0x80808080, -L, 45, 0, 0); Sp(189, 49, 234, 52, 0x80808080, -45, 0, 0, 2);
    Ring(50, 60, 95, 105, 0x80808080); Sp(95, 60, 95, 105, 0x80808080, -L, 45, 0, 0);
    float mx = -40, my = 31, Lm = 50;  // (s67) KH2 anim group 116 (sg 110 the label) at the orb + (-40, 31); was -42,22
    Sp(183, 49, 185, 59, 0x80540B04, mx - Lm, my - 5, mx, my + 5); Sp(154, 37, 157, 47, 0x80808080, mx - 3, my - 5, mx, my + 5);
    Sp(157, 37, 181, 47, 0x80C8814D, mx, my - 5, mx + 24, my + 5); Sp(216, 40, 217, 46, 0x80808080, mx - Lm, my - 3, mx, my + 3);
}
// one member's plate as sora_hud.c party_draw does it: slot group gi_slot at frame fs, base y (pitch or 0), the track /
// HP / MP children at the plate's anchor clipped to ratio x 180 deg from 6 o'clock, the face group at frame ff on top
void PtMember(PtCanvas c, PtBin pb, byte[] hb, int member, int slotId, int fs, int faceId, int ff, float hp, float hurt, int ft, float mp, float baseY) {
    int sw = BitConverter.ToInt32(hb, 16), sh = BitConverter.ToInt32(hb, 20), soff = BitConverter.ToInt32(hb, 24);
    float ssc = sw / 512f, fsc = pb.fw / (float)pb.fps2;
    void Parts(int fi, float dx, float dy, bool face, Func<float, float, bool> keep, float kx, float ky) {
        uint off = BitConverter.ToUInt32(pb.b, pb.f0 + fi * 20), n = BitConverter.ToUInt32(pb.b, pb.f0 + fi * 20 + 4);
        for (uint k = 0; k < n; k++) {
            int p = pb.p0 + (int)(off + k) * 68;
            float l = BitConverter.ToSingle(pb.b, p), t = BitConverter.ToSingle(pb.b, p + 4), r = BitConverter.ToSingle(pb.b, p + 8), bt = BitConverter.ToSingle(pb.b, p + 12);
            var col = new float[4];
            for (int j = 0; j < 4; j++) { var cc = Seq.C(BitConverter.ToUInt32(pb.b, p + 16 + j * 4)); for (int q = 0; q < 4; q++) col[q] += cc[q] * 0.25f; }
            var X = new float[4]; var Y = new float[4];
            for (int j = 0; j < 4; j++) { X[j] = dx + BitConverter.ToSingle(pb.b, p + 32 + j * 4); Y[j] = dy + BitConverter.ToSingle(pb.b, p + 48 + j * 4); }
            if (r == l) r = l + 0.5f; if (bt == t) bt = t + 0.5f;
            if (face) c.Quad(pb.b, pb.face0 + member * pb.fw * pb.fh * 4, pb.fw, pb.fh, X, Y, l * fsc, t * fsc, r * fsc, bt * fsc, col, keep, kx, ky);
            else c.Quad(hb, soff, sw, sh, X, Y, l * ssc, t * ssc, r * ssc, bt * ssc, col, keep, kx, ky);
        }
    }
    Func<float, float, bool> Clip(float ratio) => (x, y) => MathF.Atan2(MathF.Abs(x), y) <= ratio * MathF.PI;
    int sf = pb.Frame(pb.Grp(slotId), fs);
    var (ax, ay) = pb.Anchor(sf);
    ay += baseY;
    Parts(sf, 0, baseY, false, null, 0, 0);
    if (ft >= 0 && hurt > hp) Parts(pb.Frame(pb.Grp(64), ft), ax, ay, false, Clip(hurt), ax, ay);
    if (hp > 0) Parts(pb.Frame(pb.Grp(65), 0), ax, ay, false, Clip(hp), ax, ay);
    if (mp > 0) Parts(pb.Frame(pb.Grp(66), 0), ax, ay, false, Clip(mp), ax, ay);
    Parts(pb.Frame(pb.Grp(0x100 + faceId), ff), ax, ay, true, null, 0, 0);
}
void PartyTest(string binPath, string hudPath, string outPath) {
    var pb = new PtBin(binPath); var hb = File.ReadAllBytes(hudPath);
    const float U = 4; const int CW = 200, CH = 210;  // cell in HUD units
    // {slot group, frame, face group, frame, hp, hurt, track frame, mp} x 2 members (Donald lower, Goofy above), title
    var cells = new (string what, float[][] m)[] {
        ("full HP / MP (72 f30, faces 3)", new[] { new float[] { 72, 30, 3, 30, 1, 1, -1, 1 }, new float[] { 72, 30, 3, 30, 1, 1, -1, 1 } }),
        ("Donald HP 0.6 MP 0.4; Goofy hit 1.0 -> 0.5 (70 f6, track f6, face hurt f6)", new[] { new float[] { 72, 30, 3, 30, 0.6f, 0.6f, -1, 0.4f }, new float[] { 70, 6, 1, 6, 0.5f, 1, 6, 0.8f } }),
        ("Goofy track f70 (fading), Donald MP 0 (Charge)", new[] { new float[] { 72, 30, 3, 30, 1, 1, -1, 0 }, new float[] { 72, 30, 3, 30, 0.5f, 1, 70, 0.8f } }),
        ("low HP 0.2 (67 f10 / f30, faces 0)", new[] { new float[] { 67, 10, 0, 10, 0.2f, 0.2f, -1, 1 }, new float[] { 67, 30, 0, 30, 0.2f, 0.2f, -1, 1 } }),
        ("KO: HP 0 (face 2)", new[] { new float[] { 72, 30, 2, 10, 0, 0, -1, 0.5f }, new float[] { 72, 30, 3, 30, 1, 1, -1, 1 } }),
        ("intro (71) f25 / f45", new[] { new float[] { 71, 25, 3, 0, 1, 1, -1, 1 }, new float[] { 71, 45, 3, 0, 1, 1, -1, 1 } }),
    };
    int cols = 3, rows = (cells.Length + cols - 1) / cols, W = (int)(CW * U) * cols, H = (int)(CH * U) * rows;
    var c = new PtCanvas(W, H) { unit = U };
    for (int i = 0; i < W * H; i++) { c.cv[i * 4] = 0.45f; c.cv[i * 4 + 1] = 0.55f; c.cv[i * 4 + 2] = 0.40f; c.cv[i * 4 + 3] = 1; }
    for (int i = 0; i < W * H; i++) if ((i % W) % (int)(CW * U) == 0 || (i / W) % (int)(CH * U) == 0) c.cv[i * 4] = c.cv[i * 4 + 1] = c.cv[i * 4 + 2] = 0.2f;
    for (int k = 0; k < cells.Length; k++) {
        int cx = k % cols, cy = k / cols;
        c.ox = (cx + 1) * CW * U - 60 * U; c.oy = (cy + 1) * CH * U - 45 * U;  // the ring centre 60 / 45 units in
        for (int m = 1; m >= 0; m--) {  // (KH2's order: member 2's plate, then member 1's, then the player gauge)
            var v = cells[k].m[m];
            PtMember(c, pb, hb, m, (int)v[0], (int)v[1], (int)v[2], (int)v[3], v[4], v[5], (int)v[6], v[7], m == 1 ? pb.pitch : 0);
        }
        float sox = c.ox, soy = c.oy;
        PtSoraRing(c, hb, 20);
        Console.WriteLine($"  cell {k}: {cells[k].what}");
    }
    var ob = new byte[c.cv.Length];
    for (int i = 0; i < ob.Length; i++) ob[i] = (byte)Math.Clamp(c.cv[i] * 255, 0, 255);
    Png.Write(outPath, W, H, ob);
    Console.WriteLine($"wrote {outPath} ({W}x{H})");
}
void PartyShot(string binPath, string hudPath, string shotPath, string outPath, float rx, float ry) {
    var pb = new PtBin(binPath); var hb = File.ReadAllBytes(hudPath);
    var shot = Png.Read(shotPath, out int W, out int H);
    var c = new PtCanvas(W, H) { unit = H / 448f };
    for (int i = 0; i < c.cv.Length; i++) c.cv[i] = shot[i] / 255f;
    c.ox = W - rx * c.unit; c.oy = H - ry * c.unit;
    // the plates relative to the ring centre: the bake's plate parts are relative to KH2's orb = our ring centre
    PtMember(c, pb, hb, 1, 72, 30, 3, 30, 1, 1, -1, 1, pb.pitch);
    PtMember(c, pb, hb, 0, 72, 30, 3, 30, 0.7f, 0.7f, -1, 0.6f, 0);
    PtSoraRing(c, hb, 60);
    var ob = new byte[c.cv.Length];
    for (int i = 0; i < ob.Length; i++) ob[i] = (byte)Math.Clamp(c.cv[i] * 255, 0, 255);
    Png.Write(outPath, W, H, ob);
    Console.WriteLine($"wrote {outPath} ({W}x{H}, unit {c.unit:F3} px, ring centre {c.ox:F0},{c.oy:F0} px)");
}

// ===================================================================== (s67 drive) KH2's Drive gauge
// dk::GAUGE's Drive part (PHH hud_kh2.py Gauge: setup re:0x17F9B0, tick re:0x1816C0; architecture reference only, no
// code copied). All children of Sora's orb (KH2 sets the plate's parent to the orb, so its translations are relative
// to the orb = our ring centre, unlike the party plates): plate anim group 0x15 (21, intro: slides in at 70-110) then
// 0x17 (23, normal), 0x1A (26) during MP Charge; at the plate's anchor the fill 0x19 (25, cut by KH2's SECTOR_MASK:
// sora_hud.c), the full-gauge effect 0x14 (20, loops while bars = max), the bar-up effect 0x16 (22, one-shot); the
// digit = group 28 + bars (28-37: 0-9) at the plate's anchor + (plate group 21's LightPositionX, TextPositionY)
// (re:0x17F9B0 digit.own). Form groups (0x31-0x35, 54-63) are left for the forms slice.
// drive.bin, little-endian, magic 'SKDR' v1 (runtime/sora_hud.c drive_load):
//   header   u32 'SKDR', version 1, n_grp, n_frames; i32 digit_x, digit_y; u32 0, 0                           (32 B)
//   grp[n] / frame[n] / parts = party.bin's layout (16 B / 20 B / LPart 68 B), no faces
void DriveBake(string outPath) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_first/original/field2d/us/es0field.2dd")));
    var se = bar.First(x => x.Type == Bar.EntryType.Seqd && x.Name == "gaug"); se.Stream.Position = 0;
    var s = Sequence.Read(se.Stream);
    (float x, float y, uint c) Anchor(int g, int f, (float, float, uint) keep) {
        foreach (var a in s.AnimationGroups[g].Animations) {
            if ((a.Flags & 2) == 0 || !Kh2A.Active(a, f)) continue;
            float t = Kh2A.T(a, f); var (x, y) = Kh2A.Pos(a, t);
            return (x, y, Kh2A.Col(a, t));
        }
        return keep;
    }
    var parts = new MemoryStream(); var pw = new BinaryWriter(parts);
    var frames = new List<(int off, int n, float ax, float ay, uint ac)>();
    var grps = new List<(int id, int n, int loop, int first)>();
    void Group(int g) {
        var ag = s.AnimationGroups[g];
        int n = ag.Animations.Max(a => (int)a.FrameEnd) + 1, first = frames.Count;
        (float, float, uint) anc = (0, 0, 0x80808080u);
        for (int f = 0; f < n; f++) {
            int off = (int)(parts.Position / 68), cnt = 0;
            anc = Anchor(g, f, anc);
            foreach (var a in ag.Animations) {
                if (!Kh2A.Active(a, f)) continue;
                float t = Kh2A.T(a, f);
                var (x, y) = Kh2A.Pos(a, t);
                uint c = (a.Flags & 0x400) != 0 ? 0x80808080u : Kh2A.Col(a, t);
                float sc = (a.Flags & 0x40) != 0 ? 1 : Kh2A.FL(t, a.ScaleStart, a.ScaleEnd);
                float sx = sc * ((a.Flags & 0x40) != 0 ? 1 : Kh2A.FL(t, a.ScaleXStart, a.ScaleXEnd)), sy = sc * ((a.Flags & 0x40) != 0 ? 1 : Kh2A.FL(t, a.ScaleYStart, a.ScaleYEnd));
                float rot = (a.Flags & 0x20) != 0 ? 0 : Kh2A.FL(t, a.RotationZStart, a.RotationZEnd), cs = MathF.Cos(rot), sn = MathF.Sin(rot);
                foreach (var p in s.SpriteGroups[a.SpriteGroupIndex]) {
                    var sp = s.Sprites[p.SpriteIndex];
                    pw.Write((float)sp.Left); pw.Write((float)sp.Top); pw.Write((float)sp.Right); pw.Write((float)sp.Bottom);
                    foreach (uint cc in Seq.Corners(sp, c)) pw.Write(cc);
                    var cx = new[] { p.Left * sx, p.Right * sx, p.Right * sx, p.Left * sx };
                    var cy = new[] { p.Top * sy, p.Top * sy, p.Bottom * sy, p.Bottom * sy };
                    for (int k = 0; k < 4; k++) pw.Write(x + cx[k] * cs - cy[k] * sn);
                    for (int k = 0; k < 4; k++) pw.Write(y + cx[k] * sn + cy[k] * cs);
                    pw.Write(a.ColorBlend != 0 ? 1u : 0u);
                    cnt++;
                }
            }
            frames.Add((off, cnt, anc.Item1, anc.Item2, anc.Item3));
        }
        int loop = ag.DoNotLoop != 0 ? 0xFFFF : Math.Min((int)ag.LoopStart, n - 1);
        grps.Add((g, n, loop, first));
        Console.WriteLine($"  anim group {g}: {n} frames, {(loop == 0xFFFF ? "one-shot" : $"loops from {loop}")}, f0 {frames[first].n} parts, anchor {frames[first].ax},{frames[first].ay}; last anchor {frames[^1].ax},{frames[^1].ay}");
    }
    foreach (int g in new[] { 20, 21, 22, 23, 25, 26 }) Group(g);
    for (int g = 28; g <= 37; g++) Group(g);
    // (s137 smenu) KH2's summon state (drive state 3; FUN_140180640 plate 0x8D, FUN_140181270 fill 0x8B, FUN_140180560 digits table 0x71B0E0 = 144 + bars,
    // the summon plate on the orb FUN_140180e00: 0x9A in, 0x9B loops, 0x9C out). SORA_DRIVE_SUMMON=0 = the s67 bake.
    if (Environment.GetEnvironmentVariable("SORA_DRIVE_SUMMON") != "0") {
        foreach (int g in new[] { 139, 141 }) Group(g);
        for (int g = 144; g <= 153; g++) Group(g);
        foreach (int g in new[] { 154, 155, 156 }) Group(g);
    }
    var pg = s.AnimationGroups[21];
    int dx = pg.LightPositionX, dy = pg.TextPositionY;
    Console.WriteLine($"digit at the plate's anchor + {dx},{dy} (ag 21 LightPositionX, TextPositionY)");
    using var o = new BinaryWriter(File.Create(outPath));
    // (s137) reserved[0..1] = the summon plate's digit offset (ag 141's LightPositionX, TextPositionY); 0 0 = no summon groups
    int sdx = 0, sdy = 0;
    if (grps.Any(g => g.id == 141)) { var sg = s.AnimationGroups[141]; sdx = sg.LightPositionX; sdy = sg.TextPositionY; Console.WriteLine($"summon digit at the plate's anchor + {sdx},{sdy} (ag 141)"); }
    o.Write(Encoding.ASCII.GetBytes("SKDR")); o.Write(1); o.Write(grps.Count); o.Write(frames.Count); o.Write(dx); o.Write(dy); o.Write(sdx); o.Write(sdy);
    foreach (var g in grps) { o.Write((ushort)g.id); o.Write((ushort)0); o.Write((ushort)g.n); o.Write((ushort)g.loop); o.Write((uint)g.first); o.Write(0u); }
    foreach (var f in frames) { o.Write((uint)f.off); o.Write((uint)f.n); o.Write(f.ax); o.Write(f.ay); o.Write(f.ac); }
    o.Write(parts.ToArray());
    Console.WriteLine($"wrote {outPath}: {grps.Count} groups, {frames.Count} frames, {parts.Length / 68} parts");
}
// drivetest: a software copy of sora_hud.c's drive_draw beside Sora's ring (PtSoraRing), optionally the party plates.
// KH2's SECTOR_MASK (re:0x17E580 / 0x17E700, PHH kh2_2d.SectorMask + Ft4): a 50 x 120 rectangle whose corner (pivot
// (-25, -60) from its centre) sits at the plate's anchor + (0x32, 100), turned clockwise by DRIVE_RAD 0.004014257 x
// points (fill 100 = full: 200 points), drawn into Z over the fill (z 6000 > 5000): the fill shows outside it.
static bool DriveKeep(float x, float y, float ang) {  // x, y around the pivot
    float c = MathF.Cos(ang), s = MathF.Sin(ang), lx = c * x + s * y, ly = -s * x + c * y;
    return !(lx >= -50 && lx <= 0 && ly >= -120 && ly <= 0);
}
void DriveTest(string binPath, string hudPath, string outPath, string partyPath) {
    var b = File.ReadAllBytes(binPath); var hb = File.ReadAllBytes(hudPath);
    if (Encoding.ASCII.GetString(b, 0, 4) != "SKDR") throw new Exception("not SKDR");
    int I(int o) => BitConverter.ToInt32(b, o);
    int ng = I(8), nf = I(12), ddx = I(16), ddy = I(20), g0 = 32, f0 = g0 + ng * 16, p0 = f0 + nf * 20;
    int Grp(int id) { for (int i = 0; i < ng; i++) if (BitConverter.ToUInt16(b, g0 + i * 16) == id) return i; throw new Exception($"no group {id}"); }
    int Frame(int id, int f) {
        int gi = Grp(id), n = BitConverter.ToUInt16(b, g0 + gi * 16 + 4), loop = BitConverter.ToUInt16(b, g0 + gi * 16 + 6), first = I(g0 + gi * 16 + 8);
        if (f >= n) f = loop == 0xFFFF ? n - 1 : loop + (f - loop) % (n - loop);
        return first + f;
    }
    int sw = BitConverter.ToInt32(hb, 16), sh = BitConverter.ToInt32(hb, 20), soff = BitConverter.ToInt32(hb, 24);
    float ssc = sw / 512f;
    void Parts(PtCanvas c, int fi, float dx, float dy, Func<float, float, bool> keep, float kx, float ky) {
        uint off = BitConverter.ToUInt32(b, f0 + fi * 20), n = BitConverter.ToUInt32(b, f0 + fi * 20 + 4);
        for (uint k = 0; k < n; k++) {
            int p = p0 + (int)(off + k) * 68;
            float l = BitConverter.ToSingle(b, p), t = BitConverter.ToSingle(b, p + 4), r = BitConverter.ToSingle(b, p + 8), bt = BitConverter.ToSingle(b, p + 12);
            var col = new float[4];
            for (int j = 0; j < 4; j++) { var cc = Seq.C(BitConverter.ToUInt32(b, p + 16 + j * 4)); for (int q = 0; q < 4; q++) col[q] += cc[q] * 0.25f; }
            var X = new float[4]; var Y = new float[4];
            for (int j = 0; j < 4; j++) { X[j] = dx + BitConverter.ToSingle(b, p + 32 + j * 4); Y[j] = dy + BitConverter.ToSingle(b, p + 48 + j * 4); }
            if (r == l) r = l + 0.5f; if (bt == t) bt = t + 0.5f;
            c.Quad(hb, soff, sw, sh, X, Y, l * ssc, t * ssc, r * ssc, bt * ssc, col, keep, kx, ky);
        }
    }
    PtBin pb = partyPath != null ? new PtBin(partyPath) : null;
    const float U = 4; const int CW = 230, CH = 210;
    // {plate group, frame, bars, fill (100 = full), fx group (-1 none), fx frame}
    var cells = new (string what, int plate, int pf, int bars, int fill, int fx, int ff)[] {
        ("empty: 0 bars + 0", 23, 30, 0, 0, -1, 0), ("0 bars + 50", 23, 30, 0, 50, -1, 0), ("1 bar + 99", 23, 30, 1, 99, -1, 0),
        ("2 bars + 25, bar-up fx f10", 23, 30, 2, 25, 22, 10), ("FULL 3 (fx 20 f30)", 23, 30, 3, 100, 20, 30), ("FULL 3 (fx 20 f90)", 23, 30, 3, 100, 20, 90),
        ("MP Charge plate 26 f60, 1 + 40", 26, 60, 1, 40, -1, 0), ("intro 21 f90", 21, 90, 0, 0, -1, 0), ("intro 21 f110 (end)", 21, 110, 0, 30, -1, 0),
    };
    int cols = 3, rows = (cells.Length + cols - 1) / cols, W = (int)(CW * U) * cols, H = (int)(CH * U) * rows;
    var cv = new PtCanvas(W, H) { unit = U };
    for (int i = 0; i < W * H; i++) { cv.cv[i * 4] = 0.45f; cv.cv[i * 4 + 1] = 0.55f; cv.cv[i * 4 + 2] = 0.40f; cv.cv[i * 4 + 3] = 1; }
    for (int i = 0; i < W * H; i++) if ((i % W) % (int)(CW * U) == 0 || (i / W) % (int)(CH * U) == 0) cv.cv[i * 4] = cv.cv[i * 4 + 1] = cv.cv[i * 4 + 2] = 0.2f;
    for (int k = 0; k < cells.Length; k++) {
        var e = cells[k];
        int cx = k % cols, cy = k / cols;
        cv.ox = (cx + 1) * CW * U - 60 * U; cv.oy = (cy + 1) * CH * U - 45 * U;
        if (pb != null) { PtMember(cv, pb, hb, 1, 72, 30, 3, 30, 1, 1, -1, 1, pb.pitch); PtMember(cv, pb, hb, 0, 72, 30, 3, 30, 1, 1, -1, 1, 0); }
        int pfi = Frame(e.plate, e.pf);
        float ax = BitConverter.ToSingle(b, f0 + pfi * 20 + 8), ay = BitConverter.ToSingle(b, f0 + pfi * 20 + 12);
        Parts(cv, pfi, 0, 0, null, 0, 0);
        float ang = 0.004014257f * (e.fill >= 100 ? 200 : e.fill), px = ax + 50, py = ay + 100;
        Parts(cv, Frame(25, 40), ax, ay, (x, y) => DriveKeep(x, y, ang), px, py);
        Parts(cv, Frame(28 + e.bars, 40), ax + ddx, ay + ddy, null, 0, 0);
        if (e.fx >= 0) Parts(cv, Frame(e.fx, e.ff), ax, ay, null, 0, 0);
        PtSoraRing(cv, hb, 60);
        Console.WriteLine($"  cell {k}: {e.what} (plate anchor {ax},{ay})");
    }
    var ob = new byte[cv.cv.Length];
    for (int i = 0; i < ob.Length; i++) ob[i] = (byte)Math.Clamp(cv.cv[i] * 255, 0, 255);
    Png.Write(outPath, W, H, ob);
    Console.WriteLine($"wrote {outPath} ({W}x{H})");
}

// (s55) a text quad of LvupBake's layout (frame px; uv in PS2 px of the font / icon page)
// (sorabuild) CampBake / CampSeqBake: ported unchanged from the Oct 30 tools/hudbake lineage (the only one that had them)
void CampBake(string outPath, int variant) {
    var srcs = new (string png, int lw, int lh)[] {
        ($"{Kh2}/kh2_fifth/remastered/menu/us/camp.2ld/US_camp_2ld{variant}.png", 256, 256),
        ($"{Kh2}/kh2_first/remastered/msg/us/fontimage.bar/US_fontimage_bar0.png", 512, 256) };
    var px = new List<(byte[] p, int w, int h)>();
    foreach (var s in srcs) { var p = Png.Read(s.png, out int w, out int h); px.Add((p, w, h)); }
    using var o = new BinaryWriter(File.Create(outPath));
    o.Write(0x50434B53u); o.Write(1u); o.Write((uint)srcs.Length);
    uint off = (uint)(12 + 20 * srcs.Length);
    for (int i = 0; i < srcs.Length; i++) {
        o.Write((uint)px[i].w); o.Write((uint)px[i].h); o.Write((uint)srcs[i].lw); o.Write((uint)srcs[i].lh); o.Write(off);
        off += (uint)px[i].p.Length;
    }
    foreach (var p in px) o.Write(p.p);
    Console.WriteLine($"camp.bin: {srcs.Length} images ({string.Join(", ", px.Select(p => $"{p.w}x{p.h}"))})");
}

// (s78 camp) camp_seq.bin ('SKCQ' v1): the layout is documented in runtime/sora_camp_seq.h (CsHeader and the structs
// after it, written here field by field in the same order). camp_text.txt: "key id 0xNNNN" (sys.bar id & 0x7fff) or
// "key lit "text"" per line, '#' starts a comment.
void CampSeqBake(string outPath, string textTxt) {
    var bar = Bar.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_fifth/original/menu/us/camp.2ld")));
    var le = bar.First(x => x.Type == Bar.EntryType.Layout); le.Stream.Position = 0;
    var lay = Layout.Read(le.Stream);
    var seq = lay.SequenceItems[1];
    var sysBar = Bar.Read(new MemoryStream(File.ReadAllBytes($"{Kh2}/kh2_first/original/msg/us/sys.bar")));
    var se = sysBar.First(x => x.Name == "sys" && x.Type == Bar.EntryType.List); se.Stream.Position = 0;
    var sys = Msg.Read(se.Stream);
    var enc = OpenKh.Kh2.Messages.Encoders.InternationalSystem;
    byte[] Enc(string t) => enc.Encode(OpenKh.Kh2.Messages.MsgSerializer.DeserializeText(t).ToList()).TakeWhile(b => b != 0).ToArray();
    var msgs = new List<(ushort key, byte[] data)>();
    foreach (var raw in File.ReadAllLines(textTxt)) {
        var l = raw; int h = l.IndexOf('#'); if (h >= 0 && !l.Substring(0, h).Contains('"')) l = l.Substring(0, h);
        l = l.Trim(); if (l.Length == 0) continue;
        var p = l.Split(' ', 3);
        var key = ushort.Parse(p[0]);
        if (p[1] == "id") {
            int id = Convert.ToInt32(p[2].Trim(), 16) & 0x7fff;
            var e = sys.FirstOrDefault(m => m.Id == id) ?? throw new Exception($"{textTxt}: key {key}: sys.bar has no message 0x{id:x4}");
            msgs.Add((key, e.Data.TakeWhile(b => b != 0).ToArray()));
        } else if (p[1] == "lit") {
            int a = p[2].IndexOf('"'), b = p[2].IndexOf('"', a + 1);
            msgs.Add((key, Enc(p[2].Substring(a + 1, b - a - 1))));
        } else throw new Exception($"{textTxt}: bad line '{raw}'");
    }
    var ascii = new byte[128];
    for (int c = 32; c < 127; c++) { try { var b = Enc(((char)c).ToString()); if (b.Length == 1) ascii[c] = b[0]; } catch { } }
    var adv = FontInfo("sys").Select(v => (byte)v).ToArray();
    var l2s = Enumerable.Range(0, 3).Select(g => lay.SequenceGroups[g]).ToList();
    foreach (var g in l2s) foreach (var sp in g.Sequences) if (sp.SequenceIndex != 1) throw new Exception($"layout prop uses sequence {sp.SequenceIndex}, only 1 is baked");
    var parts = seq.SpriteGroups.SelectMany(g => g).ToList();
    var anims = seq.AnimationGroups.SelectMany(g => g.Animations).ToList();
    var props = l2s.SelectMany(g => g.Sequences).ToList();

    var body = new MemoryStream(); var w = new BinaryWriter(body);
    var tabs = new List<(long off, int count)>();
    long Pos() => 88 + body.Position;
    tabs.Add((Pos(), seq.Sprites.Count));
    foreach (var s in seq.Sprites) {
        w.Write(s.Left); w.Write(s.Top); w.Write(s.Right); w.Write(s.Bottom); w.Write(s.UTranslation); w.Write(s.VTranslation);
        w.Write(s.ColorLeft); w.Write(s.ColorTop); w.Write(s.ColorRight); w.Write(s.ColorBottom);
    }
    tabs.Add((Pos(), parts.Count));
    foreach (var q in parts) { w.Write(q.Left); w.Write(q.Top); w.Write(q.Right); w.Write(q.Bottom); w.Write(q.SpriteIndex); }
    tabs.Add((Pos(), seq.SpriteGroups.Count));
    { int f = 0; foreach (var g in seq.SpriteGroups) { w.Write((ushort)f); w.Write((ushort)g.Count); f += g.Count; } }
    tabs.Add((Pos(), anims.Count));
    foreach (var a in anims) {
        foreach (var v in new[] { a.Flags, a.SpriteGroupIndex, a.FrameStart, a.FrameEnd, a.TranslateXStart, a.TranslateXEnd, a.TranslateYStart, a.TranslateYEnd,
                                  a.PivotXStart, a.PivotXEnd, a.PivotYStart, a.PivotYEnd }) w.Write((int)v);
        foreach (var v in new[] { a.RotationXStart, a.RotationXEnd, a.RotationYStart, a.RotationYEnd, a.RotationZStart, a.RotationZEnd,
                                  a.ScaleStart, a.ScaleEnd, a.ScaleXStart, a.ScaleXEnd, a.ScaleYStart, a.ScaleYEnd,
                                  a.CurveXStart, a.CurveYStart, a.CurveXEnd, a.CurveYEnd, a.BounceXStart, a.BounceXEnd, a.BounceYStart, a.BounceYEnd }) w.Write((float)v);
        w.Write((short)a.BounceXCount); w.Write((short)a.BounceYCount); w.Write((int)a.ColorBlend); w.Write((uint)a.ColorStart); w.Write((uint)a.ColorEnd);
    }
    tabs.Add((Pos(), seq.AnimationGroups.Count));
    { int f = 0; foreach (var g in seq.AnimationGroups) {
        w.Write((ushort)f); w.Write((ushort)g.Animations.Count); w.Write((short)g.DoNotLoop); w.Write((short)g.Unknown06);
        foreach (var v in new[] { g.LoopStart, g.LoopEnd, g.LightPositionX, g.TextPositionY, g.TextScale, g.UiPadding, g.TextPositionX }) w.Write((int)v);
        f += g.Animations.Count; } }
    tabs.Add((Pos(), l2s.Count));
    { int f = 0; foreach (var g in l2s) { w.Write((ushort)f); w.Write((ushort)g.Sequences.Count); f += g.Sequences.Count; } }
    tabs.Add((Pos(), props.Count));
    foreach (var sp in props) foreach (var v in new[] { sp.TextureIndex, sp.SequenceIndex, sp.AnimationGroup, sp.ShowAtFrame, sp.PositionX, sp.PositionY }) w.Write((int)v);
    tabs.Add((Pos(), msgs.Count));
    foreach (var m in msgs) { w.Write(m.key); w.Write((ushort)m.data.Length); w.Write(m.data); }
    tabs.Add((Pos(), 128)); w.Write(ascii);
    tabs.Add((Pos(), adv.Length)); w.Write(adv);

    using var o = new BinaryWriter(File.Create(outPath));
    o.Write(0x51434B53u); o.Write(1u);
    foreach (var t in tabs) { o.Write((uint)t.off); o.Write((uint)t.count); }
    o.Write(body.ToArray());
    Console.WriteLine($"camp_seq.bin: {seq.Sprites.Count} sprites, {seq.SpriteGroups.Count} sprite groups, {seq.AnimationGroups.Count} animation groups, {l2s.Count} layout groups, {msgs.Count} messages ({parts.Count} parts, {anims.Count} anims, {props.Count} props, {ascii.Count(b => b != 0)} ascii, {adv.Length} adv)");
}

record GQuad(int img, float u0, float v0, float u1, float v1, float x0, float y0, float x1, float y1, float r, float g, float b, float a);

// (s55) KH2's sprite animation evaluation as PHH kh2_2d ports it (re:0x192250 / re:0x193880), for LvupBake
static class Kh2A {
    public static float T(Sequence.Animation a, int f) {
        float span = (float)a.FrameEnd - (float)a.FrameStart, t = span == 0 ? 1 : (f - (float)a.FrameStart) / span;
        if ((a.Flags & 1) == 0) {
            float u = 1 - t, uu = u * u;
            float A = (1 - a.CurveYStart) * uu * 3 * t, B = uu * u, C = (1 - a.CurveYEnd) * u * 3 * t * t;
            t = 1 - (A + B + C);
        }
        return t;
    }
    public static int IL(float t, float a, float b) => (int)((float)((int)b - (int)a) * t) + (int)a;
    public static float FL(float t, float a, float b) => (b - a) * t + a;
    public static bool Active(Sequence.Animation a, int f) => f >= a.FrameStart && ((a.Flags & 0x8000) != 0 ? f < a.FrameEnd : f <= a.FrameEnd);
    public static (int x, int y) Pos(Sequence.Animation a, float t) {
        int x = IL(t, a.TranslateXStart, a.TranslateXEnd), y = IL(t, a.TranslateYStart, a.TranslateYEnd);
        if ((a.Flags & 0x10) == 0) {
            float tp = t * MathF.PI;
            float gx = (a.Flags & 8) != 0 ? MathF.Cos(a.BounceXCount * tp) : MathF.Sin(a.BounceXCount * tp);
            float gy = (a.Flags & 8) != 0 ? MathF.Cos(a.BounceYCount * tp) : MathF.Sin(a.BounceYCount * tp);
            x += (int)(gx * FL(t, a.BounceXStart, a.BounceXEnd));
            y += (int)(gy * FL(t, a.BounceYStart, a.BounceYEnd));
        }
        x += IL(t, a.PivotXStart, a.PivotXEnd);
        y += IL(t, a.PivotYStart, a.PivotYEnd);
        return (x, y);
    }
    public static uint Col(Sequence.Animation a, float t) {
        if ((a.Flags & 0x80) != 0) return (uint)a.ColorStart;
        uint o = 0, c0 = (uint)a.ColorStart, c1 = (uint)a.ColorEnd;
        for (int sh = 0; sh < 32; sh += 8) o |= ((uint)IL(t, (c0 >> sh) & 0xFF, (c1 >> sh) & 0xFF) & 0xFF) << sh;
        return o;
    }
}

// KH2's sequence player composes *animation groups*: each animation shows a sprite group translated / tinted over a
// frame range. Compose = the parts a group shows at frame f; the steady frame of a looping group is its LoopStart
// (after the pop-in slide), of a one-shot its last frame (the end state).
static class Seq {
    public class Piece { public Sequence.SpritePart q; public float dx, dy; public uint[] col; public int blend; }
    public static float[] C(uint c) => new[] { (c & 0xFF) / 128f, ((c >> 8) & 0xFF) / 128f, ((c >> 16) & 0xFF) / 128f, ((c >> 24) & 0xFF) / 128f };
    public static uint Mul(uint a, uint b) {   // per channel, 0x80 = 1.0
        uint o = 0;
        for (int i = 0; i < 32; i += 8) o |= Math.Min(255u, ((a >> i) & 0xFF) * ((b >> i) & 0xFF) / 128) << i;
        return o;
    }
    public static uint Lerp(uint a, uint b, float t) {
        uint o = 0;
        for (int i = 0; i < 32; i += 8) o |= (uint)Math.Clamp((int)MathF.Round(((a >> i) & 0xFF) + (((b >> i) & 0xFF) - (float)((a >> i) & 0xFF)) * t), 0, 255) << i;
        return o;
    }
    public static uint[] Corners(Sequence.Sprite sp, uint tint) => new[] { Mul((uint)sp.ColorLeft, tint), Mul((uint)sp.ColorTop, tint), Mul((uint)sp.ColorRight, tint), Mul((uint)sp.ColorBottom, tint) };
    public static int SteadyFrame(Sequence.AnimationGroup g) => g.DoNotLoop != 0 ? g.Animations.Max(a => (int)a.FrameEnd) : (int)g.LoopStart;
    public static List<Piece> Compose(Sequence s, int g, int f) {
        var grp = s.AnimationGroups[g]; var o = new List<Piece>();
        foreach (var a in grp.Animations) {
            int f0 = (int)a.FrameStart, f1 = (int)a.FrameEnd;
            if (f < f0 || f > f1) continue;
            // two animations of one sprite group meeting exactly at f (the cursor's 0-64 / 64-128): the later one wins
            if (f == f1 && f1 > f0 && grp.Animations.Any(b => b != a && b.SpriteGroupIndex == a.SpriteGroupIndex && (int)b.FrameStart == f && (int)b.FrameEnd > f)) continue;
            // the flags as OpenKh's SequenceRenderer reads them (reverse-engineered from the KH2 exe): 0x1 linear (else
            // sine ease), 0x1000 translation = Start only, 0x40 scaling off, 0x400 colour ignored (white), 0x80 colour =
            // Start only, 0x20 rotation off, 0x4000 pivot off, 0x10 bounce off
            int fl = a.Flags;
            float d = f1 > f0 ? (float)(f - f0) / (f1 - f0) : 0;
            float t = (fl & 0x1) != 0 ? d : (MathF.Sin(d * MathF.PI - MathF.PI / 2) + 1) / 2;
            float dx = (fl & 0x1000) != 0 ? a.TranslateXStart : a.TranslateXStart + (a.TranslateXEnd - a.TranslateXStart) * t;
            float dy = (fl & 0x1000) != 0 ? a.TranslateYStart : a.TranslateYStart + (a.TranslateYEnd - a.TranslateYStart) * t;
            if ((fl & 0x4000) == 0) { dx += a.PivotXStart + (a.PivotXEnd - a.PivotXStart) * t; dy += a.PivotYStart + (a.PivotYEnd - a.PivotYStart) * t; }
            float sc = (fl & 0x40) != 0 ? 1 : (a.ScaleStart + (a.ScaleEnd - a.ScaleStart) * t) * (a.ScaleXStart + (a.ScaleXEnd - a.ScaleXStart) * t);
            float rot = (fl & 0x20) != 0 ? 0 : a.RotationZStart + (a.RotationZEnd - a.RotationZStart) * t;
            if (Math.Abs(sc - 1) > 0.02f || rot != 0 || ((fl & 0x10) == 0 && (a.BounceXStart != 0 || a.BounceYStart != 0)))
                Console.WriteLine($"  (warning: anim group {g} sg {a.SpriteGroupIndex} at frame {f}: scale {sc:F2} rot {rot:F2} bounce {a.BounceXStart},{a.BounceYStart} flags {fl:X} ignored)");
            uint ac = (fl & 0x400) != 0 ? 0x80808080u : (fl & 0x80) != 0 ? (uint)a.ColorStart : Lerp((uint)a.ColorStart, (uint)a.ColorEnd, t);
            foreach (var q in s.SpriteGroups[a.SpriteGroupIndex])
                o.Add(new Piece { q = q, dx = dx, dy = dy, col = Corners(s.Sprites[q.SpriteIndex], ac), blend = (int)a.ColorBlend });
        }
        return o;
    }
}

// minimal PNG writer (RGBA8, no filter)
static class Png {
    static uint[] crcTable;
    static uint Crc(byte[] b, int off, int n) {
        if (crcTable == null) { crcTable = new uint[256]; for (uint i = 0; i < 256; i++) { uint c = i; for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crcTable[i] = c; } }
        uint crc = 0xFFFFFFFFu; for (int i = 0; i < n; i++) crc = crcTable[(crc ^ b[off + i]) & 0xFF] ^ (crc >> 8); return crc ^ 0xFFFFFFFFu;
    }
    static void Chunk(BinaryWriter w, string type, byte[] data) {
        var t = Encoding.ASCII.GetBytes(type);
        var buf = new byte[4 + data.Length]; Array.Copy(t, buf, 4); Array.Copy(data, 0, buf, 4, data.Length);
        w.Write(BE((uint)data.Length)); w.Write(buf); w.Write(BE(Crc(buf, 0, buf.Length)));
    }
    static byte[] BE(uint v) => new byte[] { (byte)(v >> 24), (byte)(v >> 16), (byte)(v >> 8), (byte)v };
    public static byte[] Read(string path, out int w, out int h) {
        var d = File.ReadAllBytes(path);
        int p = 8; w = h = 0; int bpp = 4; byte ct = 6;
        var idat = new MemoryStream();
        while (p + 8 <= d.Length) {
            int len = (d[p] << 24) | (d[p + 1] << 16) | (d[p + 2] << 8) | d[p + 3];
            string type = Encoding.ASCII.GetString(d, p + 4, 4);
            if (Environment.GetEnvironmentVariable("HUDBAKE_PNGDBG") != null) Console.WriteLine($"  chunk {type} at {p} len {len} of {d.Length}");
            if (type == "IHDR") { w = (d[p + 8] << 24) | (d[p + 9] << 16) | (d[p + 10] << 8) | d[p + 11]; h = (d[p + 12] << 24) | (d[p + 13] << 16) | (d[p + 14] << 8) | d[p + 15]; ct = d[p + 17]; if (d[p + 16] != 8) throw new Exception("png: not 8-bit"); bpp = ct == 6 ? 4 : ct == 2 ? 3 : throw new Exception("png: colour type " + ct); }
            else if (type == "IDAT") idat.Write(d, p + 8, len);
            else if (type == "IEND") break;   // (the KH2 PC PNGs have junk after IEND)
            p += 12 + len;
        }
        Console.WriteLine($"png {path}: {w}x{h} ct {ct} idat {idat.Length}");
        idat.Position = 0;
        var raw = new byte[(w * bpp + 1) * h];
        using (var z = new System.IO.Compression.ZLibStream(idat, System.IO.Compression.CompressionMode.Decompress)) { int got = 0; while (got < raw.Length) { int n = z.Read(raw, got, raw.Length - got); if (n <= 0) break; got += n; } }
        var px = new byte[w * h * bpp];
        int stride = w * bpp;
        for (int y = 0; y < h; y++) {
            int ft = raw[y * (stride + 1)];
            for (int x = 0; x < stride; x++) {
                int a = x >= bpp ? px[y * stride + x - bpp] : 0, b = y > 0 ? px[(y - 1) * stride + x] : 0, c = (x >= bpp && y > 0) ? px[(y - 1) * stride + x - bpp] : 0;
                int v = raw[y * (stride + 1) + 1 + x];
                switch (ft) { case 1: v += a; break; case 2: v += b; break; case 3: v += (a + b) / 2; break; case 4: { int pp = a + b - c, pa = Math.Abs(pp - a), pb = Math.Abs(pp - b), pc = Math.Abs(pp - c); v += (pa <= pb && pa <= pc) ? a : pb <= pc ? b : c; break; } }
                px[y * stride + x] = (byte)v;
            }
        }
        if (bpp == 4) return px;
        var o = new byte[w * h * 4];
        for (int i = 0; i < w * h; i++) { o[i * 4] = px[i * 3]; o[i * 4 + 1] = px[i * 3 + 1]; o[i * 4 + 2] = px[i * 3 + 2]; o[i * 4 + 3] = 255; }
        return o;
    }
    public static void Write(string path, int w, int h, byte[] rgba) {
        using var f = new BinaryWriter(File.Create(path));
        f.Write(new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 });
        var ihdr = new byte[13]; Array.Copy(BE((uint)w), 0, ihdr, 0, 4); Array.Copy(BE((uint)h), 0, ihdr, 4, 4); ihdr[8] = 8; ihdr[9] = 6;
        Chunk(f, "IHDR", ihdr);
        var raw = new byte[(w * 4 + 1) * h];
        for (int y = 0; y < h; y++) { raw[y * (w * 4 + 1)] = 0; Array.Copy(rgba, y * w * 4, raw, y * (w * 4 + 1) + 1, w * 4); }
        var ms = new MemoryStream();
        using (var z = new System.IO.Compression.ZLibStream(ms, System.IO.Compression.CompressionLevel.Optimal, true)) z.Write(raw, 0, raw.Length);
        Chunk(f, "IDAT", ms.ToArray());
        Chunk(f, "IEND", new byte[0]);
    }
}

// (s57 party) the previews' readers
class PtBin {
    public byte[] b; public int ng, nf, nface, fw, fh, fps2, slotdx, slotdy, pitch, g0, f0, p0, face0;
    public PtBin(string path) {
        b = File.ReadAllBytes(path);
        if (Encoding.ASCII.GetString(b, 0, 4) != "SKPT") throw new Exception("not SKPT");
        int I(int o) => BitConverter.ToInt32(b, o);
        ng = I(8); nf = I(12); nface = I(16); fw = I(20); fh = I(24); fps2 = I(28); slotdx = I(40); slotdy = I(44); pitch = I(48);
        g0 = 56; f0 = g0 + ng * 16; p0 = f0 + nf * 20; face0 = b.Length - nface * fw * fh * 4;
    }
    public int Grp(int id) { for (int i = 0; i < ng; i++) if (BitConverter.ToUInt16(b, g0 + i * 16) == id) return i; return -1; }
    public int N(int gi) => BitConverter.ToUInt16(b, g0 + gi * 16 + 4);
    public int Frame(int gi, int f) {  // the frame index (looping / holding as sora_hud.c pt_frame)
        int n = N(gi), loop = BitConverter.ToUInt16(b, g0 + gi * 16 + 6), first = BitConverter.ToInt32(b, g0 + gi * 16 + 8);
        if (f >= n) f = loop == 0xFFFF ? n - 1 : loop + (f - loop) % (n - loop);
        return first + f;
    }
    public (float ax, float ay) Anchor(int fi) => (BitConverter.ToSingle(b, f0 + fi * 20 + 8), BitConverter.ToSingle(b, f0 + fi * 20 + 12));
}
class PtCanvas {
    public float[] cv; public int W, H; public float unit, ox, oy;   // ox, oy: the current origin (px)
    public PtCanvas(int w, int h) { W = w; H = h; cv = new float[w * h * 4]; }
    // one quad: corners (HUD units around the origin) TL TR BR BL, uv rect (texels), flat colour, an optional clip on
    // the HUD-unit position around (cx, cy)
    public void Quad(byte[] img, int ioff, int iw, int ih, float[] X, float[] Y, float u0, float v0, float u1, float v1, float[] col, Func<float, float, bool> keep = null, float cx = 0, float cy = 0) {
        float minx = X.Min() * unit + ox, maxx = X.Max() * unit + ox, miny = Y.Min() * unit + oy, maxy = Y.Max() * unit + oy;
        float ex = X[1] - X[0], ey = Y[1] - Y[0], fx = X[3] - X[0], fy = Y[3] - Y[0], det = ex * fy - ey * fx;
        if (Math.Abs(det) < 1e-6f) return;
        for (int py = Math.Max((int)MathF.Floor(miny), 0); py < Math.Min((int)MathF.Ceiling(maxy), H); py++)
            for (int px = Math.Max((int)MathF.Floor(minx), 0); px < Math.Min((int)MathF.Ceiling(maxx), W); px++) {
                float lx = (px + 0.5f - ox) / unit, ly = (py + 0.5f - oy) / unit, dx = lx - X[0], dy = ly - Y[0];
                float s = (dx * fy - dy * fx) / det, t = (ex * dy - ey * dx) / det;
                if (s < 0 || s >= 1 || t < 0 || t >= 1) continue;
                if (keep != null && !keep(lx - cx, ly - cy)) continue;
                int tx = Math.Clamp((int)(u0 + s * (u1 - u0)), 0, iw - 1), ty = Math.Clamp((int)(v0 + t * (v1 - v0)), 0, ih - 1);
                int ti = ioff + (ty * iw + tx) * 4, ci = (py * W + px) * 4;
                float a = img[ti + 3] / 255f * Math.Min(col[3], 1);
                for (int k = 0; k < 3; k++) cv[ci + k] = cv[ci + k] * (1 - a) + Math.Min(img[ti + k] / 255f * col[k], 1) * a;
            }
    }
}

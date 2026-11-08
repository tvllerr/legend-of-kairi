Legend of Kairi (Sora in BotW) - third-party licenses
======================================================

The installer kit ships these programs and libraries. Each one keeps its own license; the full texts are in this folder.
No game files are included: everything is built on your PC from your own copies of the games.

Component                                   License                    File(s) here
------------------------------------------  -------------------------  ---------------------------------------------
Savage Wind (the BotW recompilation)        MIT                        SavageWind-MIT.txt
OpenKH (KH2 file tools, DLLs in openkh/)    Apache-2.0                 OpenKH-Apache-2.0.txt, OpenKH-NOTICE.txt
vgmstream-cli r2117 (vgmstream/)            ISC-style (vgmstream)      vgmstream-COPYING.txt
  its bundled libraries: FFmpeg libs (avcodec/avformat/avutil/swresample) LGPL-2.1+, libmpg123 LGPL-2.1,
  libvorbis BSD-3-Clause, libspeex BSD-3-Clause, libcelt BSD-2-Clause, libatrac9 MIT, libg719_decode (ITU-T / Polycom
  reference decoder terms)                                             LGPL-2.1.txt (for the LGPL ones)
FFmpeg 9.0.2 (ffmpeg/ffmpeg.exe, Windows    GPL-3.0                    GPL-3.0.txt
  build by Gyan Doshi)
BusyBox-w32 by Ron Yorston (busybox/sh.exe) GPL-2.0                    GPL-2.0.txt
Python 3.12 embeddable (python/)            PSF License                Python-LICENSE.txt
NumPy                                       BSD-3-Clause AND 0BSD AND  numpy-LICENSE.txt
                                              MIT AND Zlib AND CC0-1.0
Pillow                                      MIT-CMU (HPND)             pillow-LICENSE.txt
oead by leoetlino                           GPL-2.0-or-later           oead-LICENSE.md, GPL-2.0.txt
.NET 8 runtime (dotnet/)                    MIT (+ third-party)        dotnet-LICENSE.txt, dotnet-ThirdPartyNotices.txt
WebView2 SDK (installer window)             BSD-3-Clause (Microsoft)   WebView2-LICENSE.txt, WebView2-NOTICE.txt
Fira Sans (installer font)                  SIL Open Font License 1.1  FiraSans-OFL.txt

Source code of the GPL / LGPL programs:
- FFmpeg: https://ffmpeg.org/download.html (build: https://www.gyan.dev/ffmpeg/builds/, release 9.0.2)
- BusyBox-w32: https://frippery.org/busybox/ (source: https://github.com/rmyorston/busybox-w32)
- vgmstream and its libraries: https://github.com/vgmstream/vgmstream (release r2117)
- oead: https://github.com/zeldamods/oead

Credits: tvllerr (the mod); a lot of ideas came from PHH by YG123, a Borderlands 2 mod where you play as Sora from KH2 in a
similar way; the Savage Wind team; OpenKH; vgmstream; FFmpeg; BusyBox-w32; Python, NumPy, Pillow, oead; .NET; WebView2; Fira Sans.

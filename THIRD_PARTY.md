# Official RetroArch Vita payload

The VitaRetro repository does not contain a RetroArch binary, core, ROM, or BIOS.
The cloud build fetches RetroArch 1.22.2 Vita VPK and data directly from
https://buildbot.libretro.com/stable/1.22.2/playstation/vita/ .

RetroArch and its individual cores are separate upstream works with their own
licenses. The VitaRetro MIT license applies only to VitaRetro's original source
and assets; it does not relicense the downloaded executables or data. Source,
notices, and core-specific licenses are published by Libretro at
https://github.com/libretro/RetroArch and https://github.com/libretro .

The Vita pairing QR renderer includes Project Nayuki's QR Code generator C
implementation in `src/vendor/qrcodegen.c` and `src/vendor/qrcodegen.h`.
It is Copyright (c) Project Nayuki and distributed under the MIT license
whose full notice appears at the top of each vendored file:
https://github.com/nayuki/QR-Code-generator .

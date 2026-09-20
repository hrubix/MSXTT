// MSXTT2.ROM — debug ROM using the shared canonical UNAPI client
// Build: build-msxtt2-rom.bat  →  node build.js projname=msxttrom2

DoRun = false;

ProjName = "msxttrom2";
ProjModules = [
	"src/tt/main", "src/tt/net_rom", "src/tt/parse",
	"src/tt/splash", "src/tt/gfx", "src/tt/gfx_scr5", "src/tt/gfx_scr2",
	"src/tt/tt_font"
];
LibModules = [ "system", "bios", "memory", "vdp" ];
AddSources = [
	"src/tt/unapi_tcp_slim.asm",
	"src/tt/scr2_expand.s"
];

Machine = "12";
Target = "ROM_16K_P2";
CheckVersion = false;
CustomISR = "NONE";
DOSParseArg = false;
ROMDelayBoot = true;
AddROMSignature = false;
ForceRamAddr = 0xD180;
Optim = "Size";
CompileOpt = "-DTT_SLIM=1 -DTT_ROM_DBG=1";

DiskFiles = [];
PostBuildScripts = [];

AppSignature = false;

Verbose = true;

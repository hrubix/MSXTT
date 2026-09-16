// MSXTT.ROM — combined Screen 2/5 NOS Teletekst (page 2 at 8000h, direct HTTPS)
// Build: build-msxtt-rom.bat  →  node build.js projname=msxttrom
// Do not add this to build.bat (DOS2 COM stays separate).

DoRun = false;

ProjName = "msxttrom";
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
ForceRamAddr = 0xC200;
Optim = "Size";
CompileOpt = "-DTT_SLIM=1";

DiskFiles = [];
PostBuildScripts = [];

AppSignature = false;

Verbose = true;

// MSXTT3.ROM — canonical ROM UNAPI discovery with splash diagnostics
// Build: build-msxtt3-rom.bat  →  node build.js projname=msxttrom3
// (ROM cart artifact — not DOS .COM; findings apply to bare-ROM UNAPI.)

DoRun = false;

ProjName = "msxttrom3";
ProjModules = [
	"src/tt/main", "src/tt/net_rom", "src/tt/parse",
	"src/tt/splash", "src/tt/gfx", "src/tt/gfx_scr5", "src/tt/gfx_scr2",
	"src/tt/tt_font"
];
LibModules = [ "system", "bios", "memory", "vdp" ];
AddSources = [
	"src/tt/rom_entry.asm",
	"src/tt/unapi_tcp_slim.asm",
	"src/tt/scr2_expand.s"
];

Machine = "12";
Target = "ROM_16K_P2";
CheckVersion = false;
CustomISR = "NONE";
DOSParseArg = false;
ROMDelayBoot = false; /* match TELNET ROM: run at cart INIT so Pico EXTBIO is not lost after Nextor */
AddROMSignature = false;
ForceRamAddr = 0xD180;
Optim = "Size";
CompileOpt = "-DTT_SLIM=1 -DTT_ROM_DBG=1 -DTT_ROM_SAFE_STACK=1";

DiskFiles = [];
PostBuildScripts = [];

AppSignature = false;

Verbose = true;

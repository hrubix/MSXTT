// MSXTTPP — Pico+ combined MSX1/MSX2 Teletekst (direct TLS, no TT.CFG)
// Build: build-msxttpp.bat  →  node build.js projname=msxttpp
// Do not add this to build.bat (emulator COM stays separate).

DoRun = false;

ProjName = "msxttpp";
ProjModules = [
	"src/tt/main", "src/tt/net_pico", "src/tt/parse",
	"src/tt/splash", "src/tt/gfx", "src/tt/gfx_scr5", "src/tt/gfx_scr2",
	"src/tt/tt_font"
];
LibModules = [ "system", "bios", "memory", "vdp", "dos" ];
AddSources = [
	"src/tt/unapi_tcp_slim.asm",
	"src/tt/scr2_expand.s"
];

Machine = "12";
Target = "DOS2";
CheckVersion = false;
CustomISR = "NONE";
DOSParseArg = false;
Optim = "Size";

CompileOpt = "-DTT_SLIM=1";

DiskFiles = [];

AppSignature = false;

PostBuildScripts = [];

Verbose = true;

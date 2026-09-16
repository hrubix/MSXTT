// MSXTT — slim combined MSX1/MSX2 Teletekst (HTTP via TT.CFG)
// Build: build.bat  →  node build.js projname=msxtt

DoRun = false;

ProjName = "msxtt";
ProjModules = [
	"src/tt/main", "src/tt/net", "src/tt/parse",
	"src/tt/splash", "src/tt/gfx", "src/tt/gfx_scr5", "src/tt/gfx_scr2",
	"src/tt/tt_font"
];
LibModules = [ "system", "bios", "memory", "vdp", "dos" ];
AddSources = [
	"MSXgl/engine/src/network/unapi_tcp.asm",
	"src/tt/scr2_expand.s"
];

Machine = "12";
Target = "DOS2";
CheckVersion = false;
CustomISR = "NONE";
DOSParseArg = false;
Optim = "Size";

CompileOpt = "-DTT_SLIM=1";

DiskFiles = [ "disk/AUTOEXEC.BAT", "disk/TT.CFG", "disk/UNAPI.COM" ];

AppSignature = false;

PostBuildScripts = [];

Verbose = true;

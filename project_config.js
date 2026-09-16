// Default MSXgl config (required by build.js). Overlay: msxtt.js
// Build: build.bat → projname=msxtt

DoRun = false;

ProjName = "msxtt";
ProjModules = [
	"src/tt/main", "src/tt/net", "src/tt/parse",
	"src/tt/splash", "src/tt/gfx", "src/tt/gfx_scr5", "src/tt/gfx_scr2"
];
LibModules = [ "system", "bios", "memory", "vdp", "keyboard", "dos" ];
AddSources = [
	"MSXgl/engine/src/network/unapi_tcp.asm",
	"src/tt/scr2_expand.s"
];

Machine = "12";
Target = "DOS2";
CheckVersion = false;
CustomISR = "NONE";
DOSParseArg = false;

DiskFiles = [ "disk/AUTOEXEC.BAT", "disk/TT.CFG", "disk/UNAPI.COM" ];

AppSignature = true;
AppCompany = "NS";
AppID = "TT";

PostBuildScripts = [];

Verbose = true;

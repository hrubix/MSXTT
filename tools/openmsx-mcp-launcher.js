/**
 * openMSX launcher for mcp-openmsx on Windows.
 *
 * Node/CreateProcess exits openMSX immediately (code 1) in this environment,
 * while ShellExecute (`cmd /c start`) works. mcp-openmsx waits for
 * %TEMP%\openmsx-default\socket.<spawned-pid>, so this wrapper:
 *   1) starts openMSX via ShellExecute
 *   2) finds the new openMSX PID + socket
 *   3) mirrors the socket file under this wrapper's PID
 *   4) stays alive until killed, then terminates openMSX
 */
const { spawn, execSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const OPENMSX_EXE = process.env.OPENMSX_REAL_EXECUTABLE
  || 'C:\\Program Files\\openMSX\\openmsx.exe';
const OPENMSX_CWD = path.dirname(OPENMSX_EXE);
const wrapperPid = process.pid;
const args = process.argv.slice(2);
const tempDir = process.env.TEMP || process.env.TMP || os.tmpdir();
const socketDir = path.join(tempDir, 'openmsx-default');

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function listOpenmsxPids() {
  try {
    const out = execSync('tasklist /FI "IMAGENAME eq openmsx.exe" /FO CSV /NH', {
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'ignore'],
    });
    const pids = [];
    for (const line of out.split(/\r?\n/)) {
      const m = line.match(/^"openmsx\.exe","(\d+)"/i);
      if (m) pids.push(Number(m[1]));
    }
    return pids;
  } catch {
    return [];
  }
}

function log(msg) {
  // mcp-openmsx reads stderr diagnostics while connecting
  process.stderr.write(`[openmsx-mcp-launcher] ${msg}\n`);
}

async function main() {
  if (!fs.existsSync(OPENMSX_EXE)) {
    process.stderr.write(`Error: openMSX not found: ${OPENMSX_EXE}\n`);
    process.exit(1);
  }

  fs.mkdirSync(socketDir, { recursive: true });
  const before = new Set(listOpenmsxPids());

  // ShellExecute path that actually starts openMSX on this PC
  const child = spawn('cmd.exe', ['/c', 'start', '', OPENMSX_EXE, ...args], {
    cwd: OPENMSX_CWD,
    stdio: 'ignore',
    windowsHide: true,
    detached: true,
  });
  child.unref();
  log(`started via ShellExecute args=[${args.join(' ')}] wrapperPid=${wrapperPid}`);

  let omsxPid = null;
  for (let i = 0; i < 50; i++) {
    await sleep(100);
    const neu = listOpenmsxPids().filter((p) => !before.has(p));
    if (neu.length) {
      omsxPid = neu[neu.length - 1];
      break;
    }
  }
  if (!omsxPid) {
    process.stderr.write('Error: openMSX process did not appear after ShellExecute\n');
    process.exit(1);
  }
  log(`openMSX pid=${omsxPid}`);

  const srcSocket = path.join(socketDir, `socket.${omsxPid}`);
  const dstSocket = path.join(socketDir, `socket.${wrapperPid}`);

  let port = null;
  for (let i = 0; i < 80; i++) {
    if (fs.existsSync(srcSocket)) {
      try {
        port = fs.readFileSync(srcSocket, 'utf8').trim();
        if (port) break;
      } catch {
        // retry
      }
    }
    await sleep(100);
  }
  if (!port) {
    process.stderr.write(`Error: openMSX socket not found: ${srcSocket}\n`);
    try {
      execSync(`taskkill /PID ${omsxPid} /F`, { stdio: 'ignore' });
    } catch { /* ignore */ }
    process.exit(1);
  }

  fs.writeFileSync(dstSocket, `${port}\n`, 'utf8');
  log(`mirrored socket ${srcSocket} -> ${dstSocket} port=${port}`);

  const cleanup = () => {
    try {
      if (fs.existsSync(dstSocket)) fs.unlinkSync(dstSocket);
    } catch { /* ignore */ }
    try {
      execSync(`taskkill /PID ${omsxPid} /F`, { stdio: 'ignore' });
    } catch { /* ignore */ }
  };

  process.on('exit', cleanup);
  process.on('SIGINT', () => {
    cleanup();
    process.exit(0);
  });
  process.on('SIGTERM', () => {
    cleanup();
    process.exit(0);
  });

  // Stay alive while openMSX runs (mcp-openmsx owns this process handle)
  for (;;) {
    await sleep(1000);
    const still = listOpenmsxPids().includes(omsxPid);
    if (!still) {
      log('openMSX exited; launcher ending');
      cleanup();
      process.exit(0);
    }
  }
}

main().catch((err) => {
  process.stderr.write(`Error: ${err && err.stack ? err.stack : err}\n`);
  process.exit(1);
});

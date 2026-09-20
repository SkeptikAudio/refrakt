// GUI fit regression check -- Skeptik Audio plugins.   Run:  node Tests/gui_fit_check.js
//
// Guards against the plugin GUI being CROPPED, RESCALED WRONG or SCROLLING
// inside the host window. Loads the real Source/ui/public/index.html in headless
// Edge (same Chromium as the WebView2 the plugin ships in) and checks, via the
// DevTools protocol:
//
//   PART A -- static viewports. At a range of viewport sizes (natural size,
//     narrower, shorter, much bigger, tiny):
//       1. the design-size root's LAYOUT size never changes (nothing reflows --
//          a flex item that shrinks in a narrow viewport was the original bug),
//       2. what is DRAWN always fits inside the viewport (nothing cropped),
//       3. at/above natural size it is drawn at scale 1,
//       4. no scrollbar can appear (overflow hidden on the page).
//   PART B -- host simulation. For several JUCE-logical -> CSS pixel ratios
//     (the WebView2/JUCE DPI mismatch: 1.0, 0.865, 1.15, 0.5, 2.0, ...), replay
//     what the native side does: the page reports its size, the native size
//     formula (mirrored below in CONFIG.nativeSize -- KEEP IN SYNC with
//     handleReportContentSize in PluginEditor.cpp) picks a new window size, and the
//     viewport is resized to match. After that correction the GUI must fit
//     exactly: nothing cropped, drawn at scale 1, no dead space.
//
// Needs Microsoft Edge (Windows) and Node 22+ (global WebSocket/fetch); skips
// itself (exit 0) where Edge is missing, e.g. on macOS CI.
//
// ---- per-plugin configuration -------------------------------------------------
const CONFIG = {
  page: '../Source/ui/public/index.html',
  root: '.plugin',                 // element whose LAYOUT size is the design size
  fitVar: 'fitZoom',               // page global holding the current fit factor
  reportName: 'reportContentSize', // native function name the page calls to report its size
  initialLogical: { w: 940, h: 760 }, // the editor's baseWidth/baseHeight
  deadSpaceTolerance: 6,           // px of leftover background allowed after the window correction (text row heights can differ 2-4px with the viewport the page loaded at)
  layoutHeightTolerance: 12,       // px: layout height may drift this much when the page scales via CSS zoom (text rounding); 0 for transform-based scaling
  // Mirror of the native handleReportContentSize: given the reported args and the
  // window's current logical size, return the new logical size.
  nativeSize(args, L) {
    const [contentH, vw, vh] = args;          // Refrakt reports (contentH, viewportW, viewportH)
    const kDesignWidth = 940;                 // RefraktEditor::kDesignWidth
    const rw = vw / L.w, rh = vh / L.h;
    return { w: Math.ceil(kDesignWidth / rw - 1e-6), h: Math.ceil(contentH / rh - 1e-6) };
  },
  // Optional: also run Part A with the page in "embedded" mode is automatic (a
  // window.__JUCE__ stub is always injected, as in the real plugin).
};
// -------------------------------------------------------------------------------

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const os = require('os');

const EDGE = [
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  'C:/Program Files/Microsoft/Edge/Application/msedge.exe',
].find(p => fs.existsSync(p));
if (!EDGE) { console.log('SKIP: Microsoft Edge not found'); process.exit(0); }

const page = 'file:///' + path.resolve(__dirname, CONFIG.page).replace(/\\/g, '/');
const port = 9300 + Math.floor(Math.random() * 500);
const userDir = fs.mkdtempSync(path.join(os.tmpdir(), 'skeptik-fit-'));
const edge = spawn(EDGE, ['--headless=new', '--disable-gpu', `--remote-debugging-port=${port}`,
  `--user-data-dir=${userDir}`, 'about:blank'], { stdio: 'ignore' });

// Kill Edge's WHOLE process tree (renderer/gpu/crashpad children too) -- edge.kill() alone
// only kills the parent and leaves hundreds of orphaned processes after many runs.
function killEdge() {
  const cp = require('child_process');
  try { cp.execSync(`taskkill /pid ${edge.pid} /T /F`, { stdio: 'ignore' }); }
  catch (e) { try { edge.kill(); } catch (e2) {} }
  // Some helpers (crash reporter etc.) are detached from the tree: also kill anything
  // still using this run's private profile folder.
  try {
    const ps = "Get-CimInstance Win32_Process -Filter \"Name='msedge.exe'\" | Where-Object { $_.CommandLine -like '*" + path.basename(userDir) + "*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }";
    cp.execFileSync('powershell', ['-NoProfile', '-Command', ps], { stdio: 'ignore' });
  } catch (e) {}
}
const sleep = ms => new Promise(r => setTimeout(r, ms));
let failures = 0;
function check(name, ok, info) { if (!ok) { failures++; console.log('FAIL', name, info ?? ''); } else console.log('ok  ', name); }

(async () => {
  let targets;
  for (let i = 0; i < 50; i++) {
    try { targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json(); if (targets.length) break; } catch (e) {}
    await sleep(200);
  }
  const ws = new WebSocket(targets.find(t => t.type === 'page').webSocketDebuggerUrl);
  await new Promise(r => ws.addEventListener('open', r));
  let id = 0; const pending = new Map();
  ws.addEventListener('message', m => { const d = JSON.parse(m.data); if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); } });
  const send = (method, params = {}) => new Promise(r => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
  const evalJs = async expr => (await send('Runtime.evaluate', { expression: expr, returnByValue: true })).result.result.value;
  const setViewport = (w, h) => send('Emulation.setDeviceMetricsOverride', { width: Math.round(w), height: Math.round(h), deviceScaleFactor: 1, mobile: false });

  await send('Page.enable');
  // Stub of the JUCE bridge: makes the page believe it is inside the plugin
  // (embedded mode) and records every native call the page makes.
  await send('Page.addScriptToEvaluateOnNewDocument', { source:
    `window.__reports=[];window.__JUCE__={backend:{emitEvent(n,p){if(p&&p.name==='${CONFIG.reportName}')window.__reports.push(p.params)},addEventListener(){}}};` });

  const measure = `(() => { const p = document.querySelector('${CONFIG.root}'); const r = p.getBoundingClientRect();
    return { lw: p.offsetWidth, lh: p.offsetHeight, right: r.right, bottom: r.bottom, vw: innerWidth, vh: innerHeight, fit: ${CONFIG.fitVar},
             ovH: getComputedStyle(document.documentElement).overflow, ovB: getComputedStyle(document.body).overflow,
             reports: window.__reports.slice() }; })()`;

  async function load(vw, vh) {
    await setViewport(vw, vh);
    await send('Page.navigate', { url: page });
    await sleep(1800);
  }

  // ---------------- PART A ----------------
  console.log('--- Part A: static viewports');
  await load(1400, 1000);
  const nat = await evalJs(measure);
  check('natural layout size measured', nat.lw > 0 && nat.lh > 0, `${nat.lw}x${nat.lh}`);

  const sizes = [[nat.lw, nat.lh], [nat.lw, nat.lh + 40], [nat.lw + 80, nat.lh + 80], [nat.lw * 1.5, nat.lh * 1.5],
                 [nat.lw * 0.87, nat.lh + 40], [nat.lw * 0.87, nat.lh * 0.87], [nat.lw * 0.6, nat.lh * 0.6],
                 [nat.lw, nat.lh * 0.7], [nat.lw * 0.4, nat.lh * 1.2]].map(([w, h]) => [Math.round(w), Math.round(h)]);
  for (const [w, h] of sizes) {
    await setViewport(w, h);
    await sleep(400);
    const m = await evalJs(measure);
    const tag = `${w}x${h}`;
    check(`${tag}: layout size unchanged (no reflow / flex shrink)`, Math.abs(m.lw - nat.lw) <= 0 && Math.abs(m.lh - nat.lh) <= CONFIG.layoutHeightTolerance, `${m.lw}x${m.lh} vs ${nat.lw}x${nat.lh}`);
    check(`${tag}: drawn GUI fits inside viewport (nothing cropped)`, m.right <= w + 0.5 && m.bottom <= h + 0.5, `right ${m.right.toFixed(1)} bottom ${m.bottom.toFixed(1)}`);
    // overflow:hidden on html+body guarantees no scrollbar (clientHeight etc. are
    // meaningless here: pages without a doctype render in quirks mode).
    check(`${tag}: no scrollbar (overflow hidden)`, m.ovH === 'hidden' || m.ovB === 'hidden', `${m.ovH}/${m.ovB}`);
    if (w >= nat.lw && h >= nat.lh) check(`${tag}: drawn at natural scale`, m.fit > 0.999 && m.fit < 1.001, m.fit);
  }

  // ---------------- PART B ----------------
  console.log('--- Part B: host window/DPI simulation');
  for (const ratio of [1.0, 0.865, 1.15, 0.5, 0.75, 1.5, 2.0]) {
    let L = { ...CONFIG.initialLogical };
    await load(L.w * ratio, L.h * ratio);
    let m = await evalJs(measure);
    let applied = 0;
    for (let i = 0; i < m.reports.length; i++) {           // replay each report like the native side
      const args = m.reports[i];
      const next = CONFIG.nativeSize(args, L);
      if (next.w > 0 && next.h > 0 && (next.w !== L.w || next.h !== L.h)) { L = next; applied++; }
    }
    await setViewport(L.w * ratio, L.h * ratio);
    await sleep(600);
    m = await evalJs(measure);
    const tag = `ratio ${ratio}`;
    check(`${tag}: page reported its size`, m.reports.length >= 1, m.reports.length);
    check(`${tag}: GUI fits after correction (nothing cropped)`, m.right <= m.vw + 0.5 && m.bottom <= m.vh + 0.5,
          `drawn ${m.right.toFixed(1)}x${m.bottom.toFixed(1)} in ${m.vw}x${m.vh}`);
    // 0.5% tolerance: viewport sizes are whole pixels, so the DPI ratio the native side
    // derives has ~1px of quantisation error (worst at tiny/extreme ratios); the fit safety
    // net then absorbs it with a sub-percent scale instead of cropping.
    check(`${tag}: drawn at natural scale after correction (within 0.5%)`, m.fit > 0.995, m.fit);
    check(`${tag}: no dead space after correction (<=${CONFIG.deadSpaceTolerance}px)`, m.vw - m.right <= CONFIG.deadSpaceTolerance && m.vh - m.bottom <= CONFIG.deadSpaceTolerance,
          `dead ${(m.vw - m.right).toFixed(1)}x${(m.vh - m.bottom).toFixed(1)}`);
  }

  ws.close(); killEdge();
  await sleep(500);
  try { fs.rmSync(userDir, { recursive: true, force: true }); } catch (e) {}
  console.log(failures ? `FAILURES: ${failures}` : 'ALL PASS');
  process.exit(failures ? 1 : 0);
})().catch(e => { console.error(e); killEdge(); process.exit(1); });

/* Vord C5 Serial Monitor (Web Serial) — live device log viewer.
   Independent of the flasher: it opens its own connection to an
   already-flashed board so you can watch boot output, the AP address,
   and the heap-health log.

   When a WROOM-32 classic-BT scout is wired to the C5, the C5 relays the
   scout's UART traffic to its USB console tagged [wroom] (and logs its own
   scout-link activity as [bt_uart]). The Source filter below lets you view the
   C5's own output, just the scout's relayed traffic, or both combined. */

const $ = (id) => document.getElementById(id);

let port = null;
let reader = null;
let keepReading = false;
let readLoopDone = null;
const decoder = new TextDecoder();

// Line-buffered log with per-line source tags so the Source filter can show
// C5 logs, the relayed WROOM-32 scout traffic, or both. Device output is
// line-oriented (println), so we buffer a partial chunk until its newline,
// classify the completed line, then retain + display it. Retaining lines lets
// the filter re-render the whole view retroactively when it changes.
const MAX_LINES = 4000;
let lines = [];       // { src: 'c5' | 'wroom' | 'meta', text: string (incl. newline) }
let lineBuf = "";

function currentFilter() {
  const f = $("mon-filter");
  return f ? f.value : "all";
}

// 'meta' = our own connect/disconnect notices: always visible.
function passes(src) {
  if (src === "meta") return true;
  const f = currentFilter();
  return f === "all" || f === src;
}

// Scout traffic the C5 relays is prefixed [wroom]; the C5's own scout-link
// logs use [bt_uart]. Both are classified as the scout source so "WROOM-32
// only" shows the full picture of the link. Anything else is the C5 itself.
function classify(line) {
  return /^\s*\[(wroom|bt_uart)\]/.test(line) ? "wroom" : "c5";
}

// Low-level scroll-aware append; keeps the view pinned to the bottom only if
// the user is already there, and bounds the on-screen buffer.
function appendOut(text) {
  const el = $("mon-output");
  if (!el) return;
  const atBottom = el.scrollTop + el.clientHeight >= el.scrollHeight - 6;
  el.textContent += text;
  if (el.textContent.length > 120000) el.textContent = el.textContent.slice(-90000);
  if (atBottom) el.scrollTop = el.scrollHeight;
}

// Rebuild the whole view from retained lines under the current filter.
function render() {
  const el = $("mon-output");
  if (!el) return;
  el.textContent = lines.filter((l) => passes(l.src)).map((l) => l.text).join("");
  el.scrollTop = el.scrollHeight;
}

function pushLine(src, text) {
  lines.push({ src, text });
  if (lines.length > MAX_LINES) lines = lines.slice(-MAX_LINES);
  if (passes(src)) appendOut(text);
}

// Feed raw device bytes: split into complete lines, classify, retain + show.
function feed(chunk) {
  lineBuf += chunk;
  let nl;
  while ((nl = lineBuf.indexOf("\n")) >= 0) {
    const line = lineBuf.slice(0, nl + 1);   // keep the trailing newline
    lineBuf = lineBuf.slice(nl + 1);
    pushLine(classify(line), line);
  }
}

// Our own notices (connect/disconnect/errors) — shown regardless of filter.
function meta(text) {
  pushLine("meta", text);
}

function status(msg) {
  const s = $("mon-status");
  if (s) s.textContent = msg;
}

function setConnected(on) {
  $("mon-connect").hidden = on;
  $("mon-disconnect").hidden = !on;
}

async function readLoop() {
  while (port && port.readable && keepReading) {
    reader = port.readable.getReader();
    try {
      for (;;) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value) feed(decoder.decode(value, { stream: true }));
      }
    } catch (e) {
      meta("\n[read error] " + e.message + "\n");
    } finally {
      try { reader.releaseLock(); } catch (_e) {}
      reader = null;
    }
  }
}

async function connect() {
  if (!("serial" in navigator)) {
    alert("Web Serial requires Chrome, Edge, or Opera on desktop.");
    return;
  }
  try {
    status("Select serial port...");
    port = await navigator.serial.requestPort();
    const sel = $("mon-baud");
    const baud = parseInt(sel ? sel.value : "115200", 10) || 115200;
    await port.open({ baudRate: baud });
    setConnected(true);
    status("Connected @ " + baud);
    meta("\n--- connected @ " + baud + " baud ---\n");
    keepReading = true;
    readLoopDone = readLoop();
  } catch (e) {
    status("Connect failed: " + e.message);
    port = null;
  }
}

async function disconnect() {
  keepReading = false;
  try { if (reader) await reader.cancel(); } catch (_e) {}
  try { if (readLoopDone) await readLoopDone; } catch (_e) {}
  try { if (port) await port.close(); } catch (_e) {}
  port = null;
  // Flush any buffered partial line so it isn't lost.
  if (lineBuf) { pushLine(classify(lineBuf), lineBuf); lineBuf = ""; }
  setConnected(false);
  status("Disconnected");
  meta("\n--- disconnected ---\n");
}

document.addEventListener("DOMContentLoaded", () => {
  const c = $("mon-connect");
  const d = $("mon-disconnect");
  const clr = $("mon-clear");
  const flt = $("mon-filter");

  if (!("serial" in navigator)) {
    if (c) c.disabled = true;
    status("Web Serial not supported in this browser");
  }

  if (c) c.addEventListener("click", connect);
  if (d) d.addEventListener("click", disconnect);
  if (clr) clr.addEventListener("click", () => {
    lines = [];
    lineBuf = "";
    const el = $("mon-output");
    if (el) el.textContent = "";
  });
  if (flt) flt.addEventListener("change", render);

  // Release the port if the user closes/reloads the page mid-session.
  window.addEventListener("beforeunload", () => {
    if (port) { try { port.close(); } catch (_e) {} }
  });
});

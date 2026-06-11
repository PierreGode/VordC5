/* Vord C5 Serial Monitor (Web Serial) — live device log viewer.
   Independent of the flasher: it opens its own connection to an
   already-flashed board so you can watch boot output, the AP address,
   and the heap-health log. */

const $ = (id) => document.getElementById(id);

let port = null;
let reader = null;
let keepReading = false;
let readLoopDone = null;
const decoder = new TextDecoder();

function out(text) {
  const el = $("mon-output");
  if (!el) return;
  // Keep the view pinned to the bottom only if the user is already there.
  const atBottom = el.scrollTop + el.clientHeight >= el.scrollHeight - 6;
  el.textContent += text;
  // Bound the buffer so a long session can't grow memory without limit.
  if (el.textContent.length > 120000) el.textContent = el.textContent.slice(-90000);
  if (atBottom) el.scrollTop = el.scrollHeight;
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
        if (value) out(decoder.decode(value, { stream: true }));
      }
    } catch (e) {
      out("\n[read error] " + e.message + "\n");
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
    out("\n--- connected @ " + baud + " baud ---\n");
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
  setConnected(false);
  status("Disconnected");
  out("\n--- disconnected ---\n");
}

document.addEventListener("DOMContentLoaded", () => {
  const c = $("mon-connect");
  const d = $("mon-disconnect");
  const clr = $("mon-clear");

  if (!("serial" in navigator)) {
    if (c) c.disabled = true;
    status("Web Serial not supported in this browser");
  }

  if (c) c.addEventListener("click", connect);
  if (d) d.addEventListener("click", disconnect);
  if (clr) clr.addEventListener("click", () => {
    const el = $("mon-output");
    if (el) el.textContent = "";
  });

  // Release the port if the user closes/reloads the page mid-session.
  window.addEventListener("beforeunload", () => {
    if (port) { try { port.close(); } catch (_e) {} }
  });
});

// The remote control web page served by RemoteControl (GET /). One self-contained file, no external resources,
// so it works without internet access. Split into several string literals because MSVC limits the length of one.

const char* RemoteControlPage()
{
    static const char* page =
        R"PAGE(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#111318">
<title>projectM Remote</title>
<style>
:root {
  --bg: #111318; --panel: #1b1e26; --button: #262a35; --button-active: #3a4152; --text: #e8eaf0;
  --muted: #9aa1b2; --accent: #7c5cff; --accent-text: #ffffff; --good: #3ecf8e; --warn: #ffb547; --bad: #ff5c7a;
}
* { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
html, body { margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.35 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
main { max-width: 560px; margin: 0 auto; padding: 12px 16px calc(24px + env(safe-area-inset-bottom)); }
section { background: var(--panel); border-radius: 14px; padding: 12px; margin-bottom: 12px; }
h2 { font-size: 13px; font-weight: 600; letter-spacing: .04em; text-transform: uppercase; color: var(--muted); margin: 0 0 10px; }
/* Fixed height (two lines, then "...") so a long preset name never moves the buttons below. */
#preset { font-size: 17px; font-weight: 600; word-break: break-word; height: 2.7em; overflow: hidden;
  display: -webkit-box; -webkit-box-orient: vertical; -webkit-line-clamp: 2; line-clamp: 2; }
.badges { display: flex; flex-wrap: nowrap; gap: 6px; margin-top: 8px; overflow: hidden; white-space: nowrap; }
.badge { font-size: 12px; padding: 3px 9px; border-radius: 99px; background: var(--button); color: var(--muted); }
.badge.on { background: var(--accent); color: var(--accent-text); }
.badge.warn { background: var(--warn); color: #222; }
.grid { display: grid; gap: 8px; grid-template-columns: repeat(3, 1fr); }
.grid.two { grid-template-columns: repeat(2, 1fr); }
.grid.four { grid-template-columns: repeat(4, 1fr); }
button { appearance: none; border: 0; border-radius: 12px; background: var(--button); color: var(--text);
  font: inherit; font-weight: 600; min-height: 56px; padding: 8px; cursor: pointer; touch-action: manipulation; }
button:active { background: var(--button-active); transform: scale(.98); }
button.on { background: var(--accent); color: var(--accent-text); }
button.big { min-height: 72px; font-size: 18px; }
button:disabled { opacity: .35; }
.row { display: flex; align-items: center; justify-content: space-between; gap: 12px; min-height: 44px; }
.row > label:not(.switch) { flex: 1; }
.value { min-width: 3.2em; text-align: right; font-variant-numeric: tabular-nums; color: var(--muted); }
/* Large sliders for fingers: tall touch area, thick track, big thumb. */
input[type=range] { -webkit-appearance: none; appearance: none; width: 100%; height: 48px; margin: 0;
  background: transparent; touch-action: pan-y; }
input[type=range]::-webkit-slider-runnable-track { height: 12px; border-radius: 99px;
  background: linear-gradient(to right, var(--accent) var(--fill, 0%), var(--button) var(--fill, 0%)); }
input[type=range]::-moz-range-track { height: 12px; border-radius: 99px; background: var(--button); }
input[type=range]::-moz-range-progress { height: 12px; border-radius: 99px; background: var(--accent); }
input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 36px; height: 36px;
  margin-top: -12px; border-radius: 50%; background: var(--accent); border: 3px solid var(--text); }
input[type=range]::-moz-range-thumb { width: 30px; height: 30px; border-radius: 50%; background: var(--accent);
  border: 3px solid var(--text); }
input[type=range]:disabled { opacity: .35; }
.switch { position: relative; width: 60px; height: 34px; flex: none; }
.switch input { position: absolute; opacity: 0; width: 0; height: 0; }
.switch span { position: absolute; inset: 0; background: var(--button); border-radius: 99px; transition: .15s; }
.switch span::before { content: ""; position: absolute; width: 28px; height: 28px; left: 3px; top: 3px;
  background: var(--text); border-radius: 50%; transition: .15s; }
.switch input:checked + span { background: var(--accent); }
.switch input:checked + span::before { transform: translateX(26px); }
#connection { font-size: 12px; color: var(--muted); text-align: center; margin-top: 4px; }
#connection.bad { color: var(--bad); }
.slider-block + .slider-block { margin-top: 6px; }
</style>
</head>
)PAGE"
        R"PAGE(<body>
<main>
  <section>
    <div id="preset">Connecting&hellip;</div>
    <div class="badges">
      <span class="badge" id="badge-mode">All presets</span>
      <span class="badge" id="badge-locked">Unlocked</span>
      <span class="badge" id="badge-favorite">Not a favorite</span>
      <span class="badge" id="badge-shuffle">Shuffle</span>
    </div>
  </section>

  <section>
    <h2>Presets</h2>
    <div class="grid">
      <button class="big" data-action="previous">&#9664; Prev</button>
      <button class="big" data-action="random">Random</button>
      <button class="big" data-action="next">Next &#9654;</button>
    </div>
    <div class="row" style="margin-top:8px">
      <label for="smooth">Smooth transition</label>
      <label class="switch"><input type="checkbox" id="smooth"><span></span></label>
    </div>
    <div class="grid two" style="margin-top:4px">
      <button id="lock" data-action="lock">Lock</button>
      <button id="shuffle" data-action="shuffle">Shuffle</button>
    </div>
  </section>

  <section>
    <h2>Effects</h2>
    <div class="grid four" id="mirror">
      <button data-mirror="off">No mirror</button>
      <button data-mirror="leftright">Left &#9656; Right</button>
      <button data-mirror="topbottom">Top &#9656; Bottom</button>
      <button data-mirror="fourway">4-Way</button>
    </div>
    <div class="slider-block" style="margin-top:6px">
      <div class="row"><label for="zoom">Zoom</label><span class="value" id="zoom-value"></span></div>
      <input type="range" id="zoom" min="1" max="4" step="0.1" data-action="zoom">
    </div>
    <div class="slider-block">
      <div class="row"><label for="ambient">Ambient Speed</label><span class="value" id="ambient-value"></span></div>
      <input type="range" id="ambient" min="0.1" max="1" step="0.05" data-action="ambient-speed">
    </div>
  </section>

  <section>
    <h2>Music reaction</h2>
    <div class="row">
      <label for="averaging">Beat Averaging</label>
      <label class="switch"><input type="checkbox" id="averaging"><span></span></label>
    </div>
    <div class="slider-block">
      <div class="row"><label for="sensitivity">Beat Sensitivity</label><span class="value" id="sensitivity-value"></span></div>
      <input type="range" id="sensitivity" min="0" max="2" step="0.05" data-action="beat-sensitivity">
    </div>
  </section>

  <section>
    <h2>Favorites and disabled presets</h2>
    <div class="grid two">
      <button data-action="favorite-save">&#9733; Save favorite</button>
      <button data-action="favorite-remove">Remove favorite</button>
      <button id="disable" data-action="disable">Disable preset</button>
      <button data-action="undo">Undo</button>
      <button id="mode-favorites" data-action="mode-favorites">Favorites only</button>
      <button id="mode-disabled" data-action="mode-disabled">Disabled only</button>
    </div>
  </section>

  <div id="connection">Connecting&hellip;</div>
</main>
)PAGE"
        R"PAGE(<script>
"use strict";
const $ = (id) => document.getElementById(id);
const smooth = $("smooth");
try { smooth.checked = localStorage.getItem("smooth") === "1"; } catch (e) {}
smooth.addEventListener("change", () => { try { localStorage.setItem("smooth", smooth.checked ? "1" : "0"); } catch (e) {} });

let status = null;
let sliderBusyUntil = 0; // Don't move sliders while the user drags them.

async function send(action, value) {
  const params = new URLSearchParams();
  if (value !== undefined) params.set("value", String(value));
  if (["next", "previous", "random"].includes(action) && smooth.checked) params.set("smooth", "1");
  try {
    const response = await fetch("/api/" + action + (params.toString() ? "?" + params : ""),
      { method: "POST", headers: { "X-ProjectM": "1" } });
    if (!response.ok) throw new Error(response.status);
    setTimeout(refresh, 150);
  } catch (e) {
    showConnection(false);
  }
}

function showConnection(ok) {
  const element = $("connection");
  element.textContent = ok ? "Connected" : "Not connected. Is projectM running with the remote control on?";
  element.className = ok ? "" : "bad";
}

function setBadge(id, text, on, warn) {
  const element = $(id);
  element.textContent = text;
  element.className = "badge" + (on ? " on" : "") + (warn ? " warn" : "");
}

function updateFill(slider) {
  const min = Number(slider.min), max = Number(slider.max);
  slider.style.setProperty("--fill", ((Number(slider.value) - min) / (max - min) * 100) + "%");
}

function setSlider(id, value, digits) {
  if (Date.now() > sliderBusyUntil) $(id).value = value;
  updateFill($(id));
  $(id + "-value").textContent = Number(value).toFixed(digits);
}

function render(s) {
  status = s;
  $("preset").textContent = s.preset || "(no preset)";
  const modeNames = { all: "All presets", favorites: "Favorites only", disabled: "Disabled only" };
  setBadge("badge-mode", modeNames[s.mode] || s.mode, s.mode !== "all", s.mode === "disabled");
  setBadge("badge-locked", s.locked ? "Locked" : "Unlocked", s.locked);
  setBadge("badge-favorite", s.favorite ? "\u2605 Favorite" : "Not a favorite", s.favorite);
  setBadge("badge-shuffle", s.shuffle ? "Shuffle on" : "Shuffle off", s.shuffle);
  $("lock").classList.toggle("on", s.locked);
  $("lock").textContent = s.locked ? "Locked" : "Lock";
  $("shuffle").classList.toggle("on", s.shuffle);
  $("mode-favorites").classList.toggle("on", s.mode === "favorites");
  $("mode-disabled").classList.toggle("on", s.mode === "disabled");
  const disable = $("disable");
  disable.textContent = s.mode === "disabled" ? "Re-enable preset" : "Disable preset";
  disable.dataset.action = s.mode === "disabled" ? "reenable" : "disable";
  disable.disabled = s.mode === "favorites";
  $("averaging").checked = s.beatAveraging;
  $("sensitivity").disabled = s.beatAveraging;
  setSlider("sensitivity", s.beatSensitivity, 2);
  setSlider("ambient", s.ambientSpeed, 2);
  setSlider("zoom", s.zoom, 1);
  for (const button of $("mirror").querySelectorAll("button")) {
    button.classList.toggle("on", button.dataset.mirror === s.mirror);
  }
}

async function refresh() {
  try {
    const response = await fetch("/api/status", { cache: "no-store" });
    if (!response.ok) throw new Error(response.status);
    render(await response.json());
    showConnection(true);
  } catch (e) {
    showConnection(false);
  }
}

for (const button of document.querySelectorAll("button[data-action]")) {
  button.addEventListener("click", () => send(button.dataset.action));
}
for (const button of document.querySelectorAll("button[data-mirror]")) {
  button.addEventListener("click", () => send("mirror", button.dataset.mirror));
}
$("averaging").addEventListener("change", (e) => send("beat-averaging", e.target.checked ? "1" : "0"));

for (const slider of document.querySelectorAll("input[type=range]")) {
  let timer = 0;
  slider.addEventListener("input", () => {
    sliderBusyUntil = Date.now() + 1500;
    updateFill(slider);
    $(slider.id + "-value").textContent = Number(slider.value).toFixed(slider.id === "zoom" ? 1 : 2);
    clearTimeout(timer);
    timer = setTimeout(() => send(slider.dataset.action, slider.value), 120);
  });
}

refresh();
setInterval(() => { if (!document.hidden) refresh(); }, 1000);
document.addEventListener("visibilitychange", () => { if (!document.hidden) refresh(); });
</script>
</body>
</html>
)PAGE";
    return page;
}

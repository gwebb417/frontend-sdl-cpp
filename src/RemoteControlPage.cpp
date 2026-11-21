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
  --bg: #111318; --panel: #1b1e26; --button: #262a35; --button-active: #3a4152; --button-border: transparent;
  --track: #262a35; --text: #e8eaf0; --muted: #9aa1b2; --accent: #7c5cff; --accent-text: #ffffff;
  --good: #3ecf8e; --good-text: #0d2a1d; --warn: #ffb547; --warn-text: #222222; --bad: #ff5c7a; --bad-text: #ffffff;
  --h: 56px; --hb: 72px; --fs: 16px; --fsb: 17px;
}
/* Light theme: dark text, visible button borders and a page background that sets the panels apart. */
html.light {
  --bg: #dfe3ec; --panel: #ffffff; --button: #eceff5; --button-active: #d5dae5; --button-border: #b9c0cf;
  --track: #c5cbd8; --text: #14161c; --muted: #404757;
  --good: #15803d; --good-text: #ffffff; --warn: #b45309; --warn-text: #ffffff; --bad: #c0264a; --bad-text: #ffffff;
}
html.size-compact { --h: 44px; --hb: 56px; --fs: 14px; --fsb: 15px; }
html.size-large { --h: 72px; --hb: 88px; --fs: 19px; --fsb: 21px; }
* { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
html, body { margin: 0; background: var(--bg); color: var(--text);
  font: 16px/1.35 system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
main { max-width: 560px; margin: 0 auto; padding: 12px 16px calc(24px + env(safe-area-inset-bottom)); }
section { background: var(--panel); border-radius: 14px; padding: 12px; margin-bottom: 12px; }
h2 { flex: 1; font-size: 13px; font-weight: 600; letter-spacing: .04em; text-transform: uppercase; color: var(--muted); margin: 0; }
.head { display: flex; align-items: center; gap: 6px; margin-bottom: 10px; min-height: 20px; }
/* Fixed height (two lines, then "...") so a long preset name never moves the buttons below. */
#preset { font-size: 17px; font-weight: 600; word-break: break-word; height: 2.7em; overflow: hidden;
  display: -webkit-box; -webkit-box-orient: vertical; -webkit-line-clamp: 2; line-clamp: 2; }
.badges { display: flex; flex-wrap: nowrap; gap: 6px; margin-top: 8px; overflow: hidden; white-space: nowrap; }
.badge { font-size: 12px; padding: 3px 9px; border-radius: 99px; background: var(--button); color: var(--muted);
  border: 1px solid var(--button-border); }
.badge.on { background: var(--accent); color: var(--accent-text); border-color: var(--accent); }
.badge.warn { background: var(--warn); color: var(--warn-text); border-color: var(--warn); }
.grid { display: grid; gap: 8px; grid-template-columns: repeat(3, 1fr); }
.grid.two { grid-template-columns: repeat(2, 1fr); }
.grid.four { grid-template-columns: repeat(4, 1fr); }
button { appearance: none; border: 1px solid var(--button-border); border-radius: 12px; background: var(--button);
  color: var(--text); position: relative; font: inherit; font-size: var(--fs); font-weight: 600; min-height: var(--h);
  padding: 8px; cursor: pointer; touch-action: manipulation; overflow-wrap: anywhere; }
button:active { background: var(--button-active); transform: scale(.98); }
button.on { background: var(--accent); color: var(--accent-text); border-color: var(--accent); }
button.big { min-height: var(--hb); font-size: var(--fsb); }
button small { display: block; font-size: .7em; font-weight: 500; opacity: .8; }
button:disabled { opacity: .35; }
/* Confirmation of a button press: dimmed while projectM runs it, then green with a check or red. */
button.busy { opacity: .55; }
button.ok { background: var(--good) !important; color: var(--good-text) !important; border-color: var(--good) !important; }
button.fail { background: var(--bad) !important; color: var(--bad-text) !important; border-color: var(--bad) !important; }
button.ok::after, button.fail::after { position: absolute; top: 3px; right: 7px; font-size: 13px; }
button.ok::after { content: "\2713"; }
button.fail::after { content: "!"; }
#last { font-size: 13px; margin-top: 8px; min-height: 1.35em; color: var(--muted);
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
#last.good { color: var(--good); }
#last.bad { color: var(--bad); }
.grid.effects button { min-height: calc(var(--h) * .85); font-size: calc(var(--fs) * .88); }
.row { display: flex; align-items: center; justify-content: space-between; gap: 12px; min-height: 44px; }
.row > label:not(.switch) { flex: 1; }
.value { min-width: 3.2em; text-align: right; font-variant-numeric: tabular-nums; color: var(--muted); }
.note { font-size: 13px; color: var(--muted); margin: 8px 0; }
.dim { opacity: .45; }
)PAGE"
        R"PAGE(/* Large sliders for fingers: tall touch area, thick track, big thumb. */
input[type=range] { -webkit-appearance: none; appearance: none; width: 100%; height: 48px; margin: 0;
  background: transparent; touch-action: pan-y; }
input[type=range]::-webkit-slider-runnable-track { height: 12px; border-radius: 99px;
  background: linear-gradient(to right, var(--accent) var(--fill, 0%), var(--track) var(--fill, 0%)); }
input[type=range]::-moz-range-track { height: 12px; border-radius: 99px; background: var(--track); }
input[type=range]::-moz-range-progress { height: 12px; border-radius: 99px; background: var(--accent); }
input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; appearance: none; width: 36px; height: 36px;
  margin-top: -12px; border-radius: 50%; background: var(--accent); border: 3px solid var(--text); }
input[type=range]::-moz-range-thumb { width: 30px; height: 30px; border-radius: 50%; background: var(--accent);
  border: 3px solid var(--text); }
input[type=range]:disabled { opacity: .35; }
/* "- / +" buttons instead of sliders (Appearance setting). */
.pm-row { display: none; gap: 8px; margin: 4px 0; }
.pm-row button { flex: 1; font-size: calc(var(--fs) * 1.3); }
html.pm input[type=range] { display: none; }
html.pm .pm-row { display: flex; }
.switch { position: relative; width: 60px; height: 34px; flex: none; }
.switch input { position: absolute; opacity: 0; width: 0; height: 0; }
.switch span { position: absolute; inset: 0; background: var(--track); border-radius: 99px; transition: .15s; }
.switch span::before { content: ""; position: absolute; width: 28px; height: 28px; left: 3px; top: 3px;
  background: #ffffff; border-radius: 50%; transition: .15s; box-shadow: 0 0 0 1px rgba(0, 0, 0, .15); }
.switch input:checked + span { background: var(--accent); }
.switch input:checked + span::before { transform: translateX(26px); }
#connection { font-size: 12px; color: var(--muted); text-align: center; margin-top: 4px; }
#connection.bad { color: var(--bad); }
.slider-block + .slider-block { margin-top: 6px; }
/* Edit layout mode. */
.layout-bar { display: flex; gap: 8px; margin-bottom: 8px; }
.layout-bar button { flex: 1; min-height: calc(var(--h) * .8); font-size: calc(var(--fs) * .9); }
.layout-bar #layout-reset { display: none; }
.edit-only, .tools { display: none; }
.hidden-item { display: none; }
section.empty { display: none; }
main.edit .edit-only { display: block; }
main.edit .tools { display: flex; gap: 6px; }
main.edit .layout-bar #layout-reset { display: block; }
main.edit section[data-section] { outline: 2px dashed color-mix(in srgb, var(--accent) 55%, transparent); outline-offset: -2px; }
main.edit .hidden-item, main.edit section.empty { display: block; }
main.edit .hidden-item { opacity: .35; }
main.edit .body { pointer-events: none; }
main.edit .body .grid[data-grid], main.edit .body .item-tools, main.edit .body .edit-only { pointer-events: auto; }
main.edit .grid[data-grid] button.selected { outline: 3px solid var(--warn); outline-offset: -3px; }
.tools button { min-height: calc(var(--h) * .62); min-width: 40px; padding: 4px 10px; font-size: calc(var(--fs) * .82); border-radius: 10px; }
.item-tools { display: none; margin-top: 8px; }
main.edit .item-tools { display: block; }
.item-tools .line { display: flex; gap: 6px; align-items: center; margin-top: 6px; }
.item-tools .line > button, .seg button { flex: 1; min-height: calc(var(--h) * .7); font-size: calc(var(--fs) * .85); }
.item-tools .caption, .appearance .caption { font-size: 13px; color: var(--muted); margin: 10px 0 4px; }
.seg { display: flex; gap: 6px; }
.swatches { display: flex; gap: 10px; flex-wrap: wrap; }
.swatches button { width: 40px; min-height: 40px; height: 40px; border-radius: 50%; padding: 0; border: 3px solid transparent; }
.swatches button.on { border-color: var(--text); }
input[type=text], input[type=number], select { width: 100%; height: calc(var(--h) * .72); border-radius: 10px;
  border: 1px solid var(--button-border); background: var(--bg); color: var(--text); padding: 0 10px; font: inherit;
  font-size: calc(var(--fs) * .95); }
html:not(.light) input[type=text], html:not(.light) input[type=number], html:not(.light) select { border-color: var(--track); }
.step { display: flex; align-items: center; gap: 6px; background: var(--bg); border-radius: 10px; padding: 6px 8px; margin-top: 6px; font-size: 14px; }
.step span { flex: 1; }
.step button { min-height: calc(var(--h) * .6); min-width: calc(var(--h) * .6); padding: 2px 8px; font-size: calc(var(--fs) * .8); }
.hint { font-size: 13px; color: var(--warn); margin: 0 0 12px; }
#custom-new { width: 100%; margin-top: 8px; background: transparent; border: 2px dashed var(--muted); }
</style>
</head>
)PAGE"
        R"PAGE(<body>
<main id="main">
  <section>
    <div id="preset">Connecting&hellip;</div>
    <div class="badges">
      <span class="badge" id="badge-mode">All presets</span>
      <span class="badge" id="badge-locked">Unlocked</span>
      <span class="badge" id="badge-favorite">Not a favorite</span>
      <span class="badge" id="badge-shuffle">Shuffle</span>
    </div>
    <div id="last" aria-live="polite"></div>
  </section>

  <div id="sections">
  <section data-section="presets">
    <div class="head"><h2>Presets</h2><span class="tools"></span></div>
    <div class="body">
      <div class="grid four" data-grid="presets-nav">
        <button class="big" data-id="previous" data-action="previous">&#9664;<small>Prev</small></button>
        <button class="big" data-id="back" data-action="back">&#8630;<small>Back (last)</small></button>
        <button class="big" data-id="random" data-action="random">&#8644;<small>Random</small></button>
        <button class="big" data-id="next" data-action="next">&#9654;<small>Next</small></button>
      </div>
      <div class="row" style="margin-top:8px">
        <label for="smooth">Smooth transition</label>
        <label class="switch"><input type="checkbox" id="smooth"><span></span></label>
      </div>
      <div class="grid two" style="margin-top:4px" data-grid="presets-modes">
        <button id="lock" data-id="lock" data-action="lock">Lock</button>
        <button id="shuffle" data-id="shuffle" data-action="shuffle">Shuffle</button>
      </div>
    </div>
  </section>

  <section data-section="custom" class="empty">
    <div class="head"><h2>My buttons</h2><span class="tools"></span></div>
    <div class="body">
      <div class="grid" data-grid="custom" id="custom"></div>
      <button class="edit-only" id="custom-new">+ New button</button>
    </div>
  </section>

  <section data-section="transitions">
    <div class="head"><h2>Transitions</h2><span class="tools"></span></div>
    <div class="body">
      <div class="slider-block">
        <div class="row"><label for="display">Preset Display Duration</label><span class="value" id="display-value"></span></div>
        <input type="range" id="display" min="5" max="180" step="1" data-action="display-duration" data-digits="0" data-unit=" s">
      </div>
      <div class="slider-block">
        <div class="row"><label for="transition">Transition Duration</label><span class="value" id="transition-value"></span></div>
        <input type="range" id="transition" min="0" max="10" step="0.5" data-action="transition-duration" data-digits="1" data-unit=" s">
      </div>
      <p class="note">Transition effects (each transition picks one at random)</p>
      <div class="grid four effects" id="effects"></div>
      <div class="row" style="margin-top:10px">
        <label for="hardcuts">Hard Cuts</label>
        <label class="switch"><input type="checkbox" id="hardcuts"><span></span></label>
      </div>
      <div id="hardcut-block">
        <div class="slider-block">
          <div class="row"><label for="hardcut-duration">Earliest Hard Cut After</label><span class="value" id="hardcut-duration-value"></span></div>
          <input type="range" id="hardcut-duration" min="1" max="120" step="1" data-action="hard-cut-duration" data-digits="0" data-unit=" s">
        </div>
        <div class="slider-block">
          <div class="row"><label for="hardcut-threshold">Hard Cut Threshold</label><span class="value" id="hardcut-threshold-value"></span></div>
          <input type="range" id="hardcut-threshold" min="0" max="5" step="0.1" data-action="hard-cut-threshold" data-digits="1">
        </div>
      </div>
    </div>
  </section>
)PAGE"
        R"PAGE(
  <section data-section="effects">
    <div class="head"><h2>Effects</h2><span class="tools"></span></div>
    <div class="body">
      <div class="grid four" id="mirror" data-grid="effects-mirror">
        <button data-id="off" data-mirror="off">No mirror</button>
        <button data-id="leftright" data-mirror="leftright">Left &#9656; Right</button>
        <button data-id="topbottom" data-mirror="topbottom">Top &#9656; Bottom</button>
        <button data-id="fourway" data-mirror="fourway">4-Way</button>
      </div>
      <div class="slider-block" style="margin-top:6px">
        <div class="row"><label for="zoom">Zoom</label><span class="value" id="zoom-value"></span></div>
        <input type="range" id="zoom" min="1" max="4" step="0.1" data-action="zoom" data-digits="1">
      </div>
      <div class="slider-block">
        <div class="row"><label for="ambient">Ambient Speed</label><span class="value" id="ambient-value"></span></div>
        <input type="range" id="ambient" min="0.1" max="1" step="0.05" data-action="ambient-speed" data-digits="2">
      </div>
    </div>
  </section>

  <section data-section="music">
    <div class="head"><h2>Music reaction</h2><span class="tools"></span></div>
    <div class="body">
      <div class="row">
        <label for="averaging">Beat Averaging</label>
        <label class="switch"><input type="checkbox" id="averaging"><span></span></label>
      </div>
      <div class="slider-block">
        <div class="row"><label for="sensitivity">Beat Sensitivity</label><span class="value" id="sensitivity-value"></span></div>
        <input type="range" id="sensitivity" min="0" max="2" step="0.05" data-action="beat-sensitivity" data-digits="2">
      </div>
    </div>
  </section>

  <section data-section="favorites">
    <div class="head"><h2>Favorites and disabled presets</h2><span class="tools"></span></div>
    <div class="body">
      <div class="grid two" data-grid="favorites">
        <button data-id="favorite-save" data-action="favorite-save">&#9733; Save favorite</button>
        <button data-id="favorite-remove" data-action="favorite-remove">Remove favorite</button>
        <button data-id="disable" id="disable" data-action="disable">Disable preset</button>
        <button data-id="undo" data-action="undo">Undo</button>
        <button data-id="mode-favorites" id="mode-favorites" data-action="mode-favorites">Favorites only</button>
        <button data-id="mode-disabled" id="mode-disabled" data-action="mode-disabled">Disabled only</button>
      </div>
    </div>
  </section>
  </div>

  <p class="hint edit-only">Move or hide sections with the buttons on each one. Tap a button to select it (orange
    outline), then move, hide or rename it below its group. Changes are saved for every device.</p>

  <section class="edit-only appearance">
    <div class="head"><h2>Appearance (all devices)</h2></div>
    <div class="caption">Button size</div>
    <div class="seg" data-appearance="size">
      <button data-value="compact">Compact</button><button data-value="normal">Normal</button><button data-value="large">Large</button>
    </div>
    <div class="caption">Sliders</div>
    <div class="seg" data-appearance="sliders">
      <button data-value="slider">Slider</button><button data-value="buttons">&minus; / + buttons</button>
    </div>
    <div class="caption">Theme</div>
    <div class="seg" data-appearance="theme">
      <button data-value="dark">Dark</button><button data-value="light">Light</button>
    </div>
    <div class="caption">Accent color</div>
    <div class="swatches" id="swatches"></div>
  </section>

  <div class="layout-bar">
    <button id="layout-edit">Edit layout</button>
    <button id="layout-reset">Reset to default</button>
  </div>

  <div id="connection">Connecting&hellip;</div>
</main>
)PAGE"
        R"PAGE(<script>
"use strict";
const $ = (id) => document.getElementById(id);
const main = $("main");
const smooth = $("smooth");
try { smooth.checked = localStorage.getItem("smooth") === "1"; } catch (e) {}
smooth.addEventListener("change", () => { try { localStorage.setItem("smooth", smooth.checked ? "1" : "0"); } catch (e) {} });

let status = null;
let sliderBusyUntil = 0; // Don't move sliders while the user drags them.
let editing = false;

// Runs one command in projectM and returns its result ({ok, message} or {ok: false, error}).
async function post(action, value) {
  const params = new URLSearchParams();
  if (value !== undefined && value !== null && value !== "") params.set("value", String(value));
  if (["next", "previous", "random", "back"].includes(action) && smooth.checked) params.set("smooth", "1");
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), 5000);
  let result;
  try {
    const response = await fetch("/api/" + action + (params.toString() ? "?" + params : ""),
      { method: "POST", headers: { "X-ProjectM": "1" }, signal: controller.signal });
    result = await response.json().catch(() => null);
    if (!result) result = { ok: false, error: "Error " + response.status };
    showConnection(true);
  } catch (e) {
    result = { ok: false, error: e.name === "AbortError" ? "No answer from projectM" : "Not connected" };
    showConnection(false);
  }
  clearTimeout(timeout);
  return result;
}

// Sends a command and shows what projectM did: on the pressed button (element), in the line under the
// preset name, and with a short vibration on phones that support it (haptic).
async function send(action, value, element, haptic) {
  if (element) { element.classList.remove("ok", "fail"); element.classList.add("busy"); }
  const result = await post(action, value);
  showResult(result, element, haptic || !!element);
  setTimeout(refresh, 150);
}

function showResult(result, element, haptic) {
  const last = $("last");
  last.textContent = result.ok ? "✓ " + (result.message || "Done") : "✕ " + (result.error || "Failed");
  last.className = result.ok ? "good" : "bad";
  if (haptic && navigator.vibrate) { try { navigator.vibrate(result.ok ? 15 : [40, 60, 40]); } catch (e) {} }
  if (element) {
    element.classList.remove("busy");
    element.classList.add(result.ok ? "ok" : "fail");
    clearTimeout(element.feedbackTimer);
    element.feedbackTimer = setTimeout(() => element.classList.remove("ok", "fail"), result.ok ? 900 : 1800);
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

function formatValue(slider, value) {
  return Number(value).toFixed(Number(slider.dataset.digits || 2)) + (slider.dataset.unit || "");
}

// The number shows the real value, even if it's outside the slider's range (set in the settings window).
function setSlider(id, value) {
  const slider = $(id);
  if (Date.now() > sliderBusyUntil) slider.value = value;
  updateFill(slider);
  $(id + "-value").textContent = formatValue(slider, value);
  const plusMinus = slider.nextElementSibling;
  if (plusMinus && plusMinus.classList.contains("pm-row")) {
    for (const button of plusMinus.children) button.disabled = slider.disabled;
  }
}

// Text of buttons whose label depends on the state (e.g. "Lock"/"Locked"), unless the user renamed them.
function setStateLabel(button, text) {
  if (button.dataset.stateLabel === text) return;
  button.dataset.stateLabel = text;
  updateLabel(button);
}

function renderEffects(effects) {
  const grid = $("effects");
  if (grid.dataset.keys !== effects.map((e) => e.key).join(",")) {
    grid.dataset.keys = effects.map((e) => e.key).join(",");
    grid.textContent = "";
    for (const effect of effects) {
      const button = document.createElement("button");
      button.textContent = effect.name;
      button.dataset.key = effect.key;
      button.addEventListener("click", () => {
        const on = button.classList.contains("on");
        send("transition-effect", effect.key + ":" + (on ? "0" : "1"), button);
      });
      grid.appendChild(button);
    }
  }
  for (const button of grid.children) {
    const effect = effects.find((e) => e.key === button.dataset.key);
    button.classList.toggle("on", !!(effect && effect.on));
  }
}
)PAGE"
        R"PAGE(
function render(s) {
  status = s;
  $("preset").textContent = s.preset || "(no preset)";
  const modeNames = { all: "All presets", favorites: "Favorites only", disabled: "Disabled only" };
  setBadge("badge-mode", modeNames[s.mode] || s.mode, s.mode !== "all", s.mode === "disabled");
  setBadge("badge-locked", s.locked ? "Locked" : "Unlocked", s.locked);
  setBadge("badge-favorite", s.favorite ? "★ Favorite" : "Not a favorite", s.favorite);
  setBadge("badge-shuffle", s.shuffle ? "Shuffle on" : "Shuffle off", s.shuffle);
  $("lock").classList.toggle("on", s.locked);
  setStateLabel($("lock"), s.locked ? "Locked" : "Lock");
  $("shuffle").classList.toggle("on", s.shuffle);
  $("mode-favorites").classList.toggle("on", s.mode === "favorites");
  $("mode-disabled").classList.toggle("on", s.mode === "disabled");
  const disable = $("disable");
  setStateLabel(disable, s.mode === "disabled" ? "Re-enable preset" : "Disable preset");
  disable.dataset.action = s.mode === "disabled" ? "reenable" : "disable";
  disable.disabled = s.mode === "favorites" && !editing;
  $("averaging").checked = s.beatAveraging;
  $("sensitivity").disabled = s.beatAveraging;
  setSlider("sensitivity", s.beatSensitivity);
  setSlider("ambient", s.ambientSpeed);
  setSlider("zoom", s.zoom);
  for (const button of $("mirror").querySelectorAll("button")) {
    button.classList.toggle("on", button.dataset.mirror === s.mirror);
  }
  if (s.displayDuration !== undefined) {
    setSlider("display", s.displayDuration);
    setSlider("transition", s.transitionDuration);
    renderEffects(s.transitionEffects || []);
    $("hardcuts").checked = s.hardCuts;
    $("hardcut-block").classList.toggle("dim", !s.hardCuts);
    setSlider("hardcut-duration", s.hardCutDuration);
    setSlider("hardcut-threshold", s.hardCutThreshold);
  }
  if (s.layoutRevision !== undefined && s.layoutRevision !== layoutRevision && !editing) {
    layoutRevision = s.layoutRevision;
    loadLayout();
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

// ---- Custom buttons ("My buttons"): run several commands in order. ----
const STEP_TYPES = [
  { action: "next", label: "Next preset" },
  { action: "previous", label: "Previous preset" },
  { action: "random", label: "Random preset" },
  { action: "back", label: "Back (last played)" },
  { action: "set-mode", label: "Play", choices: [["all", "All presets"], ["favorites", "Favorites only"], ["disabled", "Disabled only"]] },
  { action: "set-lock", label: "Preset lock", choices: [["1", "Locked"], ["0", "Unlocked"]] },
  { action: "set-shuffle", label: "Shuffle", choices: [["1", "On"], ["0", "Off"]] },
  { action: "favorite-save", label: "Save favorite" },
  { action: "mirror", label: "Mirror", choices: [["off", "No mirror"], ["leftright", "Left ▸ Right"], ["topbottom", "Top ▸ Bottom"], ["fourway", "4-Way"]] },
  { action: "zoom", label: "Zoom", range: [1, 4, 0.1], value: 1 },
  { action: "ambient-speed", label: "Ambient Speed", range: [0.1, 1, 0.05], value: 1 },
  { action: "beat-averaging", label: "Beat Averaging", choices: [["1", "On"], ["0", "Off"]] },
  { action: "beat-sensitivity", label: "Beat Sensitivity", range: [0, 2, 0.05], value: 1 },
  { action: "display-duration", label: "Preset Display Duration", range: [1, 240, 1], value: 30, unit: " s" },
  { action: "transition-duration", label: "Transition Duration", range: [0, 10, 0.5], value: 3, unit: " s" },
  { action: "hard-cuts", label: "Hard Cuts", choices: [["1", "On"], ["0", "Off"]] },
  { action: "hard-cut-duration", label: "Earliest Hard Cut After", range: [1, 240, 1], value: 20, unit: " s" },
  { action: "hard-cut-threshold", label: "Hard Cut Threshold", range: [0, 10, 0.1], value: 1 },
];

function stepLabel(step) {
  const type = STEP_TYPES.find((t) => t.action === step.action);
  if (!type) return step.action;
  if (type.choices) {
    const choice = type.choices.find((c) => c[0] === String(step.value));
    return type.label + " → " + (choice ? choice[1] : step.value);
  }
  if (type.range) return type.label + " → " + Number(step.value) + (type.unit || "");
  return type.label;
}

async function runCustom(button, custom) {
  if (!custom.steps.length) {
    showResult({ ok: false, error: custom.name + ": no steps yet (add them in Edit layout)" }, button, true);
    return;
  }
  button.classList.remove("ok", "fail");
  button.classList.add("busy");
  for (let i = 0; i < custom.steps.length; i++) {
    const step = custom.steps[i];
    const result = await post(step.action, step.value);
    if (!result.ok) {
      showResult({ ok: false, error: custom.name + ": step " + (i + 1) + " (" + stepLabel(step) + ") failed: " + (result.error || "") }, button, true);
      setTimeout(refresh, 150);
      return;
    }
  }
  showResult({ ok: true, message: custom.name + ": " + custom.steps.length + (custom.steps.length === 1 ? " step" : " steps") + " done" }, button, true);
  setTimeout(refresh, 150);
}
)PAGE"
        R"PAGE(
// ---- Layout: order, visibility, names and columns of sections and buttons, appearance and custom buttons.
// Saved in projectM for all devices. ----
let layoutRevision = null;
let selected = null; // The button selected in edit mode.
let saveTimer = 0;
const ACCENTS = [["#7c5cff", "#ffffff"], ["#2563eb", "#ffffff"], ["#22c55e", "#0b2a17"], ["#fb923c", "#2b1405"], ["#e11d74", "#ffffff"], ["#facc15", "#2a2203"]];
const DEFAULT_APPEARANCE = { size: "normal", sliders: "slider", theme: "dark", accent: ACCENTS[0][0] };
let appearance = Object.assign({}, DEFAULT_APPEARANCE);
let names = {};   // grid id -> { button id -> name }
let columns = {}; // grid id -> buttons per row
let customs = []; // [{ id, name, steps: [{ action, value }] }]
const sectionsElement = $("sections");
const customGrid = $("custom");
const sectionIds = (parent) => [...parent.children].map((e) => e.dataset.section);
const itemIds = (grid) => [...grid.children].map((e) => e.dataset.id);
const defaultSections = sectionIds(sectionsElement);
const defaultItems = {};
for (const grid of document.querySelectorAll("[data-grid]")) {
  defaultItems[grid.dataset.grid] = itemIds(grid);
  grid.dataset.defaultColumns = grid.classList.contains("four") ? 4 : grid.classList.contains("two") ? 2 : 3;
  for (const button of grid.children) button.dataset.original = button.innerHTML;
}

function updateLabel(button) {
  const grid = button.parentElement;
  if (grid === customGrid) {
    const custom = customs.find((c) => c.id === button.dataset.id);
    button.textContent = custom && custom.name ? custom.name : "(no name)";
    return;
  }
  const name = ((names[grid.dataset.grid] || {})[button.dataset.id] || "").trim();
  if (name) button.textContent = name;
  else if (button.dataset.stateLabel) button.textContent = button.dataset.stateLabel;
  else button.innerHTML = button.dataset.original;
}

function applyAppearance() {
  const root = document.documentElement;
  root.classList.toggle("light", appearance.theme === "light");
  root.classList.toggle("size-compact", appearance.size === "compact");
  root.classList.toggle("size-large", appearance.size === "large");
  root.classList.toggle("pm", appearance.sliders === "buttons");
  const accent = ACCENTS.find((a) => a[0] === appearance.accent) || ACCENTS[0];
  root.style.setProperty("--accent", accent[0]);
  root.style.setProperty("--accent-text", accent[1]);
  document.querySelector('meta[name="theme-color"]').content = appearance.theme === "light" ? "#dfe3ec" : "#111318";
  for (const group of document.querySelectorAll("[data-appearance]")) {
    for (const button of group.children) button.classList.toggle("on", appearance[group.dataset.appearance] === button.dataset.value);
  }
  for (const swatch of $("swatches").children) swatch.classList.toggle("on", swatch.dataset.value === accent[0]);
}

function buildCustomButtons() {
  customGrid.textContent = "";
  for (const custom of customs) {
    const button = document.createElement("button");
    button.dataset.id = custom.id;
    button.addEventListener("click", () => { const c = customs.find((x) => x.id === custom.id); if (c) runCustom(button, c); });
    customGrid.appendChild(button);
    updateLabel(button);
  }
  defaultItems.custom = customs.map((c) => c.id);
  customGrid.closest("section").classList.toggle("empty", customs.length === 0);
}

// Saved order first; elements the saved layout doesn't know (e.g. added by an update) go after the element
// they follow by default.
function mergeOrder(defaults, saved) {
  const result = (Array.isArray(saved) ? saved : []).filter((id, i, all) => defaults.includes(id) && all.indexOf(id) === i);
  defaults.forEach((id, index) => {
    if (result.includes(id)) return;
    const before = index > 0 ? result.indexOf(defaults[index - 1]) : -1;
    result.splice(before + 1, 0, id);
  });
  return result;
}

function validCustoms(list) {
  if (!Array.isArray(list)) return [];
  return list.filter((c) => c && typeof c.id === "string").map((c) => ({
    id: c.id, name: typeof c.name === "string" ? c.name.slice(0, 40) : "",
    steps: (Array.isArray(c.steps) ? c.steps : []).filter((s) => s && typeof s.action === "string")
      .map((s) => ({ action: s.action, value: s.value === undefined ? "" : String(s.value) })).slice(0, 20),
  })).slice(0, 24);
}

function applyLayout(layout) {
  layout = layout && typeof layout === "object" ? layout : {};
  appearance = Object.assign({}, DEFAULT_APPEARANCE, layout.appearance && typeof layout.appearance === "object" ? layout.appearance : {});
  applyAppearance();
  customs = validCustoms(layout.custom);
  buildCustomButtons();
  const hiddenSections = Array.isArray(layout.hiddenSections) ? layout.hiddenSections : [];
  for (const id of mergeOrder(defaultSections, layout.sections)) {
    const section = sectionsElement.querySelector('[data-section="' + id + '"]');
    sectionsElement.appendChild(section);
    section.classList.toggle("hidden-item", hiddenSections.includes(id));
  }
  const grids = layout.grids && typeof layout.grids === "object" ? layout.grids : {};
  names = {};
  columns = {};
  for (const grid of document.querySelectorAll("[data-grid]")) {
    const id = grid.dataset.grid;
    const saved = grids[id] || {};
    const hidden = Array.isArray(saved.hidden) ? saved.hidden : [];
    names[id] = saved.names && typeof saved.names === "object" ? Object.assign({}, saved.names) : {};
    if ([2, 3, 4].includes(saved.columns)) columns[id] = saved.columns;
    for (const itemId of mergeOrder(defaultItems[id], saved.order)) {
      const button = grid.querySelector('[data-id="' + itemId + '"]');
      grid.appendChild(button);
      button.classList.toggle("hidden-item", hidden.includes(itemId));
      updateLabel(button);
    }
    applyColumns(grid);
  }
  if (selected) selected.classList.remove("selected");
  selected = null;
  updateTools();
}

function applyColumns(grid) {
  const count = columns[grid.dataset.grid] || Number(grid.dataset.defaultColumns);
  grid.style.gridTemplateColumns = "repeat(" + count + ", 1fr)";
}
)PAGE"
        R"PAGE(
function currentLayout() {
  const layout = { version: 2, appearance, sections: sectionIds(sectionsElement), hiddenSections: [], grids: {},
    custom: itemIds(customGrid).map((id) => customs.find((c) => c.id === id)).filter(Boolean) };
  for (const section of sectionsElement.children) {
    if (section.classList.contains("hidden-item")) layout.hiddenSections.push(section.dataset.section);
  }
  for (const grid of document.querySelectorAll("[data-grid]")) {
    const id = grid.dataset.grid;
    const entry = {
      order: itemIds(grid),
      hidden: [...grid.children].filter((b) => b.classList.contains("hidden-item")).map((b) => b.dataset.id),
    };
    const gridNames = {};
    for (const [key, value] of Object.entries(names[id] || {})) if (value && value.trim()) gridNames[key] = value.trim().slice(0, 40);
    if (Object.keys(gridNames).length) entry.names = gridNames;
    if (columns[id]) entry.columns = columns[id];
    layout.grids[id] = entry;
  }
  return layout;
}

async function loadLayout() {
  try {
    const response = await fetch("/api/layout", { cache: "no-store" });
    if (response.ok) applyLayout(await response.json());
  } catch (e) {}
}

async function saveLayout(layout) {
  try {
    const response = await fetch("/api/layout", { method: "POST", headers: { "X-ProjectM": "1", "Content-Type": "application/json" },
      body: JSON.stringify(layout) });
    if (!response.ok) throw new Error(response.status);
  } catch (e) {
    showResult({ ok: false, error: "Could not save the layout" }, null, false);
  }
}

// keepTools: don't rebuild the edit tools, e.g. while typing a name (rebuilding would close the keyboard).
function layoutChanged(keepTools) {
  if (!keepTools) updateTools();
  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => saveLayout(currentLayout()), 400);
}

function makeButton(text, label, onClick) {
  const button = document.createElement("button");
  button.textContent = text;
  if (label) button.setAttribute("aria-label", label);
  button.addEventListener("click", onClick);
  return button;
}

function makeElement(tag, className, text) {
  const element = document.createElement(tag);
  if (className) element.className = className;
  if (text) element.textContent = text;
  return element;
}

function updateTools() {
  for (const section of sectionsElement.children) {
    const tools = section.querySelector(".tools");
    tools.textContent = "";
    tools.append(
      makeButton("▲", "Move up", () => { if (section.previousElementSibling) { sectionsElement.insertBefore(section, section.previousElementSibling); layoutChanged(); } }),
      makeButton("▼", "Move down", () => { if (section.nextElementSibling) { sectionsElement.insertBefore(section.nextElementSibling, section); layoutChanged(); } }),
      makeButton(section.classList.contains("hidden-item") ? "Show" : "Hide", "Hide or show", () => { section.classList.toggle("hidden-item"); layoutChanged(); }));
  }
  for (const grid of document.querySelectorAll("[data-grid]")) {
    let tools = grid.nextElementSibling;
    if (!tools || !tools.classList.contains("item-tools")) {
      tools = makeElement("div", "item-tools");
      grid.after(tools);
    }
    tools.textContent = "";
    const id = grid.dataset.grid;
    tools.append(makeElement("div", "caption", "Buttons per row"));
    const perRow = makeElement("div", "seg");
    for (const count of [2, 3, 4]) {
      const button = makeButton(String(count), count + " per row", () => { columns[id] = count; applyColumns(grid); layoutChanged(); });
      button.classList.toggle("on", (columns[id] || Number(grid.dataset.defaultColumns)) === count);
      perRow.append(button);
    }
    tools.append(perRow);
    if (!selected || selected.parentElement !== grid) {
      if (grid.children.length) tools.append(makeElement("div", "caption", "Tap a button above to move, hide or rename it."));
      continue;
    }
    const item = selected;
    const line = makeElement("div", "line");
    line.append(
      makeButton("◀ Move", "Move left", () => { if (item.previousElementSibling) { grid.insertBefore(item, item.previousElementSibling); layoutChanged(); } }),
      makeButton("Move ▶", "Move right", () => { if (item.nextElementSibling) { grid.insertBefore(item.nextElementSibling, item); layoutChanged(); } }),
      makeButton(item.classList.contains("hidden-item") ? "Show" : "Hide", "Hide or show", () => { item.classList.toggle("hidden-item"); layoutChanged(); }));
    tools.append(line, makeElement("div", "caption", "Name of the selected button"));
    const input = document.createElement("input");
    input.type = "text";
    input.maxLength = 40;
    if (grid === customGrid) {
      const custom = customs.find((c) => c.id === item.dataset.id);
      input.value = custom ? custom.name : "";
      input.addEventListener("input", () => { if (custom) custom.name = input.value; updateLabel(item); layoutChanged(true); });
      tools.append(input);
      if (custom) appendStepEditor(tools, custom, item);
    } else {
      names[id] = names[id] || {};
      input.value = names[id][item.dataset.id] || "";
      const original = document.createElement("div");
      original.innerHTML = item.dataset.original;
      input.placeholder = item.dataset.stateLabel || original.textContent.trim();
      input.addEventListener("input", () => { names[id][item.dataset.id] = input.value; updateLabel(item); layoutChanged(true); });
      tools.append(input, makeElement("div", "caption", "Leave empty for the original name."));
    }
  }
}
)PAGE"
        R"PAGE(
function appendStepEditor(tools, custom, item) {
  tools.append(makeElement("div", "caption", "Steps (run in order)"));
  custom.steps.forEach((step, index) => {
    const row = makeElement("div", "step");
    row.append(makeElement("span", "", (index + 1) + ". " + stepLabel(step)),
      makeButton("▲", "Move step up", () => { if (index > 0) { custom.steps.splice(index - 1, 0, custom.steps.splice(index, 1)[0]); layoutChanged(); } }),
      makeButton("▼", "Move step down", () => { if (index < custom.steps.length - 1) { custom.steps.splice(index + 1, 0, custom.steps.splice(index, 1)[0]); layoutChanged(); } }),
      makeButton("✕", "Remove step", () => { custom.steps.splice(index, 1); layoutChanged(); }));
    tools.append(row);
  });
  if (!custom.steps.length) tools.append(makeElement("div", "caption", "No steps yet."));

  // New step: what, and the value if it needs one.
  tools.append(makeElement("div", "caption", "Add a step"));
  const type = document.createElement("select");
  STEP_TYPES.forEach((t, i) => { const option = document.createElement("option"); option.value = i; option.textContent = t.label; type.append(option); });
  const valueBox = makeElement("div", "");
  valueBox.style.marginTop = "6px";
  let valueInput = null;
  const updateValueInput = () => {
    const t = STEP_TYPES[Number(type.value)];
    valueBox.textContent = "";
    valueInput = null;
    if (t.choices) {
      valueInput = document.createElement("select");
      for (const choice of t.choices) { const option = document.createElement("option"); option.value = choice[0]; option.textContent = choice[1]; valueInput.append(option); }
    } else if (t.range) {
      valueInput = document.createElement("input");
      valueInput.type = "number";
      valueInput.min = t.range[0]; valueInput.max = t.range[1]; valueInput.step = t.range[2]; valueInput.value = t.value;
      valueInput.inputMode = "decimal";
    }
    if (valueInput) valueBox.append(valueInput);
  };
  type.addEventListener("change", updateValueInput);
  updateValueInput();
  const line = makeElement("div", "line");
  line.append(
    makeButton("+ Add step", "", () => {
      const t = STEP_TYPES[Number(type.value)];
      let value = valueInput ? valueInput.value : "";
      if (t.range) {
        const number = Number(value);
        if (value === "" || !isFinite(number)) { showResult({ ok: false, error: "Enter a number for " + t.label }, null, false); return; }
        value = String(Math.min(t.range[1], Math.max(t.range[0], number)));
      }
      if (custom.steps.length >= 20) { showResult({ ok: false, error: "A button can have at most 20 steps" }, null, false); return; }
      custom.steps.push({ action: t.action, value });
      layoutChanged();
    }),
    makeButton("Delete button", "", () => {
      if (!confirm('Delete the button "' + (custom.name || "(no name)") + '" for all devices?')) return;
      customs = customs.filter((c) => c.id !== custom.id);
      item.remove();
      selected = null;
      defaultItems.custom = customs.map((c) => c.id);
      layoutChanged();
    }));
  tools.append(type, valueBox, line);
}

function setEditing(on) {
  editing = on;
  main.classList.toggle("edit", on);
  $("layout-edit").textContent = on ? "Done" : "Edit layout";
  $("layout-edit").classList.toggle("on", on);
  if (selected) selected.classList.remove("selected");
  selected = null;
  customGrid.closest("section").classList.toggle("empty", customs.length === 0);
  updateTools();
  if (status) render(status);
  if (!on) { clearTimeout(saveTimer); saveLayout(currentLayout()); }
}

$("layout-edit").addEventListener("click", () => setEditing(!editing));
$("layout-reset").addEventListener("click", () => {
  if (!confirm("Reset the layout, appearance and custom buttons to the default for all devices?")) return;
  clearTimeout(saveTimer);
  applyLayout({});
  saveLayout({});
});
$("custom-new").addEventListener("click", () => {
  if (customs.length >= 24) return;
  const custom = { id: "c" + Date.now().toString(36), name: "My button " + (customs.length + 1), steps: [] };
  customs.push(custom);
  const order = itemIds(customGrid);
  const hidden = [...customGrid.children].filter((b) => b.classList.contains("hidden-item")).map((b) => b.dataset.id);
  buildCustomButtons();
  for (const id of order) {
    const button = customGrid.querySelector('[data-id="' + id + '"]');
    customGrid.appendChild(button);
    button.classList.toggle("hidden-item", hidden.includes(id));
  }
  applyColumns(customGrid);
  selected = customGrid.querySelector('[data-id="' + custom.id + '"]');
  selected.classList.add("selected");
  layoutChanged();
});
for (const group of document.querySelectorAll("[data-appearance]")) {
  for (const button of group.children) {
    button.addEventListener("click", () => { appearance[group.dataset.appearance] = button.dataset.value; applyAppearance(); layoutChanged(); });
  }
}
for (const accent of ACCENTS) {
  const swatch = makeButton("", "Accent color " + accent[0], () => { appearance.accent = accent[0]; applyAppearance(); layoutChanged(); });
  swatch.dataset.value = accent[0];
  swatch.style.background = accent[0];
  $("swatches").append(swatch);
}

// In edit mode, a tap on a button selects it instead of running its action.
document.addEventListener("click", (event) => {
  if (!editing) return;
  const button = event.target.closest("[data-grid] > button");
  if (!button) return;
  event.stopPropagation();
  event.preventDefault();
  if (selected) selected.classList.remove("selected");
  selected = selected === button ? null : button;
  if (selected) selected.classList.add("selected");
  updateTools();
}, true);
)PAGE"
        R"PAGE(
for (const button of document.querySelectorAll("button[data-action]")) {
  button.addEventListener("click", () => send(button.dataset.action, undefined, button));
}
for (const button of document.querySelectorAll("button[data-mirror]")) {
  button.addEventListener("click", () => send("mirror", button.dataset.mirror, button));
}
$("averaging").addEventListener("change", (e) => send("beat-averaging", e.target.checked ? "1" : "0", null, true));
$("hardcuts").addEventListener("change", (e) => send("hard-cuts", e.target.checked ? "1" : "0", null, true));

for (const slider of document.querySelectorAll("input[type=range]")) {
  let timer = 0;
  slider.addEventListener("input", () => {
    sliderBusyUntil = Date.now() + 1500;
    updateFill(slider);
    $(slider.id + "-value").textContent = formatValue(slider, slider.value);
    clearTimeout(timer);
    timer = setTimeout(() => send(slider.dataset.action, slider.value), 120);
  });

  // "- / +" buttons, shown instead of the slider if chosen in Appearance. Each tap is one step.
  const plusMinus = makeElement("div", "pm-row");
  for (const direction of [-1, 1]) {
    const button = makeButton(direction < 0 ? "−" : "+", direction < 0 ? "Decrease" : "Increase", () => {
      const step = Number(slider.step) || 1;
      const digits = (String(slider.step).split(".")[1] || "").length;
      let value = Math.round((Number(slider.value) + direction * step) / step) * step;
      value = Math.min(Number(slider.max), Math.max(Number(slider.min), value));
      slider.value = value.toFixed(digits);
      sliderBusyUntil = Date.now() + 1500;
      updateFill(slider);
      $(slider.id + "-value").textContent = formatValue(slider, slider.value);
      send(slider.dataset.action, slider.value, button);
    });
    plusMinus.append(button);
  }
  slider.after(plusMinus);
}

applyAppearance();
updateTools();
refresh();
setInterval(() => { if (!document.hidden) refresh(); }, 1000);
document.addEventListener("visibilitychange", () => { if (!document.hidden) refresh(); });
</script>
</body>
</html>
)PAGE";
    return page;
}

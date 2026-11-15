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
h2 { flex: 1; font-size: 13px; font-weight: 600; letter-spacing: .04em; text-transform: uppercase; color: var(--muted); margin: 0; }
.head { display: flex; align-items: center; gap: 6px; margin-bottom: 10px; min-height: 20px; }
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
button { appearance: none; border: 0; border-radius: 12px; background: var(--button); color: var(--text); position: relative;
  font: inherit; font-weight: 600; min-height: 56px; padding: 8px; cursor: pointer; touch-action: manipulation; }
button:active { background: var(--button-active); transform: scale(.98); }
button.on { background: var(--accent); color: var(--accent-text); }
button.big { min-height: 72px; font-size: 17px; }
button small { display: block; font-size: 11px; font-weight: 500; opacity: .75; }
button:disabled { opacity: .35; }
/* Confirmation of a button press: dimmed while projectM runs it, then green with a check or red. */
button.busy { opacity: .55; }
button.ok { background: var(--good) !important; color: #0d2a1d !important; }
button.fail { background: var(--bad) !important; color: #ffffff !important; }
button.ok::after, button.fail::after { position: absolute; top: 3px; right: 7px; font-size: 13px; }
button.ok::after { content: "\2713"; }
button.fail::after { content: "!"; }
#last { font-size: 13px; margin-top: 8px; min-height: 1.35em; color: var(--muted);
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
#last.good { color: var(--good); }
#last.bad { color: var(--bad); }
.grid.effects button { min-height: 48px; font-size: 14px; }
.row { display: flex; align-items: center; justify-content: space-between; gap: 12px; min-height: 44px; }
.row > label:not(.switch) { flex: 1; }
.value { min-width: 3.2em; text-align: right; font-variant-numeric: tabular-nums; color: var(--muted); }
.note { font-size: 13px; color: var(--muted); margin: 8px 0; }
.dim { opacity: .45; }
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
/* Edit layout mode. */
.layout-bar { display: flex; gap: 8px; margin-bottom: 12px; }
.layout-bar button { flex: 1; min-height: 44px; font-size: 14px; }
.layout-bar #layout-reset { display: none; }
.edit-only, .tools { display: none; }
.hidden-item { display: none; }
main.edit .edit-only { display: block; }
main.edit .tools { display: flex; gap: 6px; }
main.edit .layout-bar #layout-reset { display: block; }
main.edit section[data-section] { outline: 2px dashed rgba(124, 92, 255, .45); outline-offset: -2px; }
main.edit .hidden-item { display: block; opacity: .35; }
main.edit section.hidden-item { display: block; }
main.edit .grid button.hidden-item { display: block; }
main.edit .body { pointer-events: none; }
main.edit .body .grid[data-grid], main.edit .body .item-tools { pointer-events: auto; }
main.edit .grid[data-grid] button.selected { outline: 3px solid var(--warn); outline-offset: -3px; }
.tools button { min-height: 34px; min-width: 40px; padding: 4px 10px; font-size: 13px; border-radius: 10px; }
.item-tools { display: none; gap: 6px; margin-top: 8px; }
main.edit .item-tools { display: flex; }
.item-tools button { flex: 1; min-height: 38px; font-size: 13px; }
.hint { font-size: 13px; color: var(--warn); margin: 0 0 12px; }
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

  <div class="layout-bar">
    <button id="layout-edit">Edit layout</button>
    <button id="layout-reset">Reset to default</button>
  </div>
  <p class="hint edit-only">Move or hide sections with the buttons on each one. Tap a button to select it, then move
    or hide it with the buttons below it. Changes are saved for every device.</p>

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

// Sends a command and shows what projectM did: on the pressed button (element), in the line under the
// preset name, and with a short vibration on phones that support it (haptic).
async function send(action, value, element, haptic) {
  const params = new URLSearchParams();
  if (value !== undefined) params.set("value", String(value));
  if (["next", "previous", "random", "back"].includes(action) && smooth.checked) params.set("smooth", "1");
  if (element) { element.classList.remove("ok", "fail"); element.classList.add("busy"); }
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
  showResult(result, element, haptic || !!element);
  setTimeout(refresh, 150);
}

function showResult(result, element, haptic) {
  const last = $("last");
  last.textContent = result.ok ? "\u2713 " + (result.message || "Done") : "\u2715 " + (result.error || "Failed");
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

function render(s) {
  status = s;
  $("preset").textContent = s.preset || "(no preset)";
  const modeNames = { all: "All presets", favorites: "Favorites only", disabled: "Disabled only" };
  setBadge("badge-mode", modeNames[s.mode] || s.mode, s.mode !== "all", s.mode === "disabled");
  setBadge("badge-locked", s.locked ? "Locked" : "Unlocked", s.locked);
  setBadge("badge-favorite", s.favorite ? "★ Favorite" : "Not a favorite", s.favorite);
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
)PAGE"
        R"PAGE(
// ---- Layout: order and visibility of sections and buttons, saved in projectM for all devices. ----
let layoutRevision = null;
let selected = null; // The button selected in edit mode.
let saveTimer = 0;
const sectionsElement = $("sections");
const sectionIds = (parent) => [...parent.children].map((e) => e.dataset.section);
const itemIds = (grid) => [...grid.children].map((e) => e.dataset.id);
const defaultSections = sectionIds(sectionsElement);
const defaultItems = {};
for (const grid of document.querySelectorAll("[data-grid]")) defaultItems[grid.dataset.grid] = itemIds(grid);

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

function applyLayout(layout) {
  layout = layout && typeof layout === "object" ? layout : {};
  const hiddenSections = Array.isArray(layout.hiddenSections) ? layout.hiddenSections : [];
  for (const id of mergeOrder(defaultSections, layout.sections)) {
    const section = sectionsElement.querySelector('[data-section="' + id + '"]');
    sectionsElement.appendChild(section);
    section.classList.toggle("hidden-item", hiddenSections.includes(id));
  }
  const grids = layout.grids && typeof layout.grids === "object" ? layout.grids : {};
  for (const grid of document.querySelectorAll("[data-grid]")) {
    const saved = grids[grid.dataset.grid] || {};
    const hidden = Array.isArray(saved.hidden) ? saved.hidden : [];
    for (const id of mergeOrder(defaultItems[grid.dataset.grid], saved.order)) {
      const button = grid.querySelector('[data-id="' + id + '"]');
      grid.appendChild(button);
      button.classList.toggle("hidden-item", hidden.includes(id));
    }
  }
  updateTools();
}

function currentLayout() {
  const layout = { version: 1, sections: sectionIds(sectionsElement), hiddenSections: [], grids: {} };
  for (const section of sectionsElement.children) {
    if (section.classList.contains("hidden-item")) layout.hiddenSections.push(section.dataset.section);
  }
  for (const grid of document.querySelectorAll("[data-grid]")) {
    layout.grids[grid.dataset.grid] = {
      order: itemIds(grid),
      hidden: [...grid.children].filter((b) => b.classList.contains("hidden-item")).map((b) => b.dataset.id),
    };
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
    showConnection(false);
  }
}

function layoutChanged() {
  updateTools();
  clearTimeout(saveTimer);
  saveTimer = setTimeout(() => saveLayout(currentLayout()), 300);
}

function toolButton(text, label, onClick) {
  const button = document.createElement("button");
  button.textContent = text;
  button.setAttribute("aria-label", label);
  button.addEventListener("click", onClick);
  return button;
}

function updateTools() {
  for (const section of sectionsElement.children) {
    const tools = section.querySelector(".tools");
    tools.textContent = "";
    tools.append(
      toolButton("▲", "Move up", () => { if (section.previousElementSibling) { sectionsElement.insertBefore(section, section.previousElementSibling); layoutChanged(); } }),
      toolButton("▼", "Move down", () => { if (section.nextElementSibling) { sectionsElement.insertBefore(section.nextElementSibling, section); layoutChanged(); } }),
      toolButton(section.classList.contains("hidden-item") ? "Show" : "Hide", "Hide or show", () => { section.classList.toggle("hidden-item"); layoutChanged(); }));
  }
  for (const grid of document.querySelectorAll("[data-grid]")) {
    let tools = grid.nextElementSibling;
    if (!tools || !tools.classList.contains("item-tools")) {
      tools = document.createElement("div");
      tools.className = "item-tools";
      grid.after(tools);
    }
    const mine = selected && selected.parentElement === grid;
    tools.textContent = "";
    tools.append(
      toolButton("◀ Move", "Move left", () => { if (mine && selected.previousElementSibling) { grid.insertBefore(selected, selected.previousElementSibling); layoutChanged(); } }),
      toolButton("Move ▶", "Move right", () => { if (mine && selected.nextElementSibling) { grid.insertBefore(selected.nextElementSibling, selected); layoutChanged(); } }),
      toolButton(mine && selected.classList.contains("hidden-item") ? "Show" : "Hide", "Hide or show", () => { if (mine) { selected.classList.toggle("hidden-item"); layoutChanged(); } }));
    for (const button of tools.children) button.disabled = !mine;
  }
}

function setEditing(on) {
  editing = on;
  main.classList.toggle("edit", on);
  $("layout-edit").textContent = on ? "Done" : "Edit layout";
  $("layout-edit").classList.toggle("on", on);
  if (selected) selected.classList.remove("selected");
  selected = null;
  updateTools();
  if (!on) { clearTimeout(saveTimer); saveLayout(currentLayout()); }
}

$("layout-edit").addEventListener("click", () => setEditing(!editing));
$("layout-reset").addEventListener("click", () => {
  if (!confirm("Reset the layout to the default for all devices?")) return;
  clearTimeout(saveTimer);
  applyLayout({});
  saveLayout({});
});

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
}

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

#!/usr/bin/env node

"use strict";

const fs = require("node:fs");
const vm = require("node:vm");

const pagePath = process.argv[2] || "site/index.html";
const html = fs.readFileSync(pagePath, "utf8");
const scripts = Array.from(html.matchAll(/<script(?:\s[^>]*)?>([\s\S]*?)<\/script>/g));
const source = scripts.at(-1)?.[1];

function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

assert(source && source.includes("startMachine"), "inline player controller not found");
assert(html.includes('value="spectrum"'), "Spectrum selector is missing");
assert(html.includes('value="timex"'), "Timex selector is missing");
assert(html.includes("https://buymeacoffee.com/mpasternak"), "coffee link is missing");
assert(!html.includes("sponsor-link"), "ad-filtered sponsor-link class returned");

const inputs = ["spectrum", "timex"].map((value) => ({
  checked: value === "spectrum",
  disabled: false,
  listeners: {},
  value,
  addEventListener(type, listener) {
    this.listeners[type] = listener;
  }
}));
const picker = {
  attributes: {},
  querySelectorAll() {
    return inputs;
  },
  setAttribute(name, value) {
    this.attributes[name] = value;
  }
};
const status = { dataset: {}, textContent: "" };
const note = { textContent: "" };
const host = { textContent: "" };
const elements = {
  "emulator-status": status,
  jsspeccy: host,
  "machine-picker": picker,
  "machine-note": note
};
const resizeListeners = [];
const pendingTimers = new Map();
const instances = [];
let nextTimer = 1;

const documentMock = {
  fullscreenElement: null,
  getElementById(id) {
    return elements[id];
  }
};
const windowMock = {
  innerWidth: 1000,
  addEventListener(type, listener) {
    if (type === "resize") {
      resizeListeners.push(listener);
    }
  },
  clearTimeout(id) {
    pendingTimers.delete(id);
  },
  setTimeout(listener) {
    const id = nextTimer;
    nextTimer += 1;
    pendingTimers.set(id, listener);
    return id;
  }
};

function JSSpeccyMock(container, options) {
  const instance = {
    container,
    exitCalls: 0,
    options,
    readyCallback: null,
    zoomCalls: [],
    exit() {
      this.exitCalls += 1;
    },
    onReady(callback) {
      this.readyCallback = callback;
    },
    setZoom(zoom) {
      this.zoomCalls.push(zoom);
    }
  };
  instances.push(instance);
  return instance;
}

vm.runInNewContext(source, {
  document: documentMock,
  JSSpeccy: JSSpeccyMock,
  window: windowMock
});

assert(instances.length === 1, "default emulator was not created exactly once");
assert(instances[0].options.machine === 48, "default machine is not Spectrum 48K");
assert(instances[0].options.openUrl === "zx-bsd-robots-48k.tap", "wrong Spectrum TAP");
assert(instances[0].options.joystickEnabled === false, "joystick must remain disabled");
assert(picker.attributes["aria-busy"] === "true", "selector startup state is missing");
assert(inputs.every((input) => !input.disabled), "startup must not steal focus by disabling radios");

instances[0].readyCallback();
assert(status.textContent.includes("Spectrum 48K is initialized"), "wrong Spectrum status");
assert(picker.attributes["aria-busy"] === "false", "selector busy state was not cleared");

inputs[0].checked = false;
inputs[1].checked = true;
inputs[1].listeners.change();
assert(instances[0].exitCalls === 1, "old Spectrum instance was not stopped");
assert(instances.length === 2, "Timex emulator was not created");
assert(instances[1].options.machine === 2048, "Timex selector did not request TC2048");
assert(instances[1].options.openUrl === "zx-bsd-robots-timex-512.tap", "wrong Timex TAP");
assert(note.textContent.includes("512×192 hi-res"), "Timex machine note was not shown");
assert(inputs.every((input) => !input.disabled), "busy selector must remain keyboard-focusable");

inputs[1].checked = false;
inputs[0].checked = true;
inputs[0].listeners.change();
assert(instances[1].exitCalls === 1, "old Timex instance was not stopped");
assert(instances.length === 3, "Spectrum emulator was not recreated");
const statusBeforeStaleCallback = status.textContent;
instances[1].readyCallback();
assert(status.textContent === statusBeforeStaleCallback, "stale callback replaced current status");
instances[2].readyCallback();

inputs[0].checked = false;
inputs[1].checked = true;
inputs[1].listeners.change();
assert(instances.length === 4, "Timex emulator was not recreated after rapid switch");
instances[3].readyCallback();
assert(status.textContent.includes("Timex TC2048 is initialized"), "wrong Timex status");

assert(resizeListeners.length === 1, "resize listener must be installed exactly once");
windowMock.innerWidth = 719;
resizeListeners[0]();
assert(instances[3].zoomCalls.at(-1) === 1, "responsive zoom did not update current emulator");
documentMock.fullscreenElement = {};
windowMock.innerWidth = 1000;
resizeListeners[0]();
assert(instances[3].zoomCalls.length === 1, "fullscreen resize changed emulator zoom");

console.log("Pages player controller tests passed");

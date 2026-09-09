#!/usr/bin/env node

import assert from "node:assert/strict";
import { mkdir, writeFile } from "node:fs/promises";
import path from "node:path";

const baseUrl = process.env.SETUP_BASE_URL ?? "http://127.0.0.1:8765/";
const cdpPort = Number(process.env.SETUP_CDP_PORT ?? 9225);
const screenshotDir = path.resolve(
  process.env.SETUP_SCREENSHOT_DIR ?? ".local/setup-ui-qa",
);
const cdpBase = "http://127.0.0.1:" + cdpPort;

const wait = (milliseconds) =>
  new Promise((resolve) => setTimeout(resolve, milliseconds));

async function getPageTarget() {
  const targets = await (await fetch(cdpBase + "/json")).json();
  let page = targets.find(
    (target) => target.type === "page" && target.url.startsWith(baseUrl),
  );
  if (!page) {
    page = await (
      await fetch(cdpBase + "/json/new?" + baseUrl, { method: "PUT" })
    ).json();
  }
  return page;
}

async function openCdp() {
  const target = await getPageTarget();
  const socket = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => {
    socket.addEventListener("open", resolve, { once: true });
    socket.addEventListener("error", reject, { once: true });
  });

  let nextId = 0;
  const pending = new Map();
  const events = [];
  socket.addEventListener("message", (event) => {
    const message = JSON.parse(event.data);
    if (message.id && pending.has(message.id)) {
      const request = pending.get(message.id);
      pending.delete(message.id);
      if (message.error) request.reject(new Error(message.error.message));
      else request.resolve(message.result ?? {});
    } else {
      events.push(message);
    }
  });

  const send = (method, params = {}) =>
    new Promise((resolve, reject) => {
      const id = ++nextId;
      pending.set(id, { resolve, reject });
      socket.send(JSON.stringify({ id, method, params }));
    });

  const evaluate = async (expression, awaitPromise = false) => {
    const result = await send("Runtime.evaluate", {
      expression,
      awaitPromise,
      returnByValue: true,
    });
    if (result.exceptionDetails) {
      throw new Error(
        result.exceptionDetails.exception?.description ??
          result.exceptionDetails.text ??
          "Runtime evaluation failed",
      );
    }
    return result.result?.value;
  };

  return { events, evaluate, send, socket };
}

async function saveScreenshot(cdp, filename) {
  const screenshot = await cdp.send("Page.captureScreenshot", {
    captureBeyondViewport: true,
    fromSurface: true,
    format: "png",
  });
  const destination = path.join(screenshotDir, filename);
  await mkdir(screenshotDir, { recursive: true });
  await writeFile(destination, Buffer.from(screenshot.data, "base64"));
  return destination;
}

async function main() {
  const cdp = await openCdp();
  try {
    await cdp.send("Runtime.enable");
    await cdp.send("Page.enable");
    await cdp.send("Page.navigate", { url: baseUrl });
    for (let attempt = 0; attempt < 60; attempt += 1) {
      try {
        if (
          await cdp.evaluate(
            'document.readyState === "complete" && document.querySelector("#timeout") !== null',
          )
        ) {
          break;
        }
      } catch (error) {
        // The renderer may briefly reject evaluations while navigation commits.
      }
      await wait(100);
    }

    const initialExpression =
      '(() => ({authHref: [...document.querySelectorAll("a")].find((link) => link.href === "https://auth.openai.com/codex/device").href, credentialsNotice: document.querySelectorAll(".security-note")[1].textContent, help: document.querySelectorAll(".security-note")[0].textContent, language: document.documentElement.lang, optionCount: document.querySelectorAll("#timeout option").length, scrollWidth: document.documentElement.scrollWidth, viewportWidth: window.innerWidth}))()';
    const initial = await cdp.evaluate(initialExpression);
    assert.equal(initial.language, "ja");
    assert.equal(initial.authHref, "https://auth.openai.com/codex/device");
    assert.match(initial.help, /インターネット/);
    assert.match(initial.credentialsNotice, /ChatGPT/);
    assert.equal(initial.optionCount, 9);
    assert.ok(initial.scrollWidth <= initial.viewportWidth);
    const desktopScreenshot = await saveScreenshot(cdp, "setup-desktop.png");

    await cdp.send("Emulation.setDeviceMetricsOverride", {
      deviceScaleFactor: 1,
      height: 900,
      mobile: true,
      width: 320,
    });
    const mobile = await cdp.evaluate(
      "(() => ({scrollWidth: document.documentElement.scrollWidth, viewportWidth: window.innerWidth}))()",
    );
    assert.ok(mobile.scrollWidth <= 320);
    const mobileScreenshot = await saveScreenshot(cdp, "setup-320.png");
    await cdp.send("Emulation.clearDeviceMetricsOverride");

    await cdp.evaluate('document.getElementById("language-toggle").click()');
    await wait(20);
    const english = await cdp.evaluate(
      '(() => ({credentialsNotice: document.querySelectorAll(".security-note")[1].textContent, help: document.querySelectorAll(".security-note")[0].textContent, language: document.documentElement.lang}))()',
    );
    assert.equal(english.language, "en");
    assert.match(english.help, /internet-connected/);
    assert.match(english.credentialsNotice, /ChatGPT/);
    await cdp.evaluate('document.getElementById("language-toggle").click()');

    await cdp.evaluate('document.getElementById("wifi-submit").click()');
    await wait(20);
    const validation = await cdp.evaluate(
      '(() => ({focused: document.activeElement.id, message: document.getElementById("wifi-message").textContent}))()',
    );
    assert.equal(validation.focused, "ssid");
    assert.match(validation.message, /Wi.?Fi/);

    await cdp.evaluate('(() => { document.getElementById("ssid").value = "Test-2G"; document.getElementById("password").value = "fake-password"; document.getElementById("wifi-form").requestSubmit(); })()');
    await wait(150);
    const wifi = await cdp.evaluate(
      '(() => ({connected: document.getElementById("wifi-state").textContent, ssid: document.getElementById("ssid").value}))()',
    );
    assert.equal(wifi.ssid, "Test-2G");
    assert.match(wifi.connected, /接続済み|Connected/);

    await cdp.evaluate('document.getElementById("login-submit").click()');
    await wait(150);
    const login = await cdp.evaluate(
      '(() => ({authenticated: document.getElementById("login-state").textContent, codeVisible: document.getElementById("device-code").textContent.trim().length > 0, copyEnabled: !document.getElementById("copy-code").disabled}))()',
    );
    assert.match(login.authenticated, /認証済み|Authenticated/);
    assert.equal(login.codeVisible, true);
    assert.equal(login.copyEnabled, true);

    await cdp.evaluate('(() => { document.getElementById("timeout").value = "7200000"; document.getElementById("timeout-form").requestSubmit(); })()');
    await wait(150);
    const timeout = await cdp.evaluate(
      '(() => ({message: document.getElementById("timeout-message").textContent, value: document.getElementById("timeout").value}))()',
    );
    assert.equal(timeout.value, "7200000");
    assert.match(timeout.message, /保存|saved/i);

    const consoleEvents = cdp.events.filter(
      (event) => event.method === "Runtime.consoleAPICalled",
    );
    console.log(
      JSON.stringify({
        desktopScreenshot,
        mobileScreenshot,
        checks: {
          desktopNoHorizontalOverflow: true,
          englishSwitch: true,
          loginFlow: true,
          mobileNoHorizontalOverflow: true,
          timeoutFlow: true,
          validationFocus: true,
          wifiFlow: true,
        },
        consoleEvents: consoleEvents.length,
      }),
    );
  } finally {
    await cdp.send("Emulation.clearDeviceMetricsOverride").catch(() => {});
    cdp.socket.close();
  }
}

main().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});

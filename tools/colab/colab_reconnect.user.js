// ==UserScript==
// @name         Colab: press "Reconnect" after an idle disconnect
// @namespace    openbfme
// @version      1.0
// @description  Every CHECK_MINUTES the open Colab tab is searched for the idle-disconnect dialog's "Reconnect" / "Подключиться повторно" button (and the toolbar's "Connect" when the runtime is off); a button found is pressed. Logs each press to the console.
// @match        https://colab.research.google.com/*
// @grant        none
// @run-at       document-idle
// ==/UserScript==

// The OpenBFME sandboxes run in the owner's Colab Pro notebooks (tools/colab/autostart.sh). Colab drops an idle runtime and shows the dialog
// "Подключение к среде выполнения прервано" with the buttons "Закрыть" and "Подключиться повторно" ("Close" / "Reconnect" in English). The dialog
// and the toolbar's connect button live inside web components (shadow roots), so a plain document.querySelector does not see them: the search walks
// every open shadow root. "Reconnect" continues the same session (the owner, 2026-11-16): nothing is re-run after it.
(function () {
  "use strict";

  const CHECK_MINUTES = 15; // how often to look (the owner's choice, 2026-11-16); 1 reconnects sooner
  // the dialog's button and the toolbar's connect button, in Russian and English; matched on the button's whole visible text
  const PRESS = [/^подключиться повторно$/i, /^reconnect$/i, /^подключиться$/i, /^connect$/i];

  function* allElements(root) {
    const stack = [root];
    while (stack.length) {
      const node = stack.pop();
      if (!node) continue;
      if (node.shadowRoot) stack.push(node.shadowRoot);
      const kids = node.children || [];
      for (let i = kids.length - 1; i >= 0; i--) stack.push(kids[i]);
      if (node.nodeType === 1) yield node;
    }
  }

  function textOf(el) {
    return (el.innerText || el.textContent || el.getAttribute("label") || "").replace(/\s+/g, " ").trim();
  }

  function visible(el) {
    const r = el.getBoundingClientRect();
    return r.width > 0 && r.height > 0;
  }

  function findButton() {
    for (const el of allElements(document)) {
      const tag = el.tagName.toLowerCase();
      const isButton = tag === "button" || tag.endsWith("-button") || el.getAttribute("role") === "button";
      if (!isButton || !visible(el)) continue;
      const text = textOf(el);
      if (PRESS.some((rx) => rx.test(text))) return { el, text };
    }
    return null;
  }

  function check() {
    const hit = findButton();
    const now = new Date().toLocaleTimeString();
    if (hit) {
      console.log(`[colab-reconnect] ${now}: pressing "${hit.text}"`);
      hit.el.click();
    } else {
      console.log(`[colab-reconnect] ${now}: connected, nothing to press`);
    }
  }

  console.log(`[colab-reconnect] watching every ${CHECK_MINUTES} min`);
  setInterval(check, CHECK_MINUTES * 60 * 1000);
  setTimeout(check, 10 * 1000); // once soon after the page loads
})();

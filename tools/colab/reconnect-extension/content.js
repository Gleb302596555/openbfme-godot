// Colab Reconnect (OpenBFME): the OpenBFME sandboxes run in the owner's Colab Pro notebooks. Colab drops an idle session with the dialog
// "Подключение к среде выполнения прервано" ("Закрыть" / "Подключиться повторно"; in English "Close" / "Reconnect"). Pressing "Reconnect"
// continues the same session; leaving it stops the session. This content script presses it as soon as it shows (a DOM observer) and, as a
// fallback, checks every CHECK_SECONDS. The dialog's buttons live inside web components (shadow roots): the search walks every open shadow root.
(() => {
  "use strict";

  const CHECK_SECONDS = 60;
  // the dialog's button, Russian and English, matched on the button's whole visible text
  const PRESS = [/^подключиться повторно$/i, /^reconnect$/i];
  let lastPress = 0;

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

  const textOf = (el) => (el.innerText || el.textContent || el.getAttribute("label") || "").replace(/\s+/g, " ").trim();
  const visible = (el) => {
    const r = el.getBoundingClientRect();
    return r.width > 0 && r.height > 0;
  };

  function findButton() {
    for (const el of allElements(document)) {
      const tag = el.tagName.toLowerCase();
      if (!(tag === "button" || tag.endsWith("-button") || el.getAttribute("role") === "button") || !visible(el)) continue;
      const text = textOf(el);
      if (PRESS.some((rx) => rx.test(text))) return { el, text };
    }
    return null;
  }

  function record(text) {
    const entry = { at: new Date().toISOString(), text, page: document.title || location.pathname };
    try {
      chrome.storage.local.get({ presses: [] }, (d) => {
        const presses = [entry, ...(d.presses || [])].slice(0, 50);
        chrome.storage.local.set({ presses });
      });
    } catch (e) {
      // the extension was reloaded while the tab stayed open: the press still happened
    }
  }

  function check(why) {
    if (Date.now() - lastPress < 5000) return; // one press per dialog
    const hit = findButton();
    if (!hit) return;
    lastPress = Date.now();
    console.log(`[colab-reconnect] ${new Date().toLocaleTimeString()} (${why}): pressing "${hit.text}"`);
    hit.el.click();
    record(hit.text);
  }

  // the dialog is added to the page when the session drops: react at once (debounced, the page changes often)
  let pending = null;
  new MutationObserver(() => {
    if (pending) return;
    pending = setTimeout(() => {
      pending = null;
      check("dialog");
    }, 1000);
  }).observe(document.documentElement, { childList: true, subtree: true });

  setInterval(() => check("timer"), CHECK_SECONDS * 1000);
  setTimeout(() => check("load"), 5000);
  console.log("[colab-reconnect] watching this Colab tab");
})();

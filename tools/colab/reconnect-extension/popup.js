// The extension's popup: how many times and when the content script pressed "Reconnect" (the last 50 presses, chrome.storage.local).
function render() {
  chrome.storage.local.get({ presses: [] }, (d) => {
    const presses = d.presses || [];
    document.getElementById("summary").textContent = presses.length
      ? `Нажатий: ${presses.length}. Последнее: ${new Date(presses[0].at).toLocaleString()}`
      : "Ещё ни одного нажатия.";
    const list = document.getElementById("list");
    list.textContent = "";
    for (const p of presses) {
      const li = document.createElement("li");
      li.textContent = `${new Date(p.at).toLocaleString()} — ${p.page}`;
      list.appendChild(li);
    }
  });
}
document.getElementById("clear").addEventListener("click", () => chrome.storage.local.set({ presses: [] }, render));
render();

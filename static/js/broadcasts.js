let broadcastTags = [];
let broadcastInclude = [];
let broadcastExclude = [];
let broadcastPickMode = "include";
let broadcastAttachmentType = "photo";
let broadcastFile = null;

document.addEventListener("DOMContentLoaded", () => {
    initBroadcasts();
});

function initBroadcasts() {
    if (!document.querySelector(".broadcast-page")) return;

    bindBroadcastButtons();
    bindBroadcastComposer();
    loadBroadcastTags();
    renderBroadcastChips();
    updateBroadcastPreview();
}

function bindBroadcastButtons() {
    const includeBtn = document.getElementById("broadcast-open-include");
    const excludeBtn = document.getElementById("broadcast-open-exclude");
    const closeBtn = document.getElementById("broadcast-close-modal");
    const modal = document.getElementById("broadcast-tag-modal");
    const search = document.getElementById("broadcast-tag-search");

    if (includeBtn) {
        includeBtn.onclick = () => openBroadcastTagModal("include");
    }

    if (excludeBtn) {
        excludeBtn.onclick = () => openBroadcastTagModal("exclude");
    }

    if (closeBtn && modal) {
        closeBtn.onclick = () => modal.classList.add("hidden");
    }

    if (modal) {
        modal.addEventListener("click", (e) => {
            if (e.target === modal) modal.classList.add("hidden");
        });
    }

    if (search) {
        search.oninput = () => renderBroadcastTagPicker();
    }

    const previewBtn = document.getElementById("broadcast-preview-btn");
    if (previewBtn) {
        previewBtn.onclick = previewBroadcastCount;
    }

    const startBtn = document.getElementById("broadcast-start-btn");
    if (startBtn) {
        startBtn.onclick = startBroadcastStub;
    }
}

function bindBroadcastComposer() {
    const text = document.getElementById("broadcast-text");
    const pickFileBtn = document.getElementById("broadcast-pick-file");
    const fileInput = document.getElementById("broadcast-file");

    if (text) {
        text.oninput = updateBroadcastPreview;
    }

    document.querySelectorAll(".attach-type").forEach(btn => {
        btn.onclick = () => {
            document.querySelectorAll(".attach-type").forEach(x => x.classList.remove("active"));
            btn.classList.add("active");
            broadcastAttachmentType = btn.dataset.type || "photo";
            updateBroadcastPreview();
        };
    });

    if (pickFileBtn && fileInput) {
        pickFileBtn.onclick = () => fileInput.click();

        fileInput.onchange = () => {
            broadcastFile = fileInput.files && fileInput.files[0] ? fileInput.files[0] : null;

            const nameBox = document.getElementById("broadcast-file-name");
            if (nameBox) {
                nameBox.textContent = broadcastFile ? broadcastFile.name : "Файл не выбран";
            }

            updateBroadcastPreview();
        };
    }
}

async function loadBroadcastTags() {
    try {
        const res = await fetch("/api/broadcast/tags");
        const json = await res.json();

        const tags = Array.isArray(json) ? json : (json.tags || []);

        broadcastTags = tags.map(t => ({
            name: t.name || t.tag || "",
            color: t.color || t.bg_color || "#334155",
            textColor: t.text_color || "#ffffff"
        })).filter(t => t.name);

        renderBroadcastTagPicker();
    } catch (e) {
        console.log("broadcast tags load error", e);
        broadcastTags = [];
        renderBroadcastTagPicker();
    }
}

function openBroadcastTagModal(mode) {
    broadcastPickMode = mode;

    const title = document.getElementById("broadcast-modal-title");
    const modal = document.getElementById("broadcast-tag-modal");
    const search = document.getElementById("broadcast-tag-search");

    if (title) {
        title.textContent = mode === "include"
            ? "Добавить INCLUDE теги"
            : "Добавить EXCLUDE теги";
    }

    if (search) search.value = "";

    renderBroadcastTagPicker();

    if (modal) modal.classList.remove("hidden");
}

function renderBroadcastTagPicker() {
    const box = document.getElementById("broadcast-tag-picker");
    if (!box) return;

    const search = (document.getElementById("broadcast-tag-search")?.value || "").toLowerCase().trim();

    const list = broadcastTags.filter(t => {
        if (!search) return true;
        return t.name.toLowerCase().includes(search);
    });

    if (list.length === 0) {
        box.innerHTML = `<div class="empty-chip">Теги не найдены</div>`;
        return;
    }

    box.innerHTML = list.map(t => `
        <button class="broadcast-picker-chip"
            style="background:${safeBroadcastAttr(t.color)}; color:${safeBroadcastAttr(t.textColor || "#ffffff")}"
            data-tag="${safeBroadcastAttr(t.name)}">
            ${safeBroadcast(t.name)}
        </button>
    `).join("");

    box.querySelectorAll(".broadcast-picker-chip").forEach(btn => {
        btn.onclick = () => {
            const tag = btn.dataset.tag;
            addBroadcastTag(tag);
        };
    });
}

function addBroadcastTag(tag) {
    if (!tag) return;

    if (broadcastPickMode === "include") {
        if (!broadcastInclude.includes(tag)) broadcastInclude.push(tag);
        broadcastExclude = broadcastExclude.filter(x => x !== tag);
    } else {
        if (!broadcastExclude.includes(tag)) broadcastExclude.push(tag);
        broadcastInclude = broadcastInclude.filter(x => x !== tag);
    }

    renderBroadcastChips();
    updateBroadcastPreview();
}

function removeBroadcastTag(mode, tag) {
    if (mode === "include") {
        broadcastInclude = broadcastInclude.filter(x => x !== tag);
    } else {
        broadcastExclude = broadcastExclude.filter(x => x !== tag);
    }

    renderBroadcastChips();
}

function renderBroadcastChips() {
    const includeBox = document.getElementById("broadcast-include-tags");
    const excludeBox = document.getElementById("broadcast-exclude-tags");

    if (includeBox) {
        includeBox.innerHTML = broadcastInclude.length
            ? broadcastInclude.map(t => broadcastChipHtml(t, "include")).join("")
            : `<span class="empty-chip">Теги не выбраны</span>`;
    }

    if (excludeBox) {
        excludeBox.innerHTML = broadcastExclude.length
            ? broadcastExclude.map(t => broadcastChipHtml(t, "exclude")).join("")
            : `<span class="empty-chip">Исключений нет</span>`;
    }

    document.querySelectorAll("[data-broadcast-remove]").forEach(btn => {
        btn.onclick = () => {
            removeBroadcastTag(btn.dataset.mode, btn.dataset.tag);
        };
    });
}

function broadcastChipHtml(tag, mode) {
    const found = broadcastTags.find(t => t.name === tag);
    const bg = found ? found.color : "#334155";
    const text = found ? found.textColor : "#ffffff";

    return `
        <span class="broadcast-chip" style="background:${safeBroadcastAttr(bg)}; color:${safeBroadcastAttr(text)}">
            ${safeBroadcast(tag)}
            <button data-broadcast-remove="1"
                    data-mode="${safeBroadcastAttr(mode)}"
                    data-tag="${safeBroadcastAttr(tag)}">×</button>
        </span>
    `;
}

function getBroadcastTagColor(tag) {
    const found = broadcastTags.find(t => t.name === tag);
    return found ? found.color : "#334155";
}

function updateBroadcastPreview() {
    const text = document.getElementById("broadcast-text")?.value || "";
    const previewText = document.getElementById("broadcast-preview-text");
    const previewFile = document.getElementById("broadcast-preview-file");

    if (previewText) {
        previewText.textContent = text.trim() || "Текст рассылки появится здесь";
    }

    if (previewFile) {
        if (broadcastFile) {
            previewFile.classList.remove("hidden");
            previewFile.textContent = `${broadcastAttachmentType}: ${broadcastFile.name}`;
        } else {
            previewFile.classList.add("hidden");
            previewFile.textContent = "";
        }
    }
}

async function previewBroadcastCount() {
    const btn = document.getElementById("broadcast-preview-btn");
    const countBox = document.getElementById("broadcast-count");
    const textBox = document.getElementById("broadcast-count-text");

    if (btn) btn.textContent = "Считаю...";

    const payload = getBroadcastPayload();

    try {
        const res = await fetch("/api/broadcast/preview", {
            method: "POST",
            headers: { "Content-Type": "application/json" },
            body: JSON.stringify(payload)
        });

        const json = await res.json();

        if (!json.ok) {
            if (countBox) countBox.textContent = "—";
            if (textBox) textBox.textContent = "Backend preview ещё не подключен";
            return;
        }

        if (countBox) countBox.textContent = json.count ?? 0;
        if (textBox) textBox.textContent = "Лидов попадёт под текущий фильтр";
    } catch (e) {
        if (countBox) countBox.textContent = "—";
        if (textBox) textBox.textContent = "Preview API ещё не готов";
    } finally {
        if (btn) btn.textContent = "Показать сколько лидов попадет";
    }
}

async function startBroadcastStub() {
    const payload = getBroadcastPayload();

    if (!payload.text.trim() && !broadcastFile) {
        alert("Напиши текст или выбери файл");
        return;
    }

    if (!confirm("Запустить рассылку по выбранным фильтрам?")) {
        return;
    }

    const form = new FormData();
    form.append("include", JSON.stringify(payload.include));
    form.append("exclude", JSON.stringify(payload.exclude));
    form.append("date_from", payload.date_from);
    form.append("date_to", payload.date_to);
    form.append("text", payload.text);
    form.append("attachment_type", payload.attachment_type);

    if (broadcastFile) {
        form.append("file", broadcastFile);
    }

    const btn = document.getElementById("broadcast-start-btn");
    const textBox = document.getElementById("broadcast-count-text");

    if (btn) {
        btn.disabled = true;
        btn.textContent = "Рассылка идет...";
    }

    try {
        const res = await fetch("/api/broadcast/start", {
            method: "POST",
            body: form
        });

        const json = await res.json();

        if (!json.ok) {
            alert(json.error || "Ошибка запуска рассылки");
            return;
        }

        if (textBox) {
            textBox.textContent = `Готово: отправлено ${json.sent}, ошибок ${json.failed}, всего ${json.total}`;
        }

        alert(`Рассылка завершена\nОтправлено: ${json.sent}\nОшибок: ${json.failed}\nВсего: ${json.total}`);
    } catch (e) {
        alert("Ошибка запроса /api/broadcast/start");
    } finally {
        if (btn) {
            btn.disabled = false;
            btn.textContent = "Запустить рассылку";
        }
    }
}

function getBroadcastPayload() {
    return {
        include: broadcastInclude,
        exclude: broadcastExclude,
        date_from: document.getElementById("broadcast-date-from")?.value || "",
        date_to: document.getElementById("broadcast-date-to")?.value || "",
        text: document.getElementById("broadcast-text")?.value || "",
        attachment_type: broadcastAttachmentType
    };
}

function safeBroadcast(value) {
    return String(value ?? "")
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;");
}

function safeBroadcastAttr(value) {
    return safeBroadcast(value).replaceAll("'", "&#039;");
}
let currentFunnelId = null;

async function funnelPost(url, data) {
    const res = await fetch(url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(data || {})
    });
    return await res.json();
}

async function loadFunnels() {
    const res = await fetch("/api/funnels");
    const funnels = await res.json();

    const list = document.getElementById("funnels-list");
    if (!list) return;

    list.innerHTML = "";

    funnels.forEach(f => {
        const item = document.createElement("div");
        item.className = "funnel-card";
        item.innerHTML = `
            <div class="funnel-name">${safe(f.name)}</div>
            <div class="funnel-type ${f.type}">${f.type.toUpperCase()}</div>
        `;
        item.onclick = () => openFunnel(f.id);
        list.appendChild(item);
    });
}

async function createFunnel() {
    const name = prompt("Название воронки");
    if (!name) return;

    const type = prompt("Тип: fd или rd", "fd");
    if (!type) return;

    const res = await funnelPost("/api/funnels/create", { name, type });

    await loadFunnels();

    if (res.ok && res.id) {
        await openFunnel(res.id);
    }
}

async function openFunnel(id) {
    currentFunnelId = id;

    const res = await fetch(`/api/funnels/${id}`);
    const funnel = await res.json();

    const editor = document.getElementById("funnels-editor");
    if (!editor) return;

    editor.innerHTML = `
        <div class="funnels-top">
            <button class="back-btn" onclick="closeFunnel()">← Назад</button>
            <button class="edit-btn" onclick="saveFunnelMeta()">✎ Сохранить</button>
        </div>

        <div class="funnels-meta">
            <input id="funnel-name-input" value="${safe(funnel.name)}" placeholder="Название воронки" />
            <select id="funnel-type-input">
                <option value="fd" ${funnel.type === "fd" ? "selected" : ""}>ФД</option>
                <option value="rd" ${funnel.type === "rd" ? "selected" : ""}>РД</option>
            </select>
        </div>

        <div class="steps-wrap">
            ${funnel.steps.map(step => stepHtml(step)).join("")}
        </div>

        <button class="add-step-main" onclick="addFunnelStep()">+ Добавить шаг</button>
    `;
}

function stepHtml(step) {
    return `
        <div class="step-box">
            <div class="step-title">
                <span>${safe(step.title)}</span>
                <span>⌃</span>
            </div>

            ${step.messages.map(msg => funnelMessageHtml(step.id, msg)).join("")}

            <button class="next-step-btn" onclick="addFunnelMessage(${step.id})">
                + Добавить сообщение
            </button>
        </div>
    `;
}

function funnelMessageHtml(stepId, msg) {
    return `
        <div class="msg-row" data-message-id="${msg.id}">
            <input 
    value="${safe(msg.file_path ? `[${msg.message_type}] ${msg.file_path.split(/[\\/]/).pop()}` : (msg.text || ""))}" 
    placeholder="Сообщение..." 
    onchange="saveFunnelMessage(${msg.id})"
    ${msg.file_path ? "readonly" : ""}
/>

            <button type="button" onclick="pickFunnelFile(${msg.id})">📎</button>

            <input 
                type="number" 
                step="0.5" 
                value="${msg.delay_seconds || 2.5}" 
                onchange="saveFunnelMessage(${msg.id})"
            />

            <button type="button" onclick="addFunnelMessage(${stepId})">+</button>
            <button type="button" onclick="deleteFunnelMessage(${msg.id})">🗑</button>
        </div>
    `;
}

async function saveFunnelMeta() {
    if (!currentFunnelId) return;

    const name = document.getElementById("funnel-name-input")?.value || "";
    const type = document.getElementById("funnel-type-input")?.value || "fd";

    await funnelPost("/api/funnels/update", {
        id: Number(currentFunnelId),
        name,
        type
    });

    await loadFunnels();
    await openFunnel(currentFunnelId);
}

async function addFunnelStep() {
    if (!currentFunnelId) return;

    await funnelPost("/api/funnels/add-step", {
        funnel_id: Number(currentFunnelId)
    });

    await openFunnel(currentFunnelId);
}

async function addFunnelMessage(stepId) {
    await funnelPost("/api/funnels/add-message", {
        step_id: Number(stepId)
    });

    await openFunnel(currentFunnelId);
}

async function saveFunnelMessage(messageId) {
    const row = document.querySelector(`.msg-row[data-message-id="${messageId}"]`);
    if (!row) return;

    const inputs = row.querySelectorAll("input");
    const text = inputs[0]?.value || "";
    const delay = parseFloat(inputs[1]?.value || "2.5");

    await funnelPost("/api/funnels/update-message", {
        id: Number(messageId),
        text,
        delay_seconds: delay
    });
}

async function deleteFunnelMessage(messageId) {
    if (!confirm("Удалить сообщение?")) return;

    await funnelPost("/api/funnels/delete-message", {
        id: Number(messageId)
    });

    await openFunnel(currentFunnelId);
}

function closeFunnel() {
    currentFunnelId = null;

    const editor = document.getElementById("funnels-editor");
    if (!editor) return;

    editor.innerHTML = `
        <div class="funnels-empty">
            Выбери или создай воронку
        </div>
    `;
}
async function pickFunnelFile(messageId) {
    const type = prompt("Тип файла: photo / voice / document", "photo");
    if (!type) return;

    const input = document.createElement("input");
    input.type = "file";

    if (type === "photo") {
        input.accept = "image/*";
    } else if (type === "voice") {
        input.accept = "audio/*,.ogg,.oga,.opus";
    }

    input.onchange = async () => {
        if (!input.files || !input.files[0]) return;

        const form = new FormData();
        form.append("message_id", messageId);
        form.append("message_type", type);
        form.append("file", input.files[0]);

        const res = await fetch("/api/funnels/upload-message-file", {
            method: "POST",
            body: form
        });

        const json = await res.json();

        if (!json.ok) {
            alert("Ошибка загрузки файла");
            return;
        }

        await openFunnel(currentFunnelId);
    };

    input.click();
}
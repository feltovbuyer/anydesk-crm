let currentFolder = "unread";
let currentLeadId = null;
let selectedFile = null;
let currentLeadChannel = "";

document.addEventListener("DOMContentLoaded", () => {
    bindNavigation();
    bindFolders();
    bindAdminTabs();
    bindModals();
    bindSaveBot();

    loadBots();
    loadLeads();

    setInterval(loadLeads, 2500);
    setInterval(() => {
    const replyText = document.getElementById("reply-text");
if ((replyText && document.activeElement === replyText) || selectedFile) {
    return;
}

    if (currentLeadId) loadMessages(currentLeadId);
}, 2500);
});

function bindNavigation() {
    document.querySelectorAll(".nav-item").forEach(btn => {
        btn.addEventListener("click", () => {
            document.querySelectorAll(".nav-item").forEach(x => x.classList.remove("active"));
            document.querySelectorAll(".page").forEach(x => x.classList.remove("active"));

            btn.classList.add("active");

            const page = document.getElementById(btn.dataset.page);
            if (page) page.classList.add("active");

            if (btn.dataset.page === "admin-page") loadBots();
        });
    });
}

function bindFolders() {
    document.querySelectorAll(".folder").forEach((btn, index) => {
        btn.addEventListener("click", () => {
            document.querySelectorAll(".folder").forEach(x => x.classList.remove("active"));
            btn.classList.add("active");

            const folders = ["unread", "rd", "fd", "all", "403"];
            currentFolder = folders[index] || "all";

            loadLeads();
        });
    });
}

function bindAdminTabs() {
    document.querySelectorAll(".admin-tab").forEach(btn => {
        btn.addEventListener("click", () => {
            document.querySelectorAll(".admin-tab").forEach(x => x.classList.remove("active"));
            document.querySelectorAll(".admin-box").forEach(x => x.classList.remove("active"));

            btn.classList.add("active");

            const box = document.getElementById(btn.dataset.admin);
            if (box) box.classList.add("active");

            if (btn.dataset.admin === "bots-box") loadBots();
        });
    });
}

function bindModals() {
    bindModal("open-bot-modal", "close-bot-modal", "bot-modal");
    bindModal("open-filter", "close-filter", "filter-panel", true);
    bindModal("open-folder-modal", "close-folder-modal", "folder-modal");
}

function bindModal(openId, closeId, modalId, toggle = false) {
    const open = document.getElementById(openId);
    const close = document.getElementById(closeId);
    const modal = document.getElementById(modalId);

    if (!open || !close || !modal) return;

    open.addEventListener("click", () => {
        if (toggle) modal.classList.toggle("hidden");
        else modal.classList.remove("hidden");
    });

    close.addEventListener("click", () => {
        modal.classList.add("hidden");
    });
}

function bindSaveBot() {
    const saveBotBtn = document.getElementById("save-bot-btn");
    if (!saveBotBtn) return;

    saveBotBtn.addEventListener("click", async () => {
        const body = new URLSearchParams();
        body.append("name", document.getElementById("bot-name").value);
        body.append("token", document.getElementById("bot-token").value);
        body.append("geo", document.getElementById("bot-geo").value);
        body.append("funnel", document.getElementById("bot-funnel").value);

        const res = await fetch("/api/bots/create", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body
        });

        const json = await res.json();

        if (!json.ok) {
            alert("Ошибка сохранения бота");
            return;
        }

        document.getElementById("bot-modal").classList.add("hidden");

        document.getElementById("bot-name").value = "";
        document.getElementById("bot-token").value = "";
        document.getElementById("bot-geo").value = "";
        document.getElementById("bot-funnel").value = "";

        await loadBots();
        alert("Бот сохранен");
    });
}

async function loadBots() {
    const box = document.getElementById("bots-list");
    if (!box) return;

    const res = await fetch("/api/bots");
    const json = await res.json();

    if (!json.ok || !json.bots || json.bots.length === 0) {
        box.innerHTML = `
            <div class="table-empty">
                <div class="empty-title">Ботов пока нет</div>
            </div>
        `;
        return;
    }

    box.innerHTML = `
        <div class="bots-table">
            <div class="bots-head">
                <div>Канал</div>
                <div>GEO</div>
                <div>Воронка</div>
                <div>Статус</div>
            </div>
            ${json.bots.map(bot => `
                <div class="bots-line">
                    <div class="bot-name">${safe(bot.name)}</div>
                    <div>${safe(bot.geo || "-")}</div>
                    <div>${safe(bot.funnel || "-")}</div>
                    <div><span class="status-pill active">ACTIVE</span></div>
                </div>
            `).join("")}
        </div>
    `;
}

async function loadLeads() {
    const holder = document.querySelector(".empty-list");
    if (!holder) return;

    const res = await fetch(`/api/leads?folder=${encodeURIComponent(currentFolder)}`);
    const json = await res.json();

    if (!json.ok || !json.leads || json.leads.length === 0) {
        holder.innerHTML = `
            <div class="empty-title">Лидов пока нет</div>
            <div class="empty-text">В этой папке пока пусто.</div>
        `;
                loadFolderCounts();
        return;
    }

    holder.innerHTML = `
        <div class="lead-list">
            ${json.leads.map(lead => leadHtml(lead)).join("")}
        </div>
    `;

    document.querySelectorAll(".lead-item").forEach(item => {
        item.addEventListener("click", () => {
            document.querySelectorAll(".lead-item").forEach(x => x.classList.remove("active"));
            item.classList.add("active");

            currentLeadId = item.dataset.id;
            currentLeadChannel = lead.channel || "";

            const lead = JSON.parse(decodeURIComponent(item.dataset.lead));
            renderLeadCard(lead);
            loadMessages(currentLeadId);
                loadFolderCounts();
        });
    });
}

function leadHtml(lead) {
    const name = lead.full_name || lead.username || lead.tg_user_id;
    const avatar = name.trim().charAt(0).toUpperCase();
    const unread = Number(lead.unread || 0);
    const data = encodeURIComponent(JSON.stringify(lead));

    return `
        <div class="lead-item" data-id="${lead.id}" data-lead="${data}">
            <div class="avatar">${safe(avatar)}</div>

            <div class="lead-mid">
                <div class="lead-line">
                    <div class="lead-name">${safe(name)}</div>
                    ${unread > 0 ? `<div class="unread-badge">${unread}</div>` : ""}
                </div>

                <div class="lead-msg">${safe(lead.last_message || "Нет сообщений")}</div>

                <div class="lead-tags">
                    ${lead.channel ? `<span class="mini-tag blue">${safe(lead.channel)}</span>` : ""}
                    ${lead.geo ? `<span class="mini-tag green">${safe(lead.geo)}</span>` : ""}
                    ${lead.tags ? `<span class="mini-tag yellow">${safe(lead.tags)}</span>` : ""}
                </div>
            </div>

            <div class="lead-time">${formatTime(lead.last_message_at)}</div>
        </div>
    `;
}

async function loadMessages(leadId) {
    const res = await fetch(`/api/messages?lead_id=${encodeURIComponent(leadId)}`);
    const json = await res.json();

    if (!json.ok) return;

    const chat = document.querySelector(".chat");
    if (!chat) return;

    const messages = json.messages || [];

    chat.innerHTML = `
        <div class="chat-head">
            <div>
                <div class="chat-title">Лид #${leadId}</div>
                <div class="chat-sub">Сообщения из Telegram</div>
            </div>
        </div>

        <div class="messages">
            ${messages.map(m => `
                <div class="msg ${m.sender === "admin" ? "my-msg" : "user-msg"}">
                    <div class="bubble">
    ${m.media_type === "photo"
        ? `<img class="chat-photo" src="/api/tg-file?channel=${encodeURIComponent(currentLeadChannel || "")}&file_id=${encodeURIComponent(m.media_id || "")}">`
        : (m.media_type && m.media_type !== "text" ? `<div class="media-label">📎 ${safe(m.media_type)}</div>` : "")
    }

    ${safe(m.text)}

    <div class="msg-time">${safe(m.created_at)}</div>
</div>
                </div>
            `).join("")}
        </div>

        <div class="composer">
    <button id="attach-btn" class="attach-btn" type="button">📎</button>
    <input id="file-input" type="file" hidden>
    <div id="attached-file" class="attached-file hidden"></div>
    <textarea id="reply-text" placeholder="Написать сообщение..."></textarea>
    <button id="send-reply-btn" type="button">Отправить</button>
</div>
    `;

    const msgBox = chat.querySelector(".messages");
    if (msgBox) msgBox.scrollTop = msgBox.scrollHeight;

    bindSendReply();
    bindSendFile();
}

function bindSendReply() {
    const sendBtn = document.getElementById("send-reply-btn");
    const replyText = document.getElementById("reply-text");
    const fileInput = document.getElementById("file-input");

    if (!sendBtn || !replyText) return;

    sendBtn.onclick = async () => {
        if (!currentLeadId) return;

        const text = replyText.value.trim();
        const file = selectedFile;

        if (!text && !file) return;

        sendBtn.disabled = true;
        sendBtn.textContent = "Отправка...";

        try {
            let res;

            if (file) {
                let mediaType = "document";
                const name = (file.name || "").toLowerCase();

                if (file.type.startsWith("image/")) {
                    mediaType = "photo";
                }
                else if (file.type.startsWith("video/")) {
                    mediaType = "video";
                }
                else if (name.endsWith(".ogg") || name.endsWith(".oga")) {
                    mediaType = "voice";
                }

                const form = new FormData();
                form.append("lead_id", currentLeadId);
                form.append("media_type", mediaType);
                form.append("caption", text);
                form.append("file", file);

                res = await fetch("/api/messages/send-file", {
                    method: "POST",
                    body: form
                });
            }
            else {
                const body = new URLSearchParams();
                body.append("lead_id", currentLeadId);
                body.append("text", text);

                res = await fetch("/api/messages/send", {
                    method: "POST",
                    headers: { "Content-Type": "application/x-www-form-urlencoded" },
                    body
                });
            }

            const json = await res.json();

            if (!json.ok) {
                alert("Не отправилось: " + (json.error || "unknown"));
                return;
            }

            replyText.value = "";
            selectedFile = null;

            if (fileInput) fileInput.value = "";

            const attachedFile = document.getElementById("attached-file");
            if (attachedFile) {
                attachedFile.classList.add("hidden");
                attachedFile.textContent = "";
            }

            await loadMessages(currentLeadId);
            await loadLeads();
        }
        catch (e) {
            alert("Ошибка отправки: " + e.message);
        }
        finally {
            sendBtn.disabled = false;
            sendBtn.textContent = "Отправить";
        }
    };
}


function bindSendFile() {
    const attachBtn = document.getElementById("attach-btn");
    const fileInput = document.getElementById("file-input");
    const attachedFile = document.getElementById("attached-file");

    if (!attachBtn || !fileInput) return;

    attachBtn.onclick = () => {
        fileInput.click();
    };

    fileInput.onchange = () => {
        selectedFile = fileInput.files?.[0];
if (!selectedFile) return;
const file = selectedFile;

        if (attachedFile) {
            attachedFile.classList.remove("hidden");
            attachedFile.textContent = "📎 " + file.name;
        }
    };
}

function renderLeadCard(lead) {
    const right = document.querySelector(".right .card");
    if (!right) return;

    right.innerHTML = `
        <div class="card-title">Карточка лида</div>
        <div class="info-row"><span>ID</span><b>${safe(lead.tg_user_id)}</b></div>
        <div class="info-row"><span>Создан</span><b>${safe(lead.created_at)}</b></div>
        <div class="info-row"><span>Канал</span><b>${safe(lead.channel || "-")}</b></div>
        <div class="info-row"><span>GEO</span><b>${safe(lead.geo || "-")}</b></div>
        <div class="info-row"><span>SubID</span><b>${safe(lead.subid || "-")}</b></div>
        <div class="info-row"><span>Username</span><b>${safe(lead.username || "-")}</b></div>
        <textarea class="comment" placeholder="Комментарий по лиду...">${safe(lead.comment || "")}</textarea>
    `;
}
async function loadFolderCounts() {
    const res = await fetch("/api/folder-counts");
    const json = await res.json();

    if (!json.ok) return;

    const folders = document.querySelectorAll(".folder");

    if (folders[0]) folders[0].querySelector("span").textContent = json.unread ?? 0;
    if (folders[1]) folders[1].querySelector("span").textContent = json.rd ?? 0;
    if (folders[2]) folders[2].querySelector("span").textContent = json.fd ?? 0;
    if (folders[3]) folders[3].querySelector("span").textContent = json.all ?? 0;
    if (folders[4]) folders[4].querySelector("span").textContent = json.bad403 ?? 0;
}

function formatTime(value) {
    if (!value) return "";
    return value.slice(11, 16) || value;
}

function safe(value) {
    return String(value ?? "")
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;");
}
async function loadFolderCounts() {
    const res = await fetch("/api/folder-counts");
    const json = await res.json();

    if (!json.ok) return;

    const folders = document.querySelectorAll(".folder");

    if (folders[0]) folders[0].querySelector("span").textContent = json.unread ?? 0;
    if (folders[1]) folders[1].querySelector("span").textContent = json.rd ?? 0;
    if (folders[2]) folders[2].querySelector("span").textContent = json.fd ?? 0;
    if (folders[3]) folders[3].querySelector("span").textContent = json.all ?? 0;
    if (folders[4]) folders[4].querySelector("span").textContent = json.bad403 ?? 0;
}
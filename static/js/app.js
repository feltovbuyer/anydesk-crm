let currentFolder = "unread";
let currentLeadData = null;
let currentLeadId = null;
let chatRequestSeq = 0;
let selectedFile = null;
let currentLeadChannel = "";
let lastOpenedLeadId = null;
let currentMessages = [];
let lastMessageId = 0;


document.addEventListener("DOMContentLoaded", () => {
    bindNavigation();
    bindFolders();
    bindAdminTabs();
    bindModals();
    bindSaveBot();
    bindSaveStaff();

    loadBots();
    loadLeads();

    setInterval(loadLeads, 2500);
    setInterval(() => {
    if (!currentLeadId) return;

    const replyText = document.getElementById("reply-text");
    if ((replyText && document.activeElement === replyText) || selectedFile) {
        return;
    }

    refreshMessagesAppend();
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
            if (btn.dataset.admin === "staff-box") loadStaff();
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

        const lead = JSON.parse(decodeURIComponent(item.dataset.lead));
        currentLeadData = lead;
        currentLeadChannel = lead.channel || "";
        renderLeadCard(lead);
        if (currentLeadId) {
    loadMessages(currentLeadId);
    }
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
    const requestSeq = ++chatRequestSeq;
    currentLeadId = leadId;

    const chat = document.querySelector(".chat");
    if (!chat) return;

    const msgBoxOld = document.querySelector(".messages");

    const shouldScrollBottom =
        String(leadId) !== String(lastOpenedLeadId) ||
        (
            msgBoxOld &&
            msgBoxOld.scrollHeight - msgBoxOld.scrollTop - msgBoxOld.clientHeight < 120
        );

    lastOpenedLeadId = leadId;

    const res = await fetch(`/api/messages?lead_id=${encodeURIComponent(leadId)}`);
    const json = await res.json();

    if (requestSeq !== chatRequestSeq || String(currentLeadId) !== String(leadId)) {
        return;
    }

    if (!json.ok) return;

    const messages = json.messages || [];
    currentMessages = messages;
    lastMessageId = messages.length ? Number(messages[messages.length - 1].id) : 0;

    const leadName = currentLeadData?.full_name || currentLeadData?.username || "Без имени";
    const leadTgId = currentLeadData?.tg_user_id || "";

    chat.innerHTML = `
        <div class="chat-head">
            <div>
                <div class="chat-lead-name">${safe(leadName)}</div>
                <div class="chat-lead-id">id ${safe(leadTgId)}</div>
            </div>

            <div class="chat-actions">
                <button class="chat-icon-btn" id="macro-btn" title="Макросы">⚡</button>
                <button class="chat-icon-btn" id="transfer-btn" title="Передать чат">↪</button>
                <button class="chat-icon-btn" id="unread-btn" title="Поставить непрочитку">●</button>
            </div>
        </div>

        <div class="messages">
            ${messages.map(m => messageHtml(m)).join("")}
        </div>

        <div class="reply-box">
            <button id="attach-btn">📎</button>
            <input id="reply-text" type="text" placeholder="Сообщение...">
            <button id="send-reply-btn">Отправить</button>
            <input id="file-input" type="file" hidden>
        </div>

        <div id="attached-file" class="hidden"></div>
    `;

    const msgBox = document.querySelector(".messages");

    if (shouldScrollBottom) {
        setTimeout(() => {
            if (msgBox) msgBox.scrollTop = msgBox.scrollHeight;
        }, 80);
    }

    bindSendReply();
    bindSendFile();
    bindChatActions();
}

async function refreshMessagesAppend() {
    if (!currentLeadId) return;

    const msgBox = document.querySelector(".messages");
    if (!msgBox) return;

    const wasNearBottom =
        msgBox.scrollHeight - msgBox.scrollTop - msgBox.clientHeight < 160;

    const res = await fetch(`/api/messages?lead_id=${encodeURIComponent(currentLeadId)}`);
    const json = await res.json();

    if (!json.ok) return;

    const messages = json.messages || [];
    const fresh = messages.filter(m => Number(m.id) > Number(lastMessageId));

    if (fresh.length === 0) return;

    fresh.forEach(m => {
        msgBox.insertAdjacentHTML("beforeend", messageHtml(m));
        lastMessageId = Math.max(lastMessageId, Number(m.id));
    });

    currentMessages = messages;

    if (wasNearBottom) {
        setTimeout(() => {
            msgBox.scrollTop = msgBox.scrollHeight;
        }, 100);
    }
}

function messageHtml(m) {
    const fileUrl =
        `/api/tg-file?channel=${encodeURIComponent(currentLeadChannel || "")}` +
        `&file_id=${encodeURIComponent(m.media_id || "")}`;

    let mediaHtml = "";

    if (m.media_type === "photo") {
    mediaHtml = `
        <img class="chat-photo"
             loading="lazy"
             decoding="async"
             src="${fileUrl}">
        `;
    }
    else if (m.media_type && m.media_type !== "text") {
        mediaHtml = `
            <a class="media-download"
               href="${safe(fileUrl)}"
               download>
               📎 ${safe(m.media_type)}
            </a>
        `;
    }

    return `
        <div class="msg ${m.sender === "admin" ? "my-msg" : "user-msg"}">
            <div class="bubble">
                ${mediaHtml}
                ${safe(m.text || "")}
                <div class="msg-time">${safe(m.created_at || "")}</div>
            </div>
        </div>
    `;
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

            const msgBox = document.querySelector(".messages");
            if (msgBox) {
                msgBox.scrollTop = msgBox.scrollHeight;
            }

            replyText.value = "";
            selectedFile = null;

            if (fileInput) fileInput.value = "";

            const attachedFile = document.getElementById("attached-file");
            if (attachedFile) {
                attachedFile.classList.add("hidden");
                attachedFile.textContent = "";
            }

            await refreshMessagesAppend();
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
document.addEventListener("click", (e) => {
    const img = e.target.closest(".chat-photo");
    if (!img) return;

    let modal = document.getElementById("photo-viewer");

    if (!modal) {
        modal = document.createElement("div");
        modal.id = "photo-viewer";
        modal.className = "photo-viewer";
        modal.innerHTML = `<img id="photo-viewer-img">`;
        document.body.appendChild(modal);

        modal.addEventListener("click", () => {
            modal.classList.remove("active");
        });
    }

    document.getElementById("photo-viewer-img").src = img.src;
    modal.classList.add("active");
});
function bindSaveStaff() {
    const openBtn = document.getElementById("open-staff-modal");
    const closeBtn = document.getElementById("close-staff-modal");
    const modal = document.getElementById("staff-modal");
    const saveBtn = document.getElementById("save-staff-btn");

    if (openBtn && modal) {
        openBtn.addEventListener("click", () => {
            modal.classList.remove("hidden");
        });
    }

    if (closeBtn && modal) {
        closeBtn.addEventListener("click", () => {
            modal.classList.add("hidden");
        });
    }

    if (modal) {
        modal.addEventListener("click", (e) => {
            if (e.target.classList.contains("staff-modal-backdrop")) {
                modal.classList.add("hidden");
            }
        });
    }

    if (!saveBtn) return;

    saveBtn.addEventListener("click", async () => {
        const body = new URLSearchParams();
        body.append("login", document.getElementById("staff-login").value);
        body.append("manager_tag", document.getElementById("staff-tag").value);
        body.append("work_type", document.getElementById("staff-work-type").value);
        body.append("password", document.getElementById("staff-password").value);
        body.append("percent", document.getElementById("staff-percent").value);

        const res = await fetch("/api/staff/save", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body
        });

        const json = await res.json();

        if (!json.ok) {
            alert("Ошибка сохранения сотрудника: " + (json.error || "unknown"));
            return;
        }

        document.getElementById("staff-login").value = "";
        document.getElementById("staff-tag").value = "";
        document.getElementById("staff-password").value = "";
        document.getElementById("staff-percent").value = "";

        if (modal) modal.classList.add("hidden");

        await loadStaff();
    });
}

async function loadStaff() {
    const box = document.getElementById("staff-list");
    if (!box) return;

    const res = await fetch("/api/staff");
    const json = await res.json();

    if (!json.ok || !json.staff || json.staff.length === 0) {
        box.innerHTML = `<div class="staff-empty">Сотрудников пока нет</div>`;
        return;
    }

    box.innerHTML = json.staff.map(s => `
        <div class="staff-card ${s.active === "0" ? "inactive" : ""}">
            <div class="staff-card-top">
                <div class="staff-avatar">
                    ${safe(s.login || "?").slice(0, 1).toUpperCase()}
                </div>

                <div class="staff-main">
                    <div class="staff-name">${safe(s.login)}</div>
                    <div class="staff-sub">Тег: ${safe(s.manager_tag)}</div>
                </div>

                <div class="staff-type ${s.work_type}">
                    ${workTypeLabel(s.work_type)}
                </div>
            </div>

            <div class="staff-meta">
                <div>
                    <span>Пароль</span>
                    <b>${safe(s.password)}</b>
                </div>

                <div>
                    <span>Распределение</span>
                    <b>${safe(s.percent)}%</b>
                </div>
            </div>

            <div class="staff-progress">
                <div style="width:${Number(s.percent || 0)}%"></div>
            </div>

            <div class="staff-actions">
    <button class="staff-edit-btn"
        onclick="editStaff('${safe(s.login)}','${safe(s.manager_tag)}','${safe(s.work_type)}','${safe(s.password)}','${safe(s.percent)}')">
        Редактировать
    </button>

    <button onclick="toggleStaff('${safe(s.login)}')" class="${s.active === "0" ? "staff-enable-btn" : "staff-disable-btn"}">
        ${s.active === "0" ? "Включить" : "Выключить"}
    </button>
</div>
        </div>
    `).join("");
}

function workTypeLabel(type) {
    if (type === "fd") return "ФД";
    if (type === "rd") return "РД";
    return "ФД+РД";
}

async function toggleStaff(login) {
    const body = new URLSearchParams();
    body.append("login", login);

    await fetch("/api/staff/toggle", {
        method: "POST",
        headers: { "Content-Type": "application/x-www-form-urlencoded" },
        body
    });

    await loadStaff();
}
function bindChatActions() {
    const macroBtn = document.getElementById("macro-btn");
    const transferBtn = document.getElementById("transfer-btn");
    const unreadBtn = document.getElementById("unread-btn");

    if (macroBtn) {
        macroBtn.onclick = () => {
            alert("Макросы:\n\nРетен Парагвай\nРетен Нигерия");
        };
    }

    if (transferBtn) {
        transferBtn.onclick = () => {
            alert("Передача чата: следующим патчем сделаем список менеджеров + поиск.");
        };
    }

    if (unreadBtn) {
        unreadBtn.onclick = async () => {
            if (!currentLeadId) return;

            const body = new URLSearchParams();
            body.append("lead_id", currentLeadId);

            const res = await fetch("/api/lead/mark-unread", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body
            });

            const json = await res.json();

            if (!json.ok) {
                alert("Не смог поставить непрочитку");
                return;
            }

            await loadFolderCounts();
            await loadLeads();
        };
    }
}
function lazyLoadChatPhotos() {
    const photos = Array.from(document.querySelectorAll(".photo-lazy[data-src]"));

    photos.forEach((box, i) => {
        setTimeout(() => {
            const src = box.dataset.src;

            const img = new Image();
            img.loading = "lazy";
            img.decoding = "async";
            img.className = "chat-photo";

            img.onload = () => {
                box.innerHTML = "";
                box.appendChild(img);
                box.classList.add("loaded");

                box.onclick = () => {
                    window.open(src, "_blank");
                };
            };

            img.onerror = () => {
                box.innerHTML = `<div class="photo-skeleton">Ошибка загрузки фото</div>`;
            };

            img.src = src;
        }, i * 80);
    });
}
function editStaff(login, tag, workType, password, percent) {
    const modal = document.getElementById("staff-modal");

    document.getElementById("staff-login").value = login;
    document.getElementById("staff-tag").value = tag;
    document.getElementById("staff-work-type").value = workType || "fd_rd";
    document.getElementById("staff-password").value = password;
    document.getElementById("staff-percent").value = percent;

    if (modal) modal.classList.remove("hidden");
}
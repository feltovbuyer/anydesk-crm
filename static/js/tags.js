async function loadTagsAdmin() {
    const box = document.getElementById("tags-box");
    if (!box) return;

    const res = await fetch("/api/tags");
    const tags = await res.json();

    box.innerHTML = `
        <div class="tags-admin-wrap">
            <div class="tags-admin-top">
                <input id="tag-name-input" placeholder="Название тега" />
                <input id="tag-bg-input" type="color" value="#1e293b" />
                <input id="tag-text-input" type="color" value="#ffffff" />
                <button onclick="saveTag()">+ Создать</button>
            </div>

            <div class="tags-admin-list">
                ${tags.map(tag => `
                    <div class="tag-admin-card">
                        <div
                            class="tag-admin-preview"
                            style="
                                background:${tag.bg_color};
                                color:${tag.text_color};
                            "
                        >
                            ${safe(tag.name)}
                        </div>

                        ${
                            tag.system_tag == "1"
                                ? `<div class="tag-admin-system">SYSTEM</div>`
                                : `<button onclick="deleteTag(${tag.id})">Удалить</button>`
                        }
                    </div>
                `).join("")}
            </div>
        </div>
    `;
}

async function saveTag() {
    const name = document.getElementById("tag-name-input")?.value?.trim();
    const bg = document.getElementById("tag-bg-input")?.value || "#1e293b";
    const text = document.getElementById("tag-text-input")?.value || "#ffffff";

    if (!name) return;

    await fetch("/api/tags/save", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({
            name,
            bg_color: bg,
            text_color: text
        })
    });

    loadTagsAdmin();
}

async function deleteTag(id) {
    await fetch("/api/tags/delete", {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify({ id })
    });

    loadTagsAdmin();
}
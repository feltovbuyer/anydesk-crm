#include "Database.h"

#include <iostream>

sqlite3* Database::db = nullptr;

bool Database::init(const std::string& path)
{
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "DB open error: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }

    exec("PRAGMA journal_mode=WAL;");
    exec("PRAGMA foreign_keys=ON;");

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS staff (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            login TEXT UNIQUE NOT NULL,
            password TEXT NOT NULL,
            role TEXT NOT NULL DEFAULT 'manager',
            active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )SQL");

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS staff_distribution (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            staff_login TEXT UNIQUE NOT NULL,
            manager_tag TEXT NOT NULL,
            work_type TEXT NOT NULL DEFAULT 'fd_rd',
            percent INTEGER NOT NULL DEFAULT 0,
            active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )SQL");

    exec(R"SQL(
        INSERT OR IGNORE INTO staff (login, password, role, active)
        VALUES ('feltov', '812', 'admin', 1);
    )SQL");

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS bots (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            token TEXT NOT NULL,
            geo TEXT DEFAULT '',
            funnel TEXT DEFAULT '',
            active INTEGER NOT NULL DEFAULT 1,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )SQL");

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            color TEXT DEFAULT 'blue',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )SQL");

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS user_folders (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            staff_login TEXT NOT NULL,
            name TEXT NOT NULL,
            date_from TEXT DEFAULT '',
            date_to TEXT DEFAULT '',
            tag TEXT DEFAULT '',
            created_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )SQL");

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS leads (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            tg_user_id TEXT UNIQUE NOT NULL,
            username TEXT DEFAULT '',
            full_name TEXT DEFAULT '',
            channel TEXT DEFAULT '',
            geo TEXT DEFAULT '',
            subid TEXT DEFAULT '',
            tags TEXT DEFAULT '',
            comment TEXT DEFAULT '',
            is_403 INTEGER NOT NULL DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            last_message_at TEXT DEFAULT CURRENT_TIMESTAMP
        );
    )SQL");

    auto leadCols = query("PRAGMA table_info(leads)");
    bool hasAssignedStaff = false;

    for (const auto& c : leadCols) {
        if (c.at("name") == "assigned_staff") {
            hasAssignedStaff = true;
            break;
        }
    }

    if (!hasAssignedStaff) {
        exec("ALTER TABLE leads ADD COLUMN assigned_staff TEXT DEFAULT '';");
    }

    auto leadColsDup = query("PRAGMA table_info(leads)");

    bool hasIsDuplicate = false;
    bool hasDuplicateOf = false;
    bool hasAutomationEnabled = false;
    bool hasMainChannel = false;

    for (const auto& c : leadColsDup) {
        std::string name = c.at("name");

        if (name == "is_duplicate") hasIsDuplicate = true;
        if (name == "duplicate_of") hasDuplicateOf = true;
        if (name == "automation_enabled") hasAutomationEnabled = true;
        if (name == "main_channel") hasMainChannel = true;
    }

    if (!hasIsDuplicate) {
        exec("ALTER TABLE leads ADD COLUMN is_duplicate INTEGER DEFAULT 0;");
    }

    if (!hasDuplicateOf) {
        exec("ALTER TABLE leads ADD COLUMN duplicate_of INTEGER DEFAULT 0;");
    }

    if (!hasAutomationEnabled) {
        exec("ALTER TABLE leads ADD COLUMN automation_enabled INTEGER DEFAULT 1;");
    }

    if (!hasMainChannel) {
        exec("ALTER TABLE leads ADD COLUMN main_channel TEXT DEFAULT '';");
    }

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS messages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            lead_id INTEGER NOT NULL,
            sender TEXT NOT NULL,
            text TEXT DEFAULT '',
            media_type TEXT DEFAULT '',
            media_id TEXT DEFAULT '',
            is_read INTEGER NOT NULL DEFAULT 0,
            created_at TEXT DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (lead_id) REFERENCES leads(id) ON DELETE CASCADE
        );
    )SQL");

    return true;
}

void Database::close()
{
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool Database::exec(const std::string& sql)
{
    char* err = nullptr;

    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::cerr << "SQL error: " << (err ? err : "") << std::endl;
        sqlite3_free(err);
        return false;
    }

    return true;
}

std::vector<std::map<std::string, std::string>> Database::query(const std::string& sql)
{
    std::vector<std::map<std::string, std::string>> rows;
    sqlite3_stmt* stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "SQL prepare error: " << sqlite3_errmsg(db) << std::endl;
        return rows;
    }

    int colCount = sqlite3_column_count(stmt);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::map<std::string, std::string> row;

        for (int i = 0; i < colCount; ++i) {
            const char* name = sqlite3_column_name(stmt, i);
            const unsigned char* value = sqlite3_column_text(stmt, i);

            row[name ? name : ""] = value ? reinterpret_cast<const char*>(value) : "";
        }

        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return rows;
}
std::string Database::escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (char c : s) {
        if (c == '\'') {
            out += "''";
        }
        else {
            out += c;
        }
    }

    return out;
}
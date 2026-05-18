#include <drogon/drogon.h>
#include <iostream>

#include "controllers/PageController.h"
#include "db/Database.h"
#include "controllers/BotController.h"
#include "services/TelegramService.h"
#include "controllers/StaffController.h"
#include "controllers/LeadController.h"
#include "controllers/FunnelController.h"
#include "controllers/TagController.h"
#include "controllers/BroadcastController.h"

#include <thread>
#include <chrono>

int main()
{
    Database::init("C:/Users/Rawa1zon/Documents/anydesk-web/data/anydesk.db");
    Database::exec("ATTACH DATABASE 'C:/Users/Rawa1zon/Documents/anydesk-web/data/old_crm.db' AS olddb");

    Database::exec(R"SQL(
INSERT INTO leads (
    tg_user_id, username, full_name, channel, geo, subid, tags, comment,
    is_403, created_at, last_message_at, is_duplicate, duplicate_of,
    automation_enabled, main_channel, assigned_staff
)
SELECT
    CAST(user_id AS TEXT), username, full_name, channel, '', subid, tags, comment,
    is_blocked, created_at, datetime(last_ts, 'unixepoch'), 0, NULL,
    1, channel, ''
FROM olddb.users
)SQL");

    Database::exec(R"SQL(
INSERT INTO messages (
    lead_id, sender, text, media_type, media_id, is_read, created_at
)
SELECT
    l.id, m.sender, m.text, m.media_type, m.media_id, m.is_read, m.time
FROM olddb.messages m
JOIN olddb.users u ON u.user_id = m.user_id
JOIN leads l ON l.tg_user_id = CAST(u.user_id AS TEXT)
)SQL");

    auto oldCnt = Database::query("SELECT COUNT(*) AS c FROM olddb.users");
    auto oldMsgCnt = Database::query("SELECT COUNT(*) AS c FROM olddb.messages");
    auto newCnt = Database::query("SELECT COUNT(*) AS c FROM leads");
    auto newMsgCnt = Database::query("SELECT COUNT(*) AS c FROM messages");

    std::cout << "[MIGRATION] old users: " << oldCnt[0].at("c") << std::endl;
    std::cout << "[MIGRATION] old messages: " << oldMsgCnt[0].at("c") << std::endl;
    std::cout << "[MIGRATION] new leads total: " << newCnt[0].at("c") << std::endl;
    std::cout << "[MIGRATION] new messages total: " << newMsgCnt[0].at("c") << std::endl;

    Database::exec("DETACH DATABASE olddb");

    std::cout << "[MIGRATION] old crm imported" << std::endl;

    drogon::app()
        .setDocumentRoot("C:/Users/Rawa1zon/Documents/anydesk-web/static")
        .enableSession(60 * 60 * 24)
        .addListener("0.0.0.0", 4040);

    PageController::registerRoutes();
    BotController::registerRoutes();
    FunnelController::registerRoutes();
    StaffController::registerRoutes();
    LeadController::registerRoutes();
    TagController::registerRoutes();
    BroadcastController::registerRoutes();

    std::thread tgStarter([]() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        TelegramService::startAll();
        });

    drogon::app().run();

    TelegramService::stopAll();

    if (tgStarter.joinable()) {
        tgStarter.join();
    }

    Database::close();

    return 0;
}
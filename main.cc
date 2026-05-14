#include <drogon/drogon.h>

#include "controllers/PageController.h"
#include "db/Database.h"
#include "controllers/BotController.h"
#include "services/TelegramService.h"
#include "controllers/LeadController.h"

#include <thread>
#include <chrono>

int main()
{
    Database::init("C:/Users/Rawa1zon/Documents/anydesk-web/data/anydesk.db");

    drogon::app()
        .setDocumentRoot("C:/Users/Rawa1zon/Documents/anydesk-web/static")
        .enableSession(60 * 60 * 24)
        .addListener("0.0.0.0", 4040);

    PageController::registerRoutes();
    BotController::registerRoutes();
    LeadController::registerRoutes();

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
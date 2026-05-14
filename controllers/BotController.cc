#include "BotController.h"
#include "../services/BotService.h"

#include <drogon/drogon.h>

void BotController::registerRoutes()
{
    drogon::app().registerHandler(
        "/api/bots/create",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto name = req->getParameter("name");
            auto token = req->getParameter("token");
            auto geo = req->getParameter("geo");
            auto funnel = req->getParameter("funnel");

            bool ok = BotService::create(name, token, geo, funnel);

            Json::Value json;
            json["ok"] = ok;

            if (!ok) {
                json["error"] = "name/token required";
            }

            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            callback(resp);
        },
        { drogon::Post }
    );

    drogon::app().registerHandler(
        "/api/bots",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto bots = BotService::all();

            Json::Value arr(Json::arrayValue);

            for (const auto& b : bots) {
                Json::Value item;
                item["id"] = b.id;
                item["name"] = b.name;
                item["token"] = b.token;
                item["geo"] = b.geo;
                item["funnel"] = b.funnel;
                item["active"] = b.active;
                item["created_at"] = b.createdAt;
                arr.append(item);
            }

            Json::Value json;
            json["ok"] = true;
            json["bots"] = arr;

            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            callback(resp);
        },
        { drogon::Get }
    );
}
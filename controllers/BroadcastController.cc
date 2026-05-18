#include "BroadcastController.h"
#include "../services/BroadcastService.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <filesystem>
#include <drogon/MultiPart.h>
#include <memory>
#include <ctime>
#include <iostream>


using namespace drogon;

void BroadcastController::registerRoutes()
{
    app().registerHandler(
        "/api/broadcast/start",
        [](const HttpRequestPtr& req,
            std::function<void(const HttpResponsePtr&)>&& cb)
        {
            BroadcastStartRequest data;

            auto parseTags = [](const std::string& raw) {
                std::vector<std::string> out;
                Json::Value root;
                Json::CharReaderBuilder builder;
                std::string errs;

                std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

                if (!reader->parse(raw.c_str(), raw.c_str() + raw.size(), &root, &errs)) {
                    return out;
                }

                if (!root.isArray()) return out;

                for (const auto& v : root) {
                    out.push_back(v.asString());
                }

                return out;
                };

            drogon::MultiPartParser parser;

            if (parser.parse(req) != 0) {
                Json::Value out;
                out["ok"] = false;
                out["error"] = "multipart parse error";
                cb(HttpResponse::newHttpJsonResponse(out));
                return;
            }

            const auto& params = parser.getParameters();

            auto param = [&](const std::string& key) -> std::string {
                auto it = params.find(key);
                if (it == params.end()) return "";
                return it->second;
                };

            data.includeTags = parseTags(param("include"));
            data.excludeTags = parseTags(param("exclude"));
            data.dateFrom = param("date_from");
            data.dateTo = param("date_to");
            data.text = param("text");
            data.attachmentType = param("attachment_type");

            if (data.attachmentType.empty()) {
                data.attachmentType = "document";
            }

            const auto& files = parser.getFiles();

            if (!files.empty()) {

                const auto& f = files[0];

                std::string ext = ".bin";
                std::string original = f.getFileName();

                auto dot = original.find_last_of('.');
                if (dot != std::string::npos) {
                    ext = original.substr(dot);
                }

                std::filesystem::path uploadDir =
                    std::filesystem::current_path() / "uploads" / "broadcasts";

                std::filesystem::create_directories(uploadDir);

                std::filesystem::path savePath =
                    uploadDir / ("broadcast_" + std::to_string(std::time(nullptr)) + ext);

                f.saveAs(savePath.string());

                data.filePath = savePath.string();

                std::cout << "[BROADCAST FILE] saved: " << data.filePath << std::endl;

                if (std::filesystem::exists(savePath)) {
                    std::cout << "[BROADCAST FILE] size: "
                        << std::filesystem::file_size(savePath)
                        << std::endl;
                }
                else {
                    std::cout << "[BROADCAST FILE] NOT FOUND AFTER SAVE" << std::endl;
                }
            }

            auto result = BroadcastService::start(data);

            Json::Value out;
            out["ok"] = true;
            out["total"] = result.total;
            out["sent"] = result.sent;
            out["failed"] = result.failed;

            cb(HttpResponse::newHttpJsonResponse(out));
        },
        { Post }
    );
    
    app().registerHandler(
        "/api/broadcast/preview",
        [](const HttpRequestPtr& req,
            std::function<void(const HttpResponsePtr&)>&& cb)
        {
            auto json = req->getJsonObject();

            if (!json) {
                Json::Value out;
                out["ok"] = false;
                out["error"] = "invalid json";

                auto resp = HttpResponse::newHttpJsonResponse(out);
                cb(resp);
                return;
            }

            BroadcastPreviewRequest data;

            if ((*json).isMember("include") && (*json)["include"].isArray()) {
                for (const auto& t : (*json)["include"]) {
                    data.includeTags.push_back(t.asString());
                }
            }

            if ((*json).isMember("exclude") && (*json)["exclude"].isArray()) {
                for (const auto& t : (*json)["exclude"]) {
                    data.excludeTags.push_back(t.asString());
                }
            }

            if ((*json).isMember("date_from")) {
                data.dateFrom = (*json)["date_from"].asString();
            }

            if ((*json).isMember("date_to")) {
                data.dateTo = (*json)["date_to"].asString();
            }

            int count = BroadcastService::previewCount(data);

            Json::Value out;
            out["ok"] = true;
            out["count"] = count;

            auto resp = HttpResponse::newHttpJsonResponse(out);
            cb(resp);
        },
        { Post }
    );
}
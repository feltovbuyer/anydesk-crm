#include "PageController.h"
#include "../services/RenderService.h"

#include <drogon/drogon.h>

static bool isLogged(const drogon::HttpRequestPtr& req)
{
    auto session = req->session();
    if (!session) return false;

    try {
        return session->get<int>("logged") == 1;
    }
    catch (...) {
        return false;
    }
}

void PageController::registerRoutes()
{
    drogon::app().registerHandler(
        "/",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto resp = drogon::HttpResponse::newRedirectionResponse("/login");
            callback(resp);
        }
    );

    drogon::app().registerHandler(
        "/login",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(RenderService::loginPage());
            callback(resp);
        },
        { drogon::Get }
    );

    drogon::app().registerHandler(
        "/login",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto login = req->getParameter("login");
            auto password = req->getParameter("password");

            if (login == "feltov" && password == "812") {
                auto session = req->session();
                session->insert("logged", 1);
                session->insert("login", login);
                session->insert("role", std::string("admin"));

                auto resp = drogon::HttpResponse::newRedirectionResponse("/app");
                callback(resp);
                return;
            }

            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(RenderService::loginPage("Неверный логин или пароль"));
            callback(resp);
        },
        { drogon::Post }
    );

    drogon::app().registerHandler(
        "/logout",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            auto session = req->session();
            if (session) {
                session->erase("logged");
                session->erase("login");
                session->erase("role");
            }

            auto resp = drogon::HttpResponse::newRedirectionResponse("/login");
            callback(resp);
        }
    );

    drogon::app().registerHandler(
        "/app",
        [](const drogon::HttpRequestPtr& req,
            std::function<void(const drogon::HttpResponsePtr&)>&& callback)
        {
            if (!isLogged(req)) {
                auto resp = drogon::HttpResponse::newRedirectionResponse("/login");
                callback(resp);
                return;
            }

            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setContentTypeCode(drogon::CT_TEXT_HTML);
            resp->setBody(RenderService::appPage());
            callback(resp);
        }
    );
}
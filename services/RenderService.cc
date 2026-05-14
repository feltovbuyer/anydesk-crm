#include "RenderService.h"

#include <fstream>
#include <sstream>

std::string RenderService::readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::in | std::ios::binary);

    if (!file.is_open()) {
        return "<h1>View not found</h1><p>" + path + "</p>";
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string RenderService::view(const std::string& fileName)
{
    return readFile("C:/Users/Rawa1zon/Documents/anydesk-web/views/" + fileName);
}

std::string RenderService::loginPage(const std::string& error)
{
    std::string html = view("login.html");

    if (!error.empty()) {
        std::string errorHtml = "<div class='login-error'>" + error + "</div>";
        size_t pos = html.find("{{error}}");
        if (pos != std::string::npos) {
            html.replace(pos, 9, errorHtml);
        }
    }
    else {
        size_t pos = html.find("{{error}}");
        if (pos != std::string::npos) {
            html.replace(pos, 9, "");
        }
    }

    return html;
}

std::string RenderService::appPage()
{
    return view("app.html");
}
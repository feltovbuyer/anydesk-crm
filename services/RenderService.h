#pragma once
#include <string>

class RenderService
{
public:
    static std::string view(const std::string& fileName);
    static std::string loginPage(const std::string& error = "");
    static std::string appPage();

private:
    static std::string readFile(const std::string& path);
};
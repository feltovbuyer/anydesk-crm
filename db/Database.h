#pragma once

#include <string>
#include <vector>
#include <map>
#include <sqlite3.h>

class Database
{
public:
    static bool init(const std::string& path);
    static void close();

    static bool exec(const std::string& sql);
    static std::vector<std::map<std::string, std::string>> query(const std::string& sql);

    static std::string escape(const std::string& s);

private:
    static sqlite3* db;
};
#include "../db/Database.h"
#include <iostream>

int main()
{
    Database::init("data/anydesk.db");

    auto dump = [](const std::string& table)
        {
            std::cout << "\n=== " << table << " ===\n";

            auto rows = Database::query("PRAGMA table_info(" + table + ")");

            for (const auto& r : rows) {
                std::cout
                    << r.at("name")
                    << " | "
                    << r.at("type")
                    << "\n";
            }
        };

    dump("leads");
    dump("messages");
    dump("bots");

    Database::close();
    return 0;
}
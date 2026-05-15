cmake_minimum_required(VERSION 3.20)

project(anydesk_web LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)
find_package(CURL REQUIRED)

add_executable(anydesk_web
    main.cc

    controllers/PageController.cc

    services/RenderService.cc

    controllers/BotController.cc
    services/BotService.cc
    controllers/LeadController.cc

    services/TelegramService.cc
    services/LeadService.cc
    services/TelegramFileService.cc

    db/Database.cc
)

target_link_libraries(anydesk_web
    PRIVATE
    Drogon::Drogon
    unofficial::sqlite3::sqlite3
    CURL::libcurl
)
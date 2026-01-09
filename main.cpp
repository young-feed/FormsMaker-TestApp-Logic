#include <httplib.h>
#include "dbmanager.hpp"
#include "auth_handler.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main() {
    // 1. Инициализация БД
    DBManager db("host=localhost port=5432 dbname=forms_db user=postgres password=22833967");
    
    // 2. Инициализация сервера
    httplib::Server svr;

    // Эндпоинт для мягкого удаления курса
    svr.Delete("/courses/:id", [&](const httplib::Request& req, httplib::Response& res) {
        // Извлекаем токен из заголовка Authorization
        std::string auth_header = req.get_header_value("Authorization");
        if (auth_header.empty() || auth_header.length() < 7) {
            res.status = 401;
            res.set_content("Missing token", "text/plain");
            return;
        }
        std::string token = auth_header.substr(7); // Отрезаем "Bearer "

        // Проверка доступа через наш AuthHandler
        int status = AuthHandler::check_access(token, "course:delete", db);
        
        if (status != 200) {
            res.status = status;
            if (status == 418) res.set_content("User is blocked", "text/plain");
            return;
        }

        // Логика мягкого удаления
        int course_id = std::stoi(req.path_params.at("id"));
        if (db.soft_delete_course(course_id)) {
            res.status = 200;
            res.set_content(json({{"status", "success"}, {"message", "Course marked as deleted"}}).dump(), "application/json");
        } else {
            res.status = 500;
        }
    });

    std::cout << "Server started at http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
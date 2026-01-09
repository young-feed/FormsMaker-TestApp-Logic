#include <iostream>
#include "dbmanager.hpp"
#include "auth_handler.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main() {
    try {
        // Инициализация БД
        DBManager db("host=localhost dbname=forms_db user=postgres password=root");

        // Имитация входящего запроса
        std::string mock_token = "eyJhbGci... (ваш токен)";
        std::string action_needed = "course:read";

        // 1. Middleware: Проверка доступа
        int auth_status = AuthHandler::check_access(mock_token, action_needed, db);

        if (auth_status == 418) {
            std::cout << "HTTP Status: 418 I'm a teapot (User Blocked)" << std::endl;
            return 0;
        } else if (auth_status != 200) {
            std::cout << "HTTP Status: " << auth_status << " Unauthorized/Forbidden" << std::endl;
            return 0;
        }

        // 2. Логика ресурса: Получение списка курсов (с учетом мягкого удаления)
        PGresult* res = db.query("SELECT id, title FROM courses WHERE is_deleted = false", {});
        
        json response = json::array();
        for (int i = 0; i < PQntuples(res); i++) {
            response.push_back({
                {"id", std::stoi(PQgetvalue(res, i, 0))},
                {"title", PQgetvalue(res, i, 1)}
            });
        }
        PQclear(res);

        std::cout << "Response JSON: " << response.dump(4) << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Fatal Error: " << e.what() << std::endl;
    }
    return 0;
}
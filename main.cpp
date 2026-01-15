#include <httplib.h>
#include "dbmanager.hpp"
#include "auth_handler.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
std::string get_token(const httplib::Request& req) {
    if (req.has_header("Authorization")) {
        std::string auth = req.get_header_value("Authorization");
        if (auth.length() > 7) return auth.substr(7); // Удаляем "Bearer "
    }
    return "";
}

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

    

    svr.Get("/courses", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = get_token(req);
        // Проверка доступа и блокировки (418)
        //int status = AuthHandler::check_access(token, "course:read", db); 
    
        /*if (status != 200) {
            res.status = status;
            return;
        }*/

    // SQL запрос с учетом мягкого удаления
        //Выбираем только id и title тех курсов, которые не удалены
    PGresult* result = db.query("SELECT id, title FROM courses WHERE is_deleted = false", {});

    if (PQresultStatus(result) != PGRES_TUPLES_OK) {
        res.status = 500;
        res.set_content("Ошибка БД", "text/plain");
        PQclear(result);
        return;
    }

    // 3. Формируем JSON массив из строк таблицы
    json j_list = json::array();
    int rows = PQntuples(result);

    for (int i = 0; i < rows; i++) {
        json course;
        course["id"] = std::stoi(PQgetvalue(result, i, 0)); // Колонка id
        course["title"] = PQgetvalue(result, i, 1);        // Колонка title
        j_list.push_back(course);
    }

    // 4. Очищаем память и отправляем результат
    PQclear(result);
    
    // dump(4) сделает вывод красивым и читаемым в браузере
    res.set_content(j_list.dump(4), "application/json"); 
});

    svr.Post("/questions/:id/update", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = get_token(req);
        int status = AuthHandler::check_access(token, "question:write", db);
    
        if (status != 200) {
            res.status = status;
            return;
        }

        auto j_body = nlohmann::json::parse(req.body);
        int q_id = std::stoi(req.path_params.at("id"));
    
    // Добавление новой версии вместо изменения старой
        if (db.create_new_question_version(q_id, j_body["text"], j_body["options"].dump())) {
            res.status = 201;
            res.set_content("{\"message\":\"New version created\"}", "application/json");
        }
    });

    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("<h1>Main Module is Running</h1><p>Server is alive!</p>", "text/html");
    });

    // Это эндпоинт для проверки здоровья системы
    svr.Get("/health", [&](const httplib::Request& req, httplib::Response& res) {
        res.status = 200;
        res.set_content("OK", "text/plain");
    });
    std::cout << "Server started at http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
#pragma once
#define JWT_DISABLE_PICOJSON
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <string>
#include "dbmanager.hpp"
#include <httplib.h> 

class AuthHandler
{
public:
    using nlohmann_traits = jwt::traits::nlohmann_json;

    static int AuthHandler::check_access(const std::string& token, const std::string& permission, DBManager& db) {
    // 1. Сначала проверяем блокировку в локальной БД (как требует ТЗ)
    std::string user_id = get_id_from_token(token);
    PGresult* res = db.query("SELECT is_blocked FROM users WHERE id = $1", {user_id});
    if (PQntuples(res) > 0 && std::string(PQgetvalue(res, 0, 0)) == "t") {
        PQclear(res);
        return 418; // I'm a teapot (заблокирован)
    }
    PQclear(res);

    // 2. Отправляем запрос в модуль на Go для проверки прав
    httplib::Client cli("http://localhost:8081"); // Адрес вашего Go модуля
    
    nlohmann::json body = {
        {"token", token},
        {"permission", permission}
    };

    if (auto res = cli.Post("/verify", body.dump(), "application/json")) {
        if (res->status == 200) return 200; // Доступ разрешен
        if (res->status == 403) return 403; // Нет прав
    }
    
    return 401; // Ошибка авторизации или сервиса
}

    static bool is_owner(const std::string &token, const std::string &course_id, DBManager &db)
    {
        std::string user_id = AuthHandler::get_id_from_token(token);
        if (user_id.empty())
            return false;

        // Предполагаем, что в таблице courses есть колонка teacher_id
        PGresult *res = db.query("SELECT 1 FROM courses WHERE id = $1 AND teacher_id = $2", {course_id, user_id});
        bool owned = (PQntuples(res) > 0);
        PQclear(res);

        return owned;
    }

    static bool is_course_teacher(const std::string& token, const std::string& course_id, DBManager& db) {
    // 1. Извлекаем ID пользователя из токена
    std::string user_id = get_id_from_token(token);
    if (user_id.empty() || course_id.empty()) return false;

    // 2. Проверяем в таблице courses, назначен ли этот пользователь преподавателем
    // Предполагается, что в БД есть колонка teacher_id (или автор курса)
    PGresult* res = db.query(
        "SELECT 1 FROM courses WHERE id = $1 AND teacher_id = $2 AND is_deleted = false", 
        {course_id, user_id}
    );

    bool is_teacher = (PQntuples(res) > 0);
    PQclear(res);
    
    return is_teacher;
    }

    static bool is_question_owner(const std::string& token, const std::string& logical_id, DBManager& db) {
        // 1. Извлекаем ID текущего пользователя из JWT токена
        std::string user_id = get_id_from_token(token);
        if (user_id.empty()) return false;

        // 2. Проверяем в базе данных, является ли этот пользователь автором вопроса
        // Используем logical_id, так как авторство распространяется на все версии вопроса
        PGresult* res = db.query(
            "SELECT 1 FROM questions WHERE logical_id = $1 AND author_id = $2 LIMIT 1", 
            {logical_id, user_id}
        );

        bool owned = (PQntuples(res) > 0);
        PQclear(res);
        
        return owned;
    }

    static std::string get_id_from_token(const std::string &token)
    {
        if (token.empty())
        {
            return "";
        }

        try
        {
            // Декодируем токен без верификации (верификация обычно делается в check_access)
            auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token);

            // В JWT идентификатор пользователя обычно хранится в поле "id" или "sub" (subject)
            // Убедитесь, что при генерации токена вы использовали то же имя поля
            if (decoded.has_payload_claim("id"))
            {
                return decoded.get_payload_claim("id").as_string();
            }
            else if (decoded.has_payload_claim("sub"))
            {
                return decoded.get_payload_claim("sub").as_string();
            }
        }
        catch (const std::exception &e)
        {
            // Если токен поврежден или формат неверный
            std::cerr << "JWT Decode Error: " << e.what() << std::endl;
        }

        return "";
    }
};
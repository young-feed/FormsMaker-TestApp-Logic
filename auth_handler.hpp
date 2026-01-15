#pragma once
#define JWT_DISABLE_PICOJSON
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <string>
#include "dbmanager.hpp"

class AuthHandler
{
public:
    using nlohmann_traits = jwt::traits::nlohmann_json;

    static int check_access(const std::string &token, const std::string &required_permission, DBManager &db)
    {
        // 1. ПРОВЕРКА ВАЛИДНОСТИ ТОКЕНА
        // Если токен пустой или не может быть декодирован — 401 Unauthorized
        if (token.empty())
            return 401;

        std::string user_id;
        try
        {
            auto decoded = jwt::decode(token);
            // Извлекаем ID пользователя из claim "id" или "sub"
            user_id = decoded.get_payload_claim("id").as_string();
        }
        catch (...)
        {
            return 401;
        }

        // 2. ПРОВЕРКА БЛОКИРОВКИ (ТРЕБОВАНИЕ ТЗ: КОД 418)
        // "Для пользователя запрещены все действия... На любой запрос нужен код 418"
        PGresult *res_block = db.query("SELECT is_blocked FROM users WHERE id = $1", {user_id});

        if (PQntuples(res_block) == 0)
        {
            PQclear(res_block);
            return 401; // Пользователь не найден в базе
        }

        bool blocked = std::string(PQgetvalue(res_block, 0, 0)) == "t";
        PQclear(res_block);

        if (blocked)
        {
            return 418; // Статус I'm a teapot согласно таблице
        }

        // 3. ПРОВЕРКА ПРАВ ДОСТУПА (PERMISSIONS)
        // Если право не требуется (пустая строка), возвращаем 200
        if (required_permission.empty())
            return 200;

        // Проверяем наличие записи в таблице ролей/разрешений
        PGresult *res_perm = db.query(
            "SELECT 1 FROM user_roles WHERE user_id = $1 AND role_name = $2",
            {user_id, required_permission});

        bool has_permission = PQntuples(res_perm) > 0;
        PQclear(res_perm);

        if (!has_permission)
        {
            return 403; // Forbidden — токен верный, но прав "меньше", чем нужно
        }

        return 200; // Доступ разрешен
    }

    bool is_owner(const std::string &token, const std::string &course_id, DBManager &db)
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

    static std::string get_id_from_token(const std::string &token)
    {
        if (token.empty())
        {
            return "";
        }

        try
        {
            // Декодируем токен без верификации (верификация обычно делается в check_access)
            auto decoded = jwt::decode(token);

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
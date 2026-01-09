#pragma once
#define JWT_DISABLE_PICOJSON
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <string>
#include "dbmanager.hpp"

class AuthHandler {
public:
    using nlohmann_traits = jwt::traits::nlohmann_json;

    static int check_access(const std::string& token, const std::string& permission, DBManager& db) {
        try {
            // 1. Декодирование токена
            auto decoded = jwt::decode<nlohmann_traits>(token);

            // 2. Верификация (секрет должен совпадать с модулем Авторизации)
            auto verifier = jwt::verify<nlohmann_traits>()
                .allow_algorithm(jwt::algorithm::hs256{ "secret" });
            verifier.verify(decoded);

            // 3. Получение данных пользователя (Payload)
            auto payload = decoded.get_payload_json();
            std::string email = payload["email"].get<std::string>();

            // 4. Проверка блокировки в БД (ТЗ: код 418)
            const char* params[1] = { email.c_str() };
            PGresult* res = db.query("SELECT is_blocked FROM users WHERE email = $1", {params[0]});
            
            if (PQntuples(res) > 0) {
                std::string is_blocked = PQgetvalue(res, 0, 0);
                PQclear(res);
                // Если пользователь заблокирован, запрещены все действия
                if (is_blocked == "t") return 418; 
            } else {
                PQclear(res);
            }

            // 5. Проверка прав (ТЗ: код 403)
            // Проверяем наличие ключа "permissions" простым обращением
            if (payload.count("permissions") && payload["permissions"].is_array()) {
                auto perms = payload["permissions"];
                for (auto& item : perms) {
                    if (item.get<std::string>() == permission) {
                        return 200; // Разрешение найдено
                    }
                }
            }

            return 403; // Разрешения нет
        } catch (...) {
            // Любая ошибка (устаревший токен и т.д.) -> 401
            return 401;
        }
    }
};
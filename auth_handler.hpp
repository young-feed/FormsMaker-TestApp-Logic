#pragma once
#include <jwt-cpp/jwt.h>
#include "db_manager.hpp"

class AuthHandler {
public:
    // Возвращает 200 (OK), 401 (Unauthorized) или 418 (Blocked)
    static int check_access(const std::string& token, const std::string& permission, DBManager& db) {
        try {
            auto decoded = jwt::decode(token);
            // Проверка подписи (замените "secret" на ваш ключ из модуля Auth)
            auto verifier = jwt::verify().allow_algorithm(jwt::algorithm::hs256{"secret"});
            verifier.verify(decoded);

            std::string email = decoded.get_payload_claim("email").as_string();

            // Проверка блокировки в БД
            const char* params[] = { email.c_str() };
            PGresult* res = db.query("SELECT is_blocked FROM users WHERE email = $1", {params[0]});
            
            if (PQntuples(res) == 0) return 401;
            bool is_blocked = (std::string(PQgetvalue(res, 0, 0)) == "t");
            PQclear(res);

            if (is_blocked) return 418; // Возвращаем статус "Я - чайник"

            // Проверка конкретного разрешения в клеймах токена
            auto perms = decoded.get_payload_claim("permissions").as_set<std::string>();
            if (perms.find(permission) == perms.end()) return 403;

            return 200;
        } catch (...) {
            return 401;
        }
    }
};
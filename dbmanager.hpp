#pragma once
#include <nlohmann/json.hpp>
#include <libpq-fe.h>
#include <string>
#include <stdexcept>

class DBManager {
    PGconn* conn;
public:
    DBManager(const std::string& conninfo) {
        conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            throw std::runtime_error(PQerrorMessage(conn));
        }
    }
    ~DBManager() { PQfinish(conn); }

    PGconn* get_conn() { return conn; }

    // Вспомогательная функция для выполнения запросов
    PGresult* DBManager::query(const std::string& sql, const std::vector<std::string>& params) {
    // Преобразуем std::vector<std::string> в массив const char* для libpq
    std::vector<const char*> paramValues;
    for (const auto& p : params) {
        paramValues.push_back(p.c_str());
    }

    PGresult* res = PQexecParams(
        conn,
        sql.c_str(),
        params.size(),       // количество параметров
        nullptr,             // типы параметров (Postgres определит сам)
        paramValues.data(),  // значения параметров
        nullptr,             // длины (не нужны для текстового формата)
        nullptr,             // форматы (текст)
        0                    // результат в текстовом формате
    );

    return res;
    }

    static nlohmann::json pg_to_json(PGresult* res) {
        auto j_array = nlohmann::json::array();
        int rows = PQntuples(res);
        int cols = PQnfields(res);

        for (int i = 0; i < rows; i++) {
            nlohmann::json item;
            for (int j = 0; j < cols; j++) {
                std::string col_name = PQfname(res, j);
                std::string col_value = PQgetvalue(res, i, j);
                item[col_name] = col_value;
            }
            j_array.push_back(item);
        }
        return j_array;
    }

    static inline bool has_attempts(const std::string& test_id, DBManager& db) {
        PGresult* res = db.query("SELECT 1 FROM attempts WHERE test_id = $1 LIMIT 1", {test_id});
        bool exists = PQntuples(res) > 0;
        PQclear(res);
        return exists;
    }

    bool soft_delete_course(int course_id) {
        std::string id_str = std::to_string(course_id);
        const char* params[1] = { id_str.c_str() };
        
        PGresult* res = query("UPDATE courses SET is_deleted = true WHERE id = $1", {params[0]});
        
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            PQclear(res);
            return false;
        }
        PQclear(res);
        return true;
    }

    // Метод для версионности вопросов
    // Метод для создания новой версии вопроса
bool create_new_question_version(int logical_id, const std::string& text, const std::string& options_json) {
    std::string s_id = std::to_string(logical_id);
    
    // SQL: Вставляем новую запись, вычисляя следующую версию как MAX(version) + 1
    // COALESCE(..., 0) нужен, если это вообще первая запись для этого id
    const char* sql = 
        "INSERT INTO questions (id, question_text, options, version) "
        "VALUES ($1, $2, $3, "
        "  (SELECT COALESCE(MAX(version), 0) + 1 FROM questions WHERE id = $1)"
        ");";

    const char* params[3] = { s_id.c_str(), text.c_str(), options_json.c_str() };
    
    PGresult* res = PQexecParams(
        conn,             // Ваш объект PGconn*
        sql,
        3,                // Количество параметров
        NULL,             // Типы параметров (NULL — пусть Postgres определит сам)
        params,
        NULL,             // Длины параметров
        NULL,             // Форматы параметров (0 — текст)
        0                 // Формат результата (0 — текст)
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Ошибка при создании версии вопроса: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}
}; // Обязательно точка с запятой после класса!
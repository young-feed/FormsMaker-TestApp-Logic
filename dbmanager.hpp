#pragma once
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
    PGresult* query(const std::string& sql, const std::vector<const char*>& params) {
        return PQexecParams(conn, sql.c_str(), params.size(), NULL, params.data(), NULL, NULL, 0);
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
    bool add_question_version(int id, const std::string& text, const std::string& options, int version) {
        std::string s_id = std::to_string(id);
        std::string s_ver = std::to_string(version);
        const char* params[4] = { s_id.c_str(), text.c_str(), options.c_str(), s_ver.c_str() };

        PGresult* ins_res = query(
            "INSERT INTO questions (id, text, options, version) VALUES ($1, $2, $3, $4)", 
            {params[0], params[1], params[2], params[3]}
        );

        bool success = (PQresultStatus(ins_res) == PGRES_COMMAND_OK);
        PQclear(ins_res);
        return success;
    }
}; // Обязательно точка с запятой после класса!
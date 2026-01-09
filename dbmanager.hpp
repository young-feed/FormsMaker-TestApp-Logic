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
    
    // SQL запрос: вместо DELETE используем UPDATE
    PGresult* res = query("UPDATE courses SET is_deleted = true WHERE id = $1", {params[0]});
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        std::cerr << "Update failed: " << PQerrorMessage(conn) << std::endl;
        PQclear(res);
        return false;
    }
    
    PQclear(res);
    return true;
}
};
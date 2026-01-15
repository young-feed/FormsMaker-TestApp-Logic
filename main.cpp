#include <httplib.h>
#include "dbmanager.hpp"
#include "auth_handler.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
std::string get_token(const httplib::Request &req)
{
    if (req.has_header("Authorization"))
    {
        std::string auth = req.get_header_value("Authorization");
        if (auth.length() > 7)
            return auth.substr(7); // Удаляем "Bearer "
    }
    return "";
}

int main()
{
    // 1. Инициализация БД
    DBManager db("host=localhost port=5432 dbname=forms_db user=postgres password=22833967");

    // 2. Инициализация сервера
    httplib::Server svr;

    // 1. Посмотреть список пользователей (ID и ФИО)
    svr.Get("/users", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string token = get_token(req);
        int status = AuthHandler::check_access(token, "user:list:read", db);
        if (status != 200) { res.status = status; return; }

        PGresult* res_db = db.query("SELECT id, full_name FROM users", {});
        json j_list = json::array();
        for (int i = 0; i < PQntuples(res_db); i++) {
            j_list.push_back({{"id", PQgetvalue(res_db, i, 0)}, {"full_name", PQgetvalue(res_db, i, 1)}});
        }
        PQclear(res_db);
        res.set_content(j_list.dump(4), "application/json"); });

    // 2. Посмотреть ФИО по ID (Разрешено всем по умолчанию)
    svr.Get("/users/:id/name", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string id = req.path_params.at("id");
        PGresult* res_db = db.query("SELECT full_name FROM users WHERE id = $1", {id});
        if (PQntuples(res_db) > 0) {
            res.set_content(json({{"full_name", PQgetvalue(res_db, 0, 0)}}).dump(), "application/json");
        } else { res.status = 404; }
        PQclear(res_db); });

    // 3. Изменить ФИО (Себе — можно, другому — по праву)
    svr.Put("/users/:id/name", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string token = get_token(req);
        std::string target_id = req.path_params.at("id");
    // Если меняем не себе, проверяем право
        if (AuthHandler::get_id_from_token(token) != target_id) {
            int status = AuthHandler::check_access(token, "user:fullName:write", db);
            if (status != 200) { res.status = status; return; }
        }
        auto j = json::parse(req.body);
        db.query("UPDATE users SET full_name = $1 WHERE id = $2", {j.at("full_name"), target_id});
        res.status = 200; });

    // 4. Посмотреть данные: курсы, оценки, тесты
    svr.Get("/users/:id/data", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string token = get_token(req);
        int status = AuthHandler::check_access(token, "user:data:read", db);
        if (status != 200) { res.status = status; return; }

        PGresult* res_db = db.query("SELECT data FROM user_academic_data WHERE user_id = $1", {req.path_params.at("id")});
        res.set_content(PQgetvalue(res_db, 0, 0), "application/json");
        PQclear(res_db); });

    // 5. Просмотр ролей пользователя
    svr.Get("/users/:id/roles", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string token = get_token(req);
        int status = AuthHandler::check_access(token, "user:roles:read", db);
        if (status != 200) { res.status = status; return; }

        PGresult* res_db = db.query("SELECT role_name FROM user_roles WHERE user_id = $1", {req.path_params.at("id")});
        json roles = json::array();
        for(int i=0; i < PQntuples(res_db); i++) roles.push_back(PQgetvalue(res_db, i, 0));
        PQclear(res_db);
        res.set_content(roles.dump(), "application/json"); });

    // 6. Изменение роли (Заменяет роль на защите)
    svr.Put("/users/:id/roles", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string token = get_token(req);
        int status = AuthHandler::check_access(token, "user:roles:write", db);
        if (status != 200) { res.status = status; return; }

        auto j = json::parse(req.body);
        db.query("UPDATE user_roles SET role_name = $1 WHERE user_id = $2", {j.at("role"), req.path_params.at("id")});
        res.status = 200; });

    // 7. Посмотреть, заблокирован ли пользователь
    svr.Get("/users/:id/block-status", [&](const httplib::Request &req, httplib::Response &res)
            {
        std::string token = get_token(req);
        int status = AuthHandler::check_access(token, "user:block:read", db);
        if (status != 200) { res.status = status; return; }

        PGresult* res_db = db.query("SELECT is_blocked FROM users WHERE id = $1", {req.path_params.at("id")});
        bool is_blocked = std::string(PQgetvalue(res_db, 0, 0)) == "t";
        PQclear(res_db);
        res.set_content(json({{"is_blocked", is_blocked}}).dump(), "application/json"); });

    // 8. Заблокировать / Разблокировать (Влияет на код 418)
    svr.Post("/users/:id/block", [&](const httplib::Request &req, httplib::Response &res){
        std::string token = get_token(req);
        int status = AuthHandler::check_access(token, "user:block:write", db);
        if (status != 200) { res.status = status; return; }

        auto j = json::parse(req.body); // Ожидаем {"blocked": true}
        bool block = j.at("blocked");
        db.query("UPDATE users SET is_blocked = $1 WHERE id = $2", {block ? "true" : "false", req.path_params.at("id")});
        res.status = 200; });


    // 1.1 Посмотреть список дисциплин
    svr.Get("/courses", [&](const httplib::Request &req, httplib::Response &res){
    // Проверка доступа (разрешение не требуется по умолчанию)
    int status = AuthHandler::check_access(get_token(req), "", db);
    if (status != 200) { res.status = status; return; }

    // Возвращает только не удаленные дисциплины
    PGresult* r = db.query("SELECT id, title, description FROM courses WHERE is_deleted = false", {});
    res.set_content(DBManager::pg_to_json(r).dump(), "application/json");
    PQclear(r); });

    // 1.3 Изменить информацию о дисциплине
    svr.Put("/courses/:id", [&](const httplib::Request &req, httplib::Response &res){
    std::string token = get_token(req);
    std::string course_id = req.path_params.at("id");
        
    // Логика: + Для своей, - Для чужих (нужно course:info:write)
    if (!AuthHandler::is_owner(token, course_id, db)) {
        int status = AuthHandler::check_access(token, "course:info:write", db);
        if (status != 200) { res.status = status; return; }
    }

    auto j = json::parse(req.body);
    db.query("UPDATE courses SET title = $1, description = $2 WHERE id = $3", 
             {j["title"], j["description"], course_id});
    res.status = 200; });

    // 1.5 Удалить дисциплину (Мягкое удаление)
    svr.Delete("/courses/:id", [&](const httplib::Request &req, httplib::Response &res)
               {
    std::string course_id = req.path_params.at("id");
    // Требуется course:del для чужих
    if (AuthHandler::check_access(get_token(req), "course:del", db) != 200) { res.status = 403; return; }

    // Эффект: реально ничего не удаляется, тесты и оценки скрываются
    db.query("UPDATE courses SET is_deleted = true WHERE id = $1", {course_id});
    res.status = 204; });
    // 2.3 Активировать/Деактивировать тест
    svr.Patch("/courses/:id/tests/:tid", [&](const httplib::Request &req, httplib::Response &res)
              {
    if (AuthHandler::check_access(get_token(req), "course:test:write", db) != 200) { res.status = 403; return; }

    std::string test_id = req.path_params.at("tid");
    bool active = json::parse(req.body).at("active");

    db.query("UPDATE tests SET is_active = $1 WHERE id = $2", {active ? "true" : "false", test_id});

    // Эффект: если деактивирован, все начатые попытки завершаются
    if (!active) {
        db.query("UPDATE attempts SET status = 'finished' WHERE test_id = $1 AND status = 'in_progress'", {test_id});
    }
    res.status = 200; });

    // 2.4 Добавить тест в дисциплину
    svr.Post("/courses/:id/tests", [&](const httplib::Request &req, httplib::Response &res)
             {
    if (AuthHandler::check_access(get_token(req), "course:test:add", db) != 200) { res.status = 403; return; }
    
    auto j = json::parse(req.body);
    // По умолчанию тест не активен
    PGresult* r = db.query("INSERT INTO tests (course_id, title, is_active) VALUES ($1, $2, false) RETURNING id", 
                           {req.path_params.at("id"), j["title"]});
    res.set_content(json({{"id", PQgetvalue(r, 0, 0)}}).dump(), "application/json");
    PQclear(r); });
    // 3.2 Записать пользователя на дисциплину
    svr.Post("/courses/:id/students", [&](const httplib::Request &req, httplib::Response &res)
             {
    std::string token = get_token(req);
    auto j = json::parse(req.body);
    std::string target_user_id = j.at("user_id");

    // Логика: + Себя (без прав), - Других (нужно course:user:add)
    if (AuthHandler::get_id_from_token(token) != target_user_id) {
        if (AuthHandler::check_access(token, "course:user:add", db) != 200) { res.status = 403; return; }
    }

    db.query("INSERT INTO course_students (course_id, user_id) VALUES ($1, $2)", 
             {req.path_params.at("id"), target_user_id});
    res.status = 201; });

    // 3.3 Отчислить пользователя
    svr.Delete("/courses/:id/students/:uid", [&](const httplib::Request &req, httplib::Response &res)
               {
    std::string token = get_token(req);
    std::string target_uid = req.path_params.at("uid");
    // Себя — можно, другого — через course:user:del
    if (AuthHandler::get_id_from_token(token) != target_uid) {
        if (AuthHandler::check_access(token, "course:user:del", db) != 200) { res.status = 403; return; }
    }

    db.query("DELETE FROM course_students WHERE course_id = $1 AND user_id = $2", 
             {req.path_params.at("id"), target_uid});
    res.status = 200; });

        // 1. Посмотреть список вопросов
    svr.Get("/questions", [&](const httplib::Request& req, httplib::Response& res) {
        std::string token = get_token(req);
        // Проверка доступа: Свои (+), Чужие (quest:list:read)
        int status = AuthHandler::check_access(token, "quest:list:read", db);
        if (status != 200) { res.status = status; return; }

        // Возвращает только последние версии для каждого вопроса
        std::string sql = R"(
            SELECT DISTINCT ON (logical_id) id, title, version, author_id 
            FROM questions 
            WHERE is_deleted = false
            ORDER BY logical_id, version DESC
        )";
        
        PGresult* r = db.query(sql, {});
        res.set_content(DBManager::pg_to_json(r).dump(), "application/json");
        PQclear(r);
    });

    // 2. Посмотреть информацию о вопросе
    svr.Get("/questions/:id/:version", [&](const httplib::Request& req, httplib::Response& res) {
        std::string q_id = req.path_params.at("id");
        std::string ver = req.path_params.at("version");
        
        // Доступ: Свои (+), Студент с попыткой (+), Остальные (quest:read)
        int status = AuthHandler::check_access(get_token(req), "quest:read", db);
        if (status != 200) { res.status = status; return; }

        PGresult* r = db.query(
            "SELECT title, text, options, correct_answer FROM questions WHERE logical_id = $1 AND version = $2", 
            {q_id, ver}
        );
        
        if (PQntuples(r) > 0) {
            res.set_content(DBManager::pg_to_json(r)[0].dump(), "application/json");
        } else { res.status = 404; }
        PQclear(r);
    });

    // 3. Изменить текст вопроса (Создание новой версии)
    svr.Put("/questions/:logical_id", [&](const httplib::Request& req, httplib::Response& res) {
        std::string l_id = req.path_params.at("logical_id");
        std::string token = get_token(req);
        
        // Доступ: Свои (+), Чужие (quest:update)
        if (!AuthHandler::is_question_owner(token, l_id, db)) {
            int status = AuthHandler::check_access(token, "quest:update", db);
            if (status != 200) { res.status = status; return; }
        }

        auto j = json::parse(req.body);
        // Создает новую версию с инкрементом
        db.query(R"(
            INSERT INTO questions (logical_id, title, text, options, correct_answer, version, author_id)
            VALUES ($1, $2, $3, $4, $5, 
                (SELECT MAX(version) + 1 FROM questions WHERE logical_id = $1), $6)
        )", {l_id, j["title"], j["text"], j["options"].dump(), j["correct"], AuthHandler::get_id_from_token(token)});
        
        res.status = 201;
    });

    // 4. Создать вопрос
    svr.Post("/questions", [&](const httplib::Request& req, httplib::Response& res) {
        // Требуется разрешение quest:create
        int status = AuthHandler::check_access(get_token(req), "quest:create", db);
        if (status != 200) { res.status = status; return; }

        auto j = json::parse(req.body);
        // Начальная версия всегда 1
        PGresult* r = db.query(
            "INSERT INTO questions (logical_id, title, text, options, correct_answer, version, author_id) "
            "VALUES (nextval('logical_id_seq'), $1, $2, $3, $4, 1, $5) RETURNING logical_id",
            {j["title"], j["text"], j["options"].dump(), j["correct"], AuthHandler::get_id_from_token(get_token(req))}
        );
        
        res.set_content(json({{"logical_id", PQgetvalue(r, 0, 0)}}).dump(), "application/json");
        PQclear(r);
    });

    // 5. Удалить вопрос
    svr.Delete("/questions/:logical_id", [&](const httplib::Request& req, httplib::Response& res) {
        std::string l_id = req.path_params.at("logical_id");
        
        // Доступ: Свои (+), Чужой (quest:del)
        if (AuthHandler::check_access(get_token(req), "quest:del", db) != 200) { res.status = 403; return; }

        // Проверка: если вопрос используется в тестах (даже удаленных), удалять нельзя
        PGresult* check = db.query("SELECT 1 FROM test_questions WHERE question_logical_id = $1", {l_id});
        if (PQntuples(check) > 0) {
            res.status = 400;
            res.set_content("Cannot delete: Question is used in tests", "text/plain");
            PQclear(check);
            return;
        }
        PQclear(check);
        // Мягкое удаление (реально не удаляется)
        db.query("UPDATE questions SET is_deleted = true WHERE logical_id = $1", {l_id});
        res.status = 200;
    });

    // 1. Удалить вопрос из теста
    svr.Delete("/tests/:id/questions/:qid", [&](const httplib::Request& req, httplib::Response& res) {
        std::string test_id = req.path_params.at("id");
        std::string token = get_token(req);

        // Доступ: Преподаватель (+) или разрешение test:quest:del
        if (!AuthHandler::is_course_teacher(token, test_id, db)) {
            int status = AuthHandler::check_access(token, "test:quest:del", db);
            if (status != 200) { res.status = status; return; }
        }

        // Условие: Если у теста ещё не было попыток прохождения
        if (DBManager::has_attempts(test_id, db)) {
            res.status = 400;
            res.set_content("Cannot modify test: attempts already exist", "text/plain");
            return;
        }

        db.query("DELETE FROM test_questions WHERE test_id = $1 AND question_id = $2", {test_id, req.path_params.at("qid")});
        res.status = 200;
    });

    // 2. Добавить вопрос в тест
    svr.Post("/tests/:id/questions", [&](const httplib::Request& req, httplib::Response& res) {
        std::string test_id = req.path_params.at("id");
        std::string token = get_token(req);
        auto j = json::parse(req.body);
        std::string q_id = j.at("question_id");

        // Доступ: Преподаватель И автор вопроса (+) или разрешение test:quest:add
        bool is_auth = AuthHandler::is_course_teacher(token, test_id, db) && AuthHandler::is_question_owner(token, q_id, db);
        if (!is_auth) {
            int status = AuthHandler::check_access(token, "test:quest:add", db);
            if (status != 200) { res.status = status; return; }
        }

        if (DBManager::has_attempts(test_id, db)) {
            res.status = 400; return;
        }

        // Добавляет в последнюю позицию
        db.query("INSERT INTO test_questions (test_id, question_id, position) "
                "VALUES ($1, $2, (SELECT COALESCE(MAX(position), 0) + 1 FROM test_questions WHERE test_id = $1))", 
                {test_id, q_id});
        res.status = 201;
    });

    // 3. Изменить порядок следования вопросов
    svr.Put("/tests/:id/questions/order", [&](const httplib::Request& req, httplib::Response& res) {
        std::string test_id = req.path_params.at("id");
        if (!AuthHandler::is_course_teacher(get_token(req), test_id, db)) {
            int status = AuthHandler::check_access(get_token(req), "test:quest:update", db);
            if (status != 200) { res.status = status; return; }
        }

        if (DBManager::has_attempts(test_id, db)) { res.status = 400; return; }

        auto j = json::parse(req.body); // Ожидаем массив ID в нужном порядке
        for (int i = 0; i < j.size(); i++) {
            db.query("UPDATE test_questions SET position = $1 WHERE test_id = $2 AND question_id = $3", 
                    {std::to_string(i), test_id, j[i].get<std::string>()});
        }
        res.status = 200;
    });

    // 4, 5, 6. Просмотр результатов (Список пользователей, Оценки, Ответы)
    // Все три эндпоинта используют общую логику прав: Преподаватель или Свои результаты
    svr.Get("/tests/:id/results/:type", [&](const httplib::Request& req, httplib::Response& res) {
        std::string test_id = req.path_params.at("id");
        std::string type = req.path_params.at("type"); // "users", "grades", or "answers"
        std::string token = get_token(req);
        std::string my_id = AuthHandler::get_id_from_token(token);

        bool is_teacher = AuthHandler::is_course_teacher(token, test_id, db);
        int status = AuthHandler::check_access(token, "test:answer:read", db);

        // Если не учитель и нет спец. прав — разрешено смотреть только СВОЕ
        std::string filter = (is_teacher || status == 200) ? "" : " AND user_id = '" + my_id + "'";
        
        std::string sql;
        if (type == "users") sql = "SELECT DISTINCT user_id FROM attempts WHERE test_id = $1" + filter;
        else if (type == "grades") sql = "SELECT user_id, score FROM attempts WHERE test_id = $1" + filter;
        else if (type == "answers") sql = "SELECT user_id, answers_json FROM attempts WHERE test_id = $1" + filter;

        PGresult* r = db.query(sql, {test_id});
        res.set_content(DBManager::pg_to_json(r).dump(), "application/json");
        PQclear(r);
    });

        // 1. СОЗДАТЬ ПОПЫТКУ
    svr.Post("/tests/:id/attempts", [&](const httplib::Request& req, httplib::Response& res) {
        std::string test_id = req.path_params.at("id");
        std::string token = get_token(req);
        std::string user_id = AuthHandler::get_id_from_token(token);

        // Проверка доступа (включая проверку на блок — код 418)
        int status = AuthHandler::check_access(token, "", db);
        if (status != 200) { res.status = status; return; }

        // УСЛОВИЕ 1: Пользователь еще не отвечал на этот тест
        PGresult* check_dup = db.query("SELECT id FROM attempts WHERE test_id = $1 AND user_id = $2", {test_id, user_id});
        if (PQntuples(check_dup) > 0) {
            PQclear(check_dup);
            res.status = 400;
            res.set_content("Attempt already exists", "text/plain");
            return;
        }
        PQclear(check_dup);

        // УСЛОВИЕ 2: Тест находится в активном состоянии
        PGresult* check_active = db.query("SELECT is_active FROM tests WHERE id = $1", {test_id});
        if (PQntuples(check_active) == 0 || std::string(PQgetvalue(check_active, 0, 0)) != "t") {
            PQclear(check_active);
            res.status = 400;
            res.set_content("Test is not active", "text/plain");
            return;
        }
        PQclear(check_active);

        // ЭФФЕКТ: Выбирается самая последняя версия каждого вопроса на момент создания
        // Попытка всегда одна, сохраняем snapshot версий
        std::string sql = R"(
            INSERT INTO attempts (user_id, test_id, questions_snapshot, status)
            VALUES ($1, $2, (
                SELECT jsonb_agg(jsonb_build_object('logical_id', logical_id, 'version', version))
                FROM (
                    SELECT DISTINCT ON (logical_id) logical_id, version 
                    FROM questions 
                    WHERE logical_id IN (SELECT question_logical_id FROM test_questions WHERE test_id = $2)
                    ORDER BY logical_id, version DESC
                ) AS latest
            ), 'in_progress') RETURNING id
        )";

        PGresult* r = db.query(sql, {user_id, test_id});
        res.set_content(json({{"attempt_id", PQgetvalue(r, 0, 0)}}).dump(), "application/json");
        PQclear(r);
    });

    // 2. ИЗМЕНИТЬ (ОТВЕТИТЬ НА ВОПРОС)
    svr.Patch("/attempts/:id/answers", [&](const httplib::Request& req, httplib::Response& res) {
        std::string attempt_id = req.path_params.at("id");
        auto j = json::parse(req.body); // { "question_logical_id": "...", "answer_index": 1 }

        // УСЛОВИЕ: Тест активен И пользователь еще не закончил попытку
        PGresult* check = db.query(
            "SELECT a.status, t.is_active FROM attempts a JOIN tests t ON a.test_id = t.id WHERE a.id = $1", 
            {attempt_id}
        );

        if (PQntuples(check) == 0 || 
            std::string(PQgetvalue(check, 0, 0)) != "in_progress" || 
            std::string(PQgetvalue(check, 0, 1)) != "t") {
            PQclear(check);
            res.status = 403;
            return;
        }
        PQclear(check);

        // Изменяет значение ответа с указанным ID
        db.query("UPDATE attempts SET answers = answers || $1::jsonb WHERE id = $2", {j.dump(), attempt_id});
        res.status = 200;
    });

    // 3. ЗАВЕРШИТЬ ПОПЫТКУ
    svr.Post("/attempts/:id/finish", [&](const httplib::Request& req, httplib::Response& res) {
        std::string attempt_id = req.path_params.at("id");

        // УСЛОВИЕ: Тест активен И пользователь еще не закончил
        // ЭФФЕКТ: Устанавливает попытку в состояние: завершено
        db.query("UPDATE attempts SET status = 'finished' WHERE id = $1 AND status = 'in_progress'", {attempt_id});
        res.status = 200;
    });

    // 4. ПОСМОТРЕТЬ ПОПЫТКУ
    svr.Get("/attempts/:id", [&](const httplib::Request& req, httplib::Response& res) {
        std::string attempt_id = req.path_params.at("id");
        std::string token = get_token(req);
        std::string user_id = AuthHandler::get_id_from_token(token);

        // ЭФФЕКТ: Возвращает массив ответов и статус состояния попытки
        PGresult* r = db.query("SELECT user_id, test_id, answers, status FROM attempts WHERE id = $1", {attempt_id});
        if (PQntuples(r) == 0) { PQclear(r); res.status = 404; return; }
        std::string owner_id = PQgetvalue(r, 0, 0);
        std::string test_id = PQgetvalue(r, 0, 1);

        // ПО УМОЛЧАНИЮ: + Преподаватель на курсе, + Смотрит свои ответы
        bool is_teacher = AuthHandler::is_course_teacher(token, "", db); // Нужно получить course_id
        if (user_id != owner_id && !is_teacher) {
            if (AuthHandler::check_access(token, "test:answer:read", db) != 200) {
                PQclear(r); res.status = 403; return;
            }
        }

        res.set_content(DBManager::pg_to_json(r)[0].dump(), "application/json");
        PQclear(r);
    });

        // 1. ПОСМОТРЕТЬ ОТВЕТ
    // Возвращает ID вопроса и индекс выбранного варианта (0+) или -1, если ответа нет.
    svr.Get("/answers/:id", [&](const httplib::Request& req, httplib::Response& res) {
        std::string answer_id = req.path_params.at("id");
        std::string token = get_token(req);
        std::string my_id = AuthHandler::get_id_from_token(token);

        // Извлекаем информацию об ответе и владельце попытки
        PGresult* r_data = db.query(
            "SELECT a.question_id, a.selected_index, p.user_id, p.test_id "
            "FROM answers a JOIN attempts p ON a.attempt_id = p.id WHERE a.id = $1", 
            {answer_id}
        );

        if (PQntuples(r_data) == 0) { PQclear(r_data); res.status = 404; return; }

        std::string owner_id = PQgetvalue(r_data, 0, 2);
        std::string test_id = PQgetvalue(r_data, 0, 3);

        // ДОСТУП: + Преподаватель на курсе, + Пользователь смотрит свой ответ, - Остальные (answer:read)
        bool is_teacher = AuthHandler::is_course_teacher(token, test_id, db);
        if (my_id != owner_id && !is_teacher) {
            if (AuthHandler::check_access(token, "answer:read", db) != 200) {
                PQclear(r_data); res.status = 403; return;
            }
        }

        res.set_content(DBManager::pg_to_json(r_data)[0].dump(), "application/json");
        PQclear(r_data);
    });

    // 2. ИЗМЕНИТЬ ОТВЕТ
    // Изменяет индекс варианта ответа на указанный.
    svr.Patch("/answers/:id", [&](const httplib::Request& req, httplib::Response& res) {
        std::string answer_id = req.path_params.at("id");
        std::string token = get_token(req);
        auto j = json::parse(req.body); // { "index": 2 }

        // УСЛОВИЕ: Попытка не завершена
        PGresult* check = db.query(
            "SELECT p.status, p.user_id FROM answers a JOIN attempts p ON a.attempt_id = p.id WHERE a.id = $1", 
            {answer_id}
        );

        if (PQntuples(check) == 0) { PQclear(check); res.status = 404; return; }
        
        bool is_finished = (std::string(PQgetvalue(check, 0, 0)) == "finished");
        bool is_owner = (AuthHandler::get_id_from_token(token) == PQgetvalue(check, 0, 1));
        PQclear(check);

        if (is_finished) {
            res.status = 400;
            res.set_content("Cannot change answer: attempt is already finished", "text/plain");
            return;
        }

        // ДОСТУП: + Пользователь отвечающий на тест, - Остальные (answer:update)
        if (!is_owner) {
            if (AuthHandler::check_access(token, "answer:update", db) != 200) {
                res.status = 403; return;
            }
        }

        db.query("UPDATE answers SET selected_index = $1 WHERE id = $2", 
                {std::to_string(j.at("index").get<int>()), answer_id});
        res.status = 200;
    });

    // 3. УДАЛИТЬ ОТВЕТ (СБРОСИТЬ)
    // Изменяет индекс варианта ответа на -1 (не определённый).
    svr.Delete("/answers/:id", [&](const httplib::Request& req, httplib::Response& res) {
        std::string answer_id = req.path_params.at("id");
        std::string token = get_token(req);

        // УСЛОВИЕ: Попытка не завершена
        PGresult* check = db.query(
            "SELECT p.status, p.user_id FROM answers a JOIN attempts p ON a.attempt_id = p.id WHERE a.id = $1", 
            {answer_id}
        );

        if (PQntuples(check) == 0) { PQclear(check); res.status = 404; return; }
        bool is_owner = (AuthHandler::get_id_from_token(token) == PQgetvalue(check, 0, 1));
        bool is_finished = (std::string(PQgetvalue(check, 0, 0)) == "finished");
        PQclear(check);

        if (is_finished) { res.status = 400; return; }

        // ДОСТУП: + Пользователь отвечающий на тест, - Остальные (answer:del)
        if (!is_owner) {
            if (AuthHandler::check_access(token, "answer:del", db) != 200) {
                res.status = 403; return;
            }
        }

        // ЭФФЕКТ: Изменяет индекс варианта ответа на -1
        db.query("UPDATE answers SET selected_index = -1 WHERE id = $1", {answer_id});
        res.status = 200;
    });

    // Обработчик для всех запросов после их выполнения
    svr.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*"); // Разрешить запросы от веб-клиента
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    });

    // Ответ на Preflight-запросы от браузера (Nginx может перехватывать их сам, но лучше продублировать)
    svr.Options(R"(.*)", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 204;
    });
    // Вызывается только Go-модулем внутри локальной сети
    svr.Post("/internal/sync-block", [&](const httplib::Request& req, httplib::Response& res) {
        auto j = json::parse(req.body); // { "user_id": "...", "is_blocked": true }
        
        std::string sql = "UPDATE users SET is_blocked = $1 WHERE id = $2";
        db.query(sql, { j["is_blocked"].get<bool>() ? "true" : "false", j["user_id"] });
        
        res.status = 200;
    });

    svr.Get("/", [](const httplib::Request &req, httplib::Response &res)
            { res.set_content("<h1>Main Module is Running</h1><p>Server is alive!</p>", "text/html"); });

    // Это эндпоинт для проверки здоровья системы
    svr.Get("/health", [&](const httplib::Request &req, httplib::Response &res)
            {
        res.status = 200;
        res.set_content("OK", "text/plain"); });
    std::cout << "Server started at http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
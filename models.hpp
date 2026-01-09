#pragma once
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace models {

// Сущность: Пользователь
struct User {
    int id;
    std::string email;
    std::string full_name;
    std::vector<std::string> roles;
    bool is_blocked;

    // Метод для удобной конвертации в JSON
    json to_json() const {
        return {
            {"id", id},
            {"email", email},
            {"full_name", full_name},
            {"roles", roles},
            {"is_blocked", is_blocked}
        };
    }
};

// Сущность: Дисциплина
struct Course {
    int id;
    std::string title;
    std::string description;
    int teacher_id;
    bool is_deleted;

    json to_json() const {
        return {
            {"id", id},
            {"title", title},
            {"description", description},
            {"teacher_id", teacher_id}
        };
    }
};

// Сущность: Вопрос (с поддержкой версионности)
struct Question {
    int id;
    int author_id;
    std::string title;
    std::string text;
    json options; // Варианты ответов храним как JSON-массив
    int correct_option_index;
    int version;

    json to_json() const {
        return {
            {"id", id},
            {"title", title},
            {"text", text},
            {"options", options},
            {"version", version}
        };
    }
};

// Сущность: Тест
struct Test {
    int id;
    int course_id;
    std::string title;
    std::vector<int> question_ids;
    bool is_active;

    json to_json() const {
        return {
            {"id", id},
            {"course_id", course_id},
            {"title", title},
            {"question_ids", question_ids},
            {"is_active", is_active}
        };
    }
};

// Сущность: Попытка прохождения
struct Attempt {
    int id;
    int user_id;
    int test_id;
    std::string status; // "in_progress" или "completed"
    std::string created_at;

    json to_json() const {
        return {
            {"id", id},
            {"test_id", test_id},
            {"status", status},
            {"created_at", created_at}
        };
    }
};

} // namespace models
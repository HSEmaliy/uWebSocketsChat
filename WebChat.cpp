#include <iostream>
#include <uwebsockets/App.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
using json = nlohmann::json;

struct UserData {
    int user_id;
    std::string name;
};

std::unordered_map<int, UserData*> online_users;

// Публичные сообщения
// user10 => server: {"command": "publick_msg", "text": "text"}
// server => all_users {"command": "publick_msg", "text": "text", user_from: 10}

void process_public_msg(json &data, auto* ws) {
    int user_id = ws->getUserData()->user_id;
    json payload = {
        {"command", data["command"]},
        {"text",data["text"]},
        {"user_from", user_id}
    };
    ws->publish("public", payload.dump());
    std::cout << "User sent Public Message: " << user_id << std::endl;
}




// Приватные сообщения 
// user10 => server: {"command": "private_msg", "text": "text", user_to: 20}
// server => user20 {"command": "private_msg", "text": "text", user_from: 10}

void process_private_msg(json data, auto* ws) {
    int user_id = ws->getUserData()->user_id;
    json payload = {
        {"command", data["command"]},
        {"text",data["text"]},
        {"user_from", user_id}
    };
    int user_to = data["user_to"];
    ws->publish("user" + std::to_string(user_to), payload.dump());
    std::cout << "User sent Private Message: " << user_id << std::endl;
}

//возможность указания имени
void process_set_name(json data, auto* ws) {
    std::string prev = ws->getUserData()->name;
    int user_id = ws->getUserData()->user_id;
    ws->getUserData()->name = data["name"];
    std::cout << "User sent Set name: " << user_id << std::endl;
    std::cout << "Prev name: " << prev;
    std::cout << "\tNew name: " << ws->getUserData()->name;
}

// оповещение о подключении юзера
// 1) подкл нового пользователя
// 2) откл пользователь
// 3) свежеподлючившемуся пользователю сообщить о всех тех, кто онлайн
// server => public {"command":"status", "user_id": 11, "online": True/False, "name": "name"}
std::string process_status(auto data, bool online) {
    json payload = {
        {"command", "status"},
        {"user_id", data->user_id},
        {"name", data->name},
        {"online", online}
    };
    return payload.dump();
}


int main()
{
    int latest_user_id = 10;

    uWS::App app = uWS::App().ws<UserData>("/*", {
        /* Settings */
        .maxPayloadLength = 100 * 1024 * 1024,
        .idleTimeout = 16,
        /* Handlers */
        // вызывается при новом подключении
        .open = [&latest_user_id](auto* ws) { 
            ws->getUserData();
            UserData* data = ws->getUserData();
            data->user_id = latest_user_id++;
            data->name = "noname"; 

            std::cout << "New user connected: " << data->user_id << std::endl;
            ws->subscribe("public");
            ws->subscribe("user" + std::to_string(data->user_id));

            ws->publish("public", process_status(data, true));

            for (auto& entry : online_users) {
                ws->send(process_status(entry.second, true), uWS::OpCode::TEXT);
            }

            online_users[data->user_id] = data;
        },
        .message = [](auto* ws, std::string_view data, uWS::OpCode opCode) {
            json parsed_data = json::parse(data);
            if (parsed_data["command"] == "public_msg") {
                process_public_msg(parsed_data, ws);
            }
            else if (parsed_data["command"] == "private_msg") {
                process_private_msg(parsed_data, ws);
            }
            else if (parsed_data["command"] == "set_name") {
                process_set_name(parsed_data, ws);
                auto* data = ws->getUserData();
                ws->publish("public", process_status(data, true));
            }
        },
        .close = [&app](auto* ws, int /*code*/, std::string_view /*message*/) {
            auto* data = ws->getUserData();
            online_users.erase(data->user_id);
            app.publish("public", process_status(data, false), uWS::OpCode::TEXT);
        }
    }).listen(9001, [](auto* listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << 9001 << std::endl;
        }
    }).run();
}

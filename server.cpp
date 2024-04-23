#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <thread>
#include <mutex>
#include <fstream>
#include <sstream>

#define MAX_LEN 200
#define NUM_COLORS 6

std::mutex cout_mtx, clients_mtx;

std::string caesarEncrypt(std::string text, int s);
std::string caesarDecrypt(std::string text, int s);

struct ClientNode {
    int id;
    std::string name;
    int socket;
    std::thread th;
    ClientNode* next;
};

class ClientRegistry {
private:
    ClientNode* head;

public:
    ClientRegistry() : head(nullptr) {} //Constructor
    ~ClientRegistry() {   //Destructor
        ClientNode* current = head;
        while (current != nullptr) {
            ClientNode* next = current->next;
            if (current->th.joinable())
                current->th.join();
            close(current->socket);
            delete current;
            current = next;
        }
    }

    void registerClient(int socket, int id, const std::string& name) {
        std::lock_guard<std::mutex> lock(clients_mtx);
        ClientNode* new_client = new ClientNode{id, name, socket, std::thread(), nullptr};
        new_client->next = head;
        head = new_client;
    }

    void unregisterClient(int id) {
        std::lock_guard<std::mutex> lock(clients_mtx);
        ClientNode *current = head, *prev = nullptr;
        while (current != nullptr) {
            if (current->id == id) {
                if (prev) {
                    prev->next = current->next;
                } else {
                    head = current->next;
                }
                if (current->th.joinable())
                    current->th.detach();
                close(current->socket);
                delete current;
                return;
            }
            prev = current;
            current = current->next;
        }
    }

    void broadcast_message(const std::string& message, int sender_id, const std::string& sender_name, int shift) {
        std::lock_guard<std::mutex> lock(clients_mtx);
        ClientNode* current = head;
        std::string encrypted_msg = caesarEncrypt(message, shift);
        while (current != nullptr) {
            if (current->id != sender_id) {
                std::string full_message = sender_name + ": " + encrypted_msg;
                send(current->socket, full_message.c_str(), full_message.length() + 1, 0);
            }
            current = current->next;
        }
    }
};

ClientRegistry clients;

std::string color(int code) {
    code %= NUM_COLORS;  // Ensure code is within the valid range of colors
    switch (code) {
        case 0: return "\033[32m"; // Green
        case 1: return "\033[33m"; // Yellow
        case 2: return "\033[31m"; // Red
        case 3: return "\033[35m"; // Magenta
        case 4: return "\033[36m"; // Cyan
        case 5: return "\033[34m"; // Blue
        default: return "\033[0m";  // Reset to default term color in case of any issue 
    }
}


void shared_print(const std::string& str, bool endLine = true) {
    std::lock_guard<std::mutex> guard(cout_mtx);
    std::cout << str;
    if (endLine)
        std::cout << std::endl;
}

bool verifyUser(const std::string& username, const std::string& password) {
    std::ifstream file("usercred");
    std::string line, usr, pwd;
    while (getline(file, line)) {
        std::istringstream iss(line);
        if (iss >> usr >> pwd) {
            if (usr == username && pwd == password) {
                return true;
            }
        }
    }
    return false;
}

void User_Register(const std::string& username, const std::string& password) {
    std::ofstream file("usercred", std::ios::app);
    file << username << " " << password << std::endl;
}

std::string caesarEncrypt(std::string text, int s) {
    std::string result = "";
    for (int i = 0; i < text.length(); i++) {
        if (isalpha(text[i])) {
            char base = islower(text[i]) ? 'a' : 'A';
            int offset = text[i] - base;
            int shifted = (offset + s) % 26;
            if (shifted < 0) {
                shifted += 26; // Correct for negative shifts
            }
            result += char(shifted + base);
        } else {
            result += text[i]; // Non-alphabetic characters remain unchanged
        }
    }
    return result;
}


std::string caesarDecrypt(std::string text, int s) {
    return caesarEncrypt(text, -s);
}
// Handle clients in case of success or failure
void handle_client(int client_socket, int id) {
    int shift = 5;  // Caesar cipher shift value
    char buffer[MAX_LEN] = {0};
    recv(client_socket, buffer, MAX_LEN, 0);

    std::istringstream ss(buffer);
    std::string command, username, password;
    ss >> command >> username >> password;

    if (command == "login" && verifyUser(username, password)) {
    std::string success_msg = "Login Successful";
    send(client_socket, caesarEncrypt(success_msg, shift).c_str(), success_msg.length() + 1, 0);
} else if (command == "register") {
    if (!verifyUser(username, password)) {
        User_Register(username, password);
        std::string reg_success_msg = "Registration Successful";
        send(client_socket, caesarEncrypt(reg_success_msg, shift).c_str(), reg_success_msg.length() + 1, 0);
    } else {
        std::string user_exists_msg = "Username already exists";
        send(client_socket, caesarEncrypt(user_exists_msg, shift).c_str(), user_exists_msg.length() + 1, 0);
    }
} else {
    std::string auth_fail_msg = "Authentication Failed";
    send(client_socket, caesarEncrypt(auth_fail_msg, shift).c_str(), auth_fail_msg.length() + 1, 0);
    return;  // Terminate this client thread if login fails
}


    clients.registerClient(client_socket, id, username);

    std::string welcome_message = username + " has joined the chat\n";
    clients.broadcast_message(welcome_message, id, "Server", shift);
    shared_print(color(id % NUM_COLORS) + welcome_message + "\033[0m");

    char str[MAX_LEN];
    while (true) {
        memset(str, 0, MAX_LEN);
        int bytes_received = recv(client_socket, str, MAX_LEN, 0);
        if (bytes_received <= 0) break;

        str[bytes_received] = '\0';  // Ensure null termination

        if (strcmp(str, "#exit") == 0) {
            std::string message = username + " has left";
            clients.broadcast_message(message, id, "Server", shift);
            shared_print(color(id % NUM_COLORS) + message + "\033[0m");
            break;
        }
        std::string decrypted_msg = caesarDecrypt(std::string(str), shift);  // Optionally decrypt if server needs to process the message
        clients.broadcast_message(decrypted_msg, id, username, shift);
        shared_print(color(id % NUM_COLORS) + username + ": " + "\033[0m" + decrypted_msg);
    }

    clients.unregisterClient(id);
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket: ");
        exit(-1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(12345);
    server.sin_addr.s_addr = INADDR_ANY;
    memset(&server.sin_zero, 0, sizeof(server.sin_zero));

    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == -1) {
        perror("bind error: ");
        exit(-1);
    }

    if (listen(server_socket, 8) == -1) {
        perror("listen error: ");
        exit(-1);
    }

std::cout << "\033[35m" << "\n\t  ================= Lets Chat =================   " << std::endl << "\033[0m";

    int id = 0;
    while (true) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int client_socket = accept(server_socket, (struct sockaddr *)&client, &len);
        if (client_socket == -1) {
            perror("accept error: ");
            continue;
        }
        id++;
        std::thread t(handle_client, client_socket, id);
        t.detach();
    }

    close(server_socket);
    return 0;
}

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

std::mutex cout_mtx, clients_mtx;  // Mutexes for managing console and client list access

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
    ClientNode* head;  // Head of the linked list of clients

public:
    ClientRegistry() : head(nullptr) {} // Constructor initializes head to nullptr
    ~ClientRegistry() {   // Destructor cleans up all client nodes
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
        ClientNode* current = head, *prev = nullptr;
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

    void broadcast_message(const std::string& msg, int sender_id) {
        std::lock_guard<std::mutex> lock(clients_mtx);
        ClientNode* current = head;
        while (current != nullptr) {
            if (current->id != sender_id) {
                send(current->socket, msg.c_str(), msg.length() + 1, 0);
            }
            current = current->next;
        }
    }
};

// Returns terminal color codes based on an integer code
std::string color(int code) {
    code %= NUM_COLORS;  // Ensure code is within the valid range of colors
    switch (code) {
        case 0: return "\033[32m"; // Green
        case 1: return "\033[33m"; // Yellow
        case 2: return "\033[31m"; // Red
        case 3: return "\033[35m"; // Magenta
        case 4: return "\033[36m"; // Cyan
        case 5: return "\033[34m"; // Blue
        default: return "\033[0m";  // Reset to default terminal color(white), in case of any issue
    }
}

void handle_client(int client_socket, int id, ClientRegistry& clients) {
    char name[MAX_LEN];
    recv(client_socket, name, sizeof(name), 0);
    std::string welcome_message = color(id % NUM_COLORS) + std::string(name) + " has joined" + color(0);
    clients.broadcast_message(welcome_message, id);

    char str[MAX_LEN];
    while (true) {
        int bytes_received = recv(client_socket, str, sizeof(str), 0);
        if (bytes_received <= 0) break;  // Break on disconnection
        std::string message = color(id % NUM_COLORS) + std::string(name) + ": " + std::string(str) + color(0);
        clients.broadcast_message(message, id);
    }
}

int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket: ");
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&server_addr.sin_zero, 0, sizeof(server_addr.sin_zero));

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind: ");
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, 10) == -1) {
        perror("listen: ");
        close(server_socket);
        return -1;
    }

    std::cout << color(4) << "Server is running on port 12345" << color(0) << std::endl;

    ClientRegistry clients;
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);
        if (client_socket == -1) {
            perror("accept: ");
            continue;
        }

        int id = ntohs(client_addr.sin_port);  // Use the client's port as a simple ID
        clients.registerClient(client_socket, id, "Anonymous");  // Register new client
        std::thread(client_handle, client_socket, id, std::ref(clients)).detach();  // Start a new thread to handle the client
    }

    close(server_socket);
    return 0;
}

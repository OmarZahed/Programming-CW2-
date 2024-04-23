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

std::mutex cout_mtx, clients_mtx;  // Mutexes for thread-safe output and client management

// Function prototypes for Caesar cipher encryption and decryption
std::string caesarEncrypt(std::string text, int s);
std::string caesarDecrypt(std::string text, int s);

// Structure to hold client data including ID, name, socket, thread, and link to next client
struct ClientNode {
    int id;
    std::string name;
    int socket;
    std::thread th;
    ClientNode* next;
};

// Class to manage a linked list of clients
class ClientRegistry {
private:
    ClientNode* head;  // Head of the linked list

public:
    ClientRegistry() : head(nullptr) {}  // Constructor initializes head to nullptr

    // Destructor cleans up all clients, joins threads, and closes sockets
    ~ClientRegistry() {   
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

    // Register a new client and add it to the front of the linked list
    void registerClient(int socket, int id, const std::string& name) {
        std::lock_guard<std::mutex> lock(clients_mtx);
        ClientNode* new_client = new ClientNode{id, name, socket, std::thread(), nullptr};
        new_client->next = head;
        head = new_client;
    }

    // Unregister a client by ID and remove it from the list, closing its socket
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

    // Broadcast an encrypted message to all clients except the sender
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

ClientRegistry clients;  // Global instance of ClientRegistry

// Returns a terminal color code based on an input code, used for colored output in the terminal
std::string color(int code) {
    code %= NUM_COLORS;  // Ensure code is within the valid range of colors
    switch (code) {
        case 0: return "\033[32m"; // Green
        case 1: return "\033[33m"; // Yellow
        case 2: return "\033[31m"; // Red
        case 3: return "\033[35m"; // Magenta
        case 4: return "\033[36m"; // Cyan
        case 5: return "\033[34m"; // Blue
        default: return "\033[0m";  // Reset to default term color(white) in case of any issue 
    }
}

// Thread-safe printing function with an optional line ending
void shared_print(const std::string& str, bool endLine = true) {
    std::lock_guard<std::mutex> guard(cout_mtx);
    std::cout << str;
    if (endLine)
        std::cout << std::endl;
}

// Checks if user credentials exist in the usercred file and are valid
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

// Adds new user credentials to the usercred file
void User_Register(const std::string& username, const std::string& password) {
    std::ofstream file("usercred", std::ios::app);
    file << username << " " << password << std::endl;
}

// Encrypts text using Caesar cipher, supporting both encryption and decryption based on shift direction
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

// Decrypts text by applying a negative shift in the Caesar cipher
std::string caesarDecrypt(std::string text, int s) {
    return caesarEncrypt(text, -s);
}

// Main server loop: sets up socket, binds, listens, and handles incoming client connections
int main() {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("socket: ");
        exit(-1);
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(12345);//port
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

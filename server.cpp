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
        ClientNode* new_client = new ClientNode{ id, name, socket, std::thread(), nullptr };
        new_client->next = head;
        head = new_client;
    }

    void unregisterClient(int id) {
        std::lock_guard<std::mutex> lock(clients_mtx);
        ClientNode* current = head, * prev = nullptr;
        while (current != nullptr) {
            if (current->id == id) {
                if (prev) {
                    prev->next = current->next;
                }
                else {
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

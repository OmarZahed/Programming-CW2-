#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <signal.h>
#include <csignal>
#include <mutex>

#define MAX_LEN 200
#define NUM_COLORS 6

bool exit_flag = false;
std::thread t_send, t_recv;
int client_socket;
const char* def_col = "\033[0m"; // Default terminal color(white), used for what appears after system notifications to return to default.

void Exit_handler(int signal);
std::string color(int code);
void send_message(int client_socket);
void recv_message(int client_socket);
void loginOrRegister();

std::string color(int code) {
    code %= NUM_COLORS;
    switch (code) {
        case 0: return "\033[35m"; // Magenta
        case 1: return "\033[32m"; // Green
        case 2: return "\033[31m"; // Red
        case 3: return "\033[36m"; // Cyan
        case 4: return "\033[34m"; // Blue
        case 5: return "\033[33m"; // Yellow
        default: return "\033[0m";
    }
}

int main() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket: ");
        exit(-1);
    }

    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_port = htons(12345);
    client.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(&client.sin_zero, 0, sizeof(client.sin_zero));

    if (connect(client_socket, (struct sockaddr *)&client, sizeof(client)) == -1) {
        perror("connect: ");
        exit(-1);
    }

    loginOrRegister();

    std::signal(SIGINT, Exit_handler);

    std::cout << color(0) << "\n\t  ================= Lets Chat =================   " << std::endl << def_col;

    std::thread t1([&]{ send_message(client_socket); });
    std::thread t2([&]{ recv_message(client_socket); });
    t_send = std::move(t1);
    t_recv = std::move(t2);

    if (t_send.joinable()) t_send.join();
    if (t_recv.joinable()) t_recv.join();

    return 0;
}

void loginOrRegister() {
    int choice;
    std::cout << "1. Login\n2. Register\nChoose (1-2): ";
    std::cin >> choice;
    std::cin.ignore();  // Consume the newline character left by std::cin >>

    char username[MAX_LEN], password[MAX_LEN], buffer[MAX_LEN];
    std::cout << "Enter username: ";
    std::cin.getline(username, MAX_LEN);
    std::cout << "Enter password: ";
    std::cin.getline(password, MAX_LEN);

    if (choice == 1) {
        snprintf(buffer, sizeof(buffer), "login %s %s", username, password);
    } else {
        snprintf(buffer, sizeof(buffer), "register %s %s", username, password);
    }

    send(client_socket, buffer, strlen(buffer) + 1, 0);  // Send choice, username, and password

    char response[MAX_LEN];
    recv(client_socket, response, MAX_LEN, 0);  // Receive login or registration status
    std::cout << response << std::endl;

    while (strstr(response, "Failed") != nullptr) {
        std::cout << "Authentication failed. Try again.\n";
        std::cout << "Enter username: ";
        std::cin.getline(username, MAX_LEN);
        std::cout << "Enter password: ";
        std::cin.getline(password, MAX_LEN);

        if (choice == 1) {
            snprintf(buffer, sizeof(buffer), "login %s %s", username, password);
        } else {
            snprintf(buffer, sizeof(buffer), "register %s %s", username, password);
        }

        send(client_socket, buffer, strlen(buffer) + 1, 0);
        recv(client_socket, response, MAX_LEN, 0);
        std::cout << response << std::endl;
    }
}

void Exit_handler(int signal) {
    const char* str = "#exit";
    send(client_socket, str, strlen(str) + 1, 0);  // Send the exit command
    exit_flag = true;
    t_send.detach();
    t_recv.detach();
    close(client_socket);
    std::exit(signal);
}

void send_message(int client_socket) {
    while (!exit_flag) {
        std::cout << color(1) << "You : " << def_col;
        char str[MAX_LEN];
        std::cin.getline(str, MAX_LEN);
        send(client_socket, str, strlen(str) + 1, 0);
        if (strcmp(str, "#exit") == 0) {
            exit_flag = true;
            break;
        }
    }
}

void recv_message(int client_socket) {
    while (!exit_flag) {
        char str[MAX_LEN];
        int bytes_received = recv(client_socket, str, MAX_LEN, 0);
        if (bytes_received <= 0) continue;

        str[bytes_received] = '\0';  // Ensure null termination

        std::cout << "\r" << str << std::endl;  // Display received message
        std::cout << color(1) << "You : " << def_col;
        fflush(stdout);  // Ensure prompt is shown immediately
    }
}

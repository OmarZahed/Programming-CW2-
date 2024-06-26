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
const char* def_col = "\033[0m"; // Default terminal color(white), used for what appeares after system notifications to return to default.

void Exit_handler(int signal);
std::string color(int code);
void send_message(int client_socket, int shift);
void recv_message(int client_socket, int shift);
void loginOrRegister(int shift);
std::string caesarEncrypt(std::string text, int s);
std::string caesarDecrypt(std::string text, int s);

std::string color(int code) {
    code %= NUM_COLORS;  // Ensure code is within the valid range of colors
    switch (code) {
        case 0: return "\033[35m"; // Magenta
        case 1: return "\033[32m"; // Green
        case 2: return "\033[31m"; // Red
        case 3: return "\033[36m"; // Cyan
        case 4: return "\033[34m"; // Blue
        case 5: return "\033[33m"; // Yellow
        default: return "\033[0m";  // Reset to default color(white) , incase of any issue
    }
}

int main() {
    int shift = 5;  // Encryption shift value
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("socket: ");
        exit(-1);
    }

    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_port = htons(12345);//port
    client.sin_addr.s_addr = inet_addr("127.0.0.1");// IP addresse 
    memset(&client.sin_zero, 0, sizeof(client.sin_zero));

    if (connect(client_socket, (struct sockaddr *)&client, sizeof(client)) == -1) {
        perror("connect: ");
        exit(-1);
    }

    loginOrRegister(shift);  // Prompt user for login or registration

    std::signal(SIGINT, Exit_handler);

    std::cout << color(0) << "\n\t  ================= Lets Chat =================   " << std::endl << def_col;

    std::thread t1([&]{ send_message(client_socket, shift); });
    std::thread t2([&]{ recv_message(client_socket, shift); });
    t_send = std::move(t1);
    t_recv = std::move(t2);

    if (t_send.joinable()) t_send.join();
    if (t_recv.joinable()) t_recv.join();

    return 0;
}
// Handles the user's decision to either log in or register and sends encrypted credentials to the server.
void loginOrRegister(int shift) {
    int choice;
    std::cout << "1. Login\n2. Register\nChoose (1-2): ";
    std::cin >> choice;
    std::cin.ignore();  // Consume the newline character left by std::cin >>

    char username[MAX_LEN], password[MAX_LEN], buffer[MAX_LEN];
    std::cout << "Enter username: ";
    std::cin.getline(username, MAX_LEN);
    std::cout << "Enter password: ";
    std::cin.getline(password, MAX_LEN);

    // Encrypt username and password before sending
    std::string encryptedUsername = caesarEncrypt(username, shift);
    std::string encryptedPassword = caesarEncrypt(password, shift);

    if (choice == 1) {
        snprintf(buffer, sizeof(buffer), "login %s %s", encryptedUsername.c_str(), encryptedPassword.c_str());
    } else {
        snprintf(buffer, sizeof(buffer), "register %s %s", encryptedUsername.c_str(), encryptedPassword.c_str());
    }

    send(client_socket, buffer, strlen(buffer) + 1, 0);  // Send choice, encrypted username, and encrypted password

    char response[MAX_LEN];
    recv(client_socket, response, MAX_LEN, 0);  // Receive login or registration status
    std::string decryptedResponse = caesarDecrypt(response, shift);
    std::cout << decryptedResponse << std::endl;

    while (strstr(decryptedResponse.c_str(), "Failed") != nullptr) {
        std::cout << "Authentication failed. Try again.\n";
        std::cout << "Enter username: ";
        std::cin.getline(username, MAX_LEN);
        std::cout << "Enter password: ";
        std::cin.getline(password, MAX_LEN);

        encryptedUsername = caesarEncrypt(username, shift);
        encryptedPassword = caesarEncrypt(password, shift);

        if (choice == 1) {
            snprintf(buffer, sizeof(buffer), "login %s %s", encryptedUsername.c_str(), encryptedPassword.c_str());
        } else {
            snprintf(buffer, sizeof(buffer), "register %s %s", encryptedUsername.c_str(), encryptedPassword.c_str());
        }

        send(client_socket, buffer, strlen(buffer) + 1, 0);
        recv(client_socket, response, MAX_LEN, 0);
        decryptedResponse = caesarDecrypt(response, shift);
        std::cout << decryptedResponse << std::endl;
    }
}
// Handles shutdown on receiving a signal like SIGINT.
void Exit_handler(int signal) {
    const char* str = "BREAK";
    send(client_socket, str, strlen(str) + 1, 0);  // Send the exit command
    exit_flag = true;
    t_send.detach();
    t_recv.detach();
    close(client_socket);
    std::exit(signal);
}
// Sends encrypted messages to the server in a loop until the user decides to exit.
void send_message(int client_socket, int shift) {
    while (!exit_flag) {
        std::cout << color(1) << "You : " << def_col;
        char str[MAX_LEN];
        std::cin.getline(str, MAX_LEN);
        std::string encrypted = caesarEncrypt(std::string(str), shift);
        send(client_socket, encrypted.c_str(), encrypted.length() + 1, 0);
        if (strcmp(str, "#exit") == 0) {
            exit_flag = true;
            break;
        }
    }
}
// Receives messages from the server, decrypts them, and displays them to the user.
void recv_message(int client_socket, int shift) {
    while (!exit_flag) {
        char str[MAX_LEN];
        int bytes_received = recv(client_socket, str, MAX_LEN, 0);
        if (bytes_received <= 0) continue;

        str[bytes_received] = '\0';  // Ensure null termination
        std::string decrypted = caesarDecrypt(std::string(str), shift);  // Decrypt before display

        std::cout << "\r" << decrypted << std::endl;  // Show decrypted message
        std::cout << color(1) << "You : " << def_col;
        fflush(stdout);  // Ensure prompt is shown immediately
    }
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

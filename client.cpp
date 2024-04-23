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

bool exit_flag = false;  // Flag to control the flow based on user exit command
std::thread t_send, t_recv;  // Threads for sending and receiving messages
int client_socket;  // Socket descriptor for the client
const char* def_col = "\033[0m";  // Default terminal color (white), resets color after messages


bool exit_flag = false;
std::thread t_send, t_recv;
int client_socket;
//forward decleration
void Exit_handler(int signal);
std::string color(int code);
void send_message(int client_socket);
void recv_message(int client_socket);
//declare register and login still needs implementation 

// Returns the terminal color string based on the provided code
std::string color(int code) {
    code %= NUM_COLORS;  // Ensure code is within the valid range of colors
    switch (code) {
        case 0: return "\033[35m";  // Magenta
        case 1: return "\033[32m";  // Green
        case 2: return "\033[31m";  // Red
        case 3: return "\033[36m";  // Cyan
        case 4: return "\033[34m";  // Blue
        case 5: return "\033[33m";  // Yellow
        default: return def_col;    // Default color
    }
}

// Entry point of the application
int main() {
    client_socket = socket(AF_INET, SOCK_STREAM, 0);  // Create socket
    if (client_socket == -1) {
        perror("socket: ");  // Error handling for socket creation
        exit(-1);
    }

    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_port = htons(12345);  // Port number
    client.sin_addr.s_addr = inet_addr("127.0.0.1");  // Server IP address
    memset(&client.sin_zero, 0, sizeof(client.sin_zero));  // Clear structure padding

    if (connect(client_socket, (struct sockaddr *)&client, sizeof(client)) == -1) {
        perror("connect: ");  // Error handling for connection
        exit(-1);
    }

    std::signal(SIGINT, Exit_handler);  // Signal handler for interrupt signal

    std::cout << color(0) << "\n\t  ================= Lets Chat =================   " << std::endl << def_col;

    // Create threads for sending and receiving messages
    std::thread t1([&]{ send_message(client_socket); });
    std::thread t2([&]{ recv_message(client_socket); });
    t_send = std::move(t1);
    t_recv = std::move(t2);

    // Wait for threads to complete
    if (t_send.joinable()) t_send.join();
    if (t_recv.joinable()) t_recv.join();

    return 0;
}
// Handles program exit on receiving SIGINT
void Exit_handler(int signal) {
    const char* str = "#exit";
    send(client_socket, str, strlen(str) + 1, 0);  // Send the exit command
    exit_flag = true;
    t_send.detach();  // Detach threads
    t_recv.detach();
    close(client_socket);  // Close the socket
    std::exit(signal);  // Exit program with signal code
}

// Handles sending messages to the server
void send_message(int client_socket) {
    while (!exit_flag) {
        std::cout << color(1) << "You : " << def_col;
        char str[MAX_LEN];
        std::cin.getline(str, MAX_LEN);  // Get message from user
        send(client_socket, str, strlen(str) + 1, 0);  // Send message
        if (strcmp(str, "#exit") == 0) {
            exit_flag = true;  // Exit if user types "#exit"
            break;
        }
    }
}

// Handles receiving messages from the server
void recv_message(int client_socket) {
    while (!exit_flag) {
        char str[MAX_LEN];
        int bytes_received = recv(client_socket, str, MAX_LEN, 0);  // Receive message
        if (bytes_received <= 0) continue;  // Handle potential errors

        str[bytes_received] = '\0';  // Ensure string is null-terminated

        std::cout << "\r" << str << std::endl;  // Display the message
        std::cout << color(1) << "You : " << def_col;
        fflush(stdout);  // Flush stdout to ensure prompt display
    }
}

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstdlib>       
#include <arpa/inet.h>   
#include <sstream>
#include "student.h" 
using namespace std;

#define PORT 8080
#define BUFFER_SIZE 1024*64

// Function to convert Student_t to JSON
string studentToJson(const student_t& student) {
    stringstream ss;
    ss << "{ \"tid\": " << ntohl(student.tid)
       << ", \"name\": \"" << student.name
       << "\", \"age\": " << ntohs(student.age)
       << ", \"department\": \"" << student.department
       << "\", \"gpa\": " << ntohl(student.gpa)
       << " }";
    return ss.str();
}

// Send student data as a single HTTP response
void sendStudentData(int client_socket) {
    student_t student;
    
    // Fill student data
    student.tid = htonl(49);
    student.age = htons(18);
    student.gpa = htonl(10);
    strncpy(student.name, "Harsh Vardhan", sizeof(student.name) - 1);
    student.name[sizeof(student.name) - 1] = '\0';
    strncpy(student.department, "CSE", sizeof(student.department) - 1);
    student.department[sizeof(student.department) - 1] = '\0';

    // Convert student struct to JSON
    string json_response = studentToJson(student);

    // Prepare HTTP response
    string http_response = "HTTP/1.1 200 OK\r\n"
                           "Content-Type: application/json\r\n"
                           "Content-Length: " + to_string(json_response.size()) + "\r\n"
                           "Connection: close\r\n"
                           "\r\n" + json_response;

    int bytesSend = send(client_socket, http_response.c_str(), http_response.size(), 0);
    if(bytesSend <= 0) {
        cout << "Data send failed\n";
    } else {
        cout << "Sent JSON Response (" << bytesSend << " bytes):\n" << json_response << endl;
    }
    
    close(client_socket);
}

// Stream student data with chunked encoding
void streamStudentData(int client_socket) {
    // Send HTTP headers with chunked encoding
    string headers = "HTTP/1.1 200 OK\r\n"
                     "Content-Type: application/json\r\n"
                     "Transfer-Encoding: chunked\r\n"
                     "Connection: close\r\n"
                     "\r\n";

    send(client_socket, headers.c_str(), headers.size(), 0);

    cout << "Starting chunked streaming...\n";
    
    // Simulated real-time streaming (sending student data in chunks)
    for (int i = 0; i < 5; i++) {
        student_t student;
        student.tid = htonl(49 + i);
        student.age = htons(18 + i);
        student.gpa = htonl(10 + i);

        strncpy(student.name, "Student_", sizeof(student.name) - 1);
        char num = '0' + i;
        strncat(student.name, &num, 1);
        student.name[sizeof(student.name) - 1] = '\0';

        strncpy(student.department, "CSE", sizeof(student.department) - 1);
        student.department[sizeof(student.department) - 1] = '\0';

        string json_data = studentToJson(student);

        // Format: <chunk size in hex>\r\n<chunk data>\r\n
        char hex_size[10];
        sprintf(hex_size, "%X\r\n", (unsigned int)json_data.size());
        
        send(client_socket, hex_size, strlen(hex_size), 0);
        send(client_socket, json_data.c_str(), json_data.size(), 0);
        send(client_socket, "\r\n", 2, 0);
        
        cout << "Sent chunk " << (i+1) << " (" << json_data.size() << " bytes)\n";
        sleep(1);  // Simulate real-time streaming delay
    }

    // Send final empty chunk to signal end of stream
    send(client_socket, "0\r\n\r\n", 5, 0);

    cout << "Streaming completed.\n";
    close(client_socket);
}

// Handle client requests
void handleClient(int client_socket) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    int bytesReceived = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytesReceived <= 0) {
        cout << "Client disconnected or error in receiving data\n";
        close(client_socket);
        return;
    }

    cout << "Received HTTP request:\n" << buffer << endl;

    // Parse the request
    if (strstr(buffer, "GET /stream_student_detail") != NULL) {
        cout << "Handling stream request\n";
        streamStudentData(client_socket);
    } else if (strstr(buffer, "GET /send_student_detail") != NULL) {
        cout << "Handling single student request\n";
        sendStudentData(client_socket);
    } else {
        // Return 404 for unknown requests
        string http_response = "HTTP/1.1 404 Not Found\r\n"
                               "Content-Type: text/plain\r\n"
                               "Content-Length: 13\r\n"
                               "Connection: close\r\n"
                               "\r\n"
                               "404 Not Found";
        
        send(client_socket, http_response.c_str(), http_response.size(), 0);
        cout << "Sent 404 response\n";
        close(client_socket);
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

    // Create a socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Allow immediate reuse of the port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        return 1;
    }

    // Bind address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == -1) {
        perror("Bind failed");
        close(server_fd);
        return 1;
    }

    // Listen for clients
    if (listen(server_fd, 5) == -1) {  // Increased backlog to 5
        perror("Listen failed");
        close(server_fd);
        return 1;
    }

    cout << "Server started on port " << PORT << ". Waiting for connections...\n";

    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr*)&address, &addr_len)) == -1) {
            perror("Accept failed");
            continue;  // Continue to next iteration instead of exiting
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), client_ip, INET_ADDRSTRLEN);
        cout << "Client connected: " << client_ip << ":" << ntohs(address.sin_port) << endl;

        // Handle client in the same thread
        handleClient(client_fd);
    }
    
    close(server_fd);
    return 0;
}

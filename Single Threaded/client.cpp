#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "student.h" 
using namespace std;

#define PORT 8080
#define BUFFER_SIZE 4096  // Increased buffer size for larger responses

// Function to extract and print JSON from HTTP response
void printJsonResponse(const char *response) {
    // Find the start of the JSON body (after "\r\n\r\n")
    const char *json_start = strstr(response, "\r\n\r\n");
    if (json_start) {
        json_start += 4; // Move past "\r\n\r\n"
        cout << "\nExtracted JSON Data:\n" << json_start << endl;
    } else {
        cout << "\nError: JSON data not found in response.\n";
    }
}

// Process chunked encoding and extract each chunk separately
void receiveStreamedData(int sock) {
    char buffer[BUFFER_SIZE];
    string response_header;
    bool header_complete = false;
    int chunk_count = 0;
    
    cout << "Receiving chunked data...\n";

    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            if (bytes_received < 0) {
                perror("Error receiving data");
            }
            cout << "Connection closed by server\n";
            break;
        }
        
        buffer[bytes_received] = '\0';
        string data(buffer);
        
        // If we haven't processed headers yet
        if (!header_complete) {
            response_header += data;
            size_t header_end = response_header.find("\r\n\r\n");
            
            if (header_end != string::npos) {
                header_complete = true;
                cout << "HTTP Headers:\n" << response_header.substr(0, header_end + 4) << endl;
                
                // The remaining data after headers is our first chunk(s)
                data = response_header.substr(header_end + 4);
            } else {
                continue; // Still waiting for complete headers
            }
        }
        
        // Process chunks in the data
        size_t pos = 0;
        while (pos < data.length()) {
            // Find the chunk size line
            size_t chunk_size_end = data.find("\r\n", pos);
            if (chunk_size_end == string::npos) break;
            
            // Extract chunk size in hex and convert to decimal
            string chunk_size_hex = data.substr(pos, chunk_size_end - pos);
            int chunk_size = 0;
            sscanf(chunk_size_hex.c_str(), "%x", &chunk_size);
            
            // Check if it's the final empty chunk
            if (chunk_size == 0) {
                cout << "End of chunked data detected (size 0)\n";
                return;
            }
            
            // Calculate positions for the chunk data
            size_t chunk_data_start = chunk_size_end + 2; // Skip \r\n after size
            size_t chunk_data_end = chunk_data_start + chunk_size;
            
            // Ensure we have enough data for the complete chunk
            if (chunk_data_end + 2 > data.length()) break;
            
            // Extract and display the chunk
            string chunk_data = data.substr(chunk_data_start, chunk_size);
            chunk_count++;
            cout << "\n--- Chunk " << chunk_count << " ---\n";
            cout << chunk_data << endl;
            
            // Move position to the start of the next chunk
            pos = chunk_data_end + 2; // Skip \r\n after chunk data
        }
    }
    
    cout << "\nReceived " << chunk_count << " chunks in total\n";
}

// Function to receive a simple (non-chunked) HTTP response
void receiveSingleResponse(int sock) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    string response;
    int total_bytes = 0;
    
    while (true) {
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) break;
        
        buffer[bytes_received] = '\0';
        response += buffer;
        total_bytes += bytes_received;
        
        // Check if we've received the full response
        if (response.find("\r\n\r\n") != string::npos) {
            // Find Content-Length header to determine if we're done
            size_t length_pos = response.find("Content-Length: ");
            if (length_pos != string::npos) {
                size_t length_end = response.find("\r\n", length_pos);
                string length_str = response.substr(length_pos + 16, length_end - (length_pos + 16));
                int content_length = stoi(length_str);
                
                // Calculate header length
                size_t header_end = response.find("\r\n\r\n");
                int header_length = header_end + 4;
                
                // Check if we've received the full body
                if (total_bytes >= header_length + content_length) {
                    break;
                }
            } else {
                // If no Content-Length, assume we're done
                break;
            }
        }
    }
    
    cout << "Received HTTP response (" << total_bytes << " bytes):\n" << response << endl;
}

int main() {
    int choice;
    cout << "Select request type:\n";
    cout << "1. Single student data\n";
    cout << "2. Streamed student data\n";
    cout << "Enter choice (1 or 2): ";
    cin >> choice;
    
    int sock;
    struct sockaddr_in server_address;

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket failed");
        return 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    // Convert address and connect
    if (inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr) <= 0) {
        perror("Invalid address");
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Connection failed");
        close(sock);
        return 1;
    }

    // Send the appropriate HTTP request based on choice
    string http_request;
    if (choice == 1) {
        http_request = "GET /send_student_detail HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
        cout << "Requesting single student data...\n";
    } else {
        http_request = "GET /stream_student_detail HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
        cout << "Requesting streamed student data...\n";
    }
    
    send(sock, http_request.c_str(), http_request.size(), 0);
    cout << "Request sent:\n" << http_request << endl;

    // Receive and process the response
    if (choice == 1) {
        receiveSingleResponse(sock);
    } else {
        receiveStreamedData(sock);
    }

    close(sock);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 9090    // Port number for Spdf server
#define BUFSIZE 1024 // Buffer size for communication

// Function to handle client requests
void handle_client(int client_socket)
{
    char buffer[BUFSIZE];
    int n;

    // Main loop to process client commands
    while ((n = read(client_socket, buffer, BUFSIZE)) > 0)
    {
        buffer[n] = '\0';                 // Null-terminate the received string
        printf("Received: %s\n", buffer); // Print the received command

        // In a complete implementation, the file would be stored here
        // For this example, we just acknowledge the receipt of the command
        write(client_socket, "PDF file stored in Spdf\n", 24);
    }

    if (n < 0)
    {
        perror("Read failed");
    }
}

int main(int argc, char *argv[])
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // If port number is passed as an argument, use it; otherwise, use default PORT 9090
    int port = (argc > 1) ? atoi(argv[1]) : PORT;

    // Create a socket for the Spdf server
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP address
    server_addr.sin_port = htons(port);       // Set the port number for Spdf

    // Bind the socket to the server address and port
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_socket, 3) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Spdf server listening on port %d\n", port);

    // Main loop to handle incoming connections from Smain
    while (1)
    {
        // Accept an incoming connection
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) < 0)
        {
            perror("Accept failed");
            continue;
        }

        // Fork a new process to handle the connection
        if (fork() == 0)
        {
            // Child process
            close(server_socket);         // Close the listening socket in the child process
            handle_client(client_socket); // Handle the client's request
            close(client_socket);         // Close the client socket when done
            exit(0);                      // Exit the child process
        }
        else
        {
            // Parent process
            close(client_socket); // Close the client socket in the parent process
        }
    }

    return 0;
}

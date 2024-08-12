#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080    // Port number for Smain server
#define BUFSIZE 1024 // Buffer size for communication

// ANSI escape codes for colorizing output
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"

int main()
{
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFSIZE];
    char command[BUFSIZE];
    char filename[BUFSIZE];
    char server_response[BUFSIZE];

    // Create a socket to connect to Smain server
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror(COLOR_RED "Socket creation error" COLOR_RESET);
        return -1;
    }

    // Set up the server address structure
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert the server IP address from text to binary form
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
    {
        perror(COLOR_RED "Invalid address or address not supported" COLOR_RESET);
        return -1;
    }

    // Connect to the Smain server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror(COLOR_RED "Connection failed" COLOR_RESET);
        return -1;
    }

    // Main loop to send commands to Smain server
    while (1)
    {
        printf(COLOR_CYAN "Enter command: " COLOR_RESET);
        fgets(command, BUFSIZE, stdin);
        command[strlen(command) - 1] = '\0'; // Remove newline character

        if (strlen(command) == 0)
        {
            continue; // Ignore empty command and prompt again
        }

        if (strcmp(command, "exit") == 0)
        {
            break; // Exit the loop if the user types "exit"
        }

        // Send the command to Smain server
        send(sock, command, strlen(command), 0);

        // Handle special cases like downloading files
        if (strncmp(command, "dfile ", 6) == 0)
        {
            // Extract filename from the command
            strcpy(filename, command + 6);

            // Receive the initial response from the server
            int n = read(sock, server_response, BUFSIZE);
            server_response[n] = '\0';

            // Check for error message
            if (strstr(server_response, "Error:") != NULL)
            {
                printf(COLOR_RED "%s\n" COLOR_RESET, server_response);
                continue; // Do not create the file if there is an error
            }

            // If no error, create the file with the correct filename
            FILE *file = fopen(filename, "wb");
            if (!file)
            {
                perror(COLOR_RED "File creation failed" COLOR_RESET);
                continue;
            }

            // Write the received content to the file
            fwrite(server_response, 1, n, file);

            // Continue reading the rest of the file content
            while ((n = read(sock, server_response, BUFSIZE)) > 0)
            {
                // Check for transfer complete message
                if (strstr(server_response, "Transfer complete\n") != NULL)
                {
                    printf(COLOR_GREEN "File download complete: %s\n" COLOR_RESET, filename);
                    break;
                }
                fwrite(server_response, 1, n, file);
            }
            fclose(file);
            continue;
        }

        // Receive and print the response from Smain server
        int n = read(sock, server_response, BUFSIZE);
        server_response[n] = '\0';
        printf("%s\n", server_response);
    }

    close(sock); // Close the socket when done
    return 0;
}

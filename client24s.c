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
    char destination_path[BUFSIZE];
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
        close(sock);
        return -1;
    }

    // Connect to the Smain server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror(COLOR_RED "Connection failed" COLOR_RESET);
        close(sock);
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

        if (strcmp(command, "cls") == 0 || strcmp(command, "clear") == 0)
        {
            system("clear");
        }

        if (strcmp(command, "exit") == 0)
        {
            break; // Exit the loop if the user types "exit"
        }

        // Handle the ufile command
        if (strncmp(command, "ufile ", 6) == 0)
        {
            // Extract filename and destination path from the command
            if (sscanf(command, "ufile %s %s", filename, destination_path) != 2)
            {
                fprintf(stderr, COLOR_RED "Error: Invalid command format for ufile\n" COLOR_RESET);
                continue;
            }

            // Open the file to be uploaded
            FILE *file = fopen(filename, "rb");
            if (!file)
            {
                perror(COLOR_RED "File open failed" COLOR_RESET);
                continue;
            }

            // Send the command to the server
            if (send(sock, command, strlen(command), 0) < 0)
            {
                perror(COLOR_RED "Failed to send command to server" COLOR_RESET);
                fclose(file);
                continue;
            }

            // Read file and send its content to the server
            char file_buffer[BUFSIZE];
            size_t bytes_read;
            while ((bytes_read = fread(file_buffer, 1, BUFSIZE, file)) > 0)
            {
                if (send(sock, file_buffer, bytes_read, 0) < 0)
                {
                    perror(COLOR_RED "Failed to send file data to server" COLOR_RESET);
                    break;
                }
            }

            fclose(file);

            // Receive the server's response
            int n = read(sock, server_response, BUFSIZE);
            if (n > 0)
            {
                server_response[n] = '\0';
                printf("%s\n", server_response);
            }
            else
            {
                perror(COLOR_RED "Failed to receive server response" COLOR_RESET);
            }
            continue;
        }

        // Handle the dfile command
        if (strncmp(command, "dfile ", 6) == 0)
        {
            // Extract filename from the command
            strcpy(filename, command + 6);

            // Send the command to the server
            if (send(sock, command, strlen(command), 0) < 0)
            {
                perror(COLOR_RED "Failed to send command to server" COLOR_RESET);
                continue;
            }

            // Open the file to save the downloaded content
            FILE *file = fopen(filename, "wb");
            if (!file)
            {
                perror(COLOR_RED "File creation failed" COLOR_RESET);
                continue;
            }

            int n;
            int transfer_complete = 0;

            // Receive and write the file content
            while ((n = read(sock, server_response, BUFSIZE)) > 0)
            {
                if (strstr(server_response, "Transfer complete\n") != NULL)
                {
                    // Stop when we get the "Transfer complete" message
                    transfer_complete = 1;
                    fwrite(server_response, 1, strstr(server_response, "Transfer complete\n") - server_response, file);
                    break;
                }
                fwrite(server_response, 1, n, file);
            }

            fclose(file);

            if (transfer_complete)
            {
                printf(COLOR_GREEN "File download complete: %s\n" COLOR_RESET, filename);
            }
            else
            {
                printf(COLOR_RED "File download failed or incomplete\n" COLOR_RESET);
            }
            continue;
        }

        // Handle the dtar command
        if (strncmp(command, "dtar ", 5) == 0)
        {
            // Extract filetype from the command
            char filetype[BUFSIZE];
            strcpy(filetype, command + 5);

            // Remove the leading dot from the filetype before constructing the filename
            const char *ext = filetype[0] == '.' ? filetype + 1 : filetype;

            // Generate the tar filename
            snprintf(filename, sizeof(filename), "%sFiles.tar", ext);

            // Send the command to the server
            if (send(sock, command, strlen(command), 0) < 0)
            {
                perror(COLOR_RED "Failed to send command to server" COLOR_RESET);
                continue;
            }

            // Open the file to save the downloaded tar content
            FILE *file = fopen(filename, "wb");
            if (!file)
            {
                perror(COLOR_RED "Tar file creation failed" COLOR_RESET);
                continue;
            }

            int n;
            int transfer_complete = 0;

            // Receive and write the tar file content
            while ((n = read(sock, server_response, BUFSIZE)) > 0)
            {
                if (strstr(server_response, "Transfer complete\n") != NULL)
                {
                    // Stop when we get the "Transfer complete" message
                    transfer_complete = 1;
                    fwrite(server_response, 1, strstr(server_response, "Transfer complete\n") - server_response, file);
                    break;
                }
                fwrite(server_response, 1, n, file);
            }

            fclose(file);

            if (transfer_complete)
            {
                printf(COLOR_GREEN "Tar file download complete: %s\n" COLOR_RESET, filename);
            }
            else
            {
                printf(COLOR_RED "Tar file download failed or incomplete\n" COLOR_RESET);
            }
            continue;
        }

        // Handle other commands (rmfile, display, help)
        if (send(sock, command, strlen(command), 0) < 0)
        {
            perror(COLOR_RED "Failed to send command to server" COLOR_RESET);
            continue;
        }

        // Receive and print the response from Smain server
        int n = read(sock, server_response, BUFSIZE);
        if (n > 0)
        {
            server_response[n] = '\0';
            printf("%s\n", server_response);
        }
        else
        {
            perror(COLOR_RED "Failed to receive server response" COLOR_RESET);
        }
    }

    close(sock); // Close the socket when done
    return 0;
}

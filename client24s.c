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
        fgets(buffer, BUFSIZE, stdin);
        buffer[strlen(buffer) - 1] = '\0'; // Remove newline character

        if (strcmp(buffer, "exit") == 0)
        {
            break; // Exit the loop if the user types "exit"
        }

        // Send the command to Smain server
        send(sock, buffer, strlen(buffer), 0);

        // Handle special cases like downloading files
        if (strncmp(buffer, "dfile ", 6) == 0)
        {
            char *filename = buffer + 6;
            FILE *file = fopen(filename, "wb");
            if (!file)
            {
                perror(COLOR_RED "File creation failed" COLOR_RESET);
                continue;
            }

            int n;
            while ((n = read(sock, buffer, BUFSIZE)) > 0)
            {
                fwrite(buffer, 1, n, file);
            }
            fclose(file);
            printf(COLOR_GREEN "File downloaded successfully: %s\n" COLOR_RESET, filename);
            continue;
        }

        // Handle tarball download case
        if (strncmp(buffer, "dtar ", 5) == 0)
        {
            char tarfile[] = "files.tar";
            FILE *file = fopen(tarfile, "wb");
            if (!file)
            {
                perror(COLOR_RED "Tarball creation failed" COLOR_RESET);
                continue;
            }

            int n;
            while ((n = read(sock, buffer, BUFSIZE)) > 0)
            {
                fwrite(buffer, 1, n, file);
            }
            fclose(file);
            printf(COLOR_GREEN "Tarball downloaded successfully: %s\n" COLOR_RESET, tarfile);
            continue;
        }

        // Receive and print the response from Smain server
        int n = read(sock, buffer, BUFSIZE);
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }

    close(sock); // Close the socket when done
    return 0;
}

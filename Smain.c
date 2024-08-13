#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define PORT 8080    // Port number for Smain server
#define BUFSIZE 1024 // Buffer size for communication

// ANSI escape codes for colorizing output
#define COLOR_RESET "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"

// Function to determine the base directory based on file extension
void get_base_dir(char *ext, char *base_dir)
{
    if (strcmp(ext, ".pdf") == 0)
    {
        snprintf(base_dir, BUFSIZE, "%s/Spdf", getenv("HOME"));
    }
    else if (strcmp(ext, ".txt") == 0)
    {
        snprintf(base_dir, BUFSIZE, "%s/Stext", getenv("HOME"));
    }
    else if (strcmp(ext, ".c") == 0)
    {
        snprintf(base_dir, BUFSIZE, "%s/Smain", getenv("HOME"));
    }
    else
    {
        base_dir[0] = '\0'; // Invalid file type
    }
}

// Function to check if an entry is a directory
int is_directory(const char *path)
{
    struct stat statbuf;
    if (stat(path, &statbuf) != 0)
    {
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}

// Function to search for a file in the directory tree
int search_file_in_tree(const char *base_path, const char *filename, char *found_path)
{
    DIR *dir = opendir(base_path);
    if (!dir)
    {
        perror(COLOR_RED "Error opening directory" COLOR_RESET);
        return 0;
    }

    struct dirent *entry;
    char path[BUFSIZE];
    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        if (is_directory(path))
        {
            if (search_file_in_tree(path, filename, found_path))
            {
                closedir(dir);
                return 1;
            }
        }
        else if (strcmp(entry->d_name, filename) == 0)
        {
            snprintf(found_path, BUFSIZE, "%s/%s", base_path, filename);
            closedir(dir);
            return 1;
        }
    }

    closedir(dir);
    return 0;
}

// Custom comparison function for sorting directory entries
int custom_alphasort(const struct dirent **a, const struct dirent **b)
{
    return strcasecmp((*a)->d_name, (*b)->d_name);
}

// Send Message to client
void send_message_to_client(int client_socket, char *message)
{
    write(client_socket, message, strlen(message));
}

void handle_ufile(int client_socket, char *filename, char *destination_path)
{
    char *ext = strrchr(filename, '.');
    char base_dir[BUFSIZE];
    char full_path[BUFSIZE];

    get_base_dir(ext, base_dir);
    if (base_dir[0] == '\0')
    {
        send_message_to_client(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET);
        return;
    }

    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, destination_path);
    char command[BUFSIZE];
    snprintf(command, sizeof(command), "mkdir -p %s", full_path);
    if (system(command) != 0)
    {
        send_message_to_client(client_socket, COLOR_RED "Error: Could not create directory\n" COLOR_RESET);
        return;
    }

    strncat(full_path, "/", BUFSIZE - strlen(full_path) - 1);
    strncat(full_path, filename, BUFSIZE - strlen(full_path) - 1);

    FILE *dest = fopen(full_path, "wb");
    if (!dest)
    {
        perror(COLOR_RED "File creation failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not create file %s\n" COLOR_RESET, full_path);
        send_message_to_client(client_socket, error_message);
        return;
    }

    // Read data sent by the client and write it to the file
    char buffer[BUFSIZE];
    ssize_t bytes;
    while ((bytes = read(client_socket, buffer, BUFSIZE)) > 0)
    {
        fwrite(buffer, 1, bytes, dest);
        if (bytes < BUFSIZE) // assuming client will close connection after sending file
            break;
    }

    fclose(dest);
    send_message_to_client(client_socket, COLOR_GREEN "File uploaded successfully\n" COLOR_RESET);
}

// Function to handle file download (dfile)
void handle_dfile(int client_socket, char *filename)
{
    char base_dir[BUFSIZE];
    char found_path[BUFSIZE] = {0};

    printf("[DEBUG] Starting dfile command\n");
    get_base_dir(strrchr(filename, '.'), base_dir);
    printf("[DEBUG] Base directory: %s\n", base_dir);
    if (base_dir[0] == '\0')
    {
        send_message_to_client(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET);
        return;
    }

    if (!search_file_in_tree(base_dir, filename, found_path))
    {
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: File %s does not exist\n" COLOR_RESET, filename);
        send_message_to_client(client_socket, error_message);
        return;
    }

    printf("[DEBUG] File found at path: %s\n", found_path);
    FILE *src_file = fopen(found_path, "rb");
    if (!src_file)
    {
        perror(COLOR_RED "File open failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not open file %s\n" COLOR_RESET, found_path);
        send_message_to_client(client_socket, error_message);
        return;
    }

    printf("[DEBUG] Sending file contents to client\n");
    char buffer[BUFSIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFSIZE, src_file)) > 0)
    {
        if (write(client_socket, buffer, bytes) < 0)
        {
            perror(COLOR_RED "Error sending file data" COLOR_RESET);
            break;
        }
    }
    fclose(src_file);

    // Send a confirmation message to indicate the end of transmission
    printf("[DEBUG] Sending transfer complete message\n");
    send_message_to_client(client_socket, "Transfer complete\n");
    printf("[DEBUG] dfile command completed\n");
}

// Function to handle file removal (rmfile)
void handle_rmfile(int client_socket, char *filename)
{
    char base_dir[BUFSIZE];
    char found_path[BUFSIZE] = {0};

    printf("[DEBUG] Starting rmfile command\n");
    get_base_dir(strrchr(filename, '.'), base_dir);
    printf("[DEBUG] Base directory: %s\n", base_dir);
    if (base_dir[0] == '\0')
    {
        send_message_to_client(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET);
        return;
    }

    if (!search_file_in_tree(base_dir, filename, found_path))
    {
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: File %s not found\n" COLOR_RESET, filename);
        send_message_to_client(client_socket, error_message);
        return;
    }

    printf("[DEBUG] File found at path: %s\n", found_path);
    if (remove(found_path) == 0)
    {
        send_message_to_client(client_socket, COLOR_GREEN "File removed successfully\n" COLOR_RESET);
    }
    else
    {
        perror(COLOR_RED "File removal failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not remove file %s\n" COLOR_RESET, found_path);
        send_message_to_client(client_socket, error_message);
    }
    printf("[DEBUG] Finished rmfile command\n");
}

// Function to create a tar file based on the file type
void handle_dtar(int client_socket, const char *filetype)
{
    char command[BUFSIZE];
    char tar_filename[BUFSIZE];
    char buffer[BUFSIZE];
    ssize_t bytes;
    char base_dir[BUFSIZE];

    get_base_dir((char *)filetype, base_dir);

    if (base_dir[0] == '\0')
    {
        printf(COLOR_RED "Unsupported file type\n" COLOR_RESET);
        send_message_to_client(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET);
        return;
    }

    // Remove the leading dot from the filetype
    const char *ext = filetype[0] == '.' ? filetype + 1 : filetype;

    // Create tarball based on file extension without the leading dot
    snprintf(tar_filename, sizeof(tar_filename), "%sFiles.tar", ext);
    snprintf(command, sizeof(command), "tar -cvf %s $(find %s -name '*.%s')", tar_filename, base_dir, ext);

    // Execute the tar command
    if (system(command) != 0)
    {
        printf(COLOR_RED "Error: Tarball creation failed\n" COLOR_RESET);
        send_message_to_client(client_socket, COLOR_RED "Error: Tarball creation failed\n" COLOR_RESET);
        return;
    }

    // Open the tar file to send its contents to the client
    FILE *tar_file = fopen(tar_filename, "rb");
    if (!tar_file)
    {
        perror(COLOR_RED "Failed to open tar file" COLOR_RESET);
        send_message_to_client(client_socket, COLOR_RED "Error: Failed to open tar file\n" COLOR_RESET);
        return;
    }

    // Send the tar file to the client
    while ((bytes = fread(buffer, 1, BUFSIZE, tar_file)) > 0)
    {
        if (write(client_socket, buffer, bytes) != bytes)
        {
            perror(COLOR_RED "Failed to send tar file contents" COLOR_RESET);
            break;
        }
    }

    fclose(tar_file);

    // Send a message indicating transfer completion
    send_message_to_client(client_socket, "Transfer complete\n");

    // Delete the tar file from the server
    if (remove(tar_filename) != 0)
    {
        perror(COLOR_RED "Failed to delete tar file" COLOR_RESET);
    }
}

// Function to handle directory display (display)
void handle_display(int client_socket, char *pathname)
{
    char base_dir[BUFSIZE];
    struct dirent **namelist;
    int n;
    int path_found = 0;

    // Buffers to accumulate files from all directories
    char c_files[BUFSIZE * 10] = "";
    char pdf_files[BUFSIZE * 10] = "";
    char txt_files[BUFSIZE * 10] = "";

    // Array of directories to check
    const char *dirs_to_check[] = {"Smain", "Spdf", "Stext"};

    for (int i = 0; i < 3; i++)
    {
        snprintf(base_dir, sizeof(base_dir), "%s/%s/%s", getenv("HOME"), dirs_to_check[i], pathname);
        n = scandir(base_dir, &namelist, NULL, custom_alphasort);
        if (n >= 0)
        {
            path_found = 1;

            // Accumulate files into respective buffers
            for (int j = 0; j < n; j++)
            {
                if (strstr(namelist[j]->d_name, ".c"))
                {
                    strncat(c_files, namelist[j]->d_name, sizeof(c_files) - strlen(c_files) - 1);
                    strncat(c_files, "\n", sizeof(c_files) - strlen(c_files) - 1);
                }
                else if (strstr(namelist[j]->d_name, ".pdf"))
                {
                    strncat(pdf_files, namelist[j]->d_name, sizeof(pdf_files) - strlen(pdf_files) - 1);
                    strncat(pdf_files, "\n", sizeof(pdf_files) - strlen(pdf_files) - 1);
                }
                else if (strstr(namelist[j]->d_name, ".txt"))
                {
                    strncat(txt_files, namelist[j]->d_name, sizeof(txt_files) - strlen(txt_files) - 1);
                    strncat(txt_files, "\n", sizeof(txt_files) - strlen(txt_files) - 1);
                }
                free(namelist[j]);
            }

            free(namelist);
        }
    }

    if (path_found)
    {
        // Send accumulated .c files
        if (strlen(c_files) > 0)
        {
            char buffer[BUFSIZE];
            send_message_to_client(client_socket, buffer);
            send_message_to_client(client_socket, c_files);
        }

        // Send accumulated .pdf files
        if (strlen(pdf_files) > 0)
        {
            char buffer[BUFSIZE];
            send_message_to_client(client_socket, buffer);
            send_message_to_client(client_socket, pdf_files);
        }

        // Send accumulated .txt files
        if (strlen(txt_files) > 0)
        {
            char buffer[BUFSIZE];
            send_message_to_client(client_socket, buffer);
            send_message_to_client(client_socket, txt_files);
        }
    }
    else
    {
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "No such path exists\n" COLOR_RESET);
        send_message_to_client(client_socket, error_message);
    }

    printf("[DEBUG] Finished display command\n");
}

// Function to send help/usage information to the client
void send_help(int client_socket)
{
    const char *help_message =
        COLOR_CYAN "Usage:\n" COLOR_RESET COLOR_CYAN "ufile <filename> <destination_path> : Upload a file to the server.\n" COLOR_RESET COLOR_CYAN "dfile <filename>                    : Download a file from the server.\n" COLOR_RESET COLOR_CYAN "rmfile <filename>                   : Remove a file from the server.\n" COLOR_RESET COLOR_CYAN "dtar <filetype>                     : Create and download a tarball of files of a specific type.\n" COLOR_RESET COLOR_CYAN "display <pathname>                  : Display the list of files in a directory.\n" COLOR_RESET COLOR_CYAN "help                                : Display this help message.\n" COLOR_RESET COLOR_CYAN "exit                                : Exit the communication with server\n" COLOR_RESET;

    send_message_to_client(client_socket, help_message);
}

// Function to handle client requests
void prcclient(int client_socket)
{
    char buffer[BUFSIZE];
    int n;

    while ((n = read(client_socket, buffer, BUFSIZE)) > 0)
    {
        buffer[n] = '\0';

        char *command = strtok(buffer, " ");
        if (strcmp(command, "ufile") == 0)
        {
            char *filename = strtok(NULL, " ");
            char *destination_path = strtok(NULL, " ");
            if (filename && destination_path)
            {
                handle_ufile(client_socket, filename, destination_path);
            }
            else
            {
                send_message_to_client(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET " Use " COLOR_GREEN "help" COLOR_RESET " to get help");
            }
        }
        else if (strcmp(command, "dfile") == 0)
        {
            char *filename = strtok(NULL, " ");
            if (filename)
            {
                handle_dfile(client_socket, filename);
            }
            else
            {
                send_message_to_client(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET " Use " COLOR_GREEN "help" COLOR_RESET " to get help");
            }
        }
        else if (strcmp(command, "rmfile") == 0)
        {
            char *filename = strtok(NULL, " ");
            if (filename)
            {
                handle_rmfile(client_socket, filename);
            }
            else
            {
                send_message_to_client(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET " Use " COLOR_GREEN "help" COLOR_RESET " to get help");
            }
        }
        else if (strcmp(command, "dtar") == 0)
        {
            char *filetype = strtok(NULL, " ");
            if (filetype)
            {
                handle_dtar(client_socket, filetype);
            }
            else
            {
                send_message_to_client(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET " Use " COLOR_GREEN "help" COLOR_RESET " to get help");
            }
        }
        else if (strcmp(command, "display") == 0)
        {
            char *pathname = strtok(NULL, " ");
            if (pathname)
            {
                handle_display(client_socket, pathname);
            }
            else
            {
                send_message_to_client(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET " Use " COLOR_GREEN "help" COLOR_RESET " to get help");
            }
        }
        else if (strcmp(command, "help") == 0)
        {
            send_help(client_socket);
        }
        else
        {
            send_message_to_client(client_socket, COLOR_RED "Error: Unknown command\n" COLOR_RESET "Use " COLOR_GREEN "help" COLOR_RESET " to get help");
        }
    }

    if (n < 0)
    {
        perror(COLOR_RED "Read failed" COLOR_RESET);
        send_message_to_client(client_socket, COLOR_RED "Error: Failed to read client request\n" COLOR_RESET);
    }
    close(client_socket); // Ensure the socket is closed after handling the request
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create the Smain socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        perror(COLOR_RED "Socket failed" COLOR_RESET);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror(COLOR_RED "Bind failed" COLOR_RESET);
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) < 0)
    {
        perror(COLOR_RED "Listen failed" COLOR_RESET);
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf(COLOR_GREEN "Smain server listening on port %d\n" COLOR_RESET, PORT);

    while (1)
    {
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) < 0)
        {
            perror(COLOR_RED "Accept failed" COLOR_RESET);
            continue;
        }

        if (fork() == 0)
        {
            close(server_socket);
            prcclient(client_socket);
            close(client_socket);
            exit(0);
        }
        else
        {
            close(client_socket);
        }

        waitpid(-1, NULL, WNOHANG);
    }

    return 0;
}

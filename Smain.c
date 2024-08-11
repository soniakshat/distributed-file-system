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

// Function to start a sub-server
void start_sub_server(const char *server_name, const char *port)
{
    if (access(server_name, X_OK) != 0)
    {
        fprintf(stderr, COLOR_RED "Error: Sub-server executable %s not found or not executable\n" COLOR_RESET, server_name);
        return;
    }

    pid_t pid = fork();
    if (pid == 0)
    {
        execlp(server_name, server_name, port, NULL);
        perror(COLOR_RED "execlp failed" COLOR_RESET);
        exit(EXIT_FAILURE);
    }
    else if (pid > 0)
    {
        printf(COLOR_GREEN "Started sub-server %s on port %s with PID %d\n" COLOR_RESET, server_name, port, pid);
    }
    else
    {
        perror(COLOR_RED "fork failed" COLOR_RESET);
        exit(EXIT_FAILURE);
    }
}

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

// Function to handle file upload (ufile)
void handle_ufile(int client_socket, char *filename, char *destination_path)
{
    char *ext = strrchr(filename, '.');
    char base_dir[BUFSIZE];
    char full_path[BUFSIZE];

    get_base_dir(ext, base_dir);
    if (base_dir[0] == '\0')
    {
        write(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET, strlen(COLOR_RED "Error: Unsupported file type\n" COLOR_RESET));
        return;
    }

    snprintf(full_path, sizeof(full_path), "%s/%s", base_dir, destination_path);
    char command[BUFSIZE];
    snprintf(command, sizeof(command), "mkdir -p %s", full_path);
    system(command);

    strncat(full_path, "/", BUFSIZE - strlen(full_path) - 1);
    strncat(full_path, filename, BUFSIZE - strlen(full_path) - 1);

    FILE *src = fopen(filename, "rb");
    if (!src)
    {
        perror(COLOR_RED "File open failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not open file %s\n" COLOR_RESET, filename);
        write(client_socket, error_message, strlen(error_message));
        return;
    }

    FILE *dest = fopen(full_path, "wb");
    if (!dest)
    {
        perror(COLOR_RED "File save failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not save file to %s\n" COLOR_RESET, full_path);
        write(client_socket, error_message, strlen(error_message));
        fclose(src);
        return;
    }

    char buffer[BUFSIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFSIZE, src)) > 0)
    {
        fwrite(buffer, 1, bytes, dest);
    }
    fclose(src);
    fclose(dest);
    write(client_socket, COLOR_GREEN "File stored successfully\n" COLOR_RESET, strlen(COLOR_GREEN "File stored successfully\n" COLOR_RESET));
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
        write(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET, strlen(COLOR_RED "Error: Unsupported file type\n" COLOR_RESET));
        return;
    }

    if (!search_file_in_tree(base_dir, filename, found_path))
    {
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: File %s does not exist\n" COLOR_RESET, filename);
        write(client_socket, error_message, strlen(error_message));
        return;
    }

    printf("[DEBUG] File found at path: %s\n", found_path);
    FILE *src_file = fopen(found_path, "rb");
    if (!src_file)
    {
        perror(COLOR_RED "File open failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not open file %s\n" COLOR_RESET, found_path);
        write(client_socket, error_message, strlen(error_message));
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
    write(client_socket, "Transfer complete\n", 18);

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
        write(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET, strlen(COLOR_RED "Error: Unsupported file type\n" COLOR_RESET));
        return;
    }

    if (!search_file_in_tree(base_dir, filename, found_path))
    {
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: File %s not found\n" COLOR_RESET, filename);
        write(client_socket, error_message, strlen(error_message));
        return;
    }

    printf("[DEBUG] File found at path: %s\n", found_path);
    if (remove(found_path) == 0)
    {
        write(client_socket, COLOR_GREEN "File removed successfully\n" COLOR_RESET, strlen(COLOR_GREEN "File removed successfully\n" COLOR_RESET));
    }
    else
    {
        perror(COLOR_RED "File removal failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not remove file %s\n" COLOR_RESET, found_path);
        write(client_socket, error_message, strlen(error_message));
    }
    printf("[DEBUG] Finished rmfile command\n");
}

// Function to handle tar creation and download (dtar)
void handle_dtar(int client_socket, char *filetype)
{
    char base_dir[BUFSIZE];
    get_base_dir(filetype, base_dir);
    if (base_dir[0] == '\0')
    {
        write(client_socket, COLOR_RED "Error: Unsupported file type\n" COLOR_RESET, strlen(COLOR_RED "Error: Unsupported file type\n" COLOR_RESET));
        return;
    }

    char command[BUFSIZE];
    snprintf(command, sizeof(command), "find %s -name '*.%s' -print0 | xargs -0 tar -cvf files.tar", base_dir, filetype);

    printf("[DEBUG] Running tar command: %s\n", command);
    if (system(command) != 0)
    {
        write(client_socket, COLOR_RED "Error: Tarball creation failed\n" COLOR_RESET, strlen(COLOR_RED "Error: Tarball creation failed\n" COLOR_RESET));
        return;
    }

    printf("[DEBUG] Tarball created successfully\n");
    FILE *tarfile = fopen("files.tar", "rb");
    if (!tarfile)
    {
        perror(COLOR_RED "Tarball open failed" COLOR_RESET);
        char error_message[BUFSIZE];
        snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not open tarball\n" COLOR_RESET);
        write(client_socket, error_message, strlen(error_message));
        return;
    }

    printf("[DEBUG] Sending tarball to client\n");
    char buffer[BUFSIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFSIZE, tarfile)) > 0)
    {
        write(client_socket, buffer, bytes);
    }
    fclose(tarfile);
    printf("[DEBUG] Finished sending tarball\n");
}

// Function to handle directory display (display)
void handle_display(int client_socket, char *pathname)
{
    char base_dir[BUFSIZE];
    char display_path[BUFSIZE];
    struct dirent **namelist;
    int n;

    printf("[DEBUG] Starting display command\n");
    snprintf(base_dir, sizeof(base_dir), "%s/Smain/%s", getenv("HOME"), pathname);
    n = scandir(base_dir, &namelist, NULL, custom_alphasort);
    if (n < 0)
    {
        snprintf(base_dir, sizeof(base_dir), "%s/Spdf/%s", getenv("HOME"), pathname);
        n = scandir(base_dir, &namelist, NULL, custom_alphasort);
        if (n < 0)
        {
            snprintf(base_dir, sizeof(base_dir), "%s/Stext/%s", getenv("HOME"), pathname);
            n = scandir(base_dir, &namelist, NULL, custom_alphasort);
            if (n < 0)
            {
                char error_message[BUFSIZE];
                snprintf(error_message, BUFSIZE, COLOR_RED "Error: Could not open directory %s\n" COLOR_RESET, pathname);
                write(client_socket, error_message, strlen(error_message));
                return;
            }
        }
    }

    printf("[DEBUG] Listing directory contents\n");
    // Buffer to store directory content
    char buffer[BUFSIZE];
    snprintf(buffer, sizeof(buffer), COLOR_CYAN "Listing contents of: %s\n" COLOR_RESET, pathname);
    write(client_socket, buffer, strlen(buffer));

    // Listing .c files
    for (int i = 0; i < n; i++)
    {
        if (strstr(namelist[i]->d_name, ".c"))
        {
            snprintf(buffer, sizeof(buffer), COLOR_GREEN "%s\n" COLOR_RESET, namelist[i]->d_name);
            write(client_socket, buffer, strlen(buffer));
        }
        free(namelist[i]);
    }

    // Listing .pdf files
    for (int i = 0; i < n; i++)
    {
        if (strstr(namelist[i]->d_name, ".pdf"))
        {
            snprintf(buffer, sizeof(buffer), COLOR_YELLOW "%s\n" COLOR_RESET, namelist[i]->d_name);
            write(client_socket, buffer, strlen(buffer));
        }
    }

    // Listing .txt files
    for (int i = 0; i < n; i++)
    {
        if (strstr(namelist[i]->d_name, ".txt"))
        {
            snprintf(buffer, sizeof(buffer), COLOR_CYAN "%s\n" COLOR_RESET, namelist[i]->d_name);
            write(client_socket, buffer, strlen(buffer));
        }
    }

    free(namelist);
    printf("[DEBUG] Finished display command\n");
}

// Function to send help/usage information to the client
void send_help(int client_socket)
{
    const char *help_message =
        COLOR_CYAN "Usage:\n"
                   "ufile <filename> <destination_path> : Upload a file to the server.\n"
                   "dfile <filename>                    : Download a file from the server.\n"
                   "rmfile <filename>                   : Remove a file from the server.\n"
                   "dtar <filetype>                     : Create and download a tarball of files of a specific type.\n"
                   "display <pathname>                  : Display the list of files in a directory.\n"
                   "help                                : Display this help message.\n" COLOR_RESET;

    write(client_socket, help_message, strlen(help_message));
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
                write(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET, strlen(COLOR_RED "Error: Invalid command format\n" COLOR_RESET));
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
                write(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET, strlen(COLOR_RED "Error: Invalid command format\n" COLOR_RESET));
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
                write(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET, strlen(COLOR_RED "Error: Invalid command format\n" COLOR_RESET));
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
                write(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET, strlen(COLOR_RED "Error: Invalid command format\n" COLOR_RESET));
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
                write(client_socket, COLOR_RED "Error: Invalid command format\n" COLOR_RESET, strlen(COLOR_RED "Error: Invalid command format\n" COLOR_RESET));
            }
        }
        else if (strcmp(command, "help") == 0)
        {
            send_help(client_socket);
        }
        else
        {
            write(client_socket, COLOR_RED "Error: Unknown command\n" COLOR_RESET, strlen(COLOR_RED "Error: Unknown command\n" COLOR_RESET));
        }
    }

    if (n < 0)
    {
        perror(COLOR_RED "Read failed" COLOR_RESET);
    }
}

int main()
{
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Start the sub-servers
    start_sub_server("./Spdf", "9090");
    start_sub_server("./Stext", "9091");

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

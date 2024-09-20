#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define TRUE (1 == 1)
#define FALSE !TRUE

#define STR_BUFF_SIZE 512

#define WORKING_DIR "/tmp/"

#define PORT 6969
#define BACKLOG_SIZE 10

#define HTML_TYPE 0
#define JPEG_TYPE 1

char pages_dir_path[STR_BUFF_SIZE];
char home_page_path[STR_BUFF_SIZE];

int listener;

void read_cfg(const char *cfg_path)
{
    char pages_dir_line[STR_BUFF_SIZE];
    char home_page_line[STR_BUFF_SIZE];
    FILE *cfg = fopen(cfg_path, "r");

    if (!cfg)
    {
        syslog(LOG_ERR, "Error opening config");
        exit(EXIT_FAILURE);
    }

    if (fscanf(cfg, "%s\n%s", pages_dir_line, home_page_line) != 2)
    {
        syslog(LOG_ERR, "Incorrect config content");
        fclose(cfg);
        exit(EXIT_FAILURE);
    }

    DIR *dir = opendir(pages_dir_line);
    if (!dir)
    {
        syslog(LOG_ERR, "Error opening pages directory");
        fclose(cfg);
        exit(EXIT_FAILURE);
    }

    closedir(dir);
    strcpy(pages_dir_path, pages_dir_line);

    FILE *home_page = fopen(home_page_line, "r");
    if (!home_page)
    {
        syslog(LOG_ERR, "Error opening home page file");
        fclose(cfg);
        exit(EXIT_FAILURE);
    }

    fclose(home_page);
    strcpy(home_page_path, home_page_line);

    fclose(cfg);
}

void signal_handler(int signal)
{
    switch (signal)
    {
    case SIGTERM:
        syslog(LOG_INFO, "SIGTERM accepted");
        close(listener);
        exit(EXIT_SUCCESS);
        break;
    }
}

void daemonize()
{
    pid_t pid = fork();

    if (pid < 0)
    {
        syslog(LOG_ERR, "Fork failed");
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    pid_t sid = setsid();

    if (sid < 0)
    {
        syslog(LOG_ERR, "SID set failed");
        exit(EXIT_FAILURE);
    }

    umask(0);

    if (chdir(WORKING_DIR) < 0)
    {
        syslog(LOG_ERR, "Working dir change failed");
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    signal(SIGTERM, signal_handler);

    syslog(LOG_INFO, "Daemonized successfully");
}

void init_listener_socket()
{
    struct sockaddr_in addr;

    // creating a new socket (will return a descriptor that can be accessed)
    // domain - internet (to work in any IP network
    // type - stream transfer with pre-connection
    // protocol - by default
    listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener < 0)
    {
        syslog(LOG_ERR, "Failed to create listener socket");
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // bind socket with IP port number
    // first parameter - returned socket descriptor 
    // second parameter - pointer to the structure with the address
    // third parameter - length of the structure
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        syslog(LOG_ERR, "Failed to bind listener");
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "Web-server started successfully");
}

void send_file(int client_socket, const char *file_path, int type)
{
    char buffer[STR_BUFF_SIZE];
    FILE *file = fopen(file_path, "r");
    if (!file)
    {
        syslog(LOG_ERR, "Error finding page %s", file_path);

        const char *header = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n";
        send(client_socket, header, strlen(header), 0);

        const char *not_found_response =
            "<html><body>\
            <h1>404 Not Found</h1>\
            <p>The requested page could not be found.</p>\
            </body></html>";
        send(client_socket, not_found_response, strlen(not_found_response), 0);

        return;
    }

    const char *header = NULL;
    switch (type)
    {
    case HTML_TYPE:
        header = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
        break;
    case JPEG_TYPE:
        header = "HTTP/1.1 200 OK\nContent-Type: image/jpeg\n\n";
        break;
    default:
        break;
    }
    send(client_socket, header, strlen(header), 0);

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        send(client_socket, buffer, bytes_read, 0);
    }
    fclose(file);
}

void handle_client(int client_socket)
{
    char buffer[STR_BUFF_SIZE];
    memset(buffer, 0, sizeof(buffer));
    
    // get buffer from client socket
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received < 0)
    {
        perror("Error reading from socket");
        close(client_socket);
        return;
    }

    char *url = strtok(buffer, " ");

    if (!url || strcmp(url, "GET") != 0)
    {
        syslog(LOG_ERR, "Invalid or unsupported request with URL: %s", url);

        const char *bad_request_response =
            "HTTP/1.1 400 Bad Request\n\
            Content-Type: text/plain\n\
            Content-Length: 11\n\n\
            Bad Request";
        send(client_socket, bad_request_response, strlen(bad_request_response), 0);

        close(client_socket);
        return;
    }

    url = strtok(NULL, " ");
    if (!url)
    {
        syslog(LOG_ERR, "Error parsing URL: %s", url);

        close(client_socket);
        return;
    }

    // process the home (start) page
    if (strcmp(url, "/") == 0)
    {
        send_file(client_socket, home_page_path, HTML_TYPE);
        return;
    }

    char file_path[STR_BUFF_SIZE];
    snprintf(file_path, sizeof(file_path), "%s%s", pages_dir_path, url);

    int type = HTML_TYPE;

    char *dot = strrchr(url, '.');
    if (dot && !strcmp(dot, ".jpeg")) 
    {
        type = JPEG_TYPE;
    }

    send_file(client_socket, file_path, type);

    close(client_socket);
}

void listen_for_client()
{
    // creating a new socket with the same properties (but no longer in the waiting state) 
    // listener - listening socket (listens and accepts other connections)
    // since we are not interested in client addresses - NULL
    int sock = accept(listener, NULL, NULL); // blocked until the client establishes a connection

    if (sock < 0)
    {
        syslog(LOG_ERR, "Failed to accept client");
        return;
    }

    switch (fork())
    {
    case -1:
        syslog(LOG_ERR, "Failed to fork new client");
        break;
    case 0:
        close(listener);
        handle_client(sock);
        close(sock);
        _exit(0);
    default:
        close(sock);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <path to config>\n", argv[0]);
        return EXIT_SUCCESS;
    }

    openlog(argv[0], LOG_PID, LOG_USER);

    read_cfg(argv[1]);

    daemonize();
    
    // create a socket and bind it to the address
    init_listener_socket();
    
    // create a queue of connection requests and listen to them
    // new client? in the queue (the server may be busy)
    // queue is full - all subsequent requests are ignored
    listen(listener, BACKLOG_SIZE);

    while (1)
    {
        // service the queue of requests 
        listen_for_client();
    }
}

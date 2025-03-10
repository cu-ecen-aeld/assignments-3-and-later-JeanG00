#include <sys/types.h>          
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>


#define SOCKET_DOMAIN       AF_INET // ipv4
// #define SOCKET_DOMAIN    AF_INET6 // ipv6
// #define SOCKET_DOMAIN    AF_UNSPEC // don't care IPv4 or IPv6

#define SOCKET_TYPE         SOCK_STREAM // TCP
// #define SOCKET_TYPE      SOCK_DGRAM // UDP

#define PORT                "9000"
#define BACKLOG             10          // how many pending connections queue will hold
#define MAXCONN             5
#define HTTP_PROTOCOL_11    "HTTP/1.1"
#define HEADER_SEPERATOR    ": "
#define CRLF                "\r\n"
#define MAX_PACKET_SIZE     32768
#define BUF_SIZE            4096
#define LOG_FILE            "/var/tmp/aesdsocketdata"
#define PID_FILE            "/var/run/aesdsocket.pid"

#define handle_error(msg) \
           do { perror(msg); exit(-1); } while (0)

int sfd, lfd; // server_fd, log_fd

typedef enum {
    ROOT_ROUTE = 0,
    NOT_FOUND_ROUTE,
    ROUTES_N,
} routes_t;
typedef enum {
    OK = 0,
    NOT_FOUND,
    HTTP_STATUS_N,
} http_status_t;
typedef enum {
    CONTENT_TYPE = 0,
    CONTENT_LENGTH,
    USER_AGENT,
    HEADERS_N,
} headers_t;
const char *HEADER_NAMES[HEADERS_N] = {
    [CONTENT_TYPE] = "Content-Type",
    [CONTENT_LENGTH] = "Content-Length",
    [USER_AGENT] = "User-Agent",
};
const char *HTTP_STATUS[HTTP_STATUS_N] = {
    [OK] = "200 OK",
    [NOT_FOUND] = "404 Not Found",
};
void signal_handler(int sig, siginfo_t *si, void *unused) {
    syslog(LOG_INFO, "Caught signal %d at address %p, exiting..", sig, si->si_addr);
    // Close and remove the PID file
    close(lfd);
    close(sfd);
    unlink(LOG_FILE);
    unlink(PID_FILE);

    closelog();
    exit(EXIT_SUCCESS);
}
// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
char *build_header(headers_t header, const char *value) {
  const char *header_name = HEADER_NAMES[header];
  size_t header_str_len = strlen(header_name) + strlen(HEADER_SEPERATOR) +
                          strlen(value) + strlen(CRLF) + 1;
  char *buf = malloc(header_str_len);
  if (buf == NULL) {
    perror("malloc");
    return NULL;
  }
  strcpy(buf, header_name);
  strcat(buf, HEADER_SEPERATOR);
  strcat(buf, value);
  strcat(buf, CRLF);
  return buf;
}
char *build_response(char **headers, size_t headers_len, http_status_t status,
                     char *body) {
  const char *http_status_full = HTTP_STATUS[status];
  size_t response_len =
      strlen(HTTP_PROTOCOL_11) + 1 + strlen(http_status_full) + strlen(CRLF);
  int k = 0;
  for (k = 0; k < headers_len; ++k) {
    response_len += strlen(headers[k]);
  }
  response_len += strlen(CRLF);
  response_len += strlen(body);
  response_len += 1;
  char *buf = malloc(response_len);
  if (buf == NULL) {
    perror("malloc");
    return NULL;
  }
  strcpy(buf, HTTP_PROTOCOL_11);
  strcat(buf, " ");
  strcat(buf, http_status_full);
  strcat(buf, CRLF);
  for (k = 0; k < headers_len; ++k) {
    strcat(buf, headers[k]);
  }
  strcat(buf, CRLF);
  strcat(buf, body);
  strcat(buf, CRLF);
  return buf;
}
char *build_ok_response_with_body(char *body) {
  char str_length[BUF_SIZE];
  snprintf(str_length, sizeof(str_length), "%lu", strlen(body));
  char *header_content_type = build_header(CONTENT_TYPE, "text/plain");
  char *header_content_length = build_header(CONTENT_LENGTH, str_length);
  char *headers[] = {
      header_content_type,
      header_content_length,
  };
  char *ok_response = build_response(headers, 2, OK, body);
  free(header_content_type);
  free(header_content_length);
  // free(headers);
  return ok_response;
}
char *build_start_line(http_status_t status) {
    const char *http_status_full = HTTP_STATUS[status];
    size_t response_len = strlen(HTTP_PROTOCOL_11) + 1 +
                          strlen(http_status_full) + strlen(CRLF) * 2;
    char *buf = malloc(response_len);
    if (buf == NULL) {
      perror("malloc");
      return NULL;
    }
    strcpy(buf, HTTP_PROTOCOL_11);
    strcat(buf, " ");
    strcat(buf, http_status_full);
    strcat(buf, CRLF);
    strcat(buf, CRLF);
    return buf;
  }
void handle_not_found(int socket_fd, char *path) {
    char *response = build_ok_response_with_body(path);
    send(socket_fd, response, strlen(response), 0);
    free(response);
}
void handle_root_route(int socket_fd) {
    char *response = build_start_line(OK);
    send(socket_fd, response, strlen(response), 0);
    free(response);
  }
routes_t parse_route(char *path) {
    routes_t route = NOT_FOUND_ROUTE;
    if (strlen(path) == 1 && path[0] == '/') {
      return ROOT_ROUTE;
    }
    // if (strncmp(path, "/files/", 7) == 0) {
    //   return FILE_ROUTE;
    // }
    char *path_copy = strdup(path);
    if (path_copy == NULL) {
      perror("strdup");
      return NOT_FOUND_ROUTE;
    }
    free(path_copy);
    return route;
}

void handle_connection(int client_fd) {
    char remoteIP[INET_ADDRSTRLEN];
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    fflush(stdout);

    if (getpeername(client_fd, (struct sockaddr *)&peer_addr, &peer_len) == 0) {
        inet_ntop(peer_addr.sin_family, get_in_addr((struct sockaddr *)&peer_addr),  remoteIP, sizeof remoteIP);
        syslog(LOG_INFO, "Accepted connection from %s", remoteIP);
        printf("Accepted connection from %s\n", remoteIP);
    } else {
        syslog(LOG_ERR, "Failed to get client IP address");
    }
    
    char buf[MAX_PACKET_SIZE];
    ssize_t bytes_received;
    while ((bytes_received = recv(client_fd, buf, sizeof(buf), 0)) > 0) {
        if (write(lfd, buf, bytes_received) == -1) {
            syslog(LOG_ERR, "Failed to write data to %s: %s", LOG_FILE, strerror(errno));
            break;
        }
        if (strncmp(buf, "GET", 3) == 0 || strncmp(buf, "POST", 4) == 0) {
            // TODO: if method not in {GET, POST, DELETE, PUT} => not http request
            char method[128];
            char path[1024];
            char http_version[128];
            char client_ip[128];
            char ua[128];
            sscanf(buf, "%s %s %s\r\nHost: %s\r\nUser-Agent: %s", method, path, http_version, client_ip, ua);
            printf("[%s]: %s %s %s %s\n\n", remoteIP, method, path, http_version, ua);
            routes_t route = parse_route(path);
            switch (route) {
                case ROOT_ROUTE:
                    handle_root_route(client_fd);
                    break;
                default:
                    handle_not_found(client_fd, path);
                    break;
            }
        } else {
            // read all contents in logs file and echo back
            lseek(lfd, 0, SEEK_SET);
            while(1) {
                int read_bytes = read(lfd, buf, sizeof(buf));
                if (read_bytes < 0) {
                    syslog(LOG_ERR, "Read log file failed %s", strerror(errno));
                    exit(-1);
                } else  if (read_bytes == 0) {
                    // EOF
                    break;
                } else {
                    if (send(client_fd, buf, read_bytes, 0) == -1) {
                        syslog(LOG_ERR, "Failed to send data to the client: %s", strerror(errno));
                    }
                }
            }
        }
    }
    syslog(LOG_INFO, "Closed connection from %s", remoteIP);
    printf("Closed connection from %s\n\n", remoteIP);
    close(client_fd);
}
void write_pid_file () {
    // Open the PID file for writing and obtain an exclusive lock
    int pid_fd = open(PID_FILE, O_CREAT | O_RDWR, 0644);
    if (pid_fd == -1) {
        syslog(LOG_ERR, "Failed to open the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // apply or remove an advisory lock on an open file
    if (flock(pid_fd, LOCK_EX | LOCK_NB) == -1) {
        syslog(LOG_ERR, "Failed to lock the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // Write the current process ID to the PID file
    char pid_str[16];
    snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    if (write(pid_fd, pid_str, strlen(pid_str)) == -1) {
        syslog(LOG_ERR, "Failed to write to the PID file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
int main (int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        // check if daemon is running
        int pid_fd = open(PID_FILE, O_RDONLY);
        if (pid_fd != -1) {
            char pid_str[16];
            ssize_t n = read(pid_fd, pid_str, sizeof(pid_str));
            if (n > 0) {
                pid_t pid = atoi(pid_str);
                if (kill(pid, 0) == 0) {
                    syslog(LOG_ERR, "Daemon is running with PID %d.", pid);
                    close(pid_fd);
                    exit(EXIT_FAILURE);
                }
            }
            close(pid_fd);
        }
        // fork
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            return EXIT_FAILURE;
        } else if (pid > 0) {
            // Parent process exit
            return EXIT_SUCCESS;
        }

        setsid();       // run a program in a new session
        umask(0);       // set file mode creation mask
        chdir("/");

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // Write the PID to the PID file
        write_pid_file();
    }

    openlog("aesdsocket", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "AESDSOCKET engine start");

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = signal_handler;
    if (sigaction(SIGINT, &sa, NULL) == -1)
        handle_error("sigaction SIGINT");
    if (sigaction(SIGTERM, &sa, NULL) == -1)
        handle_error("sigaction SIGTERM");

    int status;
    struct addrinfo hints, *servinfo, *p; // servinfo will point to the results

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = SOCKET_DOMAIN; 
    hints.ai_socktype = SOCKET_TYPE;
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    // getaddrinfo() returns a list of address structures
    if ((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        syslog(LOG_ERR, "Failed to allocate socket %s\n", gai_strerror(status));
        return EXIT_FAILURE;
    }
    // servinfo now points to a linked list of 1 or more struct addrinfos
    // loop through all the results and make a socket
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        break;
    }
    if (p == NULL) {
        syslog(LOG_ERR, "Failed to create socket after looping\n");
        return EXIT_FAILURE;
    }

    // the kernel sets sfd to blocking by default, to unblock:
    // fcntl(sfd, F_SETFL, O_NONBLOCK);

    if (bind(sfd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Failed to bind to port %s: %s", PORT, strerror(errno));
        return EXIT_FAILURE;
    }

    freeaddrinfo(servinfo); // free the linked-list, no longer needed

    // client.c connect
    // connect(sfd, servinfo->ai_addr, servinfo->ai_addrlen);

    if (listen(sfd, BACKLOG) == -1) {
        // handle_error("listen");
        syslog(LOG_ERR, "Failed to listen on the socket: %s", strerror(errno));
        return EXIT_FAILURE;
    }
    printf("Listening for connections on port: %s...\n\n", PORT);
    syslog(LOG_INFO, "Listening for connections on port: %s...\n\n", PORT);

    lfd = open(LOG_FILE, O_RDWR | O_CREAT | O_TRUNC, 0644);

    while(1) {
        int client_fd = accept(sfd, NULL, NULL);
        if (client_fd == -1) {
            syslog(LOG_ERR, "Failed to accept new connection: %s", strerror(errno));
            continue;
        }
        handle_connection(client_fd);
    }
    close(lfd);
    close(sfd);
    unlink(LOG_FILE);
    unlink(PID_FILE);

    closelog();
    return EXIT_SUCCESS;
}
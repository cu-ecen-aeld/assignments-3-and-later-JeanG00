#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define SYSLOG_OPTION       LOG_CONS
#define SYSLOG_FACILITY     LOG_USER
#define SYSLOG_PRIORITY     LOG_INFO

int main(int argc, char *argv[]) {
    // printf("argc: %d, argv: %s %s\n", argc, argv[0], argv[1]);

    char *ident = "writer";

    openlog(ident, SYSLOG_OPTION, SYSLOG_FACILITY);
    if (argc != 3) {
        syslog(LOG_ERR, "Incorrect number of arguments, %d", argc);

        printf("Error Syntax: ./write <writefile> <writestr>\n");
        closelog();
        exit(EXIT_FAILURE);
    }
    char* writefile = argv[1];
    char* writestr = argv[2];
    int fd = open(writefile, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open file: %s", writefile);
        closelog();
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written = write(fd, writestr, strlen(writestr));
    if (bytes_written == -1) {
        syslog(LOG_ERR, "Failed to write to file: %s", writefile);
        close(fd);
        closelog();
        exit(EXIT_FAILURE);
    }

    syslog(SYSLOG_PRIORITY, "Writing %s to %s", argv[2], argv[1]);

    close(fd);
    closelog();
    return EXIT_SUCCESS;
}

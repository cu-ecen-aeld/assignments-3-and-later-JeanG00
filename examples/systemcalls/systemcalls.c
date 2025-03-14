#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "systemcalls.h"

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{
    if (cmd == NULL) {
        return false;
    }
    // size_t cmd_len = strlen("which ") + strlen(cmd) + strlen(" > /dev/null 2>&1");
    // char *command = malloc(cmd_len);
    // if (command == NULL) {
    //     perror("malloc");
    //     return false;
    // }
    // strcpy(command, "which ");
    // strcat(command, cmd);
    // strcat(command, " > /dev/null 2>&1");
    // printf("BEFORE CALLING do_system : %s\n", command);

    int rc = system(cmd);
    // printf("SYSTEM EXEC: %s, %d, %d\n", cmd, rc, WIFSIGNALED(rc));
    if (rc == 0 && WIFSIGNALED(rc) == 0) {
        return true;
    }
    else if (WIFSIGNALED(rc)) {
        printf("command terminated with signal %d\n", WTERMSIG(rc));
        return false;
    }
    else if (WIFEXITED(rc)) {
        printf("command exited %d\n", WEXITSTATUS(rc));
        return false;
    }

    return true;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    // printf("STARTing CHILD %s\n", command[0]);
    // https://stackoverflow.com/questions/42690197/why-does-this-program-with-fork-print-twice/42690260#42690260
    fflush(stdout);
    pid_t pid = fork();

    if (pid < 0) {
        // fork failed
        return false;
    }
    else if (pid == 0) {
        // In child process: execute the command using execv
        // int execv(const char *pathname, char *const argv[]);
        execv(command[0], command);
        perror("execv");
        exit(EXIT_FAILURE);
    } else {
        int wstatus;
        // The waitpid() system call suspends execution of the calling thread until a child specified by pid argument has  changed  state.
        if (waitpid(pid, &wstatus, 0) == -1) {
            return false;
        }
        // If  wstatus is not NULL, wait() and waitpid() store status information in the int to which it points.
        if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
            return true;
        }
        return false;
    }

    va_end(args);

    return true;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    printf("STARTing CHILD %s\n", command[0]);
    fflush(stdout);

    int fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) { perror("open"); exit(EXIT_FAILURE); }
    int pid;
    switch (pid = fork()) {
        case -1: perror("fork"); return false;
        case 0:
            // https://stackoverflow.com/a/13784315/1446624
            if (dup2(fd, 1) < 0) { perror("dup2"); exit(EXIT_FAILURE); }
            close(fd);
            execv(command[0], command);
            perror("execv");
            exit(EXIT_FAILURE);
        default:
            int wstatus;
            if (waitpid(pid, &wstatus, 0) == -1) {
                return false;
            }
            if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
                return true;
            }
            return false;
    }

    va_end(args);

    return true;
}

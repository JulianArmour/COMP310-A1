#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <fcntl.h>

static const int COMMAND_MAX_LENGTH = 256;
static const int HISTORY_MAX_SIZE = 100;
typedef char History[100][256];

/**
 * Adds a command to the historical commands sent to tiny-shell.
 * Adding a command is O(n). I could have implemented as a linked-list
 * to achieve O(1) but since this assignment only requires n=100 and there aren't any
 * performance requirements, this is way more simple.
 *
 * @param history
 *              a historical list of commands sent to tiny-shell
 * @param command
 *              the most recent command sent to tiny-shell. To be added
 *              to history
 */
void history_add(char history[HISTORY_MAX_SIZE][COMMAND_MAX_LENGTH], char *command) {
    static int size; // the current size of the history
    if (size < HISTORY_MAX_SIZE) {
        strcpy(history[size], command);
        size++;
    } else {
        // move the older commands down to make room for the new command
        for (int i = 0; i < (HISTORY_MAX_SIZE - 1) ; ++i) {
            strcpy(history[i], history[i + 1]);
        }
        strcpy(history[HISTORY_MAX_SIZE - 1], command);
    }
}

void history_print(char history[HISTORY_MAX_SIZE][COMMAND_MAX_LENGTH]) {
    for (int i = 0; i < HISTORY_MAX_SIZE; ++i) {
        if (history[i][0] != '\0') {
            printf("%d  %s\n", i + 1, history[i]);
        } else {// history size < 100 and no more history to display
            break;
        }
    }
}

/**
 *
 * @param dest
 * @return
 */
void get_a_line(char *dest, int maxLineLength) {
    if (fgets(dest, maxLineLength, stdin) == NULL) {
        puts("Could not get command");
        exit(EXIT_FAILURE);
    }
}

/**
 *
 * @param command a command equivalent to runProgram(command)
 * @param tgtArgs where the parsed arguments will be placed. Terminated
 *              by a NULL pointer.
 */
void parseCommand(char* command, char *tgtArgs[], int argMaxLen) {
    char *arg = strtok(command, " ");
    int i = 0;
    while (arg != NULL && i < argMaxLen - 1) {
        tgtArgs[i++] = arg;
        arg = strtok(NULL, " ");
    }
    tgtArgs[i] = NULL;
}

void runProgram(char *command) {
    pid_t childPID = fork();
    if (childPID < 0) { // error
        printf("Failed to create child process");
        exit(EXIT_FAILURE);

    } else if (childPID == 0) { // in child process
        char *args[32]; // allow up to 30 program arguments, probably more than enough
        parseCommand(command, args, 32);

        // run the program
        execvp(args[0], args);

    } else {// in parent process
        // wait for child process to finish
        waitpid(childPID, NULL, 0);
    }
}

void runPipedPrograms(char *command, char *fifoPath) {
    char *command1 = strtok(command, "|");
    char *command2 = strtok(NULL, "|");

    char *program1Args[32];
    char *program2Args[32];
    parseCommand(command1, program1Args, 32);
    parseCommand(command2, program2Args, 32);

    pid_t pid2 = fork();// create process for program 2

     /** Note: notice that I'm forking to program 2 before program 1, this allows the
     tiny-shell process to wait() on program 2, which is the last program executed
     by the piped commands. If we wait on program 1, then the tiny-shell process might
     resume before program 2 has finished executing. **/
    if (pid2 == 0) {// program 2
        pid_t pid1 = fork(); //create process for program 1
        if (pid1 == 0) {// program 1
            int fifoWrite = open(fifoPath, O_WRONLY);
            // program 1 wishes to send it's output to the fifo instead of stdout. So we have to
            // overwrite the stdin file descriptor to fifoWrite.
            dup2(fifoWrite, fileno(stdout));
            // this process will never use fifoRead so close it.
            execvp(program1Args[0], program1Args);

        } else {// program 2
            int fifoRead = open(fifoPath, O_RDONLY);
            // program 2 wishes to read it's input from the fifo instead of stdin. So we have to
            // overwrite the stdin file descriptor to fifoRead.
            dup2(fifoRead, fileno(stdin));
            execvp(program2Args[0], program2Args);
        }

    } else {//tiny-shell
        waitpid(pid2, NULL, 0);
        puts("done");
    }
}

void setResourceLimit(char *strLimit) {
    // get current res limit
    struct rlimit resourceLimit;
    getrlimit(RLIMIT_DATA, &resourceLimit);

    // parse out the new desired limit
    rlim_t newLimit = (rlim_t) strtoul(strLimit, NULL, 10);

    // check if this new limit is bellow the hard cap
    if (newLimit < resourceLimit.rlim_max) {
        // set the new resource limit
        resourceLimit.rlim_cur = newLimit;
        setrlimit(RLIMIT_DATA, &resourceLimit);
        printf("New resource limit: %lu Bytes\n", (unsigned long) resourceLimit.rlim_cur);
    } else {
        printf("Failed. This size is larger than the hard limit of %lu",
               (unsigned long) resourceLimit.rlim_max);
    }
}

void my_system(char *command, char *fifoPath) {
    // historical commands sent to tiny-shell
    static History history;
    
    history_add(history, command);

    // make a copy of 'command'. Since I'm about to use strtok and I need
    // the full command if runProgram() is called
    char commandCopy[COMMAND_MAX_LENGTH];
    strcpy(commandCopy, command);

    // check for internal commands
    char *cmd = strtok(commandCopy, " ");
    if (strcmp(cmd, "chdir") == 0) {// chdir command
        char *newDirPath = strtok(NULL, " ");// chdir argument (new directory path)

        if (chdir(newDirPath) == -1) {
            printf("%s: No such file or directory\n", newDirPath);
        }

    } else if (strcmp(cmd, "history") == 0) {// history command
        history_print(history);

    } else if (strcmp(cmd, "limit") == 0) {
        char *strLimit = strtok(NULL, " ");
        setResourceLimit(strLimit);

    } else {
        if (strstr(command, " | ") == NULL) {
            runProgram(command);
        } else {
            if (fifoPath != NULL) {
                runPipedPrograms(command, fifoPath);
            } else {
                puts("No path to a FIFO is present.");
            }
        }
    }
}

int main(int argc, char **argv) {
    char* fifoPath = NULL;
    if (argc > 1) fifoPath = argv[1];

    while (1) {
        char command[COMMAND_MAX_LENGTH];
        get_a_line(command, COMMAND_MAX_LENGTH);
        if (strlen(command) > 1) {
            //remove the newline character
            command[strlen(command) - 1] = '\0';
            my_system(command, fifoPath);
        } else {
            return 0;
        }
    }
}
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <signal.h>
#include <fcntl.h>

// the maximum number of characters allowed for a user-entered command (including '\0')
static const int COMMAND_MAX_LENGTH = 256;
// the maximum number of commands to keep in history
static const int HISTORY_MAX_SIZE = 100;
// the data-structure containing the historical commands
typedef char History[100][256];

/**
 * Adds a command to the historical commands sent to tiny-shell.
 * Adding a command is O(n) when at-least 100 commands have been used in the shell.
 * But O(1) if less than 100 have been used. I could have implemented as a linked-list
 * to always achieve O(1) inserts but since this assignment only requires n=100 and
 * there aren't any performance requirements, this is way simpler.
 *
 * @param history
 *              a historical list of commands sent to tiny-shell
 * @param command
 *              the most recent command sent to tiny-shell. To be added
 *              to history
 */
void history_add(History history, char *command) {
    static int size; // the current size of the history

    if (size < HISTORY_MAX_SIZE) {
        // insert the new command in the next available spot in the array
        strcpy(history[size], command);
        size++;

    } else {
        // move the older commands down to make room for the new command,
        // which is inserted at the end of the array
        for (int i = 0; i < (HISTORY_MAX_SIZE - 1) ; ++i) {
            strcpy(history[i], history[i + 1]);
        }
        strcpy(history[HISTORY_MAX_SIZE - 1], command);
    }
}

/**
 * Prints out the current tiny-shell history to stdout
 *
 * @param history the history data-structure used by tiny-shell
 */
void history_print(History history) {
    for (int i = 0; i < HISTORY_MAX_SIZE; ++i) {
        if (history[i][0] != '\0') {
            printf("%d  %s\n", i + 1, history[i]);
        } else {// history size < 100 and no more history to display
            break;
        }
    }
}

/**
 * gets an input from the tiny-shell user.
 *
 * @param dest a buffer to store the input
 * @param maxLineLength the size of the buffer dest
 */
void get_a_line(char *dest, int maxLineLength) {
    if (fgets(dest, maxLineLength, stdin) == NULL) {
        puts("Could not get command");
        exit(EXIT_FAILURE);
    }
}

/**
 * parses a program and arguments from a string of the format "program arg1 arg2 ..."
 * to an array of strings equivalent to argv used by arbitrary programs.
 *
 * @param command a command that was sent to tiny-shell
 * @param tgtArgs where the parsed arguments will be placed. Terminated
 *              by a NULL pointer.
 * @param argMaxLen the maximum number of arguments allowed in tgtArgs
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

/**
 * runs a single program that is spawned as a child process of tiny-shell.
 *
 * @param command a tiny-shell command that is the relative path, absolute path, or name of a program in
 *          one of the directories in PATH.
 */
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

/**
 * runs 2 programs, piping the output of the first to the second.
 *
 * @param command the tiny-shell command in the form of "program1 | program2"
 * @param fifoPath the path to the fifo used to pipe the programs
 */
void runPipedPrograms(char *command, char *fifoPath) {
    // get the first and second program and arguments as a string
    char *command1 = strtok(command, "|");
    char *command2 = strtok(NULL, "|");

    // program arguments equivalent to argv used by the programs.
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
    }
}

/**
 * Sets the soft-limit of the Tiny-shell and all processes it spawns to strLimit
 *
 * @param strLimit a base-10 number represented using a string. (e.g. "1000000")
 */
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

/**
 * This is the function that handles commands inputted to Tiny-shell
 *
 * @param command a tiny-shell command string, the string character is NOT a newline char
 * @param fifoPath the filepath to the fifo created with "mkfifo"
 */
void my_system(char *command, char *fifoPath) {
    // historical commands sent to tiny-shell
    static History history;
    
    history_add(history, command);

    // make a copy of 'command'. Since I'm about to use strtok and I need
    // the full command if runProgram() or runPipedPrograms() are called
    char commandCopy[COMMAND_MAX_LENGTH];
    strcpy(commandCopy, command);

    // check for internal commands
    char *cmd = strtok(commandCopy, " ");
    if (strcmp(cmd, "chdir") == 0) {// chdir command
        // chdir argument (new directory path)
        char *newDirPath = strtok(NULL, " ");
        // attempt to change the present directory
        if (chdir(newDirPath) == -1) {
            printf("%s: No such file or directory\n", newDirPath);
        }

    } else if (strcmp(cmd, "history") == 0) {// history command
        history_print(history);

    } else if (strcmp(cmd, "limit") == 0) {// limit command
        // get the limit argument
        char *strLimit = strtok(NULL, " ");
        setResourceLimit(strLimit);

    } else {
        if (strstr(command, " | ") == NULL) {
            // not a piped command, execute the program normally
            runProgram(command);
        } else {
            // is a piped command, check if a fifo was supplied to Tiny-shell
            if (fifoPath != NULL) {
                runPipedPrograms(command, fifoPath);
            } else {
                puts("No path to a FIFO is present.");
            }
        }
    }
}

/**
 * (Note: only I/O operations using read() and write() are considered safe in a
 * signal handler function. Source: http://man7.org/linux/man-pages/man7/signal-safety.7.html
 * I found this because I was having buffering problems with printf() and found this answer:
 * https://stackoverflow.com/a/9547988)
 *
 * This is my custom SIGINT signal handler. It prompts the user if they want to exit
 * the Tiny-shell. They may answer: yes, Yes, y, Y to exit. Any other input will terminate
 * Tiny-shell.
 */
void handleSIGINT(int signal) {
    char response[4];
    char msg[] = "\n>>> Do you wish to exit Tiny-shell(y/n)? ";
    write(STDOUT_FILENO, msg, strlen(msg));
    if(read(STDIN_FILENO, response, 4) != -1) {
        if (response[0] == 'y' || response[0] == 'Y') {
            exit(EXIT_SUCCESS);
        } else {
            write(STDOUT_FILENO, ">>> ", 4);
        }
    }
}

// override SIGTSTP handler to ignore the signal
void handleSIGTSTP(int signal) {
    write(STDOUT_FILENO, "\n>>> ", 5);
}

int main(int argc, char **argv) {
    // get the fifo path from the argument (if there is one)
    char* fifoPath = NULL;
    if (argc > 1) fifoPath = argv[1];

    // set-up SIGINT
    signal(SIGINT, handleSIGINT);
    // set-up SIGTSTP
    signal(SIGTSTP, handleSIGTSTP);

    // REPL
    while (1) {
        char command[COMMAND_MAX_LENGTH];
        printf(">>> ");
        fflush(stdout);
        get_a_line(command, COMMAND_MAX_LENGTH);
        if (strlen(command) > 1) {
            //remove the newline character
            command[strlen(command) - 1] = '\0';
            my_system(command, fifoPath);
        }
    }
}
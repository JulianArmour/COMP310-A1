#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

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

void runFile(char *filePath) {
    pid_t childPID = fork();
    if (childPID < 0) {
        printf("Failed to create child process");
        exit(EXIT_FAILURE);
    } else if (childPID == 0) { // in child process
        // run the file
        execlp(filePath, filePath, (char*)NULL);
    } else {// in parent process
        // wait for child process to finish
        waitpid(childPID, NULL, 0);
    }
}

void my_system(char *command) {
    // historical commands sent to tiny-shell
    static History history;
    
    history_add(history, command);

    // check for internal commands
    char *cmd = strtok(command, " ");
    if (strcmp(cmd, "chdir") == 0) {// chdir command
        char *newDirPath = strtok(NULL, " ");// chdir argument (new directory path)
        if (chdir(newDirPath) == -1) {
            printf("%s: No such file or directory\n", newDirPath);
        }
    } else if (strcmp(cmd, "history") == 0) {// history command
        history_print(history);
    } else {// run a file
        runFile(command);
    }
}

int main(int argc, char **argv) {
    while (1) {
        char command[256];
        get_a_line(command, COMMAND_MAX_LENGTH);
        if (strlen(command) > 1) {
            //remove the newline character
            command[strlen(command) - 1] = '\0';
            my_system(command);
        } else {
            return 0;
        }
    }
}
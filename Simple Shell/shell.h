#include <signal.h>
#include <dirent.h>
#include <glob.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "path.h"

#define MAXLINE 128
#define MAXPREF 64

void signalHandler(int sig);

void printPrompt(char *prefix, char *buffer);

void setArgumentArray(char *argv[], int argc, char *buffer);

int getArgumentCount(char *buffer);

void printAllValidCmdPaths(char *cmd, struct pathelement *pathEnv);

char *getFirstValidCommandPath(char *cmd, struct pathelement *pathEnv);

char *getValidCmdPath(char *cmd, struct pathelement *pathEnv);

void printFileContents(char *file);

void waitWithTimeout(int timeout);

int isBackgroundProcess(char *argv[], int *argc);

int startRedirection(char *argv[], int *argc, int noclobber, char **type);

void endRedirection(char *type);

void dupFds(int fd, char *type);

void whichCommand(char *argv[], int argc, struct pathelement *pathEnv);

void whereCommand(char *argv[], int argc, struct pathelement *pathEnv);

void cdCommand(char *argv[], int argc, char *prevDir, char *buffer);

void pwdCommand(char *buffer);

void listCommand(char *argv[], int argc, char *buffer);

void killCommand(char *argv[], int argc);

void promptCommand(char *argv[], int argc, char *prefix, char *buffer);

void printenvCommand(char *argv[], int argc);

void setenvCommand(char *argv[], int argc, struct pathelement **pathEnv);

void noclobberCommand(int *noclobber);

void executeExternalCommand(char *argv[], int *argc, struct pathelement *pathEnv, int timeout, int noclobber);


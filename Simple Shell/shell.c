#include "shell.h"

int fgPid = -1;
int fgStatus;

int main(int mainc, char **mainv)
{
	char  *prefix = (char *)malloc(sizeof(char) * MAXPREF);
	char  *prevDir = (char *)malloc(sizeof(char) * MAXLINE);
	char  *buffer = (char *)malloc(sizeof(char) * MAXLINE);
	struct pathelement *pathEnv = getPath(getenv("PATH"));

	// Timeout is in seconds but will be converted to microseconds for use.
	// If atoi fails, it will return 0 and timeout will be set to 0 anyways.
	int timeout = (mainc > 1) ? atoi(mainv[1]) : 0;
	int noclobber = 0;
	int shouldExit = 0;
	int currentIgnoreEOFCount = 1;

	strcpy(prefix, "");
	strcpy(prevDir, "");

	signal(SIGINT, signalHandler);
	signal(SIGCHLD, signalHandler);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

	while (1)
	{
		printPrompt(prefix, buffer);
		
		if (fgets(buffer, MAXLINE, stdin) == NULL)
		{
			char *ignoreeof = getenv("ignoreeof");
			int maxIgnoreEOFCount = (ignoreeof != NULL) ? atoi(ignoreeof) : 1;
			
			// ^D will be ignored if ignoreeof is equal to 0 or '\n'.
			//
			// ^D will cause the shell to exit if ignoreeof is greater than 0 and
			// EOF has been created ignoreeof number of times.
			//
			// ^D will cause the shell will exit immediately if ignoreeof is not set, 
			// ignoreeof is set to a non-number, or if ignoreeof is set to a number
			// less than 0
			if (ignoreeof != NULL && (strcmp(ignoreeof, "") == 0 || strcmp(ignoreeof, "0") == 0))
			{
				printf("Use command 'exit' to leave the shell.\n");
				continue;
			}
			else if (currentIgnoreEOFCount >= maxIgnoreEOFCount)
			{
				printf("^D\n");
				break;
			}
			else
			{
				currentIgnoreEOFCount++;
				printf("Use command 'exit' to leave the shell.\n");
				continue;
			}
		}

		if (buffer[strlen(buffer) - 1] == '\n')
		{
			buffer[strlen(buffer) - 1] = '\0';
		}
		
		int argc = getArgumentCount(buffer);

		if (argc == 0)
		{
			continue;
		}

		char *argv[argc + 1];
		setArgumentArray(argv, argc, buffer);

		if (strcmp(argv[0], "exit") == 0)
		{
			printf("Executing built-in 'exit' command.\n");
			shouldExit = 1;
		}
		else if (strcmp(argv[0], "which") == 0)
		{
			printf("Executing built-in 'which' command.\n");
			whichCommand(argv, argc, pathEnv);
		}
		else if (strcmp(argv[0], "where") == 0)
		{
			printf("Executing built-in 'where' command.\n");
			whereCommand(argv, argc, pathEnv);
		}
		else if (strcmp(argv[0], "cd") == 0)
		{
			printf("Executing built-in 'cd' command.\n");
			cdCommand(argv, argc, prevDir, buffer);
		}
		else if (strcmp(argv[0], "pwd") == 0)
		{
			printf("Executing built-in 'pwd' command.\n");
			pwdCommand(buffer);
		}
		else if (strcmp(argv[0], "list") == 0)
		{
			printf("Executing built-in 'list' command.\n");
			listCommand(argv, argc, buffer);
		}
		else if (strcmp(argv[0], "pid") == 0)
		{
			printf("Executing built-in 'pid' command.\n");
			printf("SHELL: pid [%d] groupid [%d].\n", getpid(), getpgrp());	
		}
		else if (strcmp(argv[0], "kill") == 0)
		{
			printf("Executing built-in 'kill' command.\n");
			killCommand(argv, argc);	
		}
		else if (strcmp(argv[0], "prompt") == 0)
		{
			printf("Executing built-in 'prompt' command.\n");
			promptCommand(argv, argc, prefix, buffer);
		}
		else if (strcmp(argv[0], "printenv") == 0)
		{
			printf("Executing built-in 'printenv' command.\n");
			printenvCommand(argv, argc);
		}
		else if (strcmp(argv[0], "setenv") == 0)
		{
			printf("Executing built-in 'setenv' command.\n");
			setenvCommand(argv, argc, &pathEnv);
		}
		else if (strcmp(argv[0], "noclobber") == 0)
		{
			noclobberCommand(&noclobber);
		}
		else
		{
			executeExternalCommand(argv, &argc, pathEnv, timeout, noclobber);
		}

		for (int i = 0; i < argc; i++)
		{
			free(argv[i]);
		}

		if (shouldExit)
		{
			break;
		}
	}

	free(prefix);
	free(prevDir);
	free(buffer);
	freePath(pathEnv);

	return 0;
}

void signalHandler(int sig)
{
	if (sig == SIGCHLD)
	{
		int pid, status;

		if ((pid = waitpid(-1, &status, WNOHANG)) < 0)
		{
			perror("External command encountered a waitpid error");
		}

		if (pid == fgPid)
		{
			fgStatus = status;
			fgPid = -1;
		}
	}
}

void printPrompt(char *prefix, char *buffer)
{
	if (getcwd(buffer, MAXLINE) != NULL)
	{
		printf("%s [%s]> ", prefix, buffer);
	}
	else
	{
		perror("Could not print prompt - The length of the cwd exceeded the buffer size");
		printf("> ");
	}
}

void setArgumentArray(char *argv[], int argc, char *buffer)
{
	// Since I am accounting for wildcards in this function, wildcards are 
	// able to be used for both built-in and external command so long as 
	// the command does not receive too many arguments.

	char *arg = strtok(buffer, " ");
	glob_t files;

	// I do not need to check if using strtok returns NULL because I
	// already know from argc exactly how many times I must use the
	// function.
	for (int i = 0; i < argc; arg = strtok(NULL, " "))
	{
		glob(arg, 0, NULL, &files);
		
		// I check if i == 0 because I do not want to expand the
		// first argument.
		if (i == 0 || files.gl_pathc == 0)
		{
			argv[i] = malloc(sizeof(char) * strlen(arg) + 1);
			strncpy(argv[i], arg, strlen(arg));
			argv[i][strlen(arg)] = '\0';
			i++;
		}
		else
		{
			// Instead of just adding the single arg to argv, I add every
			// file that matches it (which also includes arg itself).
			for (char **expandedArg = files.gl_pathv; *expandedArg != NULL; ++expandedArg)
			{
				argv[i] = malloc(sizeof(char) * strlen(*expandedArg) + 1);
				strncpy(argv[i], *expandedArg, strlen(*expandedArg));
				argv[i][strlen(*expandedArg)] = '\0';
				i++;
			}
		}

		globfree(&files);
	}

	argv[argc] = NULL;
}

int getArgumentCount(char *buffer)
{
	// I need to determine argc before making argv so that I can malloc the
	// correct amount of memory.

	// A copy of the buffer needs to be made because strtok would mess it up
	// which would make it unusable for making argv.
	char buff[strlen(buffer)];
	strcpy(buff, buffer);

	int argc = 0;
	char *arg = strtok(buff, " ");
	glob_t files;

	while (arg != NULL)
	{
		glob(arg, 0, NULL, &files);

		// I check to see if argc == 0 because I do not want to
		// expand the first argument.
		if (argc == 0 || files.gl_pathc == 0)
		{
			argc++;
		}
		else
		{
			argc += files.gl_pathc;
		}

		globfree(&files);
		arg = strtok(NULL, " ");
	}

	return argc;
}

void printAllValidCmdPaths(char *cmd, struct pathelement *pathEnv)
{
	char buffer[MAXLINE];
	
	// Since pathEnv is a local copy, I can use it to iterate without
	// changing the variable in main.
	while (pathEnv != NULL)
	{
		snprintf(buffer, MAXLINE, "%s/%s", pathEnv->element, cmd);
		
		if (access(buffer, X_OK) == 0)
		{
			printf("%s\n", buffer);
		}

		pathEnv = pathEnv->next;
	}
}

char *getFirstValidCmdPath(char *cmd, struct pathelement *pathEnv)
{
	char buffer[MAXLINE];
	
	while (pathEnv != NULL)
	{
		snprintf(buffer, MAXLINE, "%s/%s", pathEnv->element, cmd);
		
		if (access(buffer, X_OK) == 0)
		{
			char *path = (char *)malloc(sizeof(char) * strlen(buffer) + 1);
			strncpy(path, buffer, strlen(buffer));
			path[strlen(buffer)] = '\0';
			return path;
		}	
		
		pathEnv = pathEnv->next;
	}

	return NULL;
}

char *getValidCmdPath(char *cmd, struct pathelement *pathEnv)
{
	// This will either return an absolute/relative path if the file exists
	// and is not a directory, a path from the PATH variable if such a
	// path exists or NULL.

	struct stat s;
	char *path = NULL;

	if (stat(cmd, &s) == 0)
	{
		if (!S_ISDIR(s.st_mode))
		{
			path = (char *)malloc(sizeof(char) * strlen(cmd) + 1);
			strncpy(path, cmd, strlen(cmd));
			path[strlen(cmd)] = '\0';
		}
	}
	else if ((path = getFirstValidCmdPath(cmd, pathEnv)) != NULL)
	{
		if (access(path, X_OK) != 0)
		{
			free(path);
			path = NULL;
		}
	}
	
	if (path == NULL)
	{
		fprintf(stderr, "Command '%s' could not be found.\n", cmd);
	}

	return path;
}

void printFileContents(char *file)
{
	DIR *d;
	struct dirent *entry;
	struct stat s;

	if (stat(file, &s) == 0)
        {
        	// Ensures that this works with both directory arguments
       		// and regular file arguments.
        	if (S_ISDIR(s.st_mode))
      		{
			if ((d = opendir(file)) != NULL)
			{
				printf("%s:\n", file);
				while ((entry = readdir(d)) != NULL)
				{
					printf("%s\n", entry->d_name);
				}

				closedir(d);
			}
			else
			{
				perror("Command 'list' encountered an error with opendir");
			}
		}
		else
		{
			printf("%s\n", file);
		}
	}
	else
	{
		fprintf(stderr, "%s: ", file);
		perror("Command 'list' encountered a stat error");
	}
}

void waitWithTimeout(int timeout)
{
        // This is in microseconds.
        float timeBetweenChecks = 100000;
        float currentTimeout = 0;

        while (1)
        {
		usleep(timeBetweenChecks);
		currentTimeout += timeBetweenChecks;

                // I need to convert timeout to microseconds since the
                // currentTimeout is in microseconds.
		
		// The shell will stop waiting if the foreground process
		// terminates or if the process has a timeout and that
		// timeout has been reached.
		if (fgPid == -1)
		{
			break;
		}
		else if (timeout != 0 && currentTimeout >= (timeout * 1000000))
                {
			printf("External command took to long.\n");
                        kill(fgPid, SIGINT);
                        break;
		}
        }
}

int isBackgroundProcess(char *argv[], int *argc)
{
	if (*argc > 1 && strcmp(argv[*argc - 1], "&") == 0)
	{
		(*argc)--;
		free(argv[*argc]);
		argv[*argc] = NULL;

		return 1;
	}

	return 0;
}

int startRedirection(char *argv[], int *argc, int noclobber, char **type)
{
	if (*argc > 2)
	{
		char *redirType = argv[*argc - 2];
		char *fileName = argv[*argc - 1];

		if (strcmp(redirType, "<") == 0 && access(fileName, F_OK) != 0)
		{
			printf("%s: File does not exist.\n", fileName);
			return -1;
		}

		if (strcmp(redirType, "<") != 0 && access(fileName, F_OK) == 0 && noclobber)
		{
			printf("%s: Cannot overwrite existing file.\n", fileName);
			return -1;
		}

		int fd;

		if (strcmp(redirType, ">") == 0 || strcmp(redirType, ">&") == 0)
		{
			fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);	
		}
		else if (strcmp(redirType, ">>") == 0 || strcmp(redirType, ">>&") == 0)
		{
			fd = open(fileName, O_WRONLY | O_CREAT | O_APPEND, 0644);
		}
		else if (strcmp(redirType, "<") == 0)
		{
			fd = open(fileName, O_RDONLY, 0644);
		}
		else
		{
			return 0;
		}

		dupFds(fd, redirType);

		*type = malloc(sizeof(char) * strlen(redirType) + 1);
		strncpy(*type, redirType, strlen(redirType));
		(*type)[strlen(redirType)] = '\0';

		free(argv[*argc - 1]);
		argv[*argc - 1] = NULL;
		free(argv[*argc - 2]);
		argv[*argc - 2] = NULL;
		(*argc) -= 2;
		
		return 1;
	}

	return 0;
}

void endRedirection(char *type)
{
	if (type != NULL)
	{
		int fd;

		if (strcmp(type, "<") == 0)
		{
			fd = open("/dev/tty", O_RDONLY);
		}
		else
		{
			fd = open("/dev/tty", O_WRONLY);
		}

		dupFds(fd, type);

		free(type);
	}
}

void dupFds(int fd, char *type)
{
	if (strcmp(type, ">") == 0 || strcmp(type, ">>") == 0)
        {
		close(1);
	}
	else if (strcmp(type, ">&") == 0 || strcmp(type, ">>&") == 0)
	{
		close(1);
		dup(fd);
		close(2);
	}
	else                
	{
		close(0);
	}

	dup(fd);
	close(fd);
}

void whichCommand(char *argv[], int argc, struct pathelement *pathEnv)
{
	if (argc > 1)
        {
		for (int i = 1; i < argc; i++)
		{
			char *p = getFirstValidCmdPath(argv[i], pathEnv);
			if (p != NULL)
			{
				printf("%s\n", p);
				free(p);
			}
		}
	}
}

void whereCommand(char *argv[], int argc, struct pathelement *pathEnv)
{
	if (argc > 1)
	{
		for (int i = 1; i < argc; i++)
		{
			printAllValidCmdPaths(argv[i], pathEnv);
		}
	}
}

void cdCommand(char *argv[], int argc, char *prevDir, char *buffer)
{
	struct stat s;

	if (argc == 1)
	{
		char *home = getenv("HOME");
		if (stat(home, &s) == 0)
		{
			if (S_ISDIR(s.st_mode))
			{
				getcwd(prevDir, MAXLINE);
				chdir(home);
			}
			else
			{
				fprintf(stderr, "Command 'cd' could not use [%s], it is not a directory.\n", home);
			}
		}
		else
		{
			perror("Command 'cd' encountered a stat error");
		}
	}
	else if (argc == 2)
	{
		if (strcmp(argv[1], "-") == 0)
		{
			// I made it so prevDir is intialized to the empty string so I can
			// check to see if it has been set yet.
			if (strcmp(prevDir, "") != 0)
			{
				getcwd(buffer, MAXLINE);
				chdir(prevDir);
				strcpy(prevDir, buffer);
			}
			else
			{
				fprintf(stderr, "Command 'cd' has not set the previous directory yet.\n");
			}
		}
		else if (stat(argv[1], &s) == 0)
		{
			if (S_ISDIR(s.st_mode))
			{
				getcwd(prevDir, MAXLINE);
				chdir(argv[1]);
			}
			else
			{
				fprintf(stderr, "Command 'cd' could not use [%s], it is not a directory.\n", argv[1]);
			}
		}
		else
		{
			perror("Command 'cd' encountered a stat error");
		}
	}
	else
	{
		fprintf(stderr, "Command 'cd' expected 0 or 1 arguments, received %d.\n", argc - 1);
	}
}

void pwdCommand(char *buffer)
{
	if (getcwd(buffer, MAXLINE) != NULL)
	{
		printf("%s\n", buffer);
	}
	else
	{
		perror("Command 'pwd' encountered an error - The length of the cwd exceeded the buffer size");
	}
}

void listCommand(char *argv[], int argc, char *buffer)
{
	if (argc == 1)
	{
		printFileContents(getcwd(buffer, MAXLINE));
	}
	else
	{
		for (int i = 1; i < argc; i++)
		{
			printFileContents(argv[i]);
		}
	}
}

void killCommand(char *argv[], int argc)
{
	int pid, sig;

	if (argc == 1)
	{
		fprintf(stderr, "Command 'kill' expected at least 1 argument, received 0.\n");
	}
	else if (argc == 2)
	{
		if ((pid = atoi(argv[1])) == 0 || kill(pid, SIGTERM) != 0)
		{
			fprintf(stderr, "Command 'kill' was unable to signal [%s] with SIGTERM.\n", argv[1]);
		}
	}
	else
	{
		// I only want to atoi the portion of argv[1] that does not
		// include '-'.
		sig = (argv[1][0] == '-') ? atoi(&argv[1][1]) : atoi(argv[1]);

		// This command will send the signal to every process that is listed
		// as an argument.
		for (int i = 2; i < argc; i++)
		{
			if ((pid = atoi(argv[i])) == 0 || sig == 0 || kill(pid, sig) != 0)
			{
				fprintf(stderr, "Command 'kill' was unable to signal [%s] with sig [%s].\n", argv[i], argv[1]);
			}
		}
	}
}

void promptCommand(char *argv[], int argc, char *prefix, char *buffer)
{
	if (argc == 1)
	{
		while (1)
		{
			// This will continue to ask for a prompt regardless
			// of whether it encounters an EOF.
			printf("Enter a prompt prefix: ");
			if (fgets(buffer, MAXLINE, stdin) != NULL)
			{
				if (buffer[strlen(buffer) - 1] == '\n')
				{
					buffer[strlen(buffer) - 1] = '\0';
				}
				
				strcpy(prefix, buffer);
				break;
			}
			else
			{
				printf("^D\n");
			}
		}
	}
	else if (argc == 2)
	{
		strcpy(prefix, argv[1]);
	}
	else
	{
		fprintf(stderr, "Command 'prompt' expected 0 or 1 arguments, received %d.\n", argc - 1);
	}
}

void printenvCommand(char *argv[], int argc)
{
	char *env;

	if (argc == 1)
	{
		env = *__environ;
		for (int i = 0; env != NULL; i++)
		{
			printf("%s\n", env);
			env = *(__environ + i);
		}
	}
	else if (argc == 2)
	{
		if ((env = getenv(argv[1])) != NULL)
		{
			printf("%s=%s\n", argv[1], env);
		}
		else
		{
			fprintf(stderr, "Command 'printenv' could not find environment variable [%s].\n", argv[1]);
		}
	}
	else
	{
		fprintf(stderr, "Command 'printenv' expected 0 or 1 arguments, received %d.\n", argc - 1);
	}
}

void setenvCommand(char *argv[], int argc, struct pathelement **pathEnv)
{
	if (argc == 1)
	{
		printenvCommand(argv, argc);
	}
	else if (argc == 2) 
	{
		setenv(argv[1], "", 1);
		
		// I use a pointer to a pointer for pathEnv here so that
		// the variable in main will be changed upon reassignment.
		if (strcmp(argv[1], "PATH") == 0)
		{
			freePath(*pathEnv);
			*pathEnv = getPath(getenv("PATH"));
		}
	}
	else if (argc == 3)
	{
		setenv(argv[1], argv[2], 1);

		if (strcmp(argv[1], "PATH") == 0)
		{
			freePath(*pathEnv);
			*pathEnv = getPath(getenv("PATH"));
		}
		
	}
	else
	{
		fprintf(stderr, "Command 'setenv' expected 0, 1, or 2 arguments, received %d.\n", argc - 1);
	}
}

void noclobberCommand(int *noclobber)
{
	*noclobber = ! (*noclobber);
	printf("noclobber is now %s.\n", (*noclobber) ? "set" : "unset");
}

void executeExternalCommand(char *argv[], int *argc, struct pathelement *pathEnv, int timeout, int noclobber)
{
	char *path = getValidCmdPath(argv[0], pathEnv);

	// The function getValidCmdPath will return NULL if no executable
	// could be found for the given path.
	if (path == NULL) { return; }

	printf("Executing '%s'.\n", argv[0]);

	int isBgProcess = isBackgroundProcess(argv, argc);
	char *redirType = NULL;
	
	if (! isBgProcess && startRedirection(argv, argc, noclobber, &redirType) < 0)
	{
		free(path);
		return;
	}

	if ((fgPid = fork()) < 0)
	{
		perror("External command encountered a fork error");
	}
	else if (fgPid == 0)
	{
		execve(path, argv, __environ);
		perror("External command could not execute");
		exit(127);
	}

	// Anything after here will only be executed by the parent.
	if (! isBgProcess)
	{
		waitWithTimeout(timeout);
		endRedirection(redirType);
	}

	if(WIFEXITED(fgStatus))
	{
		printf("Command [%s] had an exit status of [%d].\n", argv[0], WEXITSTATUS(fgStatus));
	}


	free(path);
}


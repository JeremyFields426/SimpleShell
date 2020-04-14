#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

struct pathelement *getPath(char *pathEnvTmp);

void freePath(struct pathelement *path);

struct pathelement
{
	char *element;
	struct pathelement *next;
};

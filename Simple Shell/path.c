#include "path.h"

struct pathelement *getPath(char *pathEnvTmp)
{
	struct pathelement *pathHead = NULL;
	struct pathelement **pathTail = &pathHead;

	char *pathEnv = malloc(sizeof(char) * strlen(pathEnvTmp) + 1);
	strncpy(pathEnv, pathEnvTmp, strlen(pathEnvTmp));
	pathEnv[strlen(pathEnvTmp)] = '\0';

	for (char *p = strtok(pathEnv, ":"); p != NULL; p = strtok(NULL, ":"))
	{
		*pathTail = (struct pathelement *)malloc(sizeof(struct pathelement));

                (*pathTail)->element = (char *)malloc(sizeof(char) * strlen(p) + 1);
                strncpy((*pathTail)->element, p, strlen(p));
                (*pathTail)->element[strlen(p)] = '\0';

                (*pathTail)->next = NULL;
                pathTail = &((*pathTail)->next);
	}

	free(pathEnv);

	return pathHead;
}

void freePath(struct pathelement *path)
{
	while (path != NULL)
	{
		struct pathelement *tmp = path;
		path = path->next;
		free(tmp->element);
		free(tmp);
	}
}

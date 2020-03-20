#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int capitest()
{
	printf("capi called\n");
	return 1;
}

typedef struct {
	int id;
	int status;
	char name[10];
} TestDb;

int topen(const char* name, TestDb** tdb)
{
	if (strlen(name) > 9)
		return 1;
	*tdb = malloc(sizeof(TestDb));
	if (!*tdb)
		return 1;
	(*tdb)->id = 1234;
	strcpy((*tdb)->name, name);
	(*tdb)->status = 99;

	return 0;
}

int topen2(const char* name, int* stat, TestDb** tdb)
{
	*stat = 999;
	return topen(name, tdb);
}

int tgetId(TestDb* tdb)
{
	return tdb->id;
}

const char* tgetName(TestDb* tdb)
{
	return tdb->name;
}

void tclose(TestDb *tdb)
{
	if (!tdb)
		return;

	if (tdb->id != 1234) {
		printf("tclose: unrecognized id.\n");
	}
	if (tdb->status != 99) {
		printf("tclose: unrecognized status.\n");
	}

	free(tdb);
}

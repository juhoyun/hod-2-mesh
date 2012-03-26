#include <stdio.h>

int ReadHOD(const char* hod_filename);
int WriteMesh(const char* filename);

void PrintUsage()
{
	printf("Hod2Mesh [filename]\n");
}

int main(int n, char *av[])
{
	if (n < 2)
	{
		PrintUsage();
		return 0;
	}

	if (0 == ReadHOD(av[1]))
		return -1;

	WriteMesh(av[1]);

	return 0;
}
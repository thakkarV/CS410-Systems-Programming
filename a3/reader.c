#include "reader.h"

char * read_input(bool * termiante)
{
	int buf_size = 512;
	char c;
	int index = 0;
	char * readbuf = malloc(sizeof(char) * buf_size);

	while(true)
	{
		c = getchar();

		if (c == '\n' || c == EOF)
		{
			readbuf[index] = '\0';

			if (c == EOF)
				*termiante = true;

			return readbuf;
		}
		else
		{
			readbuf[index++] = c;
		}

		// handle reallocs
		if (index >= buf_size)
		{
			buf_size += buf_size;
			readbuf = realloc(readbuf, buf_size);

			if (!readbuf)
			{
				perror("Realloc failed.");
				exit(1);
			}
		}
	}
}
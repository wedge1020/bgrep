#include "bgrep.h"

void load_file(char *optarg)
{
	char          *save                 = NULL;
	char          *token                = NULL;
	const  char   *delim                = { "," };

	token                               = strtok_r(optarg, delim, &save);
	while (token                       != NULL)
	{
		if (inFileList	               == NULL)
		{
			inFileList                  = (Infile *) malloc (sizeof(Infile));
			if (inFileList             == NULL)
			{
				fprintf(CFG.output, "Error allocating memory.\n");
				exit(1);
			}
			inFileListEnd               = inFileList;
		}
		else
		{
			inFileListEnd     -> next   = (Infile *) malloc (sizeof(Infile));
			if (inFileListEnd -> next  == NULL)
			{
				fprintf(CFG.output, "Error allocating memory.\n");
				exit(1);
			}
			inFileListEnd               = inFileListEnd -> next;
		}

		if (strcmp(token, "-")         == 0)
		{
			if (CFG.stdin_in           != 1)
			{
				CFG.stdin_in            = 1;
				inFileListEnd -> input  = stdin;
				inFileListEnd -> name   = CFG.label;
			}
		}
		else
		{
			inFileListEnd     -> input  = fopen(token, "rb");

			if (inFileListEnd -> input == NULL)
			{
				fprintf(CFG.output, "Error opening file '%s'.\n", token);
				exit(1);
			}

			inFileListEnd     -> name   = (char *)   malloc (strlen(token)+1);
			if (inFileListEnd -> name  == NULL)
			{
				fprintf(CFG.output, "Error allocating memory.\n");
				exit(1);
			}

			inFileListEnd     -> name   = strcpy(inFileListEnd -> name, token);
		}

		inFileListEnd         -> next   = NULL;
		token                           = strtok_r(optarg, delim, &save);
	}
}

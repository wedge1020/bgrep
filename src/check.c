#include "bgrep.h"

void check_patterns(int argc, char **argv)
{
	uc       a;
	uc       b;
	ulli     addr                       = 0;
	ulli     matchaddr                  = 0;
	int      flag                       = 0;
	int      i                          = 0;
	int      j                          = 0;
	usi     *data                       = NULL;
	usi      length                     = 0;
	uc      *match                      = NULL;
	Pattern *tmp                        = patternList;
	while (tmp                         != NULL)
	{
		if ((tmp -> len % 2)           == 1)
		{
			fprintf(stderr, "Error: Malformed PATTERN '%s'\n", tmp -> regex);
			exit(1);
		}

		tmp                             = tmp -> next;
	}

	length = length / 2;

	data   = (usi *) malloc (sizeof(usi) * length);
	match  = (uc *)  malloc (sizeof(uc)  * length);

	j = 0;

	for (i = 0; i < strlen(*(argv+1)); i+=2)
	{
		flag = 0;
		a    = 0;
		a    = *(*(argv+1)+i);
		if ((a >= '0') && (a <= '9'))
			a = a - 48;
		else if ((a >= 'a') && (a <= 'f'))
			a = a - 87;
		else if ((a >= 'A') && (a <= 'F'))
			a = a - 55;
		else if (a == '.')
		{
			flag = 1;
			a    = 0;
		}
		else
		{
			fprintf(stderr, "Error: Invalid pattern symbol '%c' (%hhu)\n", a, a);
			exit(1);
		}

		a = a << 4;
		a = a & 0x00F0;
		
		b = 0;
		b = *(*(argv+(1))+(i+1));
		if ((b >= '0') && (b <= '9'))
			b = b - 48;
		else if ((b >= 'a') && (b <= 'f'))
			b = b - 87;
		else if ((b >= 'A') && (b <= 'F'))
			b = b - 55;
		else if (b == '.')
		{
			flag = flag | 2;
			b    = 0;
		}
		else
		{
			fprintf(stderr, "Error: Invalid pattern symbol '%c' (%hhu)\n", b, b);
			exit(1);
		}

		b = b & 0x000F;

//		printf("a: %hhx, b: %hhx, flag: %d, j: %d\n", a, b, flag, j);

		*(data+j) = 0;
		if (flag == 0)
			*(data+j) = (a | b) & (0x00FF);
		else if (flag == 1)
			*(data+j) = 0xF000 | a | b;
		else if (flag == 2)
			*(data+j) = 0x0F00 | a | b;
		else
			*(data+j) = 0xFF00 | a | b;

		j = j + 1;
		if (*(*(argv+1)+(i+2)) == 0x20)
			i++;
	}

/*
	for (j = 0; j < length; j++)
	{
		fprintf(stdout, "data[%d]: %hx\n", j, *(data+j));
	}
	fprintf(stdout, "length: %d\n", length);
	*/

	i = 0;
	while(!feof(stdin))
	{
		a = fgetc(stdin);
//		fprintf(stdout, "a is %hhx\n", a);
		if (a == *(data+i))	
		{
//			fprintf(stdout, "matching \n");
			*(match+i) = a & 0xFF;
			i = i + 1;	
			flag = flag + 1;
		}
		else if ((*(data+i) & 0xFF00) == 0xF000)
		{
			if ((*(data+i) & 0x000F) == (a & 0x000F))
			{
				*(match+i) = a & 0xFF;
				i          = i + 1;	
				flag       = flag + 1;
			}
			else
			{
			//	printf("unmatch1\n");
				i          = 0;
				flag       = 0;
			}
		}
		else if ((*(data+i) & 0xFF00) == 0x0F00)
		{
			if ((*(data+i) & 0x00F0) == (a & 0x00F0))
			{
				*(match+i) = a & 0xFF;
				i          = i + 1;	
				flag       = flag + 1;
			}
			else
			{
			//	printf("unmatch2\n");
				i    = 0;
				flag = 0;
			}
		}
		else if ((*(data+i) & 0xFF00) == 0xFF00)
		{
			*(match+i) = a & 0xFF;
			i = i + 1;	
			flag = 1;
		}
		else
		{
//			printf("unmatch3 at pos %d (%hhx:%hhx)\n", i, a, *(data+i));
			i = 0;
			flag = 0;
		}

		if (flag == 1)
		{
//			if (matchaddr == (0-1))
				matchaddr = addr;
		}

		if (i == length)
		{
			fprintf(stdout, "%lx:", matchaddr-(length-1));
			for (j = 0; j < length; j++)
				fprintf(stdout, "%.2hhx ", (*(match+j) & 0x00FF));
			fprintf(stdout, "\n");
			i = 0;
			matchaddr = 0 - 1;
		}

		addr = addr + 1;
	}
}

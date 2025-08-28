#include <getopt.h>
#include "bgrep.h"

#define ARG_YES		2
#define ARG_MAYBE	1
#define ARG_NO   	0

void args(int argc, char **argv)
{
	int            c                             = 0;
	int            digit_optind                  = 0;
	int            this_option_optind            = 0;
	int            option_index                  = 0;
	char          *save                          = NULL;
	char          *token                         = NULL;

	const  char   *delim                         = {           "," };

	struct option  long_options[]                = {
		{ "help",                   ARG_NO,    NULL, 1   },
		{ "version",                ARG_NO,    NULL, 'V' },
		{ "regexp",                 ARG_YES,   NULL, 'e' },
		{ "file",                   ARG_YES,   NULL, 'f' },
		{ "count",                  ARG_NO,    NULL, 'c' },
		{ "files-without-match",    ARG_NO,    NULL, 'L' },
		{ "files-with-matches",     ARG_NO,    NULL, 'l' },
		{ "max-count",              ARG_YES,   NULL, 'm' },
		{ "quiet",                  ARG_NO,    NULL, 'q' },
		{ "byte-offset",            ARG_NO,    NULL, 'b' },
		{ "no-byte-offset",         ARG_NO,    NULL, 2   },
		{ "with-filename",          ARG_NO,    NULL, 'H' },
		{ "no-filename",            ARG_NO,    NULL, 'h' },
		{ "label",                  ARG_YES,   NULL, 3   },
		{ "first-byte",             ARG_NO,    NULL, 4   },
		{ "first-byte-with-prefix", ARG_NO,    NULL, 5   },
		{ "last-byte",              ARG_NO,    NULL, 6   },
		{ "last-byte-with-suffix",  ARG_NO,    NULL, 7   },
		{ "auto-mode",              ARG_NO,    NULL, 8   },
		{ "hex-mode",               ARG_NO,    NULL, 9   },
		{ "oct-mode",               ARG_NO,    NULL, 10   },
		{ "hex-prefix",             ARG_NO,    NULL, 11  },
		{ "no-hex-prefix",          ARG_NO,    NULL, 12  },
		{ "hex-suffix",             ARG_NO,    NULL, 13  },
		{ "no-hex-suffix",          ARG_NO,    NULL, 14  },
		{ "pad-output",             ARG_YES,   NULL, 'p' },
		{ "no-pad-output",          ARG_NO,    NULL, 15  },
		{ "ascii",                  ARG_NO,    NULL, 16  },
		{ "no-ascii",               ARG_NO,    NULL, 17  },
		{ "prefix-num-bytes",       ARG_YES,   NULL, 'P' },
		{ "suffix-num-bytes",       ARG_YES,   NULL, 'S' },
		{ "input-byte",             ARG_YES,   NULL, 18  },
		{ "output-byte",            ARG_YES,   NULL, 19  },
		{ "little-endian-group",    ARG_NO,    NULL, 20  },
		{ "big-endian-group",       ARG_NO,    NULL, 21  },
		{ "byte-reverse-group",     ARG_NO,    NULL, 22  },
//		{ "bit-reverse-group",      ARG_NO,    NULL, 23  },
		{ "invert-byte",            ARG_YES,   NULL, 'I' },
		{ "starting-offset",        ARG_NO,    NULL, 's' },
		{ 0,                        0,         NULL, 0   }
	};

	c = getopt_long(argc,argv,"Ve:f:cLlm:qbHhp:P:S:I:s",long_options,&option_index);
	while (c                                    != -1)
	{
		switch(c)
		{
			case 1:	// help
				display_help(argv[0]);
				break;

			case 'V':
				display_version(argv[0]);
				break;

			case 'e':
				token                            = strtok_r(optarg, delim, &save);
				while (token                    != NULL)
				{
					if (patternList	            == NULL)
					{
						patternList              = (Pattern *) malloc (sizeof(Pattern));
						if (patternList         == NULL)
						{
							fprintf(CFG.output, "Error allocating memory.\n");
							exit(1);
						}
						patternList    -> next   = NULL;
						patternListEnd           = patternList;
					}
					else
					{
						patternListEnd -> next   = (Pattern *) malloc (sizeof(Pattern));
						if(patternListEnd->next == NULL)
						{
							fprintf(CFG.output, "Error allocating memory.\n");
							exit(1);
						}
						patternListEnd           = patternListEnd -> next;
						patternListEnd -> next   = NULL;
					}

					patternListEnd     -> regex  = (unsigned char *) malloc (strlen(token)+1);
					if (patternListEnd -> regex == NULL)
					{
						fprintf(CFG.output, "Error allocating memory.\n");
						exit(1);
					}
					patternListEnd     -> regex  = (unsigned char *) memcpy (patternList -> regex, token, strlen(token)+1);
					patternListEnd     -> len    = strlen(token);
					token                        = strtok_r(optarg, delim, &save);
				}
				break;

			case 'f':
				load_file(optarg);
				break;

			case 'c':
				CFG.flags                        = CFG.flags |  COUNT;
				break;

			case 'L':
				CFG.flags                        = CFG.flags |  FILES_MATCH;
				break;

			case 'l':
				CFG.flags                        = CFG.flags & ~FILES_MATCH;
				break;

			case 'm':
				CFG.max_count                    = strtoull(optarg, NULL, CFG.base);
				break;

			case 'q':
				CFG.output                       = fopen("/dev/null", "wb");
				if (CFG.output                  == NULL)
				{
					fprintf(CFG.output, "Error opening /dev/null.\n");
					exit(1);
				}
				CFG.flags                        = CFG.flags |  QUIET;
				break;

			case 'b':
				CFG.flags                        = CFG.flags |  OFFSET;
				break;

			case 2:
				CFG.flags                        = CFG.flags & ~OFFSET;
				break;

			case 'H':
				CFG.flags                        = CFG.flags |  FILENAME;
				break;

			case 'h':
				CFG.flags                        = CFG.flags & ~FILENAME;
				break;

			case 3: //label
				CFG.label                        = (uc *) malloc (strlen(optarg)+1);
				if (CFG.label                   == NULL)
				{
					fprintf(CFG.output, "Error allocating memory.\n");
					exit(1);
				}
				CFG.label                        = strcpy(CFG.label, optarg);
				break;

			case 4: //first byte
				CFG.flags                        = CFG.flags & ~FIRSTPREFIX;
				CFG.flags                        = CFG.flags & ~LASTSUFFIX;
				CFG.flags                        = CFG.flags |  SHOWSTARTOFFSET;
				break;

			case 5: //first byte with prefix
				CFG.flags                        = CFG.flags |  FIRSTPREFIX;
				CFG.flags                        = CFG.flags & ~LASTSUFFIX;
				CFG.flags                        = CFG.flags |  SHOWSTARTOFFSET;
				break;

			case 6: //last byte
				CFG.flags                        = CFG.flags & ~FIRSTPREFIX;
				CFG.flags                        = CFG.flags & ~LASTSUFFIX;
				CFG.flags                        = CFG.flags & ~SHOWSTARTOFFSET;
				break;
			case 7: //last byte with suffix
				CFG.flags                        = CFG.flags & ~FIRSTPREFIX;
				CFG.flags                        = CFG.flags |  LASTSUFFIX;
				CFG.flags                        = CFG.flags & ~SHOWSTARTOFFSET;
				break;

			case 8:	// auto input mode (dec by default, hex/octal with prefix)
				CFG.flags                        = CFG.flags & ~SET_BASE;
				CFG.base                         = 0;
				break;

			case 9:	// hex input mode
				CFG.flags                        = CFG.flags |  SET_BASE;
				CFG.base                         = 16;
				break;

			case 10:// oct input mode
				CFG.flags                        = CFG.flags |  SET_BASE;
				CFG.base                         = 8;
				break;

			case 11://hex prefix
				CFG.flags                        = CFG.flags |  HEXLEAD;
				break;

			case 12://no hex prefix
				CFG.flags                        = CFG.flags & ~HEXLEAD;
				break;

			case 13://hex prefix
				CFG.flags                        = CFG.flags |  HEXTAIL;
				break;

			case 14://no hex prefix
				CFG.flags                        = CFG.flags & ~HEXTAIL;
				break;

			case 'p':
				CFG.flags                        = CFG.flags |  PADOFFSET;
				CFG.pad_value                    = (uc) strtoull (optarg, NULL, CFG.base);
				break;

			case 15:// no pad offset
				CFG.flags                        = CFG.flags & ~PADOFFSET;
				CFG.pad_value                    = 0;
				break;

			case 16://ascii output
				CFG.flags                        = CFG.flags |  ASCII;
				break;
				
			case 17://no ascii output
				CFG.flags                        = CFG.flags & ~ASCII;
				break;
				
			case 'O':
				if (*(optarg+0)                 == '+')
					CFG.off_by                   = +1*(sc)strtoull((optarg+1), NULL, CFG.base);
				else if (*(optarg+0)            == '-')
					CFG.off_by                   = -1*(sc)strtoull((optarg+1), NULL, CFG.base);
				else
					CFG.off_by                   = -1*(sc)strtoull  (optarg,    NULL, CFG.base);
				break;

			case 'P':
				CFG.prefix_bytes                 = atoll(optarg);
				break;

			case 'S':
				CFG.suffix_bytes                 = atoll(optarg);
				break;

			case 18://input byte size
				CFG.input_byte_size              = (uc) strtoull (optarg, NULL, CFG.base);
				break;

			case 19://output byte size
				CFG.output_byte_size             = (uc) strtoull (optarg, NULL, CFG.base);
				break;

			case 20://little endian grouping
				CFG.flags                        = CFG.flags & ~BIGENDIAN;
				break;

			case 21://big endian grouping
				CFG.flags                        = CFG.flags |  BIGENDIAN;
				break;

			case 22://byte reverse group
				CFG.flags                        = CFG.flags |  REVERSE;
				break;
				
			case 'I':
				CFG.flags                        = CFG.flags |  INVERT;
				// need to capture specified bytes to invert
				token                            = strtok_r(optarg, delim, &save);
				while (token                    != NULL)
				{
					if (invByteList	            == NULL)
					{
						invByteList              = (InvertByte *) malloc (sizeof(InvertByte));
						if (invByteList         == NULL)
						{
							fprintf(CFG.output, "Error allocating memory.\n");
							exit(1);
						}
						invByteList    -> num    = strtoull (optarg, NULL, CFG.base);
						invByteList    -> next   = NULL;
						invByteListEnd           = invByteList;
					}
					else
					{
						invByteListEnd -> next   = (InvertByte *) malloc (sizeof(InvertByte));
						if(invByteListEnd       == NULL)
						{
							fprintf(CFG.output, "Error allocating memory.\n");
							exit(1);
						}
						invByteListEnd           = invByteListEnd -> next;
						invByteListEnd -> num    = strtoull (optarg, NULL, CFG.base);
						invByteListEnd -> next   = NULL;
					}

					token                        = strtok_r(optarg, delim, &save);
				}
				break;

			case 's':
				CFG.offset                       = strtoull (optarg, NULL, CFG.base);
				break;

			default:
				display_usage(argv[0]);
		}
		c = getopt_long(argc,argv,"Ve:f:cLlm:qbHhp:P:S:I:s",long_options,&option_index);
	}

	if (optind                                  <  argc)
	{
		token                                    = strtok_r(argv[optind], delim, &save);
		while (token                            != NULL)
		{
			if (patternList	                    == NULL)
			{
				patternList                      = (Pattern *) malloc (sizeof(Pattern));
				if (patternList                 == NULL)
				{
					fprintf(CFG.output, "Error allocating memory.\n");
					exit(1);
				}
				patternList            -> next   = NULL;
				patternListEnd                   = patternList;
			}
			else
			{
				patternListEnd         -> next   = (Pattern *) malloc (sizeof(Pattern));
				if (patternListEnd     -> next  == NULL)
				{
					fprintf(CFG.output, "Error allocating memory.\n");
					exit(1);
				}
				patternListEnd                   = patternListEnd -> next;
				patternListEnd         -> next   = NULL;
			}

			patternListEnd             -> regex  = (uc *)malloc(strlen(token)+1);
			if (patternListEnd         -> regex == NULL)
			{
				fprintf(CFG.output, "Error allocating memory.\n");
				exit(1);
			}
			patternListEnd             -> regex  = (uc *)memcpy(patternList -> regex, token, strlen(token)+1);
			optind                               = optind + 1;
			token                                = strtok_r(argv[optind], delim, &save);
		}

		if (optind                              >= argc) // no file, assume STDIN if NULL file list
		{
			load_file("-");
			optind                               = optind + 1;
		}

		while (optind                           <  argc)
		{
			load_file(argv[optind]);
			optind                               = optind + 1;
		}
	}
	else
	{
		display_usage(argv[0]);
	}
}

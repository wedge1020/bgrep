#ifndef _BGREP_H
#define _BGREP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define  BGREP_AUTHOR           "Matthew Haas"
#define  BGREP_COPYRIGHT        "2015"
#define  BGREP_EMAIL            "wedge1020+bgrep@gmail.com"
#define  BGREP_URL              "http://lab46.corning-cc.edu/projects/bgrep"
#define  BGREP_VERSION          "0.2"

#define  QUIET                  1
#define  SHOWSTARTOFFSET        2
#define  COUNT                  4
#define  FILENAME               8
#define  OFFSET                 16
#define  ASCII                  32
#define  HEXLEAD                64
#define  HEXTAIL                128
#define  PADOFFSET              256
#define  INVERT                 512
#define  BIGENDIAN              1024
#define  REVERSE                2048
#define  FIRSTPREFIX            4096
#define  LASTSUFFIX             8192
#define  FILES_MATCH            16384
#define  SET_BASE               32768

typedef signed char             sc;
typedef unsigned char           uc;
typedef unsigned short int      usi;
typedef unsigned long long int  ulli;

struct infile  {
	FILE                       *input;
	char                       *name;
	struct infile              *next;
};
typedef struct infile           Infile;

Infile                         *inFileList;
Infile                         *inFileListEnd;

struct invert  {
	ulli                        num;
	struct invert              *next;
};
typedef struct invert           InvertByte;

InvertByte                     *invByteList;
InvertByte                     *invByteListEnd;

struct pattern {
	uc                         *regex;
	usi                         len;
	struct pattern             *next;
};
typedef struct pattern          Pattern;

Pattern                        *patternList;
Pattern                        *patternListEnd;

struct settings {
	FILE                       *output;				// where to send output
	sc                          stdin_in;           // are we reading from stdin at all?
	sc                          off_by;
	uc                          pad_value;
	uc                         *label;
	uc                          base;
	uc                          input_byte_size;
	uc                          output_byte_size;
	ulli                        offset;				// starting offset
	ulli                        prefix_bytes;		// additional bytes to include prior to offset
	ulli                        suffix_bytes;		// additional bytes to include after offset
	ulli                        max_count;			// maximum matches to display (0 for unbounded)
	ulli                        num_input;
	usi                         flags;				// operational modes
};
typedef struct settings         config;

config                          CFG;

void args(int, char **);
void check_patterns(int, char **);
void display_help(char *);
void display_usage(char *);
void display_version(char *);
void init(void);
void load_file(char *);

#endif

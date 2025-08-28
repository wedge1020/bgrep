#include "bgrep.h"

void init()
{
	CFG.output            = stdout;
	CFG.off_by            = 0;
	CFG.stdin_in          = 0;
	CFG.pad_value         = 0;
	CFG.label             = (uc *) malloc (strlen("(stdin)")+1);
	CFG.label             = (uc *) strcpy (CFG.label, "(stdin)");
	CFG.base              = 0;
	CFG.input_byte_size   = 8;
	CFG.output_byte_size  = 8;
	CFG.offset            = 0;
	CFG.prefix_bytes      = 0;
	CFG.suffix_bytes      = 0;
	CFG.max_count         = 0;
	CFG.num_input         = 0;
	CFG.flags             = SHOWSTARTOFFSET | BIGENDIAN | OFFSET;

	inFileList            = NULL;
	inFileListEnd         = NULL;

	invByteList           = NULL;
	invByteListEnd        = NULL;

	patternList           = NULL;
	patternListEnd        = NULL;
}

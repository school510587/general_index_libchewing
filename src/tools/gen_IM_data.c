#include <dirent.h> /* For chdir(). */
#include <libgen.h> /* For basename(). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "build_tool.h"

#define CIN_EXTENSION ".cin"

int main(int argc, char *argv[])
{
	int cin_path_id;

	for(cin_path_id = 1; cin_path_id < argc; cin_path_id++){
		size_t l = strlen( argv[cin_path_id] );
		if(l>4 && !strcmp( &argv[cin_path_id][l-4], CIN_EXTENSION) ) break;
	}

	if( cin_path_id >= argc) {
		fprintf(stderr, "Usage: %s <cin_filename>\n", argv[0]);
		exit(-1);
	}

	read_IM_cin( argv[cin_path_id] );

	/* Go to data/, where the exe is. */
	chdir(dirname( argv[0] ));

	return 0;
}

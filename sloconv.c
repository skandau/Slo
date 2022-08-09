/*

Command line tool to convert between png <> SLO format

Requires:
	-"stb_image.h" (https://github.com/nothings/stb/blob/master/stb_image.h)
	-"stb_image_write.h" (https://github.com/nothings/stb/blob/master/stb_image_write.h)
	-"SLO.h" (https://github.com/phoboslab/SLO/blob/master/SLO.h)

Compile with: 
	gcc SLOconv.c -std=c99 -O3 -o SLOconv

-- LICENSE: GPL License

Based on QOI Copyright(c) 2021 Dominic Szablewski
SLO release 2022 surya kandau

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details at
    Visit <http://www.gnu.org/copyleft/gpl.html>.

*/


#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define SLO_IMPLEMENTATION
#include "SLO.h"


#define STR_ENDS_WITH(S, E) (strcmp(S + strlen(S) - (sizeof(E)-1), E) == 0)

int main(int argc, char **argv) {
	if (argc < 3) {
		puts("Usage: SLOconv <infile> <outfile>");
		puts("Examples:");
		puts("  SLOconv input.png output.SLO");
		puts("  SLOconv input.SLO output.png");
		exit(1);
	}

	void *pixels = NULL;
	int w, h, channels;
	if (STR_ENDS_WITH(argv[1], ".png")) {
		if(!stbi_info(argv[1], &w, &h, &channels)) {
			printf("Couldn't read header %s\n", argv[1]);
			exit(1);
		}

		// Force all odd encodings to be RGBA
		if(channels != 3) {
			channels = 4;
		}

		pixels = (void *)stbi_load(argv[1], &w, &h, NULL, channels);
	}
	else if (STR_ENDS_WITH(argv[1], ".slo")) {
		SLO_desc desc;
		pixels = SLO_read(argv[1], &desc, 0);
		channels = desc.channels;
		w = desc.width;
		h = desc.height;
	}

	if (pixels == NULL) {
		printf("Couldn't load/decode %s\n", argv[1]);
		exit(1);
	}

	int encoded = 0;
	if (STR_ENDS_WITH(argv[2], ".png")) {
		encoded = stbi_write_png(argv[2], w, h, channels, pixels, 0);
	}
	else if (STR_ENDS_WITH(argv[2], ".slo")) {
		encoded = SLO_write(argv[2], pixels, &(SLO_desc){
			.width = w,
			.height = h, 
			.channels = channels,
			.colorspace = SLO_SRGB
		});
	}

	if (!encoded) {
		printf("Couldn't write/encode %s\n", argv[2]);
		exit(1);
	}

	free(pixels);
	return 0;
}

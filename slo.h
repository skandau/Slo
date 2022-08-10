/*

SLO - The "Still Looks OK" format for fast, lossy image compression


-- LICENSE: MIT License

Based on QOI Copyright(c) 2021 Dominic Szablewski
SLO release 2022 surya kandau

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files(the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and / or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions :
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.


-- About

SLO encodes and decodes images in a lossy format. Compared to stb_image and
stb_image_write SLO offers 20x-50x faster encoding, 3x-4x faster decoding and
20% better compression.


-- Synopsis

// Define `SLO_IMPLEMENTATION` in *one* C/C++ file before including this
// library to create the implementation.

#define SLO_IMPLEMENTATION
#include "SLO.h"

// Encode and store an RGBA buffer to the file system. The SLO_desc describes
// the input pixel data.
SLO_write("image_new.SLO", rgba_pixels, &(SLO_desc){
	.width = 1920,
	.height = 1080,
	.channels = 4,
	.colorspace = SLO_SRGB
});

// Load and decode a SLO image from the file system into a 32bbp RGBA buffer.
// The SLO_desc struct will be filled with the width, height, number of channels
// and colorspace read from the file header.
SLO_desc desc;
void *rgba_pixels = SLO_read("image.SLO", &desc, 4);



-- Documentation

This library provides the following functions;
- SLO_read    -- read and decode a SLO file
- SLO_decode  -- decode the raw bytes of a SLO image from memory
- SLO_write   -- encode and write a SLO file
- SLO_encode  -- encode an rgba buffer into a SLO image in memory

See the function declaration below for the signature and more information.

If you don't want/need the SLO_read and SLO_write functions, you can define
SLO_NO_STDIO before including this library.

This library uses malloc() and free(). To supply your own malloc implementation
you can define SLO_MALLOC and SLO_FREE before including this library.

This library uses memset() to zero-initialize the index. To supply your own
implementation you can define SLO_ZEROARR before including this library.


-- Data Format

A SLO file has a 14 byte header, followed by any number of data "chunks" and an
8-byte end marker.

struct SLO_header_t {
	char     magic[4];   // magic bytes "SLOf"
	uint32_t width;      // image width in pixels (BE)
	uint32_t height;     // image height in pixels (BE)
	uint8_t  channels;   // 3 = RGB, 4 = RGBA
	uint8_t  colorspace; // 0 = sRGB with linear alpha, 1 = all channels linear
};

Images are encoded row by row, left to right, top to bottom. The decoder and
encoder start with {r: 0, g: 0, b: 0, a: 255} as the previous pixel value. An
image is complete when all pixels specified by width * height have been covered.

Pixels are encoded as
 - a run of the previous pixel
 - an index into an array of previously seen pixels
 - a difference to the previous pixel value in r,g,b
 - full r,g,b or r,g,b,a values

The color channels are assumed to not be premultiplied with the alpha channel
("un-premultiplied alpha").

A running array[64] (zero-initialized) of previously seen pixel values is
maintained by the encoder and decoder. Each pixel that is seen by the encoder
and decoder is put into this array at the position formed by a hash function of
the color value. In the encoder, if the pixel value at the index matches the
current pixel, this index position is written to the stream as SLO_OP_INDEX.
The hash function for the index is:

	index_position = (r * 3 + g * 5 + b * 7 + a * 11) % 64

Each chunk starts with a 2- or 8-bit tag, followed by a number of data bits. The
bit length of chunks is divisible by 8 - i.e. all chunks are byte aligned. All
values encoded in these data bits have the most significant bit on the left.

The 8-bit tags have precedence over the 2-bit tags. A decoder must check for the
presence of an 8-bit tag first.

The byte stream's end is marked with 7 0x00 bytes followed a single 0x01 byte.


The possible chunks are:


.- SLO_OP_INDEX ----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  0  0 |     index       |
`-------------------------`
2-bit tag b00
6-bit index into the color index array: 0..63

A valid encoder must not issue 2 or more consecutive SLO_OP_INDEX chunks to the
same index. SLO_OP_RUN should be used instead.


.- SLO_OP_DIFF -----------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----+-----+-----|
|  0  1 |  dr |  dg |  db |
`-------------------------`
2-bit tag b01
2-bit   red channel difference from the previous pixel between -2..1
2-bit green channel difference from the previous pixel between -2..1
2-bit  blue channel difference from the previous pixel between -2..1

The difference to the current channel values are using a wraparound operation,
so "1 - 2" will result in 255, while "255 + 1" will result in 0.

Values are stored as unsigned integers with a bias of 2. E.g. -2 is stored as
0 (b00). 1 is stored as 3 (b11).

The alpha value remains unchanged from the previous pixel.


.- SLO_OP_LUMA -------------------------------------.
|         Byte[0]         |         Byte[1]         |
|  7  6  5  4  3  2  1  0 |  7  6  5  4  3  2  1  0 |
|-------+-----------------+-------------+-----------|
|  1  0 |  green diff     |   dr - dg   |  db - dg  |
`---------------------------------------------------`
2-bit tag b10
6-bit green channel difference from the previous pixel -32..31
4-bit   red channel difference minus green channel difference -8..7
4-bit  blue channel difference minus green channel difference -8..7

The green channel is used to indicate the general direction of change and is
encoded in 6 bits. The red and blue channels (dr and db) base their diffs off
of the green channel difference and are encoded in 4 bits. I.e.:
	dr_dg = (cur_px.r - prev_px.r) - (cur_px.g - prev_px.g)
	db_dg = (cur_px.b - prev_px.b) - (cur_px.g - prev_px.g)

The difference to the current channel values are using a wraparound operation,
so "10 - 13" will result in 253, while "250 + 7" will result in 1.

Values are stored as unsigned integers with a bias of 32 for the green channel
and a bias of 8 for the red and blue channel.

The alpha value remains unchanged from the previous pixel.


.- SLO_OP_RUN ------------.
|         Byte[0]         |
|  7  6  5  4  3  2  1  0 |
|-------+-----------------|
|  1  1 |       run       |
`-------------------------`
2-bit tag b11
6-bit run-length repeating the previous pixel: 1..62

The run-length is stored with a bias of -1. Note that the run-lengths 63 and 64
(b111110 and b111111) are illegal as they are occupied by the SLO_OP_RGB and
SLO_OP_RGBA tags.


.- SLO_OP_RGB ------------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------|
|  1  1  1  1  1  1  1  0 |   red   |  green  |  blue   |
`-------------------------------------------------------`
8-bit tag b11111110
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value

The alpha value remains unchanged from the previous pixel.


.- SLO_OP_RGBA ---------------------------------------------------.
|         Byte[0]         | Byte[1] | Byte[2] | Byte[3] | Byte[4] |
|  7  6  5  4  3  2  1  0 | 7 .. 0  | 7 .. 0  | 7 .. 0  | 7 .. 0  |
|-------------------------+---------+---------+---------+---------|
|  1  1  1  1  1  1  1  1 |   red   |  green  |  blue   |  alpha  |
`-----------------------------------------------------------------`
8-bit tag b11111111
8-bit   red channel value
8-bit green channel value
8-bit  blue channel value
8-bit alpha channel value

*/


/* -----------------------------------------------------------------------------
Header - Public functions */

#ifndef SLO_H
#define SLO_H

#ifdef __cplusplus
extern "C" {
#endif

/* A pointer to a SLO_desc struct has to be supplied to all of SLO's functions.
It describes either the input format (for SLO_write and SLO_encode), or is
filled with the description read from the file header (for SLO_read and
SLO_decode).

The colorspace in this SLO_desc is an enum where
	0 = sRGB, i.e. gamma scaled RGB channels and a linear alpha channel
	1 = all channels are linear
You may use the constants SLO_SRGB or SLO_LINEAR. The colorspace is purely
informative. It will be saved to the file header, but does not affect
how chunks are en-/decoded. */

#define SLO_SRGB   0
#define SLO_LINEAR 1

typedef struct {
	unsigned int width;
	unsigned int height;
	unsigned char channels;
	unsigned char colorspace;
} SLO_desc;

#ifndef SLO_NO_STDIO

/* Encode raw RGB or RGBA pixels into a SLO image and write it to the file
system. The SLO_desc struct must be filled with the image width, height,
number of channels (3 = RGB, 4 = RGBA) and the colorspace.

The function returns 0 on failure (invalid parameters, or fopen or malloc
failed) or the number of bytes written on success. */

int SLO_write(const char *filename, const void *data, const SLO_desc *desc);


/* Read and decode a SLO image from the file system. If channels is 0, the
number of channels from the file header is used. If channels is 3 or 4 the
output format will be forced into this number of channels.

The function either returns NULL on failure (invalid data, or malloc or fopen
failed) or a pointer to the decoded pixels. On success, the SLO_desc struct
will be filled with the description from the file header.

The returned pixel data should be free()d after use. */

void *SLO_read(const char *filename, SLO_desc *desc, int channels);

#endif /* SLO_NO_STDIO */


/* Encode raw RGB or RGBA pixels into a SLO image in memory.

The function either returns NULL on failure (invalid parameters or malloc
failed) or a pointer to the encoded data on success. On success the out_len
is set to the size in bytes of the encoded data.

The returned SLO data should be free()d after use. */

void *SLO_encode(const void *data, const SLO_desc *desc, int *out_len);


/* Decode a SLO image from memory.

The function either returns NULL on failure (invalid parameters or malloc
failed) or a pointer to the decoded pixels. On success, the SLO_desc struct
is filled with the description from the file header.

The returned pixel data should be free()d after use. */

void *SLO_decode(const void *data, int size, SLO_desc *desc, int channels);


#ifdef __cplusplus
}
#endif
#endif /* SLO_H */


/* -----------------------------------------------------------------------------
Implementation */

#ifdef SLO_IMPLEMENTATION
#include <stdlib.h>
#include <string.h>

#ifndef SLO_MALLOC
	#define SLO_MALLOC(sz) malloc(sz)
	#define SLO_FREE(p)    free(p)
#endif
#ifndef SLO_ZEROARR
	#define SLO_ZEROARR(a) memset((a),0,sizeof(a))
#endif

#define SLO_OP_INDEX  0x00 /* 00xxxxxx */
#define SLO_OP_DIFF   0x40 /* 01xxxxxx */
#define SLO_OP_LUMA   0x80 /* 10xxxxxx */
#define SLO_OP_RUN    0xc0 /* 11xxxxxx */
#define SLO_OP_RGB    0xfe /* 11111110 */
#define SLO_OP_RGBA   0xff /* 11111111 */

#define SLO_MASK_2    0xc0 /* 11000000 */

#define SLO_COLOR_HASH(C) (C.rgba.r*3 + C.rgba.g*5 + C.rgba.b*7 + C.rgba.a*11)
#define SLO_MAGIC \
	(((unsigned int)'s') << 24 | ((unsigned int)'l') << 16 | \
	 ((unsigned int)'o') <<  8 | ((unsigned int)'f'))
#define SLO_HEADER_SIZE 14

/* 2GB is the max file size that this implementation can safely handle. We guard
against anything larger than that, assuming the worst case with 5 bytes per
pixel, rounded down to a nice clean value. 400 million pixels ought to be
enough for anybody. */
#define SLO_PIXELS_MAX ((unsigned int)400000000)

typedef union {
	struct { unsigned char r, g, b, a; } rgba;
	unsigned int v;
} SLO_rgba_t;

static const unsigned char SLO_padding[8] = {0,0,0,0,0,0,0,1};

static void SLO_write_32(unsigned char *bytes, int *p, unsigned int v) {
	bytes[(*p)++] = (0xff000000 & v) >> 24;
	bytes[(*p)++] = (0x00ff0000 & v) >> 16;
	bytes[(*p)++] = (0x0000ff00 & v) >> 8;
	bytes[(*p)++] = (0x000000ff & v);
}

static unsigned int SLO_read_32(const unsigned char *bytes, int *p) {
	unsigned int a = bytes[(*p)++];
	unsigned int b = bytes[(*p)++];
	unsigned int c = bytes[(*p)++];
	unsigned int d = bytes[(*p)++];
	return a << 24 | b << 16 | c << 8 | d;
}

void *SLO_encode(const void *data, const SLO_desc *desc, int *out_len) {
	int i, max_size, p, run;
	int px_len, px_end, px_pos, channels;
	unsigned char *bytes;
	const unsigned char *pixels;
	SLO_rgba_t index[64];
	SLO_rgba_t px, px_prev;

	if (
		data == NULL || out_len == NULL || desc == NULL ||
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		desc->height >= SLO_PIXELS_MAX / desc->width
	) {
		return NULL;
	}

	max_size =
		desc->width * desc->height * (desc->channels + 1) +
		SLO_HEADER_SIZE + sizeof(SLO_padding);

	p = 0;
	bytes = (unsigned char *) SLO_MALLOC(max_size);
	if (!bytes) {
		return NULL;
	}

	SLO_write_32(bytes, &p, SLO_MAGIC);
	SLO_write_32(bytes, &p, desc->width);
	SLO_write_32(bytes, &p, desc->height);
	bytes[p++] = desc->channels;
	bytes[p++] = desc->colorspace;


	pixels = (const unsigned char *)data;

	SLO_ZEROARR(index);

	run = 0;
	px_prev.rgba.r = 0;
	px_prev.rgba.g = 0;
	px_prev.rgba.b = 0;
	px_prev.rgba.a = 255;
	px = px_prev;

	px_len = desc->width * desc->height * desc->channels;
	px_end = px_len - desc->channels;
	channels = desc->channels;

	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		px.rgba.r = pixels[px_pos + 0]>>1;
		px.rgba.g = pixels[px_pos + 1]>>1;
		px.rgba.b = pixels[px_pos + 2]>>1;

		if (channels == 4) {
			px.rgba.a = pixels[px_pos + 3];
		}
        
		if (px.v == px_prev.v ) {
			run++;
			if (run == 62 || px_pos == px_end+1) {
				bytes[p++] = SLO_OP_RUN | (run - 1);
				run = 0;
			}
		}
		else {
			int index_pos;

			if (run > 1) {
				bytes[p++] = SLO_OP_RUN | (run - 1);
				run = 0;
			}

			index_pos = SLO_COLOR_HASH(px) % 64;

			if (index[index_pos].v == px.v || index[index_pos].rgba.a == px.rgba.a * 2 || index[index_pos].rgba.a == px_prev.rgba.a * 8)  {
					bytes[p++] = SLO_OP_INDEX | index_pos;
			}
			else {
				index[index_pos] = px;
				
				if (px.rgba.a == px_prev.rgba.a) {
					signed char vr = px.rgba.r - px_prev.rgba.r;
					signed char vg = px.rgba.g - px_prev.rgba.g;
					signed char vb = px.rgba.b - px_prev.rgba.b;

					signed char vg_r = vr - vg;
					signed char vg_b = vb - vg;

					if (
						vr > -3 && vr < 2 && 
						vg > -3 && vg < 2 &&						
						vb > -3 && vb < 2
					) {
						bytes[p++] = SLO_OP_DIFF | (vr + 2) << 4 | (vg + 2) << 2 | (vb + 2);
					}
					else if (
						vg_r >  -9 && vg_r <  8 &&
						vg   > -33 && vg   < 32 &&
						vg_b >  -9 && vg_b <  8
					) {
						bytes[p++] = SLO_OP_LUMA     | (vg   + 32);
						bytes[p++] = (vg_r + 8) << 4 | (vg_b +  8);
					}
					else {
						bytes[p++] = SLO_OP_RGB;
						bytes[p++] = px.rgba.r;
						bytes[p++] = px.rgba.g;
						bytes[p++] = px.rgba.b;
					}
				}
				else {
					bytes[p++] = SLO_OP_RGBA;
					bytes[p++] = px.rgba.r;
					bytes[p++] = px.rgba.g;
					bytes[p++] = px.rgba.b;
					bytes[p++] = px.rgba.a;
				}
			}
		}
		px_prev = px;
	}

	for (i = 0; i < (int)sizeof(SLO_padding); i++) {
		bytes[p++] = SLO_padding[i];
	}

	*out_len = p;
	return bytes;
}

void *SLO_decode(const void *data, int size, SLO_desc *desc, int channels) {
	const unsigned char *bytes;
	unsigned int header_magic;
	unsigned char *pixels;
	SLO_rgba_t index[64];
	SLO_rgba_t px;
	int px_len, chunks_len, px_pos;
	int p = 0, run = 0;

	if (
		data == NULL || desc == NULL ||
		(channels != 0 && channels != 3 && channels != 4) ||
		size < SLO_HEADER_SIZE + (int)sizeof(SLO_padding)
	) {
		return NULL;
	}

	bytes = (const unsigned char *)data;

	header_magic = SLO_read_32(bytes, &p);
	desc->width = SLO_read_32(bytes, &p);
	desc->height = SLO_read_32(bytes, &p);
	desc->channels = bytes[p++];
	desc->colorspace = bytes[p++];

	if (
		desc->width == 0 || desc->height == 0 ||
		desc->channels < 3 || desc->channels > 4 ||
		desc->colorspace > 1 ||
		header_magic != SLO_MAGIC ||
		desc->height >= SLO_PIXELS_MAX / desc->width
	) {
		return NULL;
	}

	if (channels == 0) {
		channels = desc->channels;
	}

	px_len = desc->width * desc->height * channels;
	pixels = (unsigned char *) SLO_MALLOC(px_len);
	if (!pixels) {
		return NULL;
	}

	SLO_ZEROARR(index);
	px.rgba.r = 0;
	px.rgba.g = 0;
	px.rgba.b = 0;
	px.rgba.a = 255;

	chunks_len = size - (int)sizeof(SLO_padding);
	for (px_pos = 0; px_pos < px_len; px_pos += channels) {
		if (run > 0) {
			run--;
		}
		else if (p < chunks_len) {
			int b1 = bytes[p++];

			if (b1 == SLO_OP_RGB) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
			}
			else if (b1 == SLO_OP_RGBA) {
				px.rgba.r = bytes[p++];
				px.rgba.g = bytes[p++];
				px.rgba.b = bytes[p++];
				px.rgba.a = bytes[p++];
			}
			else if ((b1 & SLO_MASK_2) == SLO_OP_INDEX) {
				px = index[b1];
			}
			else if ((b1 & SLO_MASK_2) == SLO_OP_DIFF) {
				px.rgba.r += ((b1 >> 4) & 0x03) - 2;
				px.rgba.g += ((b1 >> 2) & 0x03) - 2;
				px.rgba.b += ( b1       & 0x03) - 2;
			}
			else if ((b1 & SLO_MASK_2) == SLO_OP_LUMA) {
				int b2 = bytes[p++];
				int vg = (b1 & 0x3f) - 32;
				px.rgba.r += vg - 8 + ((b2 >> 4) & 0x0f);
				px.rgba.g += vg;
				px.rgba.b += vg - 8 +  (b2       & 0x0f);
			}
			else if ((b1 & SLO_MASK_2) == SLO_OP_RUN) {
				run = (b1 & 0x3f);
			}

			index[SLO_COLOR_HASH(px) % 64] = px;
		}

		pixels[px_pos + 0] = px.rgba.r<<1;
		pixels[px_pos + 1] = px.rgba.g<<1;
		pixels[px_pos + 2] = px.rgba.b<<1;
		
		if (channels == 4) {
			pixels[px_pos + 3] = px.rgba.a;
		}
	}

	return pixels;
}

#ifndef SLO_NO_STDIO
#include <stdio.h>

int SLO_write(const char *filename, const void *data, const SLO_desc *desc) {
	FILE *f = fopen(filename, "wb");
	int size;
	void *encoded;

	if (!f) {
		return 0;
	}

	encoded = SLO_encode(data, desc, &size);
	if (!encoded) {
		fclose(f);
		return 0;
	}

	fwrite(encoded, 1, size, f);
	fclose(f);

	SLO_FREE(encoded);
	return size;
}

void *SLO_read(const char *filename, SLO_desc *desc, int channels) {
	FILE *f = fopen(filename, "rb");
	int size, bytes_read;
	void *pixels, *data;

	if (!f) {
		return NULL;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	if (size <= 0) {
		fclose(f);
		return NULL;
	}
	fseek(f, 0, SEEK_SET);

	data = SLO_MALLOC(size);
	if (!data) {
		fclose(f);
		return NULL;
	}

	bytes_read = fread(data, 1, size, f);
	fclose(f);

	pixels = SLO_decode(data, bytes_read, desc, channels);
	SLO_FREE(data);
	return pixels;
}

#endif /* SLO_NO_STDIO */
#endif /* SLO_IMPLEMENTATION */

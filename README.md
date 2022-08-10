
# SLO - The “Still Looks OK” for fast, lossy image compression

Single-file MIT licensed library for C/C++

See [SLO.h](https://github.com/skandau/SLO.h) for
the documentation and format specification.



## Why?

Compared to stb_image and stb_image_write SLO offers 20x-50x faster encoding,
3x-4x faster decoding and 20% better compression. It's also stupidly simple and
fits in about 300 lines of C.


## Example Usage

- [SLOconv.c](https://github.com/skandau/SLOconv.c)
converts between png <> SLO


## Limitations

The SLO file format allows for huge images with up to 18 exa-pixels. A streaming 
en-/decoder can handle these with minimal RAM requirements, assuming there is 
enough storage space.

This particular implementation of SLO however is limited to images with a 
maximum size of 400 million pixels. It will safely refuse to en-/decode anything
larger than that. This is not a streaming en-/decoder. It loads the whole image
file into RAM before doing any work and is not extensively optimized for 
performance (but it's still very fast).

/


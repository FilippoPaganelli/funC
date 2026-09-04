# funC

This is a little exploration project. The goal is to have fun exploring the C language, and things you can do with it.

**No** AI tools have been used to write this code, to bring back some of the pleasure of **fixing your own segmentation fault errors**.

---

### PNG_to_PPM

A simple, incomplete converter from PNG format to PPM format. Does not apply any filter to the image before writing the RGB bytes directly in a PPM output file.

The implementation is based on the [PNG](https://www.libpng.org/pub/png/spec/1.2/PNG-Contents.html) and [PPM](https://netpbm.sourceforge.net/doc/ppm.html) format definitions, available online. This [YouTube](https://www.youtube.com/watch?v=M9ZwuIv3xz8) stream has also been useful in understanding some of C's features and behaviours.

The test PNG and expected output PPM images are also provided in the repo.

Depends on [zlib](https://zlib.net/zlib_how.html) to uncompress the image data, which is compressed in the PNG data chunks.

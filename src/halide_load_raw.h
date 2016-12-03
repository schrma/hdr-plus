// basic raw image loader for Halide::Buffer<T> or other image with same API
// code largely appropriated from halide_image_io.h

#ifndef HALIDE_LOAD_RAW_H
#define HALIDE_LOAD_RAW_H

#include "Halide.h"
#include "halide_image_io.h"

namespace Halide {
namespace Tools {

namespace Internal {

struct PipeOpener {
    PipeOpener(const char* cmd, const char* mode) : f(popen(cmd, mode)) {
        // nothing
    }
    ~PipeOpener() {
        if (f != nullptr) {
            pclose(f);
        }
    }
    // read a line of data skipping lines that begin with '#"
    char *readLine(char *buf, int maxlen) {
        char *status;
        do {
            status = fgets(buf, maxlen, f);
        } while(status && buf[0] == '#');
        return(status);
    }
    FILE * const f;
};

} // namespace Internal

inline bool is_little_endian() {
    int value = 1;
    return ((char *) &value)[0] == 1;
}

inline void swap_endian_16(uint16_t &value) {
    value = ((value & 0xff)<<8)|((value & 0xff00)>>8);
}

template<Internal::CheckFunc check = Internal::CheckFail>
bool load_raw(const std::string &filename, Image<uint16_t> *im) {

    /* open file and test for it being a pgm */
    Internal::PipeOpener f(("../tools/dcraw -c -o 2 -D -4  " + filename).c_str(), "r");
    if (!check(f.f != nullptr, "File %s could not be opened for reading\n", filename.c_str())) return false;

    int width, height, maxval;
    char header[256];
    char buf[1024];
    bool fmt_binary = false;

    f.readLine(buf, 1024);
    if (!check(sscanf(buf, "%255s", header) == 1, "Could not read PGM header\n")) return false;
    if (header == std::string("P5") || header == std::string("p5"))
        fmt_binary = true;
    if (!check(fmt_binary, "Input is not binary PGM\n")) return false;

    f.readLine(buf, 1024);
    if (!check(sscanf(buf, "%d %d\n", &width, &height) == 2, "Could not read PGM width and height\n")) return false;
    f.readLine(buf, 1024);
    if (!check(sscanf(buf, "%d", &maxval) == 1, "Could not read PGM max value\n")) return false;

    if (!check(maxval == 65535, "Invalid bit depth (not 16 bits) in PGM\n")) return false;

    uint16_t *data = new uint16_t[width * height];

    if (!check(fread((void *) data, sizeof(uint16_t), width*height, f.f) == (size_t) (width*height), "Could not read PGM 16-bit data\n")) {
        delete[] data;
        return false;
    }

    if (is_little_endian()) {
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {

                swap_endian_16(data[y * width + x]);
            }
        }
    }

    Image<uint16_t> output(data, width, height); 

    *im = output;

    (*im)(0,0,0) = (*im)(0,0,0);      /* Mark dirty inside read/write functions. */

    return true;
}

} // namespace Tools
} // namespace Halide

#endif
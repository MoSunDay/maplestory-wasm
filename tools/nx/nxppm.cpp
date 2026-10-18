// Dump an NX bitmap node to a binary PPM (P6) file for pixel analysis.
// Usage: nxppm <file.nx> <node/path> <out.ppm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

#include <nlnx/bitmap.hpp>
#include <nlnx/file.hpp>
#include <nlnx/node.hpp>

int main(int argc, char* argv[])
{
    if (argc < 4)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <file.nx> <node/path> <out.ppm>\n";
        return 1;
    }
    nl::file nxfile(argv[1]);
    nl::node node = nxfile.root().resolve(argv[2]);
    if (!node || node.data_type() != nl::node::type::bitmap)
    {
        std::cerr << "Not a bitmap node\n";
        return 2;
    }
    const auto bmp = node.get_bitmap();
    const int w = bmp.width();
    const int h = bmp.height();
    std::vector<unsigned char> buf(static_cast<size_t>(w) * h * 4);
    std::memcpy(buf.data(), bmp.data(), buf.size());

    std::ofstream out(argv[3], std::ios::binary);
    out << "P6\n" << w << ' ' << h << "\n255\n";
    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const unsigned char* px = &buf[(static_cast<size_t>(y) * w + x) * 4];
            out.put(px[0]);
            out.put(px[1]);
            out.put(px[2]);
        }
    }
    const std::string apath = std::string(argv[3]) + ".a.pgm";
    std::ofstream aout(apath, std::ios::binary);
    aout << "P5\n" << w << ' ' << h << "\n255\n";
    for (size_t i = 3; i < buf.size(); i += 4)
        aout.put(buf[i]);
    std::cout << "wrote " << argv[3] << " (" << w << 'x' << h << ") + " << apath << "\n";
    return 0;
}

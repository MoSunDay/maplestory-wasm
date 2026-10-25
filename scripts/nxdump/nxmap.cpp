// Print coarse ASCII maps of an NX bitmap: alpha coverage and luminance.
// Usage: nxmap <file.nx> <node/path> [cell] [map: a|l]
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <nlnx/bitmap.hpp>
#include <nlnx/file.hpp>
#include <nlnx/node.hpp>

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <file.nx> <node/path> [cell] [a|l]\n";
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
    const int cell = argc > 3 ? std::stoi(argv[3]) : 32;
    const bool luma = argc > 4 && argv[4][0] == 'l';
    const int w = bmp.width();
    const int h = bmp.height();
    std::vector<unsigned char> buf(static_cast<size_t>(w) * h * 4);
    std::memcpy(buf.data(), bmp.data(), buf.size());
    const auto px = [&](int x, int y) -> const unsigned char* {
        return &buf[(static_cast<size_t>(y) * w + x) * 4];
    };
    std::cout << "bitmap " << w << 'x' << h << " cell " << cell
              << (luma ? " luminance" : " alpha") << '\n';
    for (int y = 0; y + cell / 2 < h; y += cell)
    {
        for (int x = 0; x + cell / 2 < w; x += cell)
        {
            unsigned long long sum = 0;
            int n = 0;
            for (int yy = y; yy < y + cell && yy < h; yy += 2)
                for (int xx = x; xx < x + cell && xx < w; xx += 2)
                {
                    const unsigned char* p = px(xx, yy);
                    if (luma)
                        sum += (p[0] + p[1] + p[2]) / 3;
                    else
                        sum += p[3];
                    ++n;
                }
            const int v = static_cast<int>(sum / (n ? n : 1));
            const char scale[] = " .:-=+*#%@";
            std::cout << scale[std::min(9, v * 10 / 256)];
        }
        std::cout << " |" << y << '\n';
    }
    return 0;
}

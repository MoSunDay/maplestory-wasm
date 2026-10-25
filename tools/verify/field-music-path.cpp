// Standalone verifier for BGM path normalization (FieldMusicPath), the pass
// that maps server bgm paths onto nx node paths for FIELD_EFFECT mode 6.
//
// No graphics or nx data is needed: it links only the pure path unit.
//
// Usage (from the repository root):
//   c++ -std=c++17 -Wall -Wextra -Werror -Isrc/client
//       tools/verify/field-music-path.cpp
//       src/client/Audio/FieldMusicPath.cpp
//       -o /tmp/field-music-path
//   /tmp/field-music-path
//
// Asserts:
//   * "Bgm03/FormatX" gains the ".img" suffix before the first slash;
//   * paths already carrying ".img" are returned unchanged;
//   * a bare directory name becomes "name.img";
//   * the empty path stays empty.

#include <iostream>
#include <string>
#include <vector>

#include "Audio/FieldMusicPath.h"

namespace
{
    using jrc::field_music::normalize_bgm_path;

    std::vector<std::string> failures;

    void expect(const std::string& input, const std::string& expected, const char* label)
    {
        const std::string actual = normalize_bgm_path(input);
        if (actual == expected)
        {
            return;
        }

        failures.push_back(label);
        std::cout << "FAIL  " << label << "\n  input:    \"" << input
                  << "\"\n  expected: \"" << expected
                  << "\"\n  actual:   \"" << actual << "\"\n";
    }
}

int main()
{
    expect("Bgm03/FormatX", "Bgm03.img/FormatX", "slash path gains .img");
    expect("Bgm03.img/FormatX", "Bgm03.img/FormatX", "img path unchanged");
    expect("BgmUI.img/Title", "BgmUI.img/Title", "img ui path unchanged");
    expect("Bgm03", "Bgm03.img", "bare name gains .img");
    expect("", "", "empty path stays empty");

    if (!failures.empty())
    {
        std::cout << failures.size() << " check(s) failed\n";
        return 1;
    }

    std::cout << "ALL CHECKS PASSED\n";
    return 0;
}

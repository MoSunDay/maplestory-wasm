// Dump NX node trees from the command line, compatible with src/nlnx.
// Usage: nxdump2 <file.nx> [node/path] [depth]
//   depth: 0 = node info only, N = recurse N levels (default 1), -1 = full
#include <iomanip>
#include <iostream>
#include <string>

#include <nlnx/bitmap.hpp>
#include <nlnx/node.hpp>
#include <nlnx/nx.hpp>

namespace nl { namespace nx { node add_file(std::string name); } }

namespace
{
std::string type_name(nl::node::type t)
{
    switch (t)
    {
    case nl::node::type::none:    return "none";
    case nl::node::type::integer: return "int";
    case nl::node::type::string:  return "string";
    case nl::node::type::bitmap:  return "bitmap";
    case nl::node::type::audio:   return "audio";
    default:                return "unknown";
    }
}

std::string value_of(const nl::node &n)
{
    switch (n.data_type())
    {
    case nl::node::type::integer:
        return std::to_string(n.get_integer());
    case nl::node::type::string:
        return "\"" + n.get_string() + "\"";
    default:
        return "";
    }
}

std::string indent_of(int depth)
{
    return std::string(static_cast<size_t>(depth) * 2, ' ');
}

void dump(const nl::node &n, int depth, int maxdepth)
{
    const std::string val = value_of(n);
    std::cout << indent_of(depth) << n.name();
    if (!val.empty())
        std::cout << " = " << val;
    if (n.data_type() == nl::node::type::bitmap)
    {
        const auto b = n.get_bitmap();
        std::cout << " [bitmap " << b.width() << "x" << b.height() << "]";
    }
    std::cout << '\n';
    if (maxdepth == 0)
        return;
    for (auto child = n.begin(); child != n.end(); ++child)
        dump(*child, depth + 1, maxdepth < 0 ? maxdepth : maxdepth - 1);
}
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " <file.nx> [node/path] [depth]\n";
        return 1;
    }

    nl::node root = nl::nx::add_file(argv[1]);
    if (argc > 2 && std::string(argv[2]) != "/")
        root = root.resolve(argv[2]);

    const int depth = argc > 3 ? std::stoi(argv[3]) : 1;
    if (!root)
    {
        std::cerr << "Node not found\n";
        return 2;
    }
    dump(root, 0, depth);
    return 0;
}

#include <fstream>
#include <vector>

#include "roaring.hh"
#include "roaring64map.hh"

using namespace roaring;

void writeToFile(const Roaring64Map& roaring, const std::string& filename) {
    std::vector<char> buf(roaring.getSizeInBytes());
    roaring.write(buf.data());
    std::ofstream out(filename, std::ios::binary);
    out.write(buf.data(), buf.size());
}

// Utility to create files with valid serialized Roaring64Maps.
int main() {
    {
        Roaring64Map roaring;
        writeToFile(roaring, "64mapempty.bin");
    }
    {
        Roaring64Map roaring;
        for (uint32_t v = 0; v < 10; ++v) {
          roaring.add(v);
        }
        writeToFile(roaring, "64map32bitvals.bin");
    }
    {
        Roaring64Map roaring;
        for (uint64_t high = 0; high < 10; ++high) {
          for (uint64_t low = 0; low < 10; ++low) {
            roaring.add((high << 32) + low);
          }
        }
        writeToFile(roaring, "64mapspreadvals.bin");
    }
    {
        Roaring64Map roaring;
        uint64_t max32 = (std::numeric_limits<uint32_t>::max)();
        for (uint64_t high = max32 - 10; high <= max32; ++high) {
          for (uint64_t low = max32 - 10; low <= max32; ++low) {
            roaring.add((high << 32) + low);
          }
        }
        writeToFile(roaring, "64maphighvals.bin");
    }
    return EXIT_SUCCESS;
}

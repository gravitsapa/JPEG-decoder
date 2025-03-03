#include <jpg_to_png.hpp>
#include <decoder.h>
#include <png_encoder.hpp>

void JpegToPng(const std::string& filename, std::string& comment,
               const std::string& output_filename) {
    std::cerr << "Running " << filename << "\n";
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        std::cerr << "Cannot open a file\n";
        throw std::invalid_argument("Cannot open a file");
    }
    auto image = Decode(fin);
    fin.close();
    comment = image.GetComment();
    WritePng(output_filename, image);
}
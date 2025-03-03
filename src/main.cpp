#include <jpg_to_png.hpp>

#include <iostream>
#include <string>
#include <exception>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "To few arguments\n";
        return 0;
    }

    std::string input_filename = argv[1];
    std::string output_filename = argv[2];
    std::string comment;

    try {
        JpegToPng(input_filename, comment, output_filename);
    } catch(std::exception& ex) {
        std::cerr << "Failed to convert\n";
        std::cerr << ex.what() << '\n';
        return 0;
    }

    std::cerr << "Successfully converted\nComment: " << comment << '\n';
    return 0;
}
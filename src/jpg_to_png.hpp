#pragma once

#include <string>
#include <iostream>
#include <fstream>

void JpegToPng(const std::string& filename, std::string& comment,
                const std::string& output_filename);
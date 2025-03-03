#define GOOGLE_STRIP_LOG 1

#include <decoder.h>
#include <map>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>
#include <string>
#include <type_traits>
#include <iostream>
#include <cmath>

#include "jpeg.h"
#include "input.h"
#include "reader.h"
#include "huffman.h"
#include "fft.h"

bool GetBit(const std::vector<bool>& data, size_t& ind) {
    if (ind >= data.size()) {
        throw std::invalid_argument("Too short bit sequence");
    }
    return data[ind++];
}

int GetCoeff(const std::vector<bool>& data, size_t& ind, int len) {
    if (len == 0) {
        return 0;
    }

    int res = 0;
    for (int i = 0; i < len; ++i) {
        res = 2 * res + GetBit(data, ind);
    }
    if (((res >> (len - 1)) & 1) == 0) {
        res -= (1 << len) - 1;
    }

    return res;
}

std::vector<int> Reorder(const std::vector<int>& data) {
    size_t ind = 0;
    std::vector<int> res(kMatrixSquare);

    for (int sum = 0; sum < 2 * kMatrixSide - 1; ++sum) {
        if (sum % 2) {
            for (int x = 0; x < kMatrixSide; ++x) {
                int y = sum - x;
                if (y < 0 || y >= kMatrixSide) {
                    continue;
                }
                res[kMatrixSide * x + y] = data[ind++];
            }
        } else {
            for (int x = kMatrixSide - 1; x >= 0; --x) {
                int y = sum - x;
                if (y < 0 || y >= kMatrixSide) {
                    continue;
                }
                res[kMatrixSide * x + y] = data[ind++];
            }
        }
    }

    return res;
}

std::vector<int> GetMatrix(const Jpeg& jpeg, size_t& ind, const size_t table_id[2], int& prev_dc,
                           size_t quant) {
    std::vector<int> matrix;
    matrix.reserve(kMatrixSquare);

    while (matrix.size() < kMatrixSquare) {
        size_t table_class = matrix.empty() ? 0 : 1;
        size_t id = table_id[table_class];

        auto& tree = jpeg.huff_tables_.data_[table_class][id].tree_;

        int value;
        while (!tree.Move(GetBit(jpeg.sos_.data_, ind), value)) {
        }

        if (!table_class) {
            matrix.push_back(GetCoeff(jpeg.sos_.data_, ind, value));
        } else {
            int zeros = (value >> 4) & 15;
            int len = value & 15;

            for (int i = 0; i < zeros; ++i) {
                matrix.push_back(0);
            }

            if (len == 0 && zeros == 0) {
                while (matrix.size() < kMatrixSquare) {
                    matrix.push_back(0);
                }
                break;
            } else {
                matrix.push_back(GetCoeff(jpeg.sos_.data_, ind, len));
            }
        }
    }

    if (matrix.size() != kMatrixSquare) {
        throw std::invalid_argument("Matrix has invalid size");
    }

    prev_dc += matrix[0];
    matrix[0] = prev_dc;

    const auto& quant_table = jpeg.tables_.tables_[quant].data_;
    for (int i = 0; i < kMatrixSquare; ++i) {
        matrix[i] *= quant_table[i];
    }

    matrix = Reorder(matrix);

    std::vector<double> input(kMatrixSquare);
    for (int i = 0; i < kMatrixSquare; ++i) {
        input[i] = static_cast<double>(matrix[i]);
    }

    std::vector<double> output(kMatrixSquare);

    DctCalculator calc(kMatrixSide, &input, &output);
    calc.Inverse();

    for (size_t i = 0; i < kMatrixSquare; ++i) {
        matrix[i] = std::max(0, std::min(255, static_cast<int>(std::round(output[i] + 128))));
    }

    // DLOG(INFO) << "DONE";
    return matrix;
}
struct ColorMatrix {
    std::vector<std::vector<std::vector<int>>> data_;
};

ColorMatrix GetBlock(const Jpeg& jpeg, size_t& ind, size_t channels, std::vector<int>& prev_dc) {
    ColorMatrix block;
    block.data_.resize(channels);

    for (size_t i = 0; i < channels; ++i) {
        block.data_[i].resize(jpeg.sos_.channels_[i].h * jpeg.sos_.channels_[i].v);
        for (auto& matrix : block.data_[i]) {
            matrix = GetMatrix(jpeg, ind, jpeg.sos_.channels_[i].table_id, prev_dc[i],
                               jpeg.sos_.channels_[i].quant_identifier_);
        }
    }

    return block;
}

void GetCoordinate(int y, int max_v, int v, int& ins, int& ind) {
    if (max_v == 2 && v == 2) {
        ind = y / kMatrixSide, ins = y % kMatrixSide;
    } else if (max_v == 2 && v == 1) {
        ind = 0, ins = y / 2;
    } else if (max_v == 1 && v == 1) {
        ind = 0, ins = y;
    } else {
        throw std::logic_error("Broken 1 <= v <= max_v <= 2");
    }
}

void GetCoordinate(int y, int x, int max_v, int max_h, int v, int h, int& ins, int& ind) {
    int ins_y, ins_x, ind_y, ind_x;
    GetCoordinate(y, max_v, v, ins_y, ind_y);
    GetCoordinate(x, max_h, h, ins_x, ind_x);
    ind = (h == 2 && v == 2) ? ind_y * 2 + ind_x : ind_y + ind_x;
    ins = ins_y * kMatrixSide + ins_x;
}

std::vector<int> GetPixel(const Jpeg& jpeg, int y, int x, const std::vector<ColorMatrix>& blocks,
                          int max_v, int max_h, int len_v, int len_h) {
    int y_ind = y / (max_v * kMatrixSide), x_ind = x / (max_h * kMatrixSide);
    int y_ins = y % (max_v * kMatrixSide), x_ins = x % (max_h * kMatrixSide);

    int ind_block = y_ind * len_h + x_ind;

    std::vector<int> res(blocks[ind_block].data_.size());
    for (int i = 0; i < res.size(); ++i) {
        int v = jpeg.sos_.channels_[i].v, h = jpeg.sos_.channels_[i].h;

        int ind, ins;
        GetCoordinate(y_ins, x_ins, max_v, max_h, v, h, ins, ind);

        res[i] = blocks[ind_block].data_[i][ind][ins];
    }

    return res;
}

RGB MakeRGB(std::vector<int> channels) {
    RGB color;
    if (channels.size() == 1) {
        color.r = color.g = color.b = channels[0];
    } else if (channels.size() == 3) {
        double y = channels[0], cb = channels[1], cr = channels[2];
        color.r = round(y + 1.402 * (cr - 128));
        color.g = round(y - 0.34414 * (cb - 128) - 0.71414 * (cr - 128));
        color.b = round(y + 1.772 * (cb - 128));
    } else {
        throw std::invalid_argument("Invalid channel amount");
    }

    color.r = std::max(0, std::min(color.r, 255));
    color.g = std::max(0, std::min(color.g, 255));
    color.b = std::max(0, std::min(color.b, 255));
    return color;
}

Image Decode(std::istream& stream) {
    Jpeg jpeg;
    Input input(&stream);

    Reader reader(input, jpeg);

    while (reader.ReadField()) {
    }

    Image image;

    if (!jpeg.begin_.Exists()) {
        throw std::invalid_argument("No begin");
    }

    if (!jpeg.end_.Exists()) {
        throw std::invalid_argument("No end");
    }

    if (jpeg.comment_.Exists()) {
        image.SetComment(jpeg.comment_.text_);
    }

    int width, high;

    if (!jpeg.info_.Exists()) {
        throw std::invalid_argument("No image info");
    } else {
        width = jpeg.info_.width_, high = jpeg.info_.high_;
        if (width == 0 || high == 0) {
            throw std::invalid_argument("Empty size");
        }

        image.SetSize(width, high);
    }

    size_t channels = jpeg.sos_.channels_.size();

    int max_h = 1, max_v = 1;

    for (size_t i = 0; i < channels; ++i) {
        auto chan = jpeg.sos_.channels_[i];
        if (chan.table_id[0] >= jpeg.huff_tables_.data_[0].size()) {
            throw std::invalid_argument("Invalid DC channel id");
        }
        if (chan.table_id[1] >= jpeg.huff_tables_.data_[1].size()) {
            throw std::invalid_argument("Invalid AC channel id");
        }
        if (chan.quant_identifier_ >= jpeg.tables_.tables_.size()) {
            throw std::invalid_argument("Invalid channel Quant table id");
        }
        if (chan.h < 1 || chan.h > 2 || chan.v < 1 || chan.v > 2) {
            throw std::invalid_argument("Invalid channel compression");
        }

        max_h = std::max(max_h, static_cast<int>(chan.h));
        max_v = std::max(max_v, static_cast<int>(chan.v));
    }

    size_t ind = 0;

    int len_v = (high - 1) / (kMatrixSide * max_v) + 1,
        len_h = (width - 1) / (kMatrixSide * max_h) + 1;

    int blocks_cnt = len_v * len_h;

    std::vector<ColorMatrix> blocks(blocks_cnt);
    std::vector<int> prev_dc(channels);

    for (auto& block : blocks) {
        block = GetBlock(jpeg, ind, channels, prev_dc);
    }

    for (int y = 0; y < high; ++y) {
        for (int x = 0; x < width; ++x) {
            image.SetPixel(y, x, MakeRGB(GetPixel(jpeg, y, x, blocks, max_v, max_h, len_v, len_h)));
        }
    }

    return image;
}

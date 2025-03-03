#pragma once

#include <istream>
#include <string>
#include <vector>
#include <cstdint>

#include "huffman.h"

struct Field {
    bool Exists() const {
        return index_ != kNotExists;
    }

    void SetIndex(size_t index) {
        index_ = index;
    }

    size_t index_ = kNotExists;

private:
    static constexpr size_t kNotExists = -1;
};

struct Begin : public Field {};

struct End : public Field {};

struct Section : public Field {};

struct Comment : public Section {
    std::string text_;
};

struct Appn : public Section {};

struct QuantTable {
    size_t len_;
    size_t identifier_;

    std::vector<size_t> data_;
};

struct QuantTables : public Section {
    std::vector<QuantTable> tables_;

    void AddTable(const QuantTable& table, size_t id) {
        tables_.resize(std::max(tables_.size(), id + 1));
        tables_[id] = table;
    }
};

struct Dht {
    Dht() {
        codes_.resize(16);
    }

    size_t class_;
    size_t identifier_;

    std::vector<uint8_t> codes_;
    std::vector<uint8_t> values_;

    mutable HuffmanTree tree_;

    static constexpr size_t kCodesLen = 16;
};

struct Dhts : public Section {
    // 0 - DC, 1 - AC
    std::vector<Dht> data_[2];
};

struct ChannelInfo {
    size_t identifier_;
    // 0 - DC, 1 - AC
    size_t table_id[2];
    size_t h;
    size_t v;
    size_t quant_identifier_;
};

struct Sos : public Section {
    std::vector<ChannelInfo> channels_;
    std::vector<bool> data_;

    void SetChannels(size_t channels) {
        if (channels == 0) {
            throw std::invalid_argument("Zero channels");
        }

        if (channels_.empty()) {
            channels_.resize(channels);
        }

        if (channels_.size() != channels) {
            throw std::invalid_argument("Broken channels");
        }
    }
};

struct Information : public Section {
    size_t precision_;
    size_t high_;
    size_t width_;
};

struct Jpeg {
    Begin begin_;
    End end_;
    Comment comment_;
    Appn app_data_;
    QuantTables tables_;
    Dhts huff_tables_;
    Information info_;
    Sos sos_;
};

constexpr size_t kMatrixSide = 8;
constexpr size_t kMatrixSquare = kMatrixSide * kMatrixSide;
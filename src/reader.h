#pragma once

#include <memory>
#include <map>

#include "jpeg.h"
#include "input.h"

class Marker {
public:
    Marker(Byte byte) {
        byte_ = byte;
    }

    bool operator<(const Marker& anoth) const {
        return byte_ < anoth.byte_;
    }

private:
    Byte byte_;
};

class SectionReader;

using SectionReaderPtr = std::unique_ptr<SectionReader>;

class SectionReader {
public:
    virtual ~SectionReader() = default;
    virtual bool ReadField(Input& input, Jpeg& jpeg) = 0;
    virtual SectionReaderPtr Copy() = 0;
};

class BeginSection : public SectionReader {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        jpeg.begin_.SetIndex(input.Index());

        return true;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new BeginSection());
    }
};

class EndSection : public SectionReader {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        jpeg.end_.SetIndex(input.Index());
        return false;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new EndSection());
    }
};

class BlockSection : public SectionReader {
protected:
    void ReadBlock(Input& input) {
        size_t size = input.ReadShort() - 2;

        block_.resize(size);
        for (size_t i = 0; i < size; ++i) {
            block_[i] = input.MustReadByte();
        }
    }

    Byte GetByte() {
        if (ind_ == block_.size()) {
            throw std::invalid_argument("Block is too short");
        }
        return block_[ind_++];
    }

    size_t Get2Bytes() {
        Byte left = GetByte();
        Byte right = GetByte();
        return Merge(left, right);
    }

    size_t CanGet() {
        return block_.size() - ind_;
    }

    std::vector<Byte> block_;
    size_t ind_ = 0;
};

class ComSection : public BlockSection {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        jpeg.comment_.SetIndex(input.Index());
        ReadBlock(input);

        std::string text(block_.size(), 0);
        for (size_t i = 0; i < block_.size(); ++i) {
            text[i] = block_[i];
        }
        jpeg.comment_.text_ = text;

        return true;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new ComSection());
    }
};

class AppSection : public BlockSection {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        jpeg.app_data_.SetIndex(input.Index());
        ReadBlock(input);

        return true;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new AppSection());
    }
};

class QuantTableSection : public BlockSection {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        jpeg.tables_.SetIndex(input.Index());
        ReadBlock(input);

        while (CanGet()) {
            Byte info = GetByte();
            QuantTable table;

            table.len_ = LeftByteHalf(info);
            table.identifier_ = RightByteHalf(info);

            table.data_.resize(kMatrixSquare);
            for (size_t i = 0; i < kMatrixSquare; ++i) {
                size_t value;
                if (table.len_) {
                    value = Get2Bytes();
                } else {
                    value = GetByte();
                }
                table.data_[i] = value;
            }

            jpeg.tables_.AddTable(table, table.identifier_);
        }
        return true;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new QuantTableSection());
    }
};

class DhtSection : public BlockSection {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        jpeg.huff_tables_.SetIndex(input.Index());
        ReadBlock(input);

        while (CanGet()) {
            Byte info = GetByte();
            Dht table;
            table.class_ = LeftByteHalf(info);

            if (table.class_ >= 2) {
                throw std::invalid_argument("Broken Huffman table class");
            }

            table.identifier_ = RightByteHalf(info);

            int values = 0;

            for (auto& code : table.codes_) {
                code = GetByte();
                values += code;
            }

            table.values_.resize(values);

            for (auto& value : table.values_) {
                value = GetByte();
            }

            table.tree_.Build(table.codes_, table.values_);

            jpeg.huff_tables_.data_[table.class_].resize(
                std::max(jpeg.huff_tables_.data_[table.class_].size(), table.identifier_ + 1));

            jpeg.huff_tables_.data_[table.class_][table.identifier_] = std::move(table);
        }

        return true;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new DhtSection());
    }
};

class InfoSection : public BlockSection {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        if (jpeg.info_.Exists()) {
            throw std::invalid_argument("Two SOF sections");
        }
        jpeg.info_.SetIndex(input.Index());
        ReadBlock(input);

        jpeg.info_.precision_ = GetByte();
        jpeg.info_.high_ = Get2Bytes();
        jpeg.info_.width_ = Get2Bytes();
        size_t channels = GetByte();

        jpeg.sos_.SetChannels(channels);

        for (size_t i = 0; i < channels; ++i) {
            size_t id = GetByte() - 1;
            if (id >= channels) {
                throw std::invalid_argument("Channel id broken");
            }

            auto& channel = jpeg.sos_.channels_[id];
            channel.identifier_ = id;
            Byte h_v = GetByte();
            channel.h = LeftByteHalf(h_v);
            channel.v = RightByteHalf(h_v);
            channel.quant_identifier_ = GetByte();
        }

        return true;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new InfoSection());
    }
};

class SosSection : public BlockSection {
public:
    bool ReadField(Input& input, Jpeg& jpeg) override {
        jpeg.sos_.SetIndex(input.Index());
        ReadBlock(input);

        size_t channels = GetByte();
        jpeg.sos_.SetChannels(channels);

        for (size_t i = 0; i < channels; ++i) {
            size_t id = GetByte() - 1;
            if (id >= channels) {
                throw std::invalid_argument("Channel id broken");
            }

            auto& channel = jpeg.sos_.channels_[id];
            channel.identifier_ = id;

            Byte t_id = GetByte();
            channel.table_id[0] = LeftByteHalf(t_id);
            channel.table_id[1] = RightByteHalf(t_id);
        }

        Byte sos_end[3];
        Byte sos_end_must_be[3] = {0, 0x3F, 0};
        for (size_t i = 0; i < 3; ++i) {
            sos_end[i] = GetByte();
            if (sos_end[i] != sos_end_must_be[i]) {
                throw std::invalid_argument("Invalid SOS section end");
            }
        }

        while (true) {
            Byte byte = input.MustReadByte();

            if (byte == 0xFF) {
                Byte mark = input.MustReadByte();
                if (mark != 0x00 && mark != 0xD9) {
                    throw std::invalid_argument("FF XX byte found while scanning SOS");
                }

                if (mark == 0xD9) {
                    jpeg.end_.SetIndex(input.Index());
                    break;
                }
            }

            for (int i = 7; i >= 0; --i) {
                jpeg.sos_.data_.push_back(GetBit(byte, i));
            }
        }

        return false;
    }

    SectionReaderPtr Copy() override {
        return SectionReaderPtr(new SosSection());
    }
};

template <class T>
SectionReaderPtr CreateSectionReaderPtr() {
    return SectionReaderPtr(new T());
}

class Reader {
public:
    Reader() = delete;

    Reader(Input& input, Jpeg& jpeg) : input_(input), jpeg_(jpeg) {
        AddReader(0xD8, CreateSectionReaderPtr<BeginSection>());
        AddReader(0xD9, CreateSectionReaderPtr<EndSection>());
        AddReader(0xFE, CreateSectionReaderPtr<ComSection>());
        for (Byte i = 0xE0; i <= 0xEF; ++i) {
            AddReader(i, CreateSectionReaderPtr<AppSection>());
        }
        AddReader(0xDB, CreateSectionReaderPtr<QuantTableSection>());
        AddReader(0xC4, CreateSectionReaderPtr<DhtSection>());
        AddReader(0xC0, CreateSectionReaderPtr<InfoSection>());
        AddReader(0xDA, CreateSectionReaderPtr<SosSection>());
    }

    bool ReadField() {
        Byte first_byte = input_.MustReadByte();

        static constexpr Byte kBeginByte = 0xFF;
        if (first_byte != kBeginByte) {
            throw std::invalid_argument("Invalid first byte");
        }

        Byte marker_byte = input_.MustReadByte();
        Marker mark(marker_byte);

        if (!ExistsReader(mark)) {
            throw std::invalid_argument("Invalid marker" + std::to_string(marker_byte));
        }

        auto reader = GetReader(mark);
        return reader->ReadField(input_, jpeg_);
    }

private:
    SectionReaderPtr GetReader(Marker mark) {
        return section_readers_[mark]->Copy();
    }

    void AddReader(Marker mark, SectionReaderPtr ptr) {
        section_readers_[mark] = std::move(ptr);
    }

    bool ExistsReader(Marker mark) {
        return section_readers_.count(mark);
    }

    Input& input_;
    Jpeg& jpeg_;

    std::map<Marker, SectionReaderPtr> section_readers_;
};

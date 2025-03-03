#pragma once

#include <string>
#include <istream>

using Byte = unsigned char;

std::string ByteToStr(Byte byte) {
    std::string s = "0123456789ABCDEF";
    return std::string(1, s[byte >> 4]) + std::string(1, s[byte & 15]);
}

size_t LeftByteHalf(Byte byte) {
    return byte >> 4;
}

size_t RightByteHalf(Byte byte) {
    return byte & 15;
}

size_t Merge(Byte left, Byte right) {
    return (static_cast<size_t>(left) << 8) + static_cast<size_t>(right);
}

bool GetBit(Byte byte, int ind) {
    return (byte >> ind) & 1;
}

class Input {
public:
    Input(std::istream* stream) {
        stream_ = stream;
        index_ = 0;
    }

    bool operator>>(Byte& byte) {
        byte = (*stream_).get();
        if (!(*stream_).good()) {
            return false;
        }
        ++index_;
        return true;
    }

    size_t Index() const {
        return index_;
    }

    Byte MustReadByte() {
        Byte byte;

        if (!((*this) >> byte)) {
            throw std::invalid_argument("Input Ended");
        }

        return byte;
    }

    size_t ReadShort() {
        Byte l = MustReadByte();
        Byte r = MustReadByte();

        return (static_cast<size_t>(l) << 8) + static_cast<size_t>(r);
    }

private:
    std::istream* stream_;

    size_t index_ = 0;
};

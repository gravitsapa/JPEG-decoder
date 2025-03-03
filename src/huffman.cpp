#include <huffman.h>
#include <optional>
#include <memory>
#include <stdexcept>

class HuffmanTree::Impl {
public:
    using CodeType = uint8_t;
    using ValType = uint8_t;

    Impl() {
        Reset();
    }

    void Build(const std::vector<CodeType> &code_lengths, const std::vector<ValType> &values) {
        static constexpr size_t kMaxCodeLength = 16;
        if (code_lengths.size() > kMaxCodeLength) {
            throw std::invalid_argument("Tree is too big");
        }

        std::vector<CodeType> code_lengths_copy;
        code_lengths_copy.reserve(code_lengths.size() + 1);
        code_lengths_copy.push_back(0);
        code_lengths_copy.insert(code_lengths_copy.end(), code_lengths.begin(), code_lengths.end());
        std::vector<ValType>::const_iterator value_it = values.begin();

        Reset();
        root_->Build(code_lengths_copy.begin(), code_lengths_copy.end(), value_it, values.end());
        ptr_ = root_;

        bool left_code = false;
        for (auto i : code_lengths_copy) {
            if (i) {
                left_code = true;
                break;
            }
        }
        bool left_values = value_it != values.end();

        if (left_code || left_values) {
            throw std::invalid_argument("Broken tree");
        }
    }

    bool Move(bool bit, int &value) {
        if (ptr_->IsTerminal()) {
            ptr_ = root_;
            throw std::invalid_argument("Terminal Node");
        }

        ptr_ = ptr_->Move(bit);
        if (ptr_->GetVal().has_value()) {
            value = static_cast<int>(ptr_->GetVal().value());
            ptr_ = root_;
            return true;
        }

        if (ptr_->IsTerminal()) {
            ptr_ = root_;
            throw std::invalid_argument("Terminal Node");
        }

        return false;
    }

private:
    void Reset() {
        root_.reset(new Node());
        ptr_ = root_;
    }

    class Node {
    public:
        using NodePtr = std::shared_ptr<Node>;

        Node() {
        }

        NodePtr Move(bool bit) {
            return bit ? right_ : left_;
        }

        std::optional<ValType> GetVal() {
            return val_;
        }

        bool IsTerminal() {
            return left_ == nullptr;
        }

        void Build(std::vector<CodeType>::iterator code_lengths,
                   std::vector<CodeType>::iterator code_end,
                   std::vector<ValType>::const_iterator &value_it,
                   std::vector<ValType>::const_iterator value_end) {
            if (code_lengths == code_end || value_it == value_end) {
                left_.reset();
                right_.reset();
                val_.reset();
                return;
            }

            if (*code_lengths) {
                (*code_lengths)--;
                val_ = *value_it;
                value_it++;
                left_.reset();
                right_.reset();
                return;
            }

            left_.reset(new Node());
            right_.reset(new Node());
            val_.reset();

            left_->Build(code_lengths + 1, code_end, value_it, value_end);
            right_->Build(code_lengths + 1, code_end, value_it, value_end);
        }

    private:
        NodePtr left_ = nullptr;
        NodePtr right_ = nullptr;

        std::optional<ValType> val_ = std::nullopt;
    };

    Node::NodePtr root_ = nullptr;
    Node::NodePtr ptr_ = nullptr;
};

HuffmanTree::HuffmanTree() {
    impl_.reset(new Impl());
}

void HuffmanTree::Build(const std::vector<uint8_t> &code_lengths,
                        const std::vector<uint8_t> &values) {
    impl_->Build(code_lengths, values);
}

bool HuffmanTree::Move(bool bit, int &value) {
    return impl_->Move(bit, value);
}

HuffmanTree::HuffmanTree(HuffmanTree &&) = default;

HuffmanTree &HuffmanTree::operator=(HuffmanTree &&) = default;

HuffmanTree::~HuffmanTree() = default;

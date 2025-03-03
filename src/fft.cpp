#include <fft.h>
#include <fftw3.h>
#include <cmath>

#include <stdexcept>

class DctCalculator::Impl {
public:
    using ValType = double;
    using DataType = std::vector<ValType>;

    Impl() = delete;

    Impl(size_t width, DataType* input, DataType* output) {
        if (!input || !output) {
            throw std::invalid_argument("Empty data");
        }

        if (width == 0 || width > input->size() || width > output->size() ||
            width * width != input->size() || width * width != output->size()) {
            throw std::invalid_argument("Invalid width");
        }

        width_ = width;
        input_ = input;
        output_ = output;
    }

    void Inverse() {
        fftw_plan p;
        static const ValType kRowFactor = sqrt(2);
        static const ValType kAllFactor = 16;

        for (size_t x = 0; x < width_; ++x) {
            (*input_)[x] *= kRowFactor;
        }

        for (size_t y = 0; y < width_; ++y) {
            (*input_)[y * width_] *= kRowFactor;
        }

        p = fftw_plan_r2r_2d(width_, width_, input_->data(), output_->data(), FFTW_REDFT01,
                             FFTW_REDFT01, FFTW_ESTIMATE);
        fftw_execute(p);

        for (size_t i = 0; i < width_ * width_; ++i) {
            (*output_)[i] /= kAllFactor;
        }

        fftw_destroy_plan(p);
    }

private:
    size_t width_;
    DataType* input_;
    DataType* output_;
};

DctCalculator::DctCalculator(size_t width, std::vector<double>* input, std::vector<double>* output)
    : impl_(new Impl(width, input, output)) {
}

void DctCalculator::Inverse() {
    impl_->Inverse();
}

DctCalculator::~DctCalculator() = default;

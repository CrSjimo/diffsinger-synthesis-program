#include <srtsepinfer/Separation/SeparationError.h>

namespace srtsepinfer {
    const std::error_category &separationErrorCategory() noexcept {
        class Category final : public std::error_category {
        public:
            const char *name() const noexcept override { return "srtsepinfer.separation"; }

            std::string message(int code) const override {
                switch (static_cast<SeparationError>(code)) {
                    case SeparationError::Cancelled: return "Audio separation was cancelled";
                    case SeparationError::NoWorker: return "Cannot start separation worker";
                    case SeparationError::Internal: return "Internal separation error";
                    case SeparationError::ContractViolation: return "Separation contract violation";
                    case SeparationError::ModelFailed: return "Separation model failed";
                    case SeparationError::ModelMismatch: return "Separation model tensor mismatch";
                }

                return "Unknown separation error";
            }
        };

        static const Category category;
        return category;
    }
}

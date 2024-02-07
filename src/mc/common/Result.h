#pragma once

#include "mc/_HeaderOutputPredefine.h"
#include "mc/enums/ErrorInfo.h"
#include "mc/enums/expected.h"

namespace Bedrock {
template <typename T, typename Err>
class Result : public nonstd::expected<T, ErrorInfo<Err>> {
public:
    using Base = nonstd::expected<T, ErrorInfo<Err>>;
    using Base::Base;
};
}; // namespace Bedrock

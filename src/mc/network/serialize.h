#pragma once

#include "mc/common/Result.h"
#include "mc/_HeaderOutputPredefine.h"

template <typename T>
struct serialize {
public:
    MCAPI static void               write(T const&, class BinaryStream&);
    MCAPI static Bedrock::Result<T> read(class ReadOnlyBinaryStream&);
};
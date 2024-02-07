#pragma once

#include "mc/_HeaderOutputPredefine.h"
#include "mc/enums/SharePtrRefTraits.h"

template <typename T0>
class OwnerPtrT : public T0::OwnerStorage {
public:
    using OwnerStackRef = typename T0::OwnerStackRef;
    using Base          = typename T0::OwnerStorage;
    using Base::Base;
};
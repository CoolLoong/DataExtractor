#pragma once

#include "mc/common/typeid_t.h"
#include "mc/_HeaderOutputPredefine.h"

enum class ObjectiveSortOrder : uchar {
    Ascending  = 0x0,
    Descending = 0x1,
};

MCTAPI class Bedrock::typeid_t<CommandRegistry>
Bedrock::type_id<class CommandRegistry, enum class ObjectiveSortOrder>();

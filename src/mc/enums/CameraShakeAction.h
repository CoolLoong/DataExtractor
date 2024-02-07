#pragma once

#include "mc/_HeaderOutputPredefine.h"
#include "mc/common/typeid_t.h"

enum class CameraShakeAction {};

MCTAPI class Bedrock::typeid_t<CommandRegistry>
Bedrock::type_id<class CommandRegistry, enum class CameraShakeAction>(void);

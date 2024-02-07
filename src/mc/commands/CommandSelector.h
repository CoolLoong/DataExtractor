#pragma once

#include "mc/_HeaderOutputPredefine.h"
#include "mc/commands/CommandOrigin.h"
#include "mc/commands/CommandSelectorBase.h"
#include "mc/commands/CommandSelectorResults.h"

class Player;

template <typename T>
class CommandSelector : CommandSelectorBase {
public:
    CommandSelector() : CommandSelectorBase(std::is_same_v<T, Player>) {}
    MCAPI CommandSelectorResults<T> results(CommandOrigin const& a0) const;
};

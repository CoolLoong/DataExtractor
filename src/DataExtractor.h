// Basic
#include "magic_enum.hpp"
#include "zlib.h"
#include <Nlohmann/json.hpp>
#include <direct.h>
#include <string>
#include <vector>

// LL
#include "ll/memory/Hook.h"
#include "ll/memory/Memory.h"
#include <ll/logger/Logger.h>

#include <pl/SymbolProvider.h>

// MC header
#include "mc/ActorDefinitionIdentifier.h"
#include "mc/CreativeItemRegistry.h"
#include "mc/Minecraft.h"
#include "mc/commands/CommandRegistry.h"
#include "mc/commands/MinecraftCommands.h"
#include "mc/common/HashedString.h"
#include "mc/enums/Brightness.h"
#include "mc/math/AABB.h"
#include "mc/math/BlockPos.h"
#include "mc/math/Vec3.h"
#include "mc/nbt/CompoundTag.h"
#include "mc/nbt/NbtIo.h"
#include "mc/network/AvailableCommandsPacket.h"
#include "mc/network/BinaryStream.h"
#include "mc/network/CraftingDataPacket.h"
#include "mc/network/ReadOnlyBinaryStream.h"
#include "mc/util/ItemColorUtil.h"
#include "mc/util/Spawner.h"
#include "mc/util/json/Value.h"
#include "mc/world/ActorFactory.h"
#include "mc/world/Biome.h"
#include "mc/world/BiomeRegistry.h"
#include "mc/world/BlockPalette.h"
#include "mc/world/BlockSource.h"
#include "mc/world/BlockTypeRegistry.h"
#include "mc/world/Dimension.h"
#include "mc/world/Experiments.h"
#include "mc/world/Level.h"
#include "mc/world/LevelData.h"
#include "mc/world/Material.h"
#include "mc/world/Mob.h"
#include "mc/world/VanillaBlockTags.h"
#include "mc/world/VanillaItemTags.h"
#include "mc/world/block/Block.h"
#include "mc/world/block/BlockLegacy.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemDescriptor.h"
#include "mc/world/item/ItemInstance.h"
#include "mc/world/item/ItemRegistryManager.h"
#include "mc/world/item/ItemRegistryRef.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemTag.h"

class ServerNetworkHandler {
public:
    // symbol: ?handle@ServerNetworkHandler@@UEAAXAEBVNetworkIdentifier@@AEBVTextPacket@@@Z
    // 只是前向声明这两个类作为参数，不需要导入它们
    MCVAPI void handle(class NetworkIdentifier const&, class TextPacket const&);
};
struct BiomeTagIDType;
struct BiomeTagSetIDType;
class GetCollisionShapeInterface {
public:
    // prevent constructor by default
    GetCollisionShapeInterface& operator=(GetCollisionShapeInterface const&);
                                GetCollisionShapeInterface(GetCollisionShapeInterface const&);
                                GetCollisionShapeInterface();
};

void dumpCreativeItemData(const ll::Logger& logger);
void dumpCommandNameSymbol(const ll::Logger& logger);
void dumpCommonCommandArgData(const ll::Logger& logger);
void dumpFullCommandArgData(const ll::Logger& logger);
void dumpCommandmConstrainedValues(const ll::Logger& logger);
void dumpBiomeData(const ll::Logger& logger);
void dumpBlockAttributesData(const ll::Logger& logger);
void dumpItemData(const ll::Logger& logger);
void dumpPalette(const ll::Logger& logger);
void dumpBlockTags(const ll::Logger& logger);
void dumpItemTags(const ll::Logger& logger);
void dumpPropertyTypeData(const ll::Logger& logger);

void extractBlock(ListTag& dest, const Block& block, const ll::Logger& logger);
void extractData();
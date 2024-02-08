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
#include "mc/world/BlockTypeRegistry.h"
#include "mc/world/Experiments.h"
#include "mc/world/Level.h"
#include "mc/world/LevelData.h"
#include "mc/world/Material.h"
#include "mc/world/Mob.h"
#include "mc/world/VanillaBlockTags.h"
#include "mc/world/VanillaItemTags.h"
#include "mc/world/block/Block.h"
#include "mc/world/block/BlockLegacy.h"
#include "mc/world/block/BlockSource.h"
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

#pragma region TOOL_FUNCTION
bool folderExists(const std::string& folderName) {
    struct stat info {};
    if (stat(folderName.c_str(), &info) != 0) {
        return false;
    }
    return info.st_mode & S_IFDIR;
}

void createFolder(const ll::Logger& logger, const std::string& folderName) {
    if (const int result = _mkdir(folderName.c_str()); result != 0) {
        logger.error("Failed to create folder.");
    } else {
        logger.info("Folder " + std::string(folderName) + " created successfully.");
    }
}

bool gzip_compress(const std::string& original_str, std::string& str) {
    z_stream d_stream = {nullptr};
    if (Z_OK != deflateInit2(&d_stream, Z_BEST_COMPRESSION, Z_DEFLATED, MAX_WBITS + 16, 9, Z_DEFAULT_STRATEGY)) {
        return false;
    }
    const unsigned long len = compressBound(original_str.size());
    auto*               buf = (unsigned char*)malloc(len);
    if (!buf) {
        return false;
    }
    d_stream.next_in   = (unsigned char*)(original_str.c_str());
    d_stream.avail_in  = original_str.size();
    d_stream.next_out  = buf;
    d_stream.avail_out = len;
    deflate(&d_stream, Z_SYNC_FLUSH);
    deflateEnd(&d_stream);
    str.assign((char*)buf, d_stream.total_out);
    free(buf);
    return true;
}

void writeNBT(const std::string& fileName, const CompoundTag& tag) {
    std::string compressed;
    gzip_compress(tag.toBinaryNbt(false), compressed);
    auto out = std::ofstream(fileName, std::ofstream::out | std::ofstream::binary | std::ofstream::trunc);
    out << compressed;
    out.close();
}

template <typename JSON_TYPE>
void writeNlohmannJSON(const std::string& fileName, JSON_TYPE& json) {
    auto out = std::ofstream(fileName, std::ofstream::out | std::ofstream::trunc);
    out << json.dump(4);
    out.close();
}

void writeJSON(const std::string& fileName, const Json::Value& json) {
    auto out = std::ofstream(fileName, std::ofstream::out | std::ofstream::trunc);
    out << json.toStyledString();
    out.close();
}

std::string aabbToStr(const AABB& aabb) {
    std::stringstream aabbStr;
    aabbStr << aabb.min.x << "," << aabb.min.y << "," << aabb.min.z << "," << aabb.max.x << "," << aabb.max.y << ","
            << aabb.max.z;
    return aabbStr.str();
}
#pragma endregion TOOL_FUNCTION

#include "DataExtractor.h"

using json = nlohmann::json;
using namespace std;

static Minecraft*         mc                = nullptr;
static Dimension*         overworld         = nullptr;
static MinecraftCommands* commands          = nullptr;
static unsigned int       blockStateCounter = 0;
static AABB               ZERO_AABB         = AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));

#pragma region HOOK
ll::Logger     hookLogger("DataExtractor-Hook");

LL_AUTO_TYPE_INSTANCE_HOOK(
    PlayerChatEventHook,
    ll::memory::HookPriority::Normal,
    ServerNetworkHandler,
    "?handle@ServerNetworkHandler@@"
    "UEAAXAEBVNetworkIdentifier@@AEBVTextPacket@@@Z",
    void,
    void* networkIdentifier,
    void* textPk
) {
    std::string originMessage = ll::memory::dAccess<std::string>(
        textPk,
        88
    ); // 通过LL提供的memory库直接基于偏移量动态访问这个地址所在的内容
    origin(networkIdentifier, textPk);
    if (originMessage == "ext") {
        extractData();
    }
}

// Dimension
LL_AUTO_TYPE_INSTANCE_HOOK(
    DimensionService,
    ll::memory::HookPriority::Normal,
    Dimension,
    "?init@Dimension@@UEAAXAEBVStructureSetRegistry@worldgen@br@@@Z",
    void*,
    Dimension* a1
) {
    if (a1->getHeight() > 256) {
        hookLogger.info("INJECT DIMENSION INSTANCE");
        overworld = a1;
    }
    return origin(a1);
}

// Recipe packet
LL_AUTO_TYPE_INSTANCE_HOOK(
    CraftingDataPacketHook,
    ll::memory::HookPriority::Normal,
    CraftingDataPacket,
    "?write@CraftingDataPacket@@UEBAXAEAVBinaryStream@@@Z",
    void,
    BinaryStream& stream
) {
    origin(stream);
    const std::string& data     = stream.getAndReleaseData();
    std::string        datacopy = data;
    stream.writeString(data, nullptr, nullptr);
    auto out = ofstream("data/crafting_data_packet.bin", ofstream::out | ofstream::binary | ofstream::trunc);
    out << datacopy;
    out.close();
    hookLogger.info("create crafting_data_packet.bin success!");
}

LL_AUTO_TYPE_INSTANCE_HOOK(
    AvailableCommandsPacketHook,
    ll::memory::HookPriority::Normal,
    AvailableCommandsPacket,
    "?write@AvailableCommandsPacket@@UEBAXAEAVBinaryStream@@@Z",
    void,
    BinaryStream& stream
) {
    origin(stream);
    const std::string& data     = stream.getAndReleaseData();
    const std::string  datacopy = data;
    stream.writeString(data, nullptr, nullptr);
    auto out = std::ofstream(
        "data/available_commands_packet.bin",
        std::ofstream::out | std::ofstream::binary | std::ofstream::trunc
    );
    out << datacopy;
    out.close();
    hookLogger.info("Create available_commands_packet.bin success!");
}

// Minecraft
LL_AUTO_TYPE_INSTANCE_HOOK(
    MinecraftHook,
    HookPriority::Normal,
    Minecraft,
    "?initAsDedicatedServer@Minecraft@@QEAAXXZ",
    void
) {
    mc = this;
    origin();
    hookLogger.info("INJECT MINECRAFT INSTANCE");
}

// MinecraftCommands
LL_AUTO_TYPE_INSTANCE_HOOK(
    MinecraftCommandsHook,
    HookPriority::Normal,
    MinecraftCommands,
    "?initCoreEnums@MinecraftCommands@@QEAAXVItemRegistryRef@@"
    "AEBVIWorldRegistriesProvider@@AEBVActorFactory@"
    "@AEBVExperiments@@AEBVBaseGameVersion@@@Z",
    void,
    void* a2,
    int64 a3,
    int64 a4,
    void* a5,
    void* a6
) {
    commands = this;
    origin(a2, a3, a4, a5, a6);
    hookLogger.info("INJECT MINECRAFTCOMMANDS INSTANCE");
}

// MinecraftCommands
LL_AUTO_TYPE_INSTANCE_HOOK(BlockHooK, HookPriority::Normal, Block, "__gen_??1Block@@UEAA@XZ", void) {
    dumpVtable(this);
    return origin();
}

#pragma endregion HOOK

#pragma region TOOL_FUNCTION
inline bool    isValidPtr(void* p) { return (p >= (void*)0x10000ull) && (p < (void*)0x000F000000000000ull); }

// 将 const char* const* 转换为 vector<string>
std::vector<std::string> convertToStringVector(const char* const* arr, size_t size) {
    std::vector<std::string> strings;
    for (size_t i = 0; i < size; ++i) {
        strings.push_back(arr[i]);
    }
    return strings;
}

std::vector<std::pair<void*, std::vector<std::string>>> dumpVtable(void* obj) {
    auto                                                    vtab  = *(uintptr_t**)obj;
    int                                                     count = 0;
    std::vector<std::pair<void*, std::vector<std::string>>> res;
    for (;; count++) {
        if (isValidPtr((void*)vtab[count])) {
            size_t*     address = 0;
            auto        result  = pl::symbol_provider::pl_lookup_symbol((void*)vtab[count], address);
            std::string str     = *result;
            if (*address != 0) {
                const auto& array = convertToStringVector(result, *address);
                res.push_back({(void*)vtab[count], array});
            }
        } else {
            break;
        }
    }
    return res;
}

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

void PluginInit() {
    ll::Logger logger("dataextractor");
    logger.info("DataExtractor plugin loaded!");
}

void extractData() {
    // ll::Logger logger("dataextractor");
    // logger.info("DataExtractor plugin start!");
    // if (!folderExists("data")) {
    //     createFolder(logger, "data");
    // }
    // dumpCommandNameSymbol(logger);
    // dumpCommonCommandArgData(logger);
    // dumpFullCommandArgData(logger);
    // dumpCommandmConstrainedValues(logger);
    // dumpBiomeData(logger);
    // dumpCreativeItemData(logger);
    // dumpBlockAttributesData(logger);
    // dumpItemData(logger);
    // dumpPalette(logger);
    // dumpBlockTags(logger);
    // dumpItemTags(logger);
    // dumpPropertyTypeData(logger);
}

void dumpCreativeItemData(const ll::Logger& logger) {
    logger.info("Extracting creative items...");

    auto         global = CompoundTag();
    unsigned int index  = 0;
    CreativeItemRegistry::forEachCreativeItemInstance([&logger, &index, &global](const ItemInstance& itemInstance) {
        if (itemInstance.getName().empty()) {
            logger.warn(
                "Failed to extract creative item - " + itemInstance.getName() + ", index: " + std::to_string(index)
            );
            return true;
        }
        logger.info("Extracting creative item - " + itemInstance.getName() + ", index: " + std::to_string(index));
        auto obj = CompoundTag();
        obj.putInt64("index", index);
        obj.putString("name", itemInstance.getItem()->getFullItemName());
        obj.putInt("damage", itemInstance.getAuxValue());
        if (itemInstance.isBlock()) {
            obj.putInt("blockStateHash", itemInstance.getBlock()->computeRawSerializationIdHashForNetwork());
        }
        if (const auto nbt = itemInstance.save(); nbt->contains("tag")) {
            obj.put("tag", nbt->getCompound("tag")->copy());
        }
        global.put(std::to_string(index), obj.copy());
        index++;
        return true;
    });
    writeNBT("data/creative_items.nbt", global);
    logger.info(R"(Creative items data has been saved to "data/creative_items.nbt")");
}

void extractBlock(ListTag& dest, const Block& block, const ll::Logger& logger) {
    auto nbt = CompoundTag();
    try {
        auto& legacy = block.getLegacyBlock();
        auto  name   = legacy.getNamespace() + ":" + legacy.getRawNameId();
        logger.info("Extracting block state - " + name + ":" + std::to_string(block.getRuntimeId()));
        const Material& material = legacy.getMaterial();
        auto            sid      = block.getSerializationId().clone();
        nbt.putString("name", sid->getString("name"));
        nbt.putString("descriptionId", block.getDescriptionId());
        nbt.putString("blockEntityName", std::string(magic_enum::enum_name(block.getBlockEntityType())));
        nbt.putCompound("states", sid->getCompound("states")->clone());
        nbt.putFloat("thickness", block.getThickness());
        nbt.putFloat("friction", block.getFriction());
        nbt.putFloat("hardness", block.getDestroySpeed());
        nbt.putFloat("explosionResistance", block.getExplosionResistance());
        nbt.putFloat("translucency", material.getTranslucency());
        nbt.putInt("version", sid->getInt("version"));
        nbt.putInt("runtimeId", block.getRuntimeId());
        if (name != "minecraft:unknown") nbt.putInt("blockStateHash", block.computeRawSerializationIdHashForNetwork());
        else nbt.putInt("blockStateHash", -2);
        nbt.putInt("burnChance", block.getFlameOdds());
        nbt.putInt("burnAbility", block.getBurnOdds());
        nbt.putInt("lightDampening", block.getLight().value);
        nbt.putInt("lightEmission", block.getLightEmission().value);
        mce::Color color    = block.getMapColor(overworld->getBlockSourceFromMainChunkSource(), BlockPos(0, 10, 0));
        auto       colornbt = CompoundTag();
        colornbt.putInt("r", static_cast<int>(color.r * 255));
        colornbt.putInt("g", static_cast<int>(color.g * 255));
        colornbt.putInt("b", static_cast<int>(color.b * 255));
        colornbt.putInt("a", static_cast<int>(color.a * 255));
        colornbt.putString("hexString", color.toHexString());
        nbt.putCompound("color", colornbt);
        auto tmp = AABB(0, 0, 0, 0, 0, 0);
        block.getCollisionShapeForCamera(
            tmp,
            *reinterpret_cast<IConstBlockSource*>(&overworld->getBlockSourceFromMainChunkSource()),
            BlockPos(0, 0, 0)
        );
        nbt.putString("aabbVisual", aabbToStr(tmp));
        auto                                           tmp2 = AABB(0, 0, 0, 0, 0, 0);
        optional_ref<GetCollisionShapeInterface const> nullRef{};
        block.getCollisionShape(
            tmp2,
            *reinterpret_cast<IConstBlockSource*>(&overworld->getBlockSourceFromMainChunkSource()),
            BlockPos(0, 0, 0),
            nullRef
        );
        nbt.putString("aabbCollision", aabbToStr(tmp2));
        nbt.putBoolean("hasCollision", tmp2 != ZERO_AABB);
        nbt.putBoolean("hasBlockEntity", block.getBlockEntityType() != BlockActorType::Undefined);
        nbt.putBoolean("isAir", block.isAir());
        nbt.putBoolean("isBounceBlock", block.isAir());
        nbt.putBoolean("isButtonBlock", block.isButtonBlock());
        nbt.putBoolean("isCropBlock", block.isCropBlock());
        nbt.putBoolean("isDoorBlock", block.isDoorBlock());
        nbt.putBoolean("isFenceBlock", block.isFenceBlock());
        nbt.putBoolean("isFenceGateBlock", block.isFenceGateBlock());
        nbt.putBoolean("isThinFenceBlock", block.isThinFenceBlock());
        nbt.putBoolean("isFallingBlock", block.isFallingBlock());
        nbt.putBoolean("isStemBlock", block.isStemBlock());
        nbt.putBoolean("isSlabBlock", block.isSlabBlock());
        nbt.putBoolean("isLiquid", material.isLiquid());
        nbt.putBoolean("isAlwaysDestroyable", material.isAlwaysDestroyable());
        nbt.putBoolean("isLavaFlammable", block.isLavaFlammable());
        nbt.putBoolean("isUnbreakable", block.isUnbreakable());
        nbt.putBoolean("isPowerSource", block.isSignalSource());
        // nbt->putBoolean("breaksFallingBlocks", block.breaksFallingBlocks(BaseGameVersion()));
        nbt.putBoolean("isWaterBlocking", block.isWaterBlocking());
        nbt.putBoolean("isMotionBlockingBlock", block.isMotionBlockingBlock());
        nbt.putBoolean("hasComparatorSignal", block.hasComparatorSignal());
        nbt.putBoolean("pushesUpFallingBlocks", block.pushesUpFallingBlocks());
        // nbt->putBoolean("waterSpreadCausesSpawn", block.waterSpreadCausesSpawn());
        nbt.putBoolean("canContainLiquid", block.getLegacyBlock().canContainLiquid());
        // nbt->putBoolean("canBeMovingBlock", material.getBlocksMotion());
        // nbt->putBoolean("blocksPrecipitation", material.getBlocksPrecipitation());
        nbt.putBoolean("superHot", material.isSuperHot());
        // nbt->putBoolean("canBeBrokenFromFalling", block.canBeBrokenFromFalling());
        nbt.putBoolean("isSolid", block.isSolid());
        // nbt->putBoolean("isSolidBlocking", material.isSolidBlocking());
        nbt.putBoolean("isContainerBlock", block.isContainerBlock());
    } catch (char* e) {
        logger.error("Exception caught {}", e);
    }

    dest.add(nbt);
}

void dumpBlockAttributesData(const ll::Logger& logger) {
    logger.info("Extracting block states' attributes...");
    const auto& palette  = mc->getLevel()->getBlockPalette();
    int         airCount = 0;
    auto        array    = nlohmann::json::array();

    auto tag          = CompoundTag();
    auto list         = ListTag();
    blockStateCounter = 0;
    while (true) {
        const auto& block = palette.getBlock(blockStateCounter);
        if (block.getName().getString() == "minecraft:air") {
            airCount++;
            if (airCount == 2) {
                blockStateCounter--;
                break;
            }
        }
        extractBlock(list, block, logger);
        blockStateCounter++;
    }
    tag.put("block", list.copyList());
    logger.info("Successfully extract " + std::to_string(blockStateCounter) + " block states' attributes!");
    writeNBT("data/block_attributes.nbt", tag);
    logger.info(R"(Block attribute data have been saved to "data/block_attributes.nbt")");
}

void extractItem(ListTag& dest, const Item& item, const ll::Logger& logger) {
    auto nbt = CompoundTag();
    logger.info("Extracting item - " + item.getFullItemName());
    nbt.putShort("id", item.getId());
    try {
        if (!item.getLegacyBlock().expired() && item.getLegacyBlock().get() != nullptr)
            nbt.putString(
                "blockId",
                item.getLegacyBlock()->getNamespace() + ":" + item.getLegacyBlock()->getRawNameId()
            );
    } catch (std::exception& e) {
        logger.warn("Exception occur when trying to get block for item " + item.getFullItemName());
    }
    nbt.putBoolean("isComponentBased", item.isComponentBased());
    nbt.putString("name", item.getFullItemName());
    nbt.putShort("maxDamage", item.getMaxDamage());
    nbt.putBoolean("isArmor", item.isArmor());
    nbt.putBoolean("isBlockPlanterItem", item.isBlockPlanterItem());
    nbt.putBoolean("isDamageable", item.isDamageable());
    nbt.putBoolean("isDye", item.isDye());
    nbt.putString("itemColorName", ItemColorUtil::getName(item.getItemColor()));
    nbt.putInt("itemColorRGB", ItemColorUtil::getRGBColor(item.getItemColor()));
    nbt.putBoolean("isFertilizer", item.isFertilizer());
    nbt.putBoolean("isThrowable", item.isThrowable());
    nbt.putBoolean("isFood", item.isFood());
    nbt.putBoolean("isUseable", item.isUseable());
    nbt.putBoolean("isElytra", item.isElytra());
    nbt.putBoolean("canBeDepleted", item.canBeDepleted());
    nbt.putBoolean("canDestroyInCreative", item.canDestroyInCreative());
    nbt.putBoolean("canUseOnSimTick", item.canUseOnSimTick());
    nbt.putBoolean("canBeCharged", item.canBeCharged());
    nbt.putString("creativeGroup", item.getCreativeGroup());
    nbt.putInt("creativeCategory", static_cast<int>(item.getCreativeCategory()));
    nbt.putInt("armorValue", item.getArmorValue());
    nbt.putInt("attackDamage", item.getAttackDamage());
    nbt.putInt("toughnessValue", item.getToughnessValue());
    nbt.putFloat("viewDamping", item.getViewDamping());
    nbt.putInt("cooldownTime", item.getCooldownTime());
    nbt.putString("cooldownType", item.getCooldownType().getString());
    nbt.putInt("maxStackSize", item.getMaxStackSize(item.buildDescriptor(0, nullptr)));
    CompoundTag           descriptionId;
    std::set<std::string> uniqueStr;
    for (int i = 0; i <= 256; ++i) {
        try {
            if (item.isValidAuxValue(i)) {
                const auto itemstack = ItemStack(item, 1, i); // ignore some invaild aux exception
                if (!uniqueStr.contains(itemstack.getDescriptionId())) {
                    uniqueStr.insert(itemstack.getDescriptionId());
                    descriptionId.putString(std::to_string(i), itemstack.getDescriptionId());
                }
            }
        } catch (...) {}
    }
    nbt.putCompound("descriptionId", descriptionId);
    dest.add(nbt);
}

void dumpItemData(const ll::Logger& logger) {
    auto  tag     = CompoundTag();
    auto  list    = ListTag();
    short counter = 0;
    for (const auto& item : ItemRegistryManager::getItemRegistry().getNameToItemMap() | std::views::values) {
        extractItem(list, *item, logger);
        counter++;
    }
    tag.put("item", list);
    logger.info("Successfully extract " + std::to_string(counter) + " items' data!");
    writeNBT("data/item_data.nbt", tag);
    logger.info(R"(Items' data have been saved to "data/item_data.nbt")");
}

void dumpPalette(const ll::Logger& logger) {
    logger.info("Extracting block palette...");

    const auto& palette = mc->getLevel()->getBlockPalette();

    auto global = CompoundTag();
    auto blocks = ListTag();
    for (int i = 0; i <= blockStateCounter; ++i) {
        blocks.add(palette.getBlock(i).getSerializationId().clone());
    }
    global.put("blocks", blocks);
    writeNBT("data/block_palette.nbt", global);
    logger.info(R"(Block palette table has been saved to "data/block_palette.nbt"))");
}

bool compareCmdSymbolByIndex(const CommandRegistry::Symbol& s1, const CommandRegistry::Symbol& s2) {
    return s1.toIndex() < s2.toIndex();
}

bool compareCmdSymbolByValue(const CommandRegistry::Symbol& s1, const CommandRegistry::Symbol& s2) {
    return s1.value() < s2.value();
}

template <typename JSON_TYPE>
void dumpCmdSymbol1(
    const ll::Logger&              logger,
    const CommandRegistry&         registry,
    JSON_TYPE&                     json,
    const CommandRegistry::Symbol& symbol
) {
    const std::string name = registry.symbolToString(symbol);
    logger.info("Extracting command arg type - " + name);
    auto obj           = std::map<std::string, std::string>();
    obj["description"] = registry.describe(symbol);
    obj["index"]       = std::to_string(symbol.toIndex());
    obj["value"]       = std::to_string(symbol.value());
    json[name]         = obj;
}

template <typename JSON_TYPE>
void dumpCmdSymbol2(
    const ll::Logger&              logger,
    const CommandRegistry&         registry,
    JSON_TYPE&                     json,
    const CommandRegistry::Symbol& symbol,
    const std::string&             key
) {
    const std::string name = registry.symbolToString(symbol);
    logger.info("Extracting command arg type - " + name);
    auto obj           = std::map<std::string, std::string>();
    obj["name"]        = name;
    obj["description"] = registry.describe(symbol);
    obj["index"]       = std::to_string(symbol.toIndex());
    obj["value"]       = std::to_string(symbol.value());
    json[key]          = obj;
}

void dumpCommandNameSymbol(const ll::Logger& logger) {
    const auto& registry             = mc->getCommands().getRegistry();
    auto        symbols_sortby_index = std::vector<CommandRegistry::Symbol>();
    auto        symbols_sortby_value = std::vector<CommandRegistry::Symbol>();
    for (auto& symbol : registry.mCommandSymbols) {
        if (!registry.isValid(symbol)) {
            continue;
        }
        if (auto name = registry.symbolToString(symbol); name == "ll" || name == "ext") {
            continue;
        }
        symbols_sortby_index.push_back(symbol);
        symbols_sortby_value.push_back(symbol);
    }
    // Sort symbol by index
    std::ranges::sort(symbols_sortby_index, compareCmdSymbolByIndex);
    std::ranges::sort(symbols_sortby_value, compareCmdSymbolByValue);
    nlohmann::ordered_json global1;
    for (auto& symbol : symbols_sortby_index) {
        dumpCmdSymbol1(logger, registry, global1, symbol);
    }
    writeNlohmannJSON("data/command_name_symbol_i.json", global1);
    nlohmann::ordered_json global2;
    for (auto& symbol : symbols_sortby_value) {
        dumpCmdSymbol1(logger, registry, global2, symbol);
    }
    writeNlohmannJSON("data/command_name_symbol_v.json", global2);
    logger.info("Command name symbol have been saved to \"data/command_name_symbol_(i/v).json\"");
}

void dumpCommonCommandArgData(const ll::Logger& logger) {
    const auto&            registry = mc->getCommands().getRegistry();
    nlohmann::ordered_json global;
    for (int i = 0; i < 1000; i++) {
        if (const int symbol = i | 0x100000; registry.isValid(symbol)) {
            dumpCmdSymbol1(logger, registry, global, symbol);
        }
    }
    writeNlohmannJSON("data/command_arg_types_common_i.json", global);
    logger.info("Common command arg type have been saved to \"data/command_arg_types_common_i.json\"");
}

void dumpFullCommandArgData(const ll::Logger& logger) {
    auto& registry             = mc->getCommands().getRegistry();
    auto  symbols_sortby_index = std::vector<CommandRegistry::Symbol>();
    auto  symbols_sortby_value = std::vector<CommandRegistry::Symbol>();
    registry.forEachNonTerminal(
        [&registry, &symbols_sortby_index, &symbols_sortby_value](const CommandRegistry::Symbol& symbol) {
            if (!registry.isValid(symbol)) {
                return;
            }
            symbols_sortby_index.push_back(symbol);
            symbols_sortby_value.push_back(symbol);
        }
    );
    // Sort symbol by index
    std::ranges::sort(symbols_sortby_index, compareCmdSymbolByIndex);
    nlohmann::ordered_json global1;
    for (auto& symbol : symbols_sortby_index) {
        dumpCmdSymbol1(logger, registry, global1, symbol);
    }
    writeNlohmannJSON("data/command_arg_types_full_i.json", global1);
    nlohmann::ordered_json global2;
    for (auto& symbol : symbols_sortby_value) {
        dumpCmdSymbol1(logger, registry, global2, symbol);
    }
    writeNlohmannJSON("data/command_arg_types_full_v.json", global2);
    logger.info("Full command arg type data have been saved to \"data/command_arg_types_full_(i/v).json\"");
}

void dumpCommandmConstrainedValues(const ll::Logger& logger) {
    auto& registry = mc->getCommands().getRegistry();
    auto  array    = nlohmann::json::array();
    for (auto& [mValue, mEnum, mConstraints] : registry.mConstrainedValues) {
        auto obj = nlohmann::json::object();
        dumpCmdSymbol2(logger, registry, obj, mValue, "value");
        dumpCmdSymbol2(logger, registry, obj, mEnum, "enum");
        obj["constraints"] = mConstraints;
        array.push_back(obj);
    }
    writeNlohmannJSON("data/command_constrained_values.json", array);
    logger.info("Command constrained values data have been saved to \"data/command_constrained_values.json\"");
}

void dumpBiomeData(const ll::Logger& logger) {
    BiomeRegistry&                                                  registry     = mc->getLevel()->getBiomeRegistry();
    auto                                                            biomeInfoMap = nlohmann::json::object();
    auto                                                            biomes       = CompoundTag();
    TagRegistry<IDType<BiomeTagIDType>, IDType<BiomeTagSetIDType>>& tagReg       = registry.getTagRegistry();
    registry.forEachBiome([&biomes, &logger, &tagReg, &biomeInfoMap](Biome& biome) {
        auto& name = biome.getName();
        int   id   = biome.getId();
        logger.info("Extracting biome data - " + name);
        auto tag = CompoundTag();
        biome.writePacketData(tag, tagReg);
        biomes.put(name, tag);
        auto obj           = nlohmann::json::object();
        obj["id"]          = id;
        obj["type"]        = std::string(magic_enum::enum_name(biome.getBiomeType()));
        biomeInfoMap[name] = obj;
    });
    writeNBT("data/biome_definitions.nbt", biomes);
    writeNlohmannJSON("data/biome_id_and_type.json", biomeInfoMap);
    logger.info(R"(Biome definitions has been saved to "data/biome_definitions.nbt" and "data/biome_id_and_type.json")"
    );
}

struct PropertyType {
    std::string           serializationName;
    std::string           valueType;
    std::set<std::string> values;
    std::string           blockName;

    bool operator==(const PropertyType& other) const {
        return serializationName == other.serializationName && valueType == other.valueType
            && values.size() == other.values.size();
    }
};

void dumpPropertyTypeData(const ll::Logger& logger) {
    logger.info("Extracting property type data...");

    std::map<std::string, std::vector<std::unique_ptr<class CompoundTag>>> blockToBlockStateData;

    auto& palette = mc->getLevel()->getBlockPalette();
    for (int i = 0; i <= blockStateCounter; ++i) {
        const Block& block = palette.getBlock(i);
        auto         name  = block.getLegacyBlock().getRawNameId();
        if (!blockToBlockStateData.contains(name)) {
            blockToBlockStateData[name] = std::vector<std::unique_ptr<class CompoundTag>>();
        }
        auto& blockStates = blockToBlockStateData[name];
        if (auto nbt = block.getSerializationId().clone();
            nbt->contains("states") && !nbt->getCompound("states")->isEmpty()) {
            blockStates.push_back(nbt->getCompound("states")->clone());
        }
    }

    std::map<std::string, std::map<std::string, PropertyType>> blockToPropertyTypeMap;

    for (auto& entry : blockToBlockStateData) {
        auto&                               states = entry.second;
        std::map<std::string, PropertyType> propertyTypeMap;

        for (auto& state : states) {
            for (auto& valueEntry : state->rawView()) {
                if (!propertyTypeMap.contains(valueEntry.first)) {
                    PropertyType p;
                    p.serializationName                  = valueEntry.first;
                    p.blockName                          = entry.first;
                    propertyTypeMap[p.serializationName] = p;
                }
                auto& propertyType = propertyTypeMap[valueEntry.first];
                switch (valueEntry.second.getId()) {
                case Tag::Type::Byte:
                    if (propertyType.valueType.empty()) {
                        propertyType.valueType = "BOOLEAN";
                    }
                    propertyType.values.insert(state->getBoolean(valueEntry.first) ? "true" : "false");
                    break;
                case Tag::Type::Int:
                    if (propertyType.valueType.empty()) {
                        propertyType.valueType = "INTEGER";
                    }
                    propertyType.values.insert(std::to_string(state->getInt(valueEntry.first)));
                    break;
                case Tag::Type::String:
                    if (propertyType.valueType.empty()) {
                        propertyType.valueType = "ENUM";
                    }
                    propertyType.values.insert(state->getString(valueEntry.first));
                    break;
                default:
                    if (propertyType.valueType.empty()) {
                        propertyType.valueType = "UNKNOWN";
                    }
                    logger.warn("Unknown tag type when dumping property type data: " + valueEntry.first);
                    break;
                }
            }
        }
        blockToPropertyTypeMap[entry.first] = propertyTypeMap;
    }

    std::set<std::string>                                     differentSizePropertyTypes;
    std::map<std::string, std::map<std::string, std::string>> specialBlockTypes;
    std::map<std::string, PropertyType>                       tmpLookUp;

    for (auto& entry : blockToPropertyTypeMap) {
        for (auto& entryInside : entry.second) {
            auto& propertyName = entryInside.first;
            auto& propertyType = entryInside.second;
            if (!tmpLookUp.contains(propertyName)) {
                tmpLookUp[propertyName] = propertyType;
            } else if (tmpLookUp[propertyName] != propertyType && !differentSizePropertyTypes.contains(propertyName)) {
                logger.warn("Property type \"" + propertyName + "\" has different size in different blocks!");
                differentSizePropertyTypes.insert(propertyName);
                auto fullBlockName = "minecraft:" + entry.first;
                if (!specialBlockTypes.contains(fullBlockName)) {
                    specialBlockTypes[fullBlockName] = std::map<std::string, std::string>();
                }
            }
        }
    }

    std::map<std::string, PropertyType> globalPropertyTypeMap;

    for (auto& entry : blockToPropertyTypeMap) {
        for (auto& entryInside : entry.second) {
            auto&                  propertyName = entryInside.first;
            auto&                  propertyType = entryInside.second;
            auto                   keyName      = std::string(propertyName);
            std::string::size_type pos          = 0;
            while ((pos = keyName.find(':', pos)) != std::string::npos) {
                keyName.replace(pos, 1, "_");
                pos++;
            }
            if (!differentSizePropertyTypes.contains(propertyName)) {
                globalPropertyTypeMap[keyName] = propertyType;
            } else {
                auto newKey                               = keyName + "_" + std::to_string(propertyType.values.size());
                auto fullBlockName                        = "minecraft:" + entry.first;
                specialBlockTypes[fullBlockName][keyName] = newKey;
                globalPropertyTypeMap[newKey]             = propertyType;
            }
        }
    }

    auto globalJson    = nlohmann::json::object();
    auto propertyTypes = nlohmann::json::object();

    for (auto& propertyTypeEntry : globalPropertyTypeMap) {
        if (propertyTypeEntry.second.serializationName.empty()) {
            continue;
        }
        auto obj = nlohmann::json::object();

        obj["serializationName"] = propertyTypeEntry.second.serializationName;
        obj["valueType"]         = propertyTypeEntry.second.valueType;
        if (propertyTypeEntry.second.valueType == "INTEGER") {
            std::vector<int> values;
            for (auto& value : propertyTypeEntry.second.values) {
                values.push_back(stoi(value));
            }
            std::ranges::sort(values);
            obj["values"] = values;
        } else if (propertyTypeEntry.second.valueType == "BOOLEAN") {
            std::vector<bool> values{false, true};
            obj["values"] = values;
        } else {
            obj["values"] = propertyTypeEntry.second.values;
        }

        propertyTypes[propertyTypeEntry.first] = obj;
    }
    globalJson["propertyTypes"]              = propertyTypes;
    globalJson["differentSizePropertyTypes"] = differentSizePropertyTypes;
    globalJson["specialBlockTypes"]          = specialBlockTypes;
    writeNlohmannJSON("data/block_property_types.json", globalJson);
    logger.info("Block property type data have been saved to \"data/block_property_types.json\"");
}

void dumpItemTags(const ll::Logger& logger) {
    logger.info("Extracting item tags...");
    nlohmann::json res = nlohmann::json::object();
#define DUMP(TAG)                                                                                                      \
    do {                                                                                                               \
        auto items = ItemRegistryManager::getItemRegistry().lookupByTag(VanillaItemTags::##TAG);                       \
        auto arr   = nlohmann::json::array();                                                                          \
        for (auto item : items) {                                                                                      \
            arr.push_back(item->getFullItemName());                                                                    \
        }                                                                                                              \
        res[VanillaItemTags::##TAG.getString()] = arr;                                                                 \
        logger.info(VanillaItemTags::##TAG.getString());                                                               \
    } while (false)
    DUMP(Armor);
    DUMP(Arrows);
    DUMP(Banners);
    DUMP(Boat);
    DUMP(Boats);
    DUMP(BookshelfBooks);
    DUMP(ChainmailTier);
    DUMP(ChestBoat);
    DUMP(Coals);
    DUMP(Cooked);
    DUMP(CrimsonStems);
    DUMP(DecoratedPotSherds);
    DUMP(DiamondTier);
    DUMP(Digger);
    DUMP(Door);
    DUMP(Fishes);
    DUMP(Food);
    DUMP(GoldenTier);
    DUMP(HangingActor);
    DUMP(HangingSign);
    DUMP(Hatchet);
    DUMP(Hoe);
    DUMP(HorseArmor);
    DUMP(IronTier);
    DUMP(LeatherTier);
    DUMP(LecternBooks);
    DUMP(Logs);
    DUMP(LogsThatBurn);
    DUMP(MangroveLogs);
    DUMP(Meat);
    DUMP(Minecart);
    DUMP(MusicDiscs);
    DUMP(NetheriteTier);
    DUMP(Pickaxe);
    DUMP(PiglinLoved);
    DUMP(PiglinRepellents);
    DUMP(Planks);
    DUMP(Sand);
    DUMP(Shovel);
    DUMP(Sign);
    DUMP(SoulFireBaseBlocks);
    DUMP(SpawnEgg);
    DUMP(StoneBricks);
    DUMP(StoneCraftingMaterials);
    DUMP(StoneTier);
    DUMP(StoneToolMaterials);
    DUMP(Sword);
    DUMP(Tool);
    DUMP(TransformMaterials);
    DUMP(TransformTemplates);
    DUMP(TransformableItems);
    DUMP(Trident);
    DUMP(TrimMaterials);
    DUMP(TrimTemplates);
    DUMP(TrimmableArmors);
    DUMP(VibrationDamper);
    DUMP(WarpedStems);
    DUMP(WoodenSlabs);
    DUMP(WoodenTier);
    DUMP(Wool);
#undef DUMP
    writeNlohmannJSON("data/item_tags.json", res);
}

void dumpBlockTags(const ll::Logger& logger) {
    logger.info("Extracting block tags...");
    nlohmann::json res = nlohmann::json::object();
#define DUMP(TAG)                                                                                                      \
    do {                                                                                                               \
        auto arr = nlohmann::json::array();                                                                            \
        BlockTypeRegistry::forEachBlock([&arr](const BlockLegacy& b) {                                                 \
            if (b.hasTag(VanillaBlockTags::##TAG)) {                                                                   \
                arr.push_back(b.getRawNameId());                                                                       \
            }                                                                                                          \
            return true;                                                                                               \
        });                                                                                                            \
        res[VanillaBlockTags::##TAG.getString()] = arr;                                                                \
        logger.info(VanillaBlockTags::##TAG.getString());                                                              \
    } while (false)
    DUMP(Acacia);
    DUMP(Birch);
    DUMP(Crop);
    DUMP(DarkOak);
    DUMP(DiamondDiggable);
    DUMP(Dirt);
    DUMP(FertilizeArea);
    DUMP(GoldDiggable);
    DUMP(Grass);
    DUMP(Gravel);
    DUMP(IronDiggable);
    DUMP(Jungle);
    DUMP(Log);
    DUMP(Metal);
    DUMP(MobSpawner);
    DUMP(NotFeatureReplaceable);
    DUMP(Oak);
    DUMP(Plant);
    DUMP(Pumpkin);
    DUMP(Rail);
    DUMP(Sand);
    DUMP(Snow);
    DUMP(Spruce);
    DUMP(Stone);
    DUMP(StoneDiggable);
    DUMP(TextSign);
    DUMP(Water);
    DUMP(Wood);
    DUMP(WoodDiggable);
#undef DUMP
    writeNlohmannJSON("data/block_tags.json", res);
}

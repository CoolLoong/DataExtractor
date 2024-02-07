#pragma once

// #define LL_HOOK_DEBUG

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "ll/base/Concepts.h" // IWYU pragma: keep
#include "ll/base/FixedString.h"
#include "ll/base/Macro.h"
#include "ll/memory/Memory.h"
#include "ll/reflection/TypeName.h"

namespace ll::memory {

template <class T>
struct IsConstMemberFun : std::false_type {};

template <class T, class Ret, class... Args>
struct IsConstMemberFun<Ret (T::*)(Args...) const> : std::true_type {};

template <class T>
constexpr bool IsConstMemberFunV = IsConstMemberFun<T>::value;

template <class T>
struct AddConstAtMemberFun {
    using type = T;
};

template <class T, class Ret, class... Args>
struct AddConstAtMemberFun<Ret (T::*)(Args...)> {
    using type = Ret (T::*)(Args...) const;
};

template <class T>
using AddConstAtMemberFunT = typename AddConstAtMemberFun<T>::type;

/**
 * @brief Hook priority enum.
 * @details The higher priority, the hook will be executed earlier
 */
enum class HookPriority : int {
    Highest = 0,
    High    = 100,
    Normal  = 200,
    Low     = 300,
    Lowest  = 400,
};

LLAPI int hook(FuncPtr target, FuncPtr detour, FuncPtr* originalFunc, HookPriority priority);

LLAPI bool unhook(FuncPtr target, FuncPtr detour);

/**
 * @brief Get the pointer of a function by identifier.
 *
 * @param identifier symbol or signature
 * @return FuncPtr
 */
LLNDAPI FuncPtr resolveIdentifier(std::string_view identifier, bool disableErrorOutput = false);

template <class T>
concept FuncPtrType = std::is_function_v<std::remove_pointer_t<T>> || std::is_member_function_pointer_v<T>;

template <FuncPtrType T>
constexpr FuncPtr resolveIdentifier(T identifier) {
    return toFuncPtr(identifier);
}

// redirect to resolveIdentifier(std::string_view)
template <class T>
constexpr FuncPtr resolveIdentifier(std::string_view identifier) {
    return resolveIdentifier(identifier);
}

template <class T>
constexpr FuncPtr resolveIdentifier(uintptr_t address) {
    return toFuncPtr(address);
}

template <class T>
constexpr FuncPtr resolveIdentifier(void* address) {
    return address;
}

template <class, FixedString>
consteval bool virtualDetector() noexcept {
    return false;
}
template <class T, T f>
consteval bool virtualDetector() noexcept {
    return reflection::getRawName<f>().find("::`vcall'{") != std::string::npos;
}

template <class... Ts>
class HookRegistrar {
public:
    static inline std::atomic_uint count{};

    static void hook() { (Ts::hook(), ...); }
    static void unhook() { (Ts::unhook(), ...); }

    HookRegistrar() noexcept {
        if (++count == 1) {
            hook();
        }
    }
    ~HookRegistrar() {
        if (--count == 0) {
            unhook();
        }
    }
                   HookRegistrar(HookRegistrar const&) noexcept { ++count; }
    HookRegistrar& operator=(HookRegistrar const& other) noexcept {
        if (this != std::addressof(other)) {
            ++count;
        }
        return *this;
    }

                   HookRegistrar(HookRegistrar&&) noexcept = default;
    HookRegistrar& operator=(HookRegistrar&&) noexcept     = default;
};

struct __declspec(empty_bases) Hook {};

} // namespace ll::memory

#define LL_HOOK_IMPL(REGISTER, FUNC_PTR, STATIC, CALL, DEF_TYPE, TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)            \
    struct DEF_TYPE : public TYPE {                                                                                    \
        using _FuncPtr          = ::ll::memory::FuncPtr;                                                               \
        using HookPriority      = ::ll::memory::HookPriority;                                                          \
        using _RawFuncType      = RET_TYPE FUNC_PTR(__VA_ARGS__);                                                      \
        using _RawConstFuncType = ::ll::memory::AddConstAtMemberFunT<_RawFuncType>;                                    \
                                                                                                                       \
        template <class T>                                                                                             \
        struct _ConstDetector {                                                                                        \
            [[maybe_unused]] static constexpr bool value = false;                                                      \
            explicit constexpr                     _ConstDetector(T) {}                                                \
        };                                                                                                             \
        template <class T>                                                                                             \
        [[maybe_unused]] _ConstDetector(T) -> _ConstDetector<T>;                                                       \
        [[maybe_unused]] _ConstDetector(_RawFuncType) -> _ConstDetector<_RawFuncType>;                                 \
        template <>                                                                                                    \
        struct _ConstDetector<_RawConstFuncType> {                                                                     \
            [[maybe_unused]] static constexpr bool value = true;                                                       \
            explicit constexpr                     _ConstDetector(_RawConstFuncType) {}                                \
        };                                                                                                             \
        template <class T = _RawFuncType, std::enable_if_t<std::is_member_function_pointer_v<T>, int> = 0>             \
        [[maybe_unused]] _ConstDetector(_RawConstFuncType) -> _ConstDetector<_RawConstFuncType>;                       \
                                                                                                                       \
        static constexpr bool _IsConstMemberFunction = decltype(_ConstDetector{IDENTIFIER})::value;                    \
                                                                                                                       \
        using _OriginFuncType = std::conditional_t<_IsConstMemberFunction, _RawConstFuncType, _RawFuncType>;           \
                                                                                                                       \
        inline static _FuncPtr        _HookTarget{};                                                                   \
        inline static _OriginFuncType _OriginalFunc{};                                                                 \
                                                                                                                       \
        template <class T>                                                                                             \
        static consteval void detector() {                                                                             \
            if constexpr (requires { ::ll::memory::virtualDetector<T, IDENTIFIER>(); }) {                              \
                if constexpr (::ll::memory::virtualDetector<T, IDENTIFIER>()) {                                        \
                    static_assert(                                                                                     \
                        ::ll::concepts::always_false<T>,                                                               \
                        #IDENTIFIER " is a virtual function, you need use prefix $ workaround to hook it."             \
                    );                                                                                                 \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
                                                                                                                       \
        template <class... Args>                                                                                       \
        STATIC RET_TYPE origin(Args&&... params) {                                                                     \
            return CALL(std::forward<Args>(params)...);                                                                \
        }                                                                                                              \
                                                                                                                       \
        STATIC RET_TYPE detour(__VA_ARGS__);                                                                           \
                                                                                                                       \
        static int hook() {                                                                                            \
            detector<_OriginFuncType>();                                                                               \
            _HookTarget = ::ll::memory::resolveIdentifier<_OriginFuncType>(IDENTIFIER);                                \
            if (_HookTarget == nullptr) {                                                                              \
                return -1;                                                                                             \
            }                                                                                                          \
            return ::ll::memory::hook(                                                                                 \
                _HookTarget,                                                                                           \
                ::ll::memory::toFuncPtr(&DEF_TYPE::detour),                                                            \
                reinterpret_cast<_FuncPtr*>(&_OriginalFunc),                                                           \
                PRIORITY                                                                                               \
            );                                                                                                         \
        }                                                                                                              \
                                                                                                                       \
        static bool unhook() { return ::ll::memory::unhook(_HookTarget, ::ll::memory::toFuncPtr(&DEF_TYPE::detour)); } \
    };                                                                                                                 \
    REGISTER;                                                                                                          \
    RET_TYPE DEF_TYPE::detour(__VA_ARGS__)

#define LL_AUTO_REG_HOOK_IMPL(FUNC_PTR, STATIC, CALL, DEF_TYPE, ...)                                                   \
    LL_VA_EXPAND(LL_HOOK_IMPL(                                                                                         \
        inline ll::memory::HookRegistrar<DEF_TYPE> DEF_TYPE##AutoRegister,                                             \
        FUNC_PTR,                                                                                                      \
        STATIC,                                                                                                        \
        CALL,                                                                                                          \
        DEF_TYPE,                                                                                                      \
        __VA_ARGS__                                                                                                    \
    ))

#define LL_MANUAL_REG_HOOK_IMPL(...) LL_VA_EXPAND(LL_HOOK_IMPL(, __VA_ARGS__))

#define LL_STATIC_HOOK_IMPL(...) LL_VA_EXPAND(LL_MANUAL_REG_HOOK_IMPL((*), static, _OriginalFunc, __VA_ARGS__))

#define LL_AUTO_STATIC_HOOK_IMPL(...) LL_VA_EXPAND(LL_AUTO_REG_HOOK_IMPL((*), static, _OriginalFunc, __VA_ARGS__))

#define LL_INSTANCE_HOOK_IMPL(DEF_TYPE, TYPE, ...)                                                                     \
    LL_VA_EXPAND(LL_MANUAL_REG_HOOK_IMPL((TYPE::*), , (this->*_OriginalFunc), DEF_TYPE, TYPE, __VA_ARGS__))

#define LL_AUTO_INSTANCE_HOOK_IMPL(DEF_TYPE, TYPE, ...)                                                                \
    LL_VA_EXPAND(LL_AUTO_REG_HOOK_IMPL((TYPE::*), , (this->*_OriginalFunc), DEF_TYPE, TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a typed static function.
 * @param DEF_TYPE The name of the hook definition.
 * @param PRIORITY ll::memory::HookPriority The priority of the hook.
 * @param TYPE The type which the function belongs to.
 * @param IDENTIFIER The identifier of the hook. It can be a function pointer, symbol, address or a signature.
 * @param RET_TYPE The return type of the hook.
 * @param ... The parameters of the hook.
 *
 * @note register or unregister by calling DEF_TYPE::hook() and DEF_TYPE::unhook().
 */
#define LL_TYPE_STATIC_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)                                       \
    LL_VA_EXPAND(LL_STATIC_HOOK_IMPL(DEF_TYPE, TYPE, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a static function.
 * @param DEF_TYPE The name of the hook definition.
 * @param PRIORITY ll::memory::HookPriority The priority of the hook.
 * @param IDENTIFIER The identifier of the hook. It can be a function pointer, symbol, address or a signature.
 * @param RET_TYPE The return type of the hook.
 * @param ... The parameters of the hook.
 *
 * @note register or unregister by calling DEF_TYPE::hook() and DEF_TYPE::unhook().
 */
#define LL_STATIC_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)                                                  \
    LL_VA_EXPAND(LL_STATIC_HOOK_IMPL(DEF_TYPE, ::ll::memory::Hook, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a typed static function.
 * @details The hook will be automatically registered and unregistered.
 * @see LL_TYPE_STATIC_HOOK for usage.
 */
#define LL_AUTO_TYPE_STATIC_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)                                  \
    LL_VA_EXPAND(LL_AUTO_STATIC_HOOK_IMPL(DEF_TYPE, TYPE, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a static function.
 * @details The hook will be automatically registered and unregistered.
 * @see LL_STATIC_HOOK for usage.
 */
#define LL_AUTO_STATIC_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)                                             \
    LL_VA_EXPAND(LL_AUTO_STATIC_HOOK_IMPL(DEF_TYPE, ::ll::memory::Hook, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a typed instance function.
 * @param DEF_TYPE The name of the hook definition.
 * @param PRIORITY ll::memory::HookPriority The priority of the hook.
 * @param TYPE The type which the function belongs to.
 * @param IDENTIFIER The identifier of the hook. It can be a function pointer, symbol, address or a signature.
 * @param RET_TYPE The return type of the hook.
 * @param ... The parameters of the hook.
 *
 * @note register or unregister by calling DEF_TYPE::hook() and DEF_TYPE::unhook().
 */
#define LL_TYPE_INSTANCE_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)                                     \
    LL_VA_EXPAND(LL_INSTANCE_HOOK_IMPL(DEF_TYPE, TYPE, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a instance function.
 * @param DEF_TYPE The name of the hook definition.
 * @param PRIORITY ll::memory::HookPriority The priority of the hook.
 * @param IDENTIFIER The identifier of the hook. It can be a function pointer, symbol, address or a signature.
 * @param RET_TYPE The return type of the hook.
 * @param ... The parameters of the hook.
 *
 * @note register or unregister by calling DEF_TYPE::hook() and DEF_TYPE::unhook().
 */
#define LL_INSTANCE_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)                                                \
    LL_VA_EXPAND(LL_INSTANCE_HOOK_IMPL(DEF_TYPE, ::ll::memory::Hook, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a typed instance function.
 * @details The hook will be automatically registered and unregistered.
 * @see LL_TYPE_INSTANCE_HOOK for usage.
 */
#define LL_AUTO_TYPE_INSTANCE_HOOK(DEF_TYPE, PRIORITY, TYPE, IDENTIFIER, RET_TYPE, ...)                                \
    LL_VA_EXPAND(LL_AUTO_INSTANCE_HOOK_IMPL(DEF_TYPE, TYPE, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

/**
 * @brief Register a hook for a instance function.
 * @details The hook will be automatically registered and unregistered.
 * @see LL_INSTANCE_HOOK for usage.
 */
#define LL_AUTO_INSTANCE_HOOK(DEF_TYPE, PRIORITY, IDENTIFIER, RET_TYPE, ...)                                           \
    LL_VA_EXPAND(LL_AUTO_INSTANCE_HOOK_IMPL(DEF_TYPE, ::ll::memory::Hook, PRIORITY, IDENTIFIER, RET_TYPE, __VA_ARGS__))

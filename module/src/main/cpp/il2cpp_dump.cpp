//
// Created by Perfare on 2020/7/4.
//

#include "il2cpp_dump.h"
#include <dlfcn.h>
#include <cstdlib>
#include <cstring>
#include <cinttypes>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include "xdl.h"
#include "log.h"
#include "il2cpp-tabledefs.h"
#include "il2cpp-class.h"
#include <sys/mman.h>

#define DO_API(r, n, p) r (*n) p

#include "il2cpp-api-functions.h"

#undef DO_API

static uint64_t il2cpp_base = 0;

void init_il2cpp_api(void *handle) {
#define DO_API(r, n, p) {                      \
    n = (r (*) p)xdl_sym(handle, #n, nullptr); \
    if(!n) {                                   \
        LOGW("api not found %s", #n);          \
    }                                          \
}

#include "il2cpp-api-functions.h"

#undef DO_API
}

std::string get_method_modifier(uint32_t flags) {
    std::stringstream outPut;
    auto access = flags & METHOD_ATTRIBUTE_MEMBER_ACCESS_MASK;
    switch (access) {
        case METHOD_ATTRIBUTE_PRIVATE:
            outPut << "private ";
            break;
        case METHOD_ATTRIBUTE_PUBLIC:
            outPut << "public ";
            break;
        case METHOD_ATTRIBUTE_FAMILY:
            outPut << "protected ";
            break;
        case METHOD_ATTRIBUTE_ASSEM:
        case METHOD_ATTRIBUTE_FAM_AND_ASSEM:
            outPut << "internal ";
            break;
        case METHOD_ATTRIBUTE_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & METHOD_ATTRIBUTE_STATIC) {
        outPut << "static ";
    }
    if (flags & METHOD_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_FINAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_REUSE_SLOT) {
            outPut << "sealed override ";
        }
    } else if (flags & METHOD_ATTRIBUTE_VIRTUAL) {
        if ((flags & METHOD_ATTRIBUTE_VTABLE_LAYOUT_MASK) == METHOD_ATTRIBUTE_NEW_SLOT) {
            outPut << "virtual ";
        } else {
            outPut << "override ";
        }
    }
    if (flags & METHOD_ATTRIBUTE_PINVOKE_IMPL) {
        outPut << "extern ";
    }
    return outPut.str();
}

bool _il2cpp_type_is_byref(const Il2CppType *type) {
    auto byref = type->byref;
    if (il2cpp_type_is_byref) {
        byref = il2cpp_type_is_byref(type);
    }
    return byref;
}

std::string dump_method(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Methods\n";
    void *iter = nullptr;
    while (auto method = il2cpp_class_get_methods(klass, &iter)) {
        //TODO attribute
        if (method->methodPointer) {
            outPut << "\t// RVA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer - il2cpp_base;
            outPut << " VA: 0x";
            outPut << std::hex << (uint64_t) method->methodPointer;
        } else {
            outPut << "\t// RVA: 0x VA: 0x0";
        }
        /*if (method->slot != 65535) {
            outPut << " Slot: " << std::dec << method->slot;
        }*/
        outPut << "\n\t";
        uint32_t iflags = 0;
        auto flags = il2cpp_method_get_flags(method, &iflags);
        outPut << get_method_modifier(flags);
        //TODO genericContainerIndex
        auto return_type = il2cpp_method_get_return_type(method);
        if (_il2cpp_type_is_byref(return_type)) {
            outPut << "ref ";
        }
        auto return_class = il2cpp_class_from_type(return_type);
        outPut << il2cpp_class_get_name(return_class) << " " << il2cpp_method_get_name(method)
               << "(";
        auto param_count = il2cpp_method_get_param_count(method);
        for (int i = 0; i < param_count; ++i) {
            auto param = il2cpp_method_get_param(method, i);
            auto attrs = param->attrs;
            if (_il2cpp_type_is_byref(param)) {
                if (attrs & PARAM_ATTRIBUTE_OUT && !(attrs & PARAM_ATTRIBUTE_IN)) {
                    outPut << "out ";
                } else if (attrs & PARAM_ATTRIBUTE_IN && !(attrs & PARAM_ATTRIBUTE_OUT)) {
                    outPut << "in ";
                } else {
                    outPut << "ref ";
                }
            } else {
                if (attrs & PARAM_ATTRIBUTE_IN) {
                    outPut << "[In] ";
                }
                if (attrs & PARAM_ATTRIBUTE_OUT) {
                    outPut << "[Out] ";
                }
            }
            auto parameter_class = il2cpp_class_from_type(param);
            outPut << il2cpp_class_get_name(parameter_class) << " "
                   << il2cpp_method_get_param_name(method, i);
            outPut << ", ";
        }
        if (param_count > 0) {
            outPut.seekp(-2, outPut.cur);
        }
        outPut << ") { }\n";
        //TODO GenericInstMethod
    }
    return outPut.str();
}

std::string dump_property(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Properties\n";
    void *iter = nullptr;
    while (auto prop_const = il2cpp_class_get_properties(klass, &iter)) {
        //TODO attribute
        auto prop = const_cast<PropertyInfo *>(prop_const);
        auto get = il2cpp_property_get_get_method(prop);
        auto set = il2cpp_property_get_set_method(prop);
        auto prop_name = il2cpp_property_get_name(prop);
        outPut << "\t";
        Il2CppClass *prop_class = nullptr;
        uint32_t iflags = 0;
        if (get) {
            outPut << get_method_modifier(il2cpp_method_get_flags(get, &iflags));
            prop_class = il2cpp_class_from_type(il2cpp_method_get_return_type(get));
        } else if (set) {
            outPut << get_method_modifier(il2cpp_method_get_flags(set, &iflags));
            auto param = il2cpp_method_get_param(set, 0);
            prop_class = il2cpp_class_from_type(param);
        }
        if (prop_class) {
            outPut << il2cpp_class_get_name(prop_class) << " " << prop_name << " { ";
            if (get) {
                outPut << "get; ";
            }
            if (set) {
                outPut << "set; ";
            }
            outPut << "}\n";
        } else {
            if (prop_name) {
                outPut << " // unknown property " << prop_name;
            }
        }
    }
    return outPut.str();
}

std::string dump_field(Il2CppClass *klass) {
    std::stringstream outPut;
    outPut << "\n\t// Fields\n";
    auto is_enum = il2cpp_class_is_enum(klass);
    void *iter = nullptr;
    while (auto field = il2cpp_class_get_fields(klass, &iter)) {
        //TODO attribute
        outPut << "\t";
        auto attrs = il2cpp_field_get_flags(field);
        auto access = attrs & FIELD_ATTRIBUTE_FIELD_ACCESS_MASK;
        switch (access) {
            case FIELD_ATTRIBUTE_PRIVATE:
                outPut << "private ";
                break;
            case FIELD_ATTRIBUTE_PUBLIC:
                outPut << "public ";
                break;
            case FIELD_ATTRIBUTE_FAMILY:
                outPut << "protected ";
                break;
            case FIELD_ATTRIBUTE_ASSEMBLY:
            case FIELD_ATTRIBUTE_FAM_AND_ASSEM:
                outPut << "internal ";
                break;
            case FIELD_ATTRIBUTE_FAM_OR_ASSEM:
                outPut << "protected internal ";
                break;
        }
        if (attrs & FIELD_ATTRIBUTE_LITERAL) {
            outPut << "const ";
        } else {
            if (attrs & FIELD_ATTRIBUTE_STATIC) {
                outPut << "static ";
            }
            if (attrs & FIELD_ATTRIBUTE_INIT_ONLY) {
                outPut << "readonly ";
            }
        }
        auto field_type = il2cpp_field_get_type(field);
        auto field_class = il2cpp_class_from_type(field_type);
        outPut << il2cpp_class_get_name(field_class) << " " << il2cpp_field_get_name(field);
        //TODO 获取构造函数初始化后的字段值
        if (attrs & FIELD_ATTRIBUTE_LITERAL && is_enum) {
            uint64_t val = 0;
            il2cpp_field_static_get_value(field, &val);
            outPut << " = " << std::dec << val;
        }
        outPut << "; // 0x" << std::hex << il2cpp_field_get_offset(field) << "\n";
    }
    return outPut.str();
}

std::string dump_type(const Il2CppType *type) {
    std::stringstream outPut;
    auto *klass = il2cpp_class_from_type(type);
    outPut << "\n// Namespace: " << il2cpp_class_get_namespace(klass) << "\n";
    auto flags = il2cpp_class_get_flags(klass);
    if (flags & TYPE_ATTRIBUTE_SERIALIZABLE) {
        outPut << "[Serializable]\n";
    }
    //TODO attribute
    auto is_valuetype = il2cpp_class_is_valuetype(klass);
    auto is_enum = il2cpp_class_is_enum(klass);
    auto visibility = flags & TYPE_ATTRIBUTE_VISIBILITY_MASK;
    switch (visibility) {
        case TYPE_ATTRIBUTE_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_PUBLIC:
            outPut << "public ";
            break;
        case TYPE_ATTRIBUTE_NOT_PUBLIC:
        case TYPE_ATTRIBUTE_NESTED_FAM_AND_ASSEM:
        case TYPE_ATTRIBUTE_NESTED_ASSEMBLY:
            outPut << "internal ";
            break;
        case TYPE_ATTRIBUTE_NESTED_PRIVATE:
            outPut << "private ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAMILY:
            outPut << "protected ";
            break;
        case TYPE_ATTRIBUTE_NESTED_FAM_OR_ASSEM:
            outPut << "protected internal ";
            break;
    }
    if (flags & TYPE_ATTRIBUTE_ABSTRACT && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "static ";
    } else if (!(flags & TYPE_ATTRIBUTE_INTERFACE) && flags & TYPE_ATTRIBUTE_ABSTRACT) {
        outPut << "abstract ";
    } else if (!is_valuetype && !is_enum && flags & TYPE_ATTRIBUTE_SEALED) {
        outPut << "sealed ";
    }
    if (flags & TYPE_ATTRIBUTE_INTERFACE) {
        outPut << "interface ";
    } else if (is_enum) {
        outPut << "enum ";
    } else if (is_valuetype) {
        outPut << "struct ";
    } else {
        outPut << "class ";
    }
    outPut << il2cpp_class_get_name(klass); //TODO genericContainerIndex
    std::vector<std::string> extends;
    auto parent = il2cpp_class_get_parent(klass);
    if (!is_valuetype && !is_enum && parent) {
        auto parent_type = il2cpp_class_get_type(parent);
        if (parent_type->type != IL2CPP_TYPE_OBJECT) {
            extends.emplace_back(il2cpp_class_get_name(parent));
        }
    }
    void *iter = nullptr;
    while (auto itf = il2cpp_class_get_interfaces(klass, &iter)) {
        extends.emplace_back(il2cpp_class_get_name(itf));
    }
    if (!extends.empty()) {
        outPut << " : " << extends[0];
        for (int i = 1; i < extends.size(); ++i) {
            outPut << ", " << extends[i];
        }
    }
    outPut << "\n{";
    outPut << dump_field(klass);
    outPut << dump_property(klass);
    outPut << dump_method(klass);
    //TODO EventInfo
    outPut << "}\n";
    return outPut.str();
}

// ==================== PATCH: dump Lua AES key ====================
static std::string u16_to_utf8(const Il2CppChar *chars, int32_t len) {
    std::string out;
    for (int32_t i = 0; i < len; ++i) {
        uint32_t c = (uint32_t) chars[i];
        if (c < 0x80) {
            out += (char) c;
        } else if (c < 0x800) {
            out += (char) (0xC0 | (c >> 6));
            out += (char) (0x80 | (c & 0x3F));
        } else {
            out += (char) (0xE0 | (c >> 12));
            out += (char) (0x80 | ((c >> 6) & 0x3F));
            out += (char) (0x80 | (c & 0x3F));
        }
    }
    return out;
}

static std::string call_string_getter(const MethodInfo *m) {
    if (!m || !il2cpp_runtime_invoke || !il2cpp_string_chars || !il2cpp_string_length) {
        return std::string();
    }
    Il2CppException *exc = nullptr;
    Il2CppObject *res = il2cpp_runtime_invoke(m, nullptr, nullptr, &exc);
    if (exc || !res) {
        return std::string();
    }
    auto *s = (Il2CppString *) res;
    return u16_to_utf8(il2cpp_string_chars(s), il2cpp_string_length(s));
}

void dump_lua_key(const char *outDir) {
    std::stringstream log;
    log << "==== lua key dump ====\n";
    auto domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (domain && il2cpp_thread_attach) {
        il2cpp_thread_attach(domain);
    }
    if (domain && il2cpp_domain_assembly_open && il2cpp_assembly_get_image && il2cpp_class_from_name) {
        const Il2CppAssembly *asmCSharp = il2cpp_domain_assembly_open(domain, "Assembly-CSharp.dll");
        Il2CppClass *klass = nullptr;
        if (asmCSharp) {
            auto image = il2cpp_assembly_get_image(asmCSharp);
            klass = il2cpp_class_from_name(image, "", "LuaModule");
            if (!klass) {
                klass = il2cpp_class_from_name(image, "XLua", "LuaModule");
            }
        }
        if (!klass) {
            log << "LuaModule not found\n";
        } else {
            log << "LuaModule = " << (void *) klass << "\n";
            const char *methNames[] = {"get_AesPassword", "CustomerLoader", "DoString",
                                       "ExecuteScript", "Import", "DoImportScript"};
            for (auto name : methNames) {
                const MethodInfo *m = il2cpp_class_get_method_from_name
                                      ? il2cpp_class_get_method_from_name(klass, name, -1) : nullptr;
                log << name << " -> ";
                if (m && m->methodPointer) {
                    log << "RVA 0x" << std::hex << ((uint64_t) m->methodPointer - il2cpp_base)
                        << std::dec << "\n";
                } else {
                    log << "not found\n";
                }
            }
            const MethodInfo *getter = nullptr;
            if (il2cpp_class_get_property_from_name && il2cpp_property_get_get_method) {
                const PropertyInfo *prop = il2cpp_class_get_property_from_name(klass, "AesPassword");
                if (prop) {
                    getter = il2cpp_property_get_get_method((PropertyInfo *) prop);
                }
            }
            if (!getter && il2cpp_class_get_method_from_name) {
                getter = il2cpp_class_get_method_from_name(klass, "get_AesPassword", 0);
            }
            std::string key;
            for (int i = 0; i < 8 && key.empty(); ++i) {
                if (i > 0) {
                    sleep(5);
                }
                key = call_string_getter(getter);
                LOGI("lua key try %d -> %zu bytes", i, key.size());
            }
            log << "AesPassword = [" << key << "]\n";
        }
    } else {
        log << "required il2cpp api missing\n";
    }
    std::string path = std::string(outDir) + "/files/lua_key.txt";
    std::ofstream out(path, std::ios::binary);
    out << log.str();
    out.close();
    LOGI("lua key -> %s", path.c_str());
}
// ================== end PATCH ==================

// ==================== PATCH v2 ====================
static Il2CppClass *find_class_any(const char *ns, const char *name) {
    if (!il2cpp_domain_get_assemblies || !il2cpp_assembly_get_image ||
        !il2cpp_image_get_class_count || !il2cpp_image_get_class) {
        return nullptr;
    }
    size_t count = 0;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &count);
    if (!assemblies) {
        return nullptr;
    }
    for (size_t i = 0; i < count; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        if (!image) {
            continue;
        }
        auto n = il2cpp_image_get_class_count(image);
        for (size_t j = 0; j < n; ++j) {
            auto k = const_cast<Il2CppClass *>(il2cpp_image_get_class(image, j));
            if (!k || !il2cpp_class_get_name) {
                continue;
            }
            auto cn = il2cpp_class_get_name(k);
            if (!cn || strcmp(cn, name) != 0) {
                continue;
            }
            if (ns && ns[0] && il2cpp_class_get_namespace) {
                auto kns = il2cpp_class_get_namespace(k);
                if (!kns || strcmp(kns, ns) != 0) {
                    continue;
                }
            }
            return k;
        }
    }
    return nullptr;
}

static std::string hex_of(const void *data, int len) {
    static const char *h = "0123456789abcdef";
    std::string s;
    auto *p = (const unsigned char *) data;
    for (int i = 0; i < len; ++i) {
        s += h[p[i] >> 4];
        s += h[p[i] & 0xF];
    }
    return s;
}

static std::string call_method_string(const MethodInfo *m, void *obj) {
    if (!m || !il2cpp_runtime_invoke || !il2cpp_string_chars || !il2cpp_string_length) {
        return std::string();
    }
    Il2CppException *exc = nullptr;
    Il2CppObject *res = il2cpp_runtime_invoke(m, obj, nullptr, &exc);
    if (exc || !res) {
        return std::string();
    }
    auto *s = (Il2CppString *) res;
    return u16_to_utf8(il2cpp_string_chars(s), il2cpp_string_length(s));
}

static std::string call_method_bytes(const MethodInfo *m, void *obj) {
    if (!m || !il2cpp_runtime_invoke || !il2cpp_array_length) {
        return std::string();
    }
    Il2CppException *exc = nullptr;
    Il2CppObject *res = il2cpp_runtime_invoke(m, obj, nullptr, &exc);
    if (exc || !res) {
        return std::string();
    }
    auto *arr = (Il2CppArray *) res;
    uint32_t len = il2cpp_array_length(arr);
    if (len == 0 || len > 64) {
        return std::string();
    }
    return hex_of(arr->vector, (int) len);
}

void dump_lua_key2(const char *outDir) {
    std::stringstream log;
    log << "==== lua key dump v2 ====\n";
    auto domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (domain && il2cpp_thread_attach) {
        il2cpp_thread_attach(domain);
    }
    if (!domain) {
        log << "no il2cpp domain\n";
    } else {
        Il2CppClass *klass = find_class_any("Engine.Modules", "LuaModule");
        if (!klass) {
            klass = find_class_any("", "LuaModule");
        }
        if (!klass) {
            klass = find_class_any(nullptr, "LuaModule");
        }
        if (!klass) {
            log << "LuaModule not found in any image\n";
        } else {
            log << "LuaModule = " << (void *) klass;
            if (il2cpp_class_get_namespace) {
                log << "  ns=[" << il2cpp_class_get_namespace(klass) << "]";
            }
            log << "\n-- fields --\n";
            if (il2cpp_class_get_fields && il2cpp_field_get_name) {
                void *iter = nullptr;
                while (auto f = il2cpp_class_get_fields(klass, &iter)) {
                    log << "  " << il2cpp_field_get_name(f) << "\n";
                }
            }
            log << "-- methods --\n";
            if (il2cpp_class_get_methods && il2cpp_method_get_name) {
                void *iter = nullptr;
                while (auto m = il2cpp_class_get_methods(klass, &iter)) {
                    log << "  " << il2cpp_method_get_name(m) << "  RVA 0x" << std::hex
                        << (m->methodPointer ? ((uint64_t) m->methodPointer - il2cpp_base) : 0)
                        << std::dec << "\n";
                }
            }
            const MethodInfo *getter = nullptr;
            if (il2cpp_class_get_property_from_name && il2cpp_property_get_get_method) {
                const PropertyInfo *prop = il2cpp_class_get_property_from_name(klass, "AesPassword");
                if (prop) {
                    getter = il2cpp_property_get_get_method((PropertyInfo *) prop);
                }
            }
            if (!getter && il2cpp_class_get_method_from_name) {
                getter = il2cpp_class_get_method_from_name(klass, "get_AesPassword", 0);
            }
            std::string key;
            for (int i = 0; i < 10 && key.empty(); ++i) {
                if (i > 0) {
                    sleep(5);
                }
                key = call_method_string(getter, nullptr);
                LOGI("lua key v2 try %d -> %zu", i, key.size());
            }
            log << "AesPassword = [" << key << "]\n";
            if (il2cpp_class_get_field_from_name && il2cpp_field_static_get_value &&
                il2cpp_object_get_class && il2cpp_class_get_method_from_name) {
                auto field = il2cpp_class_get_field_from_name(klass, "_aesManaged");
                if (field) {
                    Il2CppObject *aesObj = nullptr;
                    il2cpp_field_static_get_value(field, &aesObj);
                    log << "aesObj = " << (void *) aesObj << "\n";
                    if (aesObj) {
                        auto aesClass = il2cpp_object_get_class(aesObj);
                        if (aesClass) {
                            auto mKey = il2cpp_class_get_method_from_name(aesClass, "get_Key", 0);
                            auto mIv = il2cpp_class_get_method_from_name(aesClass, "get_IV", 0);
                            log << "AES.Key = " << call_method_bytes(mKey, aesObj) << "\n";
                            log << "AES.IV  = " << call_method_bytes(mIv, aesObj) << "\n";
                        }
                    }
                } else {
                    log << "_aesManaged field not found\n";
                }
            }
        }
    }
    std::string path = std::string(outDir) + "/files/lua_key.txt";
    std::ofstream out(path, std::ios::binary);
    out << log.str();
    out.close();
    LOGI("lua key v2 -> %s", path.c_str());
}
// ================== end PATCH v2 ==================

// ==================== PATCH v3 ====================
static int call_method_int(const MethodInfo *m, void *obj) {
    if (!m || !il2cpp_runtime_invoke || !il2cpp_object_unbox) {
        return -999;
    }
    Il2CppException *exc = nullptr;
    Il2CppObject *res = il2cpp_runtime_invoke(m, obj, nullptr, &exc);
    if (exc || !res) {
        return -998;
    }
    void *p = il2cpp_object_unbox(res);
    if (!p) {
        return -997;
    }
    return *(int32_t *) p;
}

void dump_lua_key3(const char *outDir) {
    std::string path = std::string(outDir) + "/files/lua_key.txt";
    auto domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (domain && il2cpp_thread_attach) {
        il2cpp_thread_attach(domain);
    }
    Il2CppClass *klass = find_class_any("Engine.Modules", "LuaModule");
    if (!klass) {
        klass = find_class_any(nullptr, "LuaModule");
    }
    const MethodInfo *getter = nullptr;
    FieldInfo *aesField = nullptr;
    if (klass) {
        if (il2cpp_class_get_property_from_name && il2cpp_property_get_get_method) {
            const PropertyInfo *prop = il2cpp_class_get_property_from_name(klass, "AesPassword");
            if (prop) {
                getter = il2cpp_property_get_get_method((PropertyInfo *) prop);
            }
        }
        if (!getter && il2cpp_class_get_method_from_name) {
            getter = il2cpp_class_get_method_from_name(klass, "get_AesPassword", 0);
        }
        if (il2cpp_class_get_field_from_name) {
            aesField = il2cpp_class_get_field_from_name(klass, "_aesManaged");
        }
    }
    for (int round = 0; round < 12; ++round) {
        if (round > 0) {
            sleep(10);
        }
        std::stringstream log;
        log << "==== lua key v3  round " << round << "  (t=" << (round * 10) << "s) ====\n";
        if (!klass) {
            log << "LuaModule not found\n";
        } else {
            log << "AesPassword = [" << call_method_string(getter, nullptr) << "]\n";
            if (aesField && il2cpp_field_static_get_value && il2cpp_object_get_class &&
                il2cpp_class_get_method_from_name) {
                Il2CppObject *aesObj = nullptr;
                il2cpp_field_static_get_value(aesField, &aesObj);
                log << "aesObj = " << (void *) aesObj << "\n";
                if (aesObj) {
                    auto aesClass = il2cpp_object_get_class(aesObj);
                    log << "aesClass = " << (void *) aesClass << "\n";
                    if (aesClass) {
                        auto mKey = il2cpp_class_get_method_from_name(aesClass, "get_Key", 0);
                        auto mIv = il2cpp_class_get_method_from_name(aesClass, "get_IV", 0);
                        auto mMode = il2cpp_class_get_method_from_name(aesClass, "get_Mode", 0);
                        auto mPad = il2cpp_class_get_method_from_name(aesClass, "get_Padding", 0);
                        auto mFb = il2cpp_class_get_method_from_name(aesClass, "get_FeedbackSize", 0);
                        auto mBlk = il2cpp_class_get_method_from_name(aesClass, "get_BlockSize", 0);
                        auto mKS = il2cpp_class_get_method_from_name(aesClass, "get_KeySize", 0);
                        log << "KeyHex = " << call_method_bytes(mKey, aesObj) << "\n";
                        log << "IVHex  = " << call_method_bytes(mIv, aesObj) << "\n";
                        log << "Mode = " << call_method_int(mMode, aesObj)
                            << "  Padding = " << call_method_int(mPad, aesObj)
                            << "  FeedbackSize = " << call_method_int(mFb, aesObj)
                            << "  BlockSize = " << call_method_int(mBlk, aesObj)
                            << "  KeySize = " << call_method_int(mKS, aesObj) << "\n";
                    }
                }
            } else {
                log << "aes field/API missing\n";
            }
        }
        std::ofstream out(path, std::ios::binary);
        out << log.str();
        out.close();
        LOGI("lua key v3 round %d written", round);
    }
}
// ================== end PATCH v3 ==================

// ==================== PATCH v4 : hook CustomerLoader ====================
static std::string g_luaOutDir;
static Il2CppArray *(*g_origLoader)(Il2CppString **refPath) = nullptr;

static std::string safe_name(const std::string &s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '_' || c == '-' || c == '.') {
            r += c;
        } else {
            r += '_';
        }
    }
    if (r.empty()) {
        r = "unknown";
    }
    return r;
}

static void write_blob(const std::string &path, const void *data, size_t len) {
    std::ofstream f(path, std::ios::binary);
    if (f) {
        f.write((const char *) data, len);
        f.close();
    }
}

static void dump_script(const char *tag, Il2CppString **refPath, Il2CppArray *arr) {
    if (g_luaOutDir.empty()) {
        return;
    }
    std::string name = "unknown";
    if (refPath && *refPath && il2cpp_string_chars && il2cpp_string_length) {
        name = u16_to_utf8(il2cpp_string_chars(*refPath), il2cpp_string_length(*refPath));
    }
    std::string safe = safe_name(name);
    bool hasLua = safe.size() > 4 && safe.substr(safe.size() - 4) == ".lua";
    if (!hasLua) {
        safe += ".lua";
    }
    uint32_t len = 0;
    if (arr && il2cpp_array_length) {
        len = il2cpp_array_length(arr);
    }
    if (len > 0 && len < 16u * 1024u * 1024u) {
        std::string path = g_luaOutDir + "/lua_" + safe;
        write_blob(path, arr->vector, len);
        LOGI("lua dump [%s] %s (%u bytes)", tag, safe.c_str(), len);
    } else {
        LOGI("lua dump [%s] %s -> empty/null", tag, safe.c_str());
    }
}

static Il2CppArray *hooked_CustomerLoader(Il2CppString **refPath) {
    Il2CppArray *res = nullptr;
    if (g_origLoader) {
        res = g_origLoader(refPath);
    }
    dump_script("hook", refPath, res);
    return res;
}

void install_lua_hook() {
    if (!il2cpp_class_get_method_from_name || !il2cpp_class_get_method_from_name) {
        LOGI("lua hook: api missing");
        return;
    }
    Il2CppClass *klass = find_class_any("Engine.Modules", "LuaModule");
    if (!klass) {
        klass = find_class_any(nullptr, "LuaModule");
    }
    if (!klass) {
        LOGI("lua hook: LuaModule not found");
        return;
    }
    const MethodInfo *mi = il2cpp_class_get_method_from_name(klass, "CustomerLoader", 1);
    if (!mi || !mi->methodPointer) {
        LOGI("lua hook: CustomerLoader not found");
        return;
    }
    g_origLoader = (Il2CppArray *(*)(Il2CppString **)) mi->methodPointer;
    uintptr_t addr = (uintptr_t) mi;
    uintptr_t page = addr & ~(uintptr_t) 0xFFF;
    int pr = mprotect((void *) page, 0x2000, 3);
    LOGI("lua hook: mprotect(%p) = %d", (void *) page, pr);
    if (pr == 0) {
        ((MethodInfo *) mi)->methodPointer = (Il2CppMethodPointer) hooked_CustomerLoader;
        LOGI("lua hook: pointer patched, orig = %p", g_origLoader);
    } else {
        LOGI("lua hook: patch skipped (page not writable), samples only");
    }
}

void dump_lua_scripts(const char *outDir) {
    g_luaOutDir = std::string(outDir) + "/files";
    LOGI("lua scripts dir %s", g_luaOutDir.c_str());
    if (!g_origLoader || !il2cpp_string_new) {
        LOGI("lua samples: no loader");
        return;
    }
    sleep(90);
    const char *names[] = {"AceUtils", "TileMatchDefine", "TileMatchModel", "ActivityModule"};
    const char *suffixes[] = {"", ".lua", ".lua.bytes"};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 3; ++j) {
            std::string full = std::string(names[i]) + suffixes[j];
            Il2CppString *s = il2cpp_string_new(full.c_str());
            Il2CppString *cur = s;
            Il2CppArray *arr = g_origLoader(&cur);
            dump_script(suffixes[j], &cur, arr);
            sleep(1);
        }
    }
    LOGI("lua samples done");
}
// ================== end PATCH v4 ==================

// ==================== PATCH v6 : read-only memory scan ====================
static void scan_for_name(const char *outDir, const std::string &name, int round) {
    std::string needleA = name;
    std::string needleW;
    for (size_t i = 0; i < name.size(); ++i) {
        needleW += name[i];
        needleW += (char) 0;
    }
    FILE *maps = fopen("/proc/self/maps", "r");
    if (!maps) {
        return;
    }
    char line[512];
    int hits = 0;
    while (fgets(line, sizeof(line), maps) && hits < 2) {
        unsigned long start = 0, end = 0;
        char perms[8] = {0};
        if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) != 3) {
            continue;
        }
        if (perms[0] != 'r') {
            continue;
        }
        if (strchr(line, '/') != nullptr) {
            continue;
        }
        if (end <= start) {
            continue;
        }
        unsigned long len = end - start;
        if (len > (384UL << 20)) {
            continue;
        }
        for (unsigned long off = 0; off + needleA.size() < len; off += 8) {
            void *p = (void *) (start + off);
            if (memcmp(p, needleA.data(), needleA.size()) == 0 ||
                memcmp(p, needleW.data(), needleW.size()) == 0) {
                unsigned long from = (off > 65536) ? off - 65536 : 0;
                unsigned long to = off + 65536;
                if (to > len) {
                    to = len;
                }
                std::string path = std::string(outDir) + "/files/scan_" + safe_name(name) +
                                   "_r" + std::to_string(round) + "_" + std::to_string(hits) + ".bin";
                write_blob(path, (void *) (start + from), (size_t) (to - from));
                LOGI("scan hit %s r%d @%lx", name.c_str(), round, (start + off));
                hits++;
                if (hits >= 2) {
                    break;
                }
            }
        }
    }
    fclose(maps);
}

void dump_lua_scan(const char *outDir) {
    std::vector<std::string> names;
    const char *builtin[] = {"TileMatchMainDialog", "TileMatchModel", "TileMatchDefine",
                             "ActivityModule", "AceUtils", "LevelGift", "PlayerLevelDialog",
                             "RingLinkLevelGenerator", "ActivityChessboardMgr", "AP7SignModel",
                             "ALiPayMiniProgramModel", "GardenLevelNode", "TileMatchColorVo",
                             "TileMatchParamVo", "TileMatchFinishPopup", "TileMatchGuideBookDialog"};
    for (size_t i = 0; i < sizeof(builtin) / sizeof(builtin[0]); ++i) {
        names.push_back(builtin[i]);
    }
    {
        std::string lp = std::string(outDir) + "/files/lua_names.txt";
        std::ifstream in(lp);
        std::string ln;
        while (std::getline(in, ln)) {
            if (!ln.empty() && ln[ln.size() - 1] == '\r') {
                ln.erase(ln.size() - 1);
            }
            if (ln.size() > 2) {
                names.push_back(ln);
            }
        }
    }
    LOGI("lua scan: %zu names", names.size());
    for (int round = 0; round < 3; ++round) {
        sleep(round == 0 ? 20 : 15);
        for (size_t i = 0; i < names.size(); ++i) {
            scan_for_name(outDir, names[i], round);
        }
        std::string st = std::string(outDir) + "/files/lua_scan_status.txt";
        std::ofstream stf(st, std::ios::app);
        stf << "round " << round << " done, names " << names.size() << "\n";
        stf.close();
    }
    LOGI("lua scan done");
}
// ================== end PATCH v6 ==================





void il2cpp_api_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    init_il2cpp_api(handle);
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr((void *) il2cpp_domain_get_assemblies, &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    while (!il2cpp_is_vm_thread(nullptr)) {
        LOGI("Waiting for il2cpp_init...");
        sleep(1);
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);
    // install_lua_hook();  // disabled: pointer patch made the game crash
}

void il2cpp_dump(const char *outDir) {
    LOGI("dumping...");
    size_t size;
    auto domain = il2cpp_domain_get();
    auto assemblies = il2cpp_domain_get_assemblies(domain, &size);
    std::stringstream imageOutput;
    for (int i = 0; i < size; ++i) {
        auto image = il2cpp_assembly_get_image(assemblies[i]);
        imageOutput << "// Image " << i << ": " << il2cpp_image_get_name(image) << "\n";
    }
    std::vector<std::string> outPuts;
    if (il2cpp_image_get_class) {
        LOGI("Version greater than 2018.3");
        //使用il2cpp_image_get_class
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            imageStr << "\n// Dll : " << il2cpp_image_get_name(image);
            auto classCount = il2cpp_image_get_class_count(image);
            for (int j = 0; j < classCount; ++j) {
                auto klass = il2cpp_image_get_class(image, j);
                auto type = il2cpp_class_get_type(const_cast<Il2CppClass *>(klass));
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    } else {
        LOGI("Version less than 2018.3");
        //使用反射
        auto corlib = il2cpp_get_corlib();
        auto assemblyClass = il2cpp_class_from_name(corlib, "System.Reflection", "Assembly");
        auto assemblyLoad = il2cpp_class_get_method_from_name(assemblyClass, "Load", 1);
        auto assemblyGetTypes = il2cpp_class_get_method_from_name(assemblyClass, "GetTypes", 0);
        if (assemblyLoad && assemblyLoad->methodPointer) {
            LOGI("Assembly::Load: %p", assemblyLoad->methodPointer);
        } else {
            LOGI("miss Assembly::Load");
            return;
        }
        if (assemblyGetTypes && assemblyGetTypes->methodPointer) {
            LOGI("Assembly::GetTypes: %p", assemblyGetTypes->methodPointer);
        } else {
            LOGI("miss Assembly::GetTypes");
            return;
        }
        typedef void *(*Assembly_Load_ftn)(void *, Il2CppString *, void *);
        typedef Il2CppArray *(*Assembly_GetTypes_ftn)(void *, void *);
        for (int i = 0; i < size; ++i) {
            auto image = il2cpp_assembly_get_image(assemblies[i]);
            std::stringstream imageStr;
            auto image_name = il2cpp_image_get_name(image);
            imageStr << "\n// Dll : " << image_name;
            //LOGD("image name : %s", image->name);
            auto imageName = std::string(image_name);
            auto pos = imageName.rfind('.');
            auto imageNameNoExt = imageName.substr(0, pos);
            auto assemblyFileName = il2cpp_string_new(imageNameNoExt.data());
            auto reflectionAssembly = ((Assembly_Load_ftn) assemblyLoad->methodPointer)(nullptr,
                                                                                        assemblyFileName,
                                                                                        nullptr);
            auto reflectionTypes = ((Assembly_GetTypes_ftn) assemblyGetTypes->methodPointer)(
                    reflectionAssembly, nullptr);
            auto items = reflectionTypes->vector;
            for (int j = 0; j < reflectionTypes->max_length; ++j) {
                auto klass = il2cpp_class_from_system_type((Il2CppReflectionType *) items[j]);
                auto type = il2cpp_class_get_type(klass);
                //LOGD("type name : %s", il2cpp_type_get_name(type));
                auto outPut = imageStr.str() + dump_type(type);
                outPuts.push_back(outPut);
            }
        }
    }
    LOGI("write dump file");
    auto outPath = std::string(outDir).append("/files/dump.cs");
    std::ofstream outStream(outPath);
    outStream << imageOutput.str();
    auto count = outPuts.size();
    for (int i = 0; i < count; ++i) {
        outStream << outPuts[i];
    }
    outStream.close();
    dump_lua_scan(outDir);
    LOGI("dump done!");
}
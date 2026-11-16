#pragma once
// =============================================================================
// Preferences SIMULADAS (solo [env:pc_emulator])
// =============================================================================
// Misma API que la NVS de Arduino-ESP32, pero volcada a un fichero de texto
// local (sim_nvs.dat) para que los records de los ports sobrevivan entre
// ejecuciones del emulador sin tocar la NVS real del ESP32.
// =============================================================================

#include <Arduino.h>
#include <map>
#include <string>
#include <vector>

namespace sim_nvs {

typedef std::map<std::string, std::map<std::string, std::string>> Store;

inline const char *path() { return "sim_nvs.dat"; }

inline Store &store() {
    static Store data;
    static bool loaded = false;
    if (loaded) return data;
    loaded = true;

    FILE *f = fopen(path(), "r");
    if (!f) return data;

    char line[2048];
    while (fgets(line, sizeof(line), f)) {
        // Formato: namespace<TAB>clave<TAB>hex
        char *ns = line;
        char *tab1 = strchr(ns, '\t');
        if (!tab1) continue;
        *tab1 = '\0';
        char *key = tab1 + 1;
        char *tab2 = strchr(key, '\t');
        if (!tab2) continue;
        *tab2 = '\0';
        char *hex = tab2 + 1;
        size_t len = strlen(hex);
        while (len && (hex[len - 1] == '\n' || hex[len - 1] == '\r')) hex[--len] = '\0';
        data[ns][key] = hex;
    }
    fclose(f);
    return data;
}

inline void save() {
    FILE *f = fopen(path(), "w");
    if (!f) return;
    for (const auto &ns : store()) {
        for (const auto &kv : ns.second) {
            fprintf(f, "%s\t%s\t%s\n", ns.first.c_str(), kv.first.c_str(), kv.second.c_str());
        }
    }
    fclose(f);
}

inline std::string toHex(const void *data, size_t len) {
    static const char *digits = "0123456789abcdef";
    const uint8_t *p = static_cast<const uint8_t *>(data);
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        out += digits[p[i] >> 4];
        out += digits[p[i] & 0x0F];
    }
    return out;
}

inline size_t fromHex(const std::string &hex, void *data, size_t maxLen) {
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    uint8_t *p = static_cast<uint8_t *>(data);
    size_t n = 0;
    for (size_t i = 0; i + 1 < hex.size() && n < maxLen; i += 2) {
        p[n++] = (uint8_t)((nibble(hex[i]) << 4) | nibble(hex[i + 1]));
    }
    return n;
}

} // namespace sim_nvs

class Preferences {
public:
    bool begin(const char *name, bool readOnly = false) {
        _ns = name ? name : "";
        _readOnly = readOnly;
        _open = true;
        sim_nvs::store();
        return true;
    }
    void end() { _open = false; }

    bool isKey(const char *key) const {
        auto ns = sim_nvs::store().find(_ns);
        return ns != sim_nvs::store().end() && ns->second.count(key) > 0;
    }

    void clear() {
        sim_nvs::store()[_ns].clear();
        sim_nvs::save();
    }
    void remove(const char *key) {
        sim_nvs::store()[_ns].erase(key);
        sim_nvs::save();
    }

    size_t putBytes(const char *key, const void *value, size_t len) {
        if (_readOnly || !_open) return 0;
        sim_nvs::store()[_ns][key] = sim_nvs::toHex(value, len);
        sim_nvs::save();
        return len;
    }
    size_t getBytes(const char *key, void *out, size_t len) const {
        auto ns = sim_nvs::store().find(_ns);
        if (ns == sim_nvs::store().end()) return 0;
        auto it = ns->second.find(key);
        if (it == ns->second.end()) return 0;
        return sim_nvs::fromHex(it->second, out, len);
    }

    template <typename T>
    size_t put(const char *key, const T &value) { return putBytes(key, &value, sizeof(T)); }

    template <typename T>
    T get(const char *key, const T &def) const {
        T out = def;
        if (getBytes(key, &out, sizeof(T)) != sizeof(T)) return def;
        return out;
    }

    uint8_t getUChar(const char *key, uint8_t def = 0) const {
        return get<uint8_t>(key, def);
    }
    size_t putUChar(const char *key, uint8_t v) { return put<uint8_t>(key, v); }

    uint16_t getUShort(const char *key, uint16_t def = 0) const {
        return get<uint16_t>(key, def);
    }
    size_t putUShort(const char *key, uint16_t v) { return put<uint16_t>(key, v); }

    uint32_t getUInt(const char *key, uint32_t def = 0) const {
        return get<uint32_t>(key, def);
    }
    size_t putUInt(const char *key, uint32_t v) { return put<uint32_t>(key, v); }

    bool getBool(const char *key, bool def = false) const { return get<uint8_t>(key, def ? 1 : 0) != 0; }
    size_t putBool(const char *key, bool v) { return put<uint8_t>(key, v ? 1 : 0); }

    std::string getString(const char *key, const std::string &def = "") const {
        auto ns = sim_nvs::store().find(_ns);
        if (ns == sim_nvs::store().end()) return def;
        auto it = ns->second.find(key);
        if (it == ns->second.end()) return def;
        std::string out(it->second.size() / 2, '\0');
        sim_nvs::fromHex(it->second, &out[0], out.size());
        return out;
    }
    size_t putString(const char *key, const std::string &v) {
        if (_readOnly || !_open) return 0;
        sim_nvs::store()[_ns][key] = sim_nvs::toHex(v.data(), v.size());
        sim_nvs::save();
        return v.size();
    }

private:
    std::string _ns;
    bool _readOnly = false;
    bool _open = false;
};

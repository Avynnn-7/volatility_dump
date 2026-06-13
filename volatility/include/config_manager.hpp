

#pragma once
#include <string>
#include <map>
#include <variant>
#include <mutex>
#include <nlohmann/json.hpp>

using ConfigValue = std::variant<bool, int, double, std::string>;

class ConfigManager {
public:
    
    static ConfigManager& getInstance();

    bool loadFromFile(const std::string& filename);

    bool saveToFile(const std::string& filename) const;

    template<typename T>
    T get(const std::string& key, const T& defaultValue = T{}) const;

    template<typename T>
    void set(const std::string& key, const T& value);

    bool has(const std::string& key) const;

    void remove(const std::string& key);

    std::vector<std::string> getKeys() const;

    void loadDefaults();

    bool validate() const;

    void print() const;

private:
    ConfigManager() = default;
    std::map<std::string, ConfigValue> configMap_;
    mutable std::mutex mutex_;  
    
    ConfigValue fromJson(const nlohmann::json& j) const;
    nlohmann::json toJson(const ConfigValue& value) const;
    std::string typeToString(const ConfigValue& value) const;
};

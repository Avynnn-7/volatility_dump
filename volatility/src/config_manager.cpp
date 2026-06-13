#include "config_manager.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

ConfigManager& ConfigManager::getInstance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::loadFromFile(const std::string& filename) {

    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot open config file: " << filename << std::endl;
            return false;
        }
        
        nlohmann::json j;
        file >> j;
        
        configMap_.clear();
        
        for (auto& [key, value] : j.items()) {
            configMap_[key] = fromJson(value);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load config: " << e.what() << std::endl;
        return false;
    }
}

bool ConfigManager::saveToFile(const std::string& filename) const {

    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        nlohmann::json j;
        
        for (const auto& [key, value] : configMap_) {
            j[key] = toJson(value);
        }
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Cannot create config file: " << filename << std::endl;
            return false;
        }
        
        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to save config: " << e.what() << std::endl;
        return false;
    }
}

template<typename T>
T ConfigManager::get(const std::string& key, const T& defaultValue) const {

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = configMap_.find(key);
    if (it == configMap_.end()) {
        return defaultValue;
    }
    
    try {
        return std::get<T>(it->second);
    } catch (const std::bad_variant_access&) {
        std::cerr << "Type mismatch for config key: " << key << std::endl;
        return defaultValue;
    }
}

template<typename T>
void ConfigManager::set(const std::string& key, const T& value) {

    std::lock_guard<std::mutex> lock(mutex_);
    configMap_[key] = value;
}

template bool ConfigManager::get<bool>(const std::string&, const bool&) const;
template int ConfigManager::get<int>(const std::string&, const int&) const;
template double ConfigManager::get<double>(const std::string&, const double&) const;
template std::string ConfigManager::get<std::string>(const std::string&, const std::string&) const;

template void ConfigManager::set<bool>(const std::string&, const bool&);
template void ConfigManager::set<int>(const std::string&, const int&);
template void ConfigManager::set<double>(const std::string&, const double&);
template void ConfigManager::set<std::string>(const std::string&, const std::string&);

bool ConfigManager::has(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return configMap_.find(key) != configMap_.end();
}

void ConfigManager::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    configMap_.erase(key);
}

std::vector<std::string> ConfigManager::getKeys() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> keys;
    for (const auto& [key, value] : configMap_) {
        keys.push_back(key);
    }
    return keys;
}

void ConfigManager::loadDefaults() {
    std::lock_guard<std::mutex> lock(mutex_);

    configMap_["data.min_vol"] = 0.01;
    configMap_["data.max_vol"] = 3.0;
    configMap_["data.outlier_threshold"] = 3.0;
    configMap_["data.enable_cleaning"] = true;

    configMap_["qp.tolerance"] = 1e-9;
    configMap_["qp.max_iterations"] = 10000;
    configMap_["qp.regularization_weight"] = 1e-6;
    configMap_["qp.smoothness_weight"] = 1e-4;

    configMap_["arbitrage.butterfly_threshold"] = 1e-6;
    configMap_["arbitrage.calendar_threshold"] = 1e-6;
    configMap_["arbitrage.enable_density_check"] = true;

    configMap_["log.level"] = 1; 
    configMap_["log.console_output"] = true;
    configMap_["log.file"] = "vol_arb.log";

    configMap_["performance.enable_parallel"] = true;
    configMap_["performance.cache_size"] = 1000;
}

bool ConfigManager::validate() const {
    std::lock_guard<std::mutex> lock(mutex_);
    bool valid = true;

    if (get<double>("data.min_vol", 0.01) <= 0 || get<double>("data.min_vol", 0.01) > 1.0) {
        std::cerr << "Invalid data.min_vol" << std::endl;
        valid = false;
    }
    
    if (get<double>("data.max_vol", 3.0) <= get<double>("data.min_vol", 0.01) || get<double>("data.max_vol", 3.0) > 10.0) {
        std::cerr << "Invalid data.max_vol" << std::endl;
        valid = false;
    }
    
    if (get<double>("qp.tolerance", 1e-9) <= 0 || get<double>("qp.tolerance", 1e-9) > 1e-3) {
        std::cerr << "Invalid qp.tolerance" << std::endl;
        valid = false;
    }
    
    if (get<int>("qp.max_iterations", 10000) <= 0 || get<int>("qp.max_iterations", 10000) > 100000) {
        std::cerr << "Invalid qp.max_iterations" << std::endl;
        valid = false;
    }
    
    return valid;
}

void ConfigManager::print() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::cout << "\n=== Configuration ===" << std::endl;

    std::map<std::string, std::vector<std::pair<std::string, ConfigValue>>> grouped;
    
    for (const auto& [key, value] : configMap_) {
        size_t dotPos = key.find('.');
        std::string group = (dotPos != std::string::npos) ? key.substr(0, dotPos) : "general";
        grouped[group].emplace_back(key, value);
    }
    
    for (const auto& [group, items] : grouped) {
        std::cout << "\n[" << group << "]" << std::endl;
        for (const auto& [key, value] : items) {
            std::cout << "  " << key << " = ";
            
            std::visit([&value](const auto& v) {
                if constexpr (std::is_same_v<decltype(v), const std::string&>) {
                    std::cout << "\"" << v << "\"";
                } else if constexpr (std::is_same_v<decltype(v), const bool&>) {
                    std::cout << (v ? "true" : "false");
                } else {
                    std::cout << std::fixed << std::setprecision(6) << v;
                }
            }, value);
            
            std::cout << " (" << typeToString(value) << ")" << std::endl;
        }
    }
    std::cout << std::endl;
}

ConfigValue ConfigManager::fromJson(const nlohmann::json& j) const {
    if (j.is_boolean()) return j.get<bool>();
    if (j.is_number_integer()) return j.get<int>();
    if (j.is_number_float()) return j.get<double>();
    if (j.is_string()) return j.get<std::string>();
    
    throw std::runtime_error("Unsupported JSON type in config");
}

nlohmann::json ConfigManager::toJson(const ConfigValue& value) const {
    return std::visit([](const auto& v) -> nlohmann::json {
        return v;
    }, value);
}

std::string ConfigManager::typeToString(const ConfigValue& value) const {
    if (std::holds_alternative<bool>(value)) return "bool";
    if (std::holds_alternative<int>(value)) return "int";
    if (std::holds_alternative<double>(value)) return "double";
    if (std::holds_alternative<std::string>(value)) return "string";
    return "unknown";
}

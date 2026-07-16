#pragma once

#include "Configuration.h"
#include "json.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

/* =======================================================================================
 * CONFIGURATION FILE STRUCTURE & INHERITANCE GUIDE
 * =======================================================================================
 * This parser reads JSON configuration files to define asset processing/packaging rules.
 * The system is built on a cascading hierarchy where specific rules override general ones.
 *
 * 1. The `@base` Configuration:
 * - The `@base` key is a special reserved target.
 * - It acts as the ultimate fallback for all other configurations.
 * - If a target does not explicitly or implicitly inherit from anything else, it 
 * will inherit from `@base`.
 *
 * 2. Explicit Inheritance (`extends`):
 * - You can force a target to inherit properties from any other target using the 
 * "extends" attribute.
 * - Example: 
 * "resources/special.txt": { "extends": "other_target", "compression": "none" }
 * - This will copy everything from "other_target" first, and then apply its own 
 * "compression" override.
 *
 * 3. Implicit Inheritance (Path Hierarchy):
 * - If a target represents a file path (e.g., "audio/music/track1.ogg") and it does 
 * NOT have an "extends" attribute, the parser will look for a parent directory 
 * rule to inherit from automatically.
 * - It walks up the tree: 
 * "audio/music/track1.ogg" -> looks for "audio/music" -> looks for "audio".
 * - The closest defined parent becomes the implicit base. If no parents are defined 
 * in the JSON, it falls back to `@base`.
 *
 * 4. The `ignore` Non-Inheritance Exception:
 * - CRITICAL RULE: The `ignore` attribute is NEVER inherited from any parent (neither 
 * explicitly via "extends", implicitly via folder paths, nor from "@base").
 * - Every resolved target begins with an empty list of ignores, unless it explicitly 
 * defines its own "ignore" array inside its JSON block.
 * - This prevents deep file trees from accidentally carrying over blocklists meant 
 * only for parent directories or global scopes.
 * =======================================================================================
 */

class ConfigurationParser
{
public:
    /// @brief Parses a Configuration object from a JSON string.
    static Configuration Parse(const std::string& jsonString)
    {
        try
        {
            auto j = nlohmann::json::parse(jsonString);
            return Parse(j);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error(std::string("JSON Parsing Error: ") + e.what());
        }
    }

    /// @brief Parses a Configuration object from a JSON file path.
    static Configuration ParseFromFile(const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open configuration file: " + filePath.string());
        }

        try
        {
            nlohmann::json j;
            file >> j;
            return Parse(j);
        }
        catch (const std::exception& e)
        {
            throw std::runtime_error("Error parsing file " + filePath.string() + ": " + e.what());
        }
    }

    /// @brief Parses a Configuration object directly from an existing nlohmann::json object.
    static Configuration Parse(const nlohmann::json& j)
    {
        if (!j.is_object())
        {
            throw std::runtime_error("Root of configuration must be a JSON object.");
        }

        // 1. Initialize base config with hardcoded defaults.
        TargetConfiguration baseConfig = TargetConfiguration::MakeBaseConfig();

        // 2. Separate the raw configurations into a temporary map.
        std::unordered_map<std::string, nlohmann::json> rawConfigs;
        for (auto it = j.begin(); it != j.end(); ++it)
        {
            if (it.key() == "@base")
            {
                // If "@base" is defined in JSON, overwrite the defaults.
                ParseOverride(baseConfig, it.value());
            }
            else
            {
                rawConfigs[it.key()] = it.value();
            }
        }

        // 3. Recursively resolve and cache all configurations (handles both inheritances).
        std::unordered_map<std::string, TargetConfiguration> resolvedConfigs;
        for (const auto& [key, _] : rawConfigs)
        {
            std::unordered_set<std::string> visited;
            ResolveConfig(key, rawConfigs, baseConfig, resolvedConfigs, visited);
        }

        return Configuration(baseConfig, resolvedConfigs);
    }

private:
    /// @brief Helper to map JSON string to Compression enum values.
    static Compression ParseCompression(const std::string& compStr)
    {
        std::string lowerStr = compStr;
        std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if (lowerStr == "none")
        {
            return Compression::None;
        }
        else if (lowerStr == "balanced")
        {
            return Compression::Balanced; 
        }
        else if (lowerStr == "maximum" || lowerStr == "max")
        {
            return Compression::Maximum;
        }

        throw std::runtime_error("Unknown compression type: '" + compStr + "'");
    }

    /// @brief Applies JSON fields over an existing TargetConfiguration.
    static void ParseOverride(TargetConfiguration& config, const nlohmann::json& j)
    {
        if (!j.is_object())
        {
            throw std::runtime_error("Target configuration block must be an object.");
        }

        if (j.contains("compression"))
        {
            config.compressionLevel() = ParseCompression(j["compression"].get<std::string>());
        }
        
        if (j.contains("include_extensions"))
        {
            config.includeExtensions() = j["include_extensions"].get<bool>();
        }
        
        if (j.contains("ignore"))
        {
            if (j["ignore"].is_array())
            {
                config.ignoreEntries() = j["ignore"].get<std::vector<std::string>>();
            }
            else
            {
                throw std::runtime_error("'ignore' block must be an array of strings.");
            }
        }
    }

    /// @brief Recursively resolves configurations by looking up 'extends' keys or implicit paths.
    static TargetConfiguration ResolveConfig(
        const std::string& key,
        const std::unordered_map<std::string, nlohmann::json>& rawConfigs,
        const TargetConfiguration& baseConfig,
        std::unordered_map<std::string, TargetConfiguration>& resolvedConfigs,
        std::unordered_set<std::string>& visited)
    {
        // Handle "@base" natively to terminate recursion cleanly.
        if (key == "@base")
        {
            return baseConfig;
        }

        // Check if we have already resolved this target.
        auto cacheIt = resolvedConfigs.find(key);
        if (cacheIt != resolvedConfigs.end())
        {
            return cacheIt->second;
        }

        // Circular inheritance guard.
        if (visited.count(key))
        {
            throw std::runtime_error("Circular inheritance detected at key: '" + key + "'");
        }
        visited.insert(key);

        auto rawIt = rawConfigs.find(key);
        if (rawIt == rawConfigs.end())
        {
            throw std::runtime_error("Target '" + key + "' referenced in hierarchy does not exist.");
        }

        const auto& jsonVal = rawIt->second;
        std::string parentKey = "@base"; 
        
        // Determine the parent target we should inherit from.
        if (jsonVal.contains("extends"))
        {
            // 1. Explicit Inheritance
            parentKey = jsonVal["extends"].get<std::string>();
        }
        else
        {
            // 2. Implicit Inheritance: Walk up the path hierarchy to find the closest defined parent.
            std::filesystem::path currentPath(key);
            std::filesystem::path parentPath = currentPath.parent_path();

            while (!parentPath.empty() && parentPath.string() != ".")
            {
                std::string searchKey = parentPath.generic_string();
                
                // Strip trailing slash (safety measure to match path normalization)
                if (!searchKey.empty() && searchKey.back() == '/')
                {
                    searchKey.pop_back();
                }

                if (rawConfigs.find(searchKey) != rawConfigs.end())
                {
                    parentKey = searchKey;
                    break;
                }

                parentPath = parentPath.parent_path();
            }
        }

        // Inherit everything from the calculated parent
        TargetConfiguration targetConfig = ResolveConfig(parentKey, rawConfigs, baseConfig, resolvedConfigs, visited);

        // Crucial Fix: The ignore attribute is strictly non-inheritable.
        // We clear the list copied from the parent, so this target starts fresh.
        targetConfig.ignoreEntries().clear();

        // Override with local values (this will populate ignoreEntries if "ignore" is defined here)
        ParseOverride(targetConfig, jsonVal);

        // Cache the resolved config and clear visiting flag
        resolvedConfigs[key] = targetConfig;
        visited.erase(key);

        return targetConfig;
    }
};

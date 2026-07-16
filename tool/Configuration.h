#pragma once
#include "Compression.h"
#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <unordered_map>
#include <vector>

using Path = std::filesystem::path;
using PathPass = const Path&;

class TargetConfiguration
{
public:
	inline const Compression& compressionLevel() const { return m_compression; }
	inline bool includeExtensions() const { return m_extensions; }
	inline const std::vector<std::string> ignoreEntries() const { return m_ignoreEntries; }

	inline Compression& compressionLevel() { return m_compression; }
	inline bool& includeExtensions() { return m_extensions; }
	inline std::vector<std::string>& ignoreEntries() { return m_ignoreEntries; }

	inline bool ignores(PathPass relativeResourceFile) const
	{
		std::string pathAsTargetKey = relativeResourceFile.generic_string();
		return std::find(m_ignoreEntries.begin(), m_ignoreEntries.end(), pathAsTargetKey) != m_ignoreEntries.end();
	}

	inline static TargetConfiguration MakeBaseConfig()
	{
		return TargetConfiguration(Compression::Maximum, false, { "resources.txt" });
	}

	TargetConfiguration(Compression compression, bool extensions, const std::vector<std::string> ignoreEntries)
	{
		m_compression = compression;
		m_extensions = extensions;
		m_ignoreEntries = ignoreEntries;
	}

	TargetConfiguration() {}

private:
	Compression m_compression = Compression::None;
	bool m_extensions = false;
	std::vector<std::string> m_ignoreEntries = {};
};

class Configuration
{
public:

	TargetConfiguration& BaseTargetConfig() { return m_baseTargetConfig; }
	std::unordered_map<std::string, TargetConfiguration>& TargetConfigs() { return m_targetConfigs; }

	std::pair<const TargetConfiguration&, std::string> GetConfigurationFor(PathPass absoluteResourcesDir, PathPass absoluteResourceFile) const
	{
		if(absoluteResourcesDir.is_relative() || absoluteResourceFile.is_relative()) 
		{
			return { m_baseTargetConfig, "@base" };
		}

		Path relativePath = absoluteResourceFile.lexically_relative(absoluteResourcesDir).lexically_normal();

		// If the file is outside the resource directory, fallback immediately
		if(relativePath.empty() || *relativePath.begin() == "..")
		{
			return { m_baseTargetConfig, "@base" };
		}

		// Traverse upwards from the specific file path to the root.
		// Since we start at the file and move up, more specific paths override general ones.
		Path currentSearchPath = relativePath;
		while (!currentSearchPath.empty() && currentSearchPath != ".")
		{
			std::string searchKey = currentSearchPath.generic_string();

			auto it = m_targetConfigs.find(searchKey);
			if (it != m_targetConfigs.end())
			{
				return {it->second, it->first}; // Found the most specific matching rule
			}

			// Move up one directory level
			currentSearchPath = currentSearchPath.parent_path();
		}

		// No specific rule found, return the base configuration
		return { m_baseTargetConfig, "@base" };
	}

	Configuration(TargetConfiguration baseConfig, std::unordered_map<std::string, TargetConfiguration> targetConfigs)
	{
		m_baseTargetConfig = baseConfig;
		
		for (const auto& [key, config] : targetConfigs)
		{
			std::string normalizedKey = Path(key).lexically_normal().generic_string();
			
			// Strip trailing slashes so a key like "audio/sfx/" becomes "audio/sfx".
			// This ensures it perfectly matches the results of .parent_path() later.
			if (!normalizedKey.empty() && normalizedKey.back() == '/')
			{
				normalizedKey.pop_back();
			}
			
			m_targetConfigs[normalizedKey] = config;
		}
	}

	Configuration() {}
	
private:
	TargetConfiguration m_baseTargetConfig = TargetConfiguration::MakeBaseConfig();
	std::unordered_map<std::string, TargetConfiguration> m_targetConfigs = {};
};

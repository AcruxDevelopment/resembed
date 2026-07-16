#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>
#include "Types.h"

//Create a lightweight proxy wrapper
struct auto_format {
    double value;
};
//Overload the << operator to tell std::cout how to handle the wrapper
inline std::ostream& operator<<(std::ostream& os, const auto_format& fmt) {
    if (fmt.value == 0.0 || std::abs(fmt.value) >= 0.01) {
        os << std::fixed << std::setprecision(2);
    } else {
        int precision = std::abs(std::floor(std::log10(std::abs(fmt.value))));
        os << std::fixed << std::setprecision(precision);
    }
    return os << fmt.value;
}

inline std::string makeString(const char* string, int repetition)
{
	std::string result;
	result.reserve(strlen(string)*repetition);
	for(int i = 0; i < repetition; ++i)
	{
		result.append(string);
	}
	return result;
}

inline int DirectoryNestingDepth(PathPass gbase, PathPass gtarget, bool isTargetFile)
{
    // 1. Normalize paths to resolve symlinks and shortcuts like "." or ".."
    Path base = fs::weakly_canonical(gbase);
    Path target = fs::weakly_canonical(gtarget);

    // 2. Find the relative path from base to target
    fs::path rel = target.lexically_relative(base);

    // 3. If it contains ".." or is empty, it's not nested inside the base
    if (rel.empty() || rel.string().find("..") != std::string::npos) {
        return -1; 
    }

    // 4. Count the path components to determine depth
    return std::distance(rel.begin(), rel.end()) - (isTargetFile ? 1 : 0);
}

// We use a template for the Callback so it accepts lambdas, std::function, 
// or traditional function pointers with maximum performance.
namespace fs = std::filesystem;
template <typename CallbackFunc>
void ExploreTreeFiles(PathPass targetDir, CallbackFunc callback)
{
    if (!fs::exists(targetDir) || !fs::is_directory(targetDir))
	{
        std::cerr << "Invalid target path: " << targetDir << "\n";
        return;
    }

    auto options = fs::directory_options::skip_permission_denied;

    try
	{
        for (const auto& entry : fs::recursive_directory_iterator(targetDir, options))
		{
            if (entry.is_regular_file())
			{
                // Pass the absolute path to the user's callback
                callback(fs::absolute(entry.path()));
            }
        }
    }
	catch (const fs::filesystem_error& e)
	{
        std::cerr << "Filesystem exception: " << e.what() << "\n";
    }
}

inline fs::path makeAbsoluteWithBase(const fs::path& base_dir, const fs::path& rel_path)
{
    fs::path abs_base = fs::absolute(base_dir);
    return fs::weakly_canonical(abs_base / rel_path);
}

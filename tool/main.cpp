#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include "Compression.h"
#include "Configuration.h"
#include "ConfigurationParser.h"
#include "NamingConventionConverter.h"
#include "zip_file.hpp" // Or "miniz.h" directly

namespace fs = std::filesystem;

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

std::string makeString(const char* string, int repetition)
{
	std::string result;
	result.reserve(strlen(string)*repetition);
	for(int i = 0; i < repetition; ++i)
	{
		result.append(string);
	}
	return result;
}

enum class GenerateHeaderToFileResult
{
    Ok,
    CannotOpenInputFile,
    CannotOpenOutputFile,
    CompressionFailed
};

std::string GenerateIdentifierFromPath(PathPass path, bool includeExtension)
{
    std::string namespacePath;
    for (const auto& component : path)
    {
        if (!namespacePath.empty()) 
        {
            namespacePath += "::";
        }
        // Converts "sprites" -> "Sprites", "down.png" -> "DownPng"
		std::string name = includeExtension ?
			component.string() :
			component.stem().string();
        namespacePath += ConvertToNamingConvention(name, NameCase::PascalCase);
    }
    return namespacePath;
}
std::string GenerateFilenameFromPath(PathPass path, bool includeExtensions)
{
	std::string fileName = includeExtensions ?
		path.filename() :
		path.stem();

    return ConvertToNamingConvention(fileName.c_str(), NameCase::PascalCase) + ".h";
}

GenerateHeaderToFileResult GenerateHeaderToFile(
    PathPass inputFilePath, 
    PathPass outputFilePath, 
    std::string_view identifier, 
    TargetConfiguration config,
	int backtrackDepth,
	size_t* outEmbedSizeInBinary)
{
	*outEmbedSizeInBinary = 0;
    std::filesystem::create_directories(inputFilePath.parent_path());
    std::filesystem::create_directories(outputFilePath.parent_path());

    std::ifstream inputFile(inputFilePath, std::ios::binary | std::ios::ate);
    std::ofstream outputFile(outputFilePath);

    if(!inputFile) return GenerateHeaderToFileResult::CannotOpenInputFile;
    if(!outputFile) return GenerateHeaderToFileResult::CannotOpenOutputFile;

    // Read raw file completely into memory
    std::streamsize fileSize = inputFile.tellg();
    inputFile.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> fileData(fileSize);
    if (fileSize > 0)
    {
        inputFile.read(reinterpret_cast<char*>(fileData.data()), fileSize);
    }

    // Write file header common metadata
    outputFile
        << "#pragma once\n"
        << "#include <cstdint>\n"
        << "#include <cstddef>\n"
        << "#include <memory>\n"; // Required for std::unique_ptr in Method 4

    if (config.compressionLevel() == Compression::None)
    {
        // === UNCOMPRESSED / READ-ONLY DIRECT ACCESS MODE ===
        outputFile
            << "#include <string_view>\n\n"
            << "namespace Resources::Embeds::" << identifier << " {\n"
            << "\tnamespace Internal {\n"
            << "\t\tinline constexpr uint8_t DATA[] = {\n";

        int columns = 0;
        outputFile << std::hex << std::setfill('0');

        for (size_t i = 0; i < fileData.size(); ++i)
        {
            if (columns == 0) outputFile << "\t\t\t";
            outputFile << "0x" << std::setw(2) << static_cast<int>(fileData[i]) << ",";
            if (++columns >= 16) 
            {
                outputFile << "\n";
                columns = 0;
            }
        }

		*outEmbedSizeInBinary = fileSize;

        outputFile
            << std::dec
            << (columns != 0 ? "\n" : "")
            << "\t\t};\n"
            << "\t}\n\n"
            << "\tinline constexpr size_t Size() { return sizeof(Internal::DATA); }\n"
            << "\tinline constexpr const uint8_t* Data() { return Internal::DATA; }\n"
            << "\tinline std::string_view StringView()\n"
            << "\t{\n"
            << "\t\treturn { reinterpret_cast<const char*>(Internal::DATA), sizeof(Internal::DATA) };\n"
            << "\t}\n"
            << "}\n";
    }
    else
    {
        // === COMPRESSED MODE ===
        unsigned long compressedSize = mz_compressBound(static_cast<unsigned long>(fileSize));
        std::vector<uint8_t> compressedData(compressedSize);
        
        int status = mz_compress2(
            compressedData.data(), 
            &compressedSize, 
            fileData.data(), 
            static_cast<unsigned long>(fileSize),
            (int)config.compressionLevel()
        );

        if (status != MZ_OK) 
        {
            return GenerateHeaderToFileResult::CompressionFailed;
        }
        compressedData.resize(compressedSize);

		std::string minizBacktrackPathPart = makeString("../", backtrackDepth);
        outputFile
            << "#include \"" << minizBacktrackPathPart << "miniz.h\" // Required for mz_uncompress and mz_stream at runtime\n"
            << "#include <fstream>\n"
            << "#include <filesystem>\n\n"
            << "namespace Resources::Embeds::" << identifier << " {\n"
            << "\tnamespace Internal {\n"
            << "\t\tinline constexpr size_t ORIGINAL_SIZE = " << fileSize << ";\n"
            << "\t\tinline constexpr size_t COMPRESSED_SIZE = " << compressedSize << ";\n"
            << "\t\tinline constexpr uint8_t DATA[] = {\n";

        int columns = 0;
        outputFile << std::hex << std::setfill('0');

        for (size_t i = 0; i < compressedData.size(); ++i)
        {
            if (columns == 0) outputFile << "\t\t\t";
            outputFile << "0x" << std::setw(2) << static_cast<int>(compressedData[i]) << ",";
            if (++columns >= 16) 
            {
                outputFile << "\n";
                columns = 0;
            }
        }
		*outEmbedSizeInBinary = compressedSize;

        outputFile
            << std::dec
            << (columns != 0 ? "\n" : "")
            << "\t\t};\n"
            << "\t}\n\n"
            << "\tinline constexpr size_t UncompressedSize() { return Internal::ORIGINAL_SIZE; }\n"
            << "\tinline constexpr size_t CompressedSize() { return Internal::COMPRESSED_SIZE; }\n"
            << "\tinline constexpr const uint8_t* CompressedData() { return Internal::DATA; }\n\n"
            
            // METHOD 1: Memory Decompression
            << "\tinline bool Decompress(uint8_t* destBuffer, size_t destBufferSize)\n"
            << "\t{\n"
            << "\t\tif (destBufferSize < Internal::ORIGINAL_SIZE) return false;\n"
            << "\t\tunsigned long destLen = destBufferSize;\n"
            << "\t\tint status = mz_uncompress(\n"
            << "\t\t\tdestBuffer, \n"
            << "\t\t\t&destLen, \n"
            << "\t\t\tInternal::DATA, \n"
            << "\t\t\tInternal::COMPRESSED_SIZE\n"
            << "\t\t);\n"
            << "\t\treturn status == MZ_OK;\n"
            << "\t}\n\n"
            
            // METHOD 2: Direct File Stream Decompression (Uses 32KB fixed stack chunk)
            << "\tinline bool Decompress(std::ofstream& file)\n"
            << "\t{\n"
            << "\t\tif (!file.is_open()) return false;\n"
            << "\t\tmz_stream stream = {0};\n"
            << "\t\tstream.next_in = Internal::DATA;\n"
            << "\t\tstream.avail_in = Internal::COMPRESSED_SIZE;\n"
            << "\t\tif (mz_inflateInit(&stream) != MZ_OK) return false;\n"
            << "\n"
            << "\t\tconstexpr size_t CHUNK_SIZE = 32768; // 32 KB fixed stack buffer\n"
            << "\t\tuint8_t outBuffer[CHUNK_SIZE];\n"
            << "\t\tint z_status;\n"
            << "\n"
            << "\t\tdo {\n"
            << "\t\t\tstream.next_out = outBuffer;\n"
            << "\t\t\tstream.avail_out = CHUNK_SIZE;\n"
            << "\t\t\tz_status = mz_inflate(&stream, MZ_NO_FLUSH);\n"
            << "\n"
            << "\t\t\tsize_t bytesDecompressed = CHUNK_SIZE - stream.avail_out;\n"
            << "\t\t\tif (bytesDecompressed > 0) {\n"
            << "\t\t\t\tfile.write(reinterpret_cast<const char*>(outBuffer), bytesDecompressed);\n"
            << "\t\t\t}\n"
            << "\t\t} while (z_status == MZ_OK);\n"
            << "\n"
            << "\t\tmz_inflateEnd(&stream);\n"
            << "\t\treturn z_status == MZ_STREAM_END;\n"
            << "\t}\n\n"
            
            // METHOD 3: The "Smart" Decompression (Auto-handles opening/closing the file)
            << "\tinline bool Decompress(const std::filesystem::path& outputPath)\n"
            << "\t{\n"
            << "\t\tstd::ofstream file(outputPath, std::ios::binary);\n"
            << "\t\treturn Decompress(file);\n"
            << "\t}\n\n"

            // METHOD 4: Heap Decompression.
            << "\tinline std::unique_ptr<uint8_t[]> Decompress()\n"
            << "\t{\n"
            << "\t\tauto buffer = std::make_unique<uint8_t[]>(UncompressedSize());\n"
            << "\t\tbool success = Decompress(buffer.get(), UncompressedSize());\n"
            << "\t\treturn success ? std::move(buffer) : nullptr;\n"
            << "\t}\n"
            
            << "}\n";
    }

    return GenerateHeaderToFileResult::Ok;
}

int DirectoryNestingDepth(PathPass gbase, PathPass gtarget, bool isTargetFile)
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

void ProcessFile(PathPass absoluteResourcesDir, PathPass absoluteResourceFile, PathPass outputDirectory, const Configuration& config, size_t* outEmbedSizeInBinary, bool* outProcessed)
{
	*outProcessed = false;
	*outEmbedSizeInBinary = 0;
    if(absoluteResourceFile.is_relative())
	{
		std::cout << "[ProcessFile] [ERR] Resource file is not absolute: " << absoluteResourceFile << '\n';
		std::terminate();
	}
    if(absoluteResourcesDir.is_relative())
	{
		std::cout << "[ProcessFile] [ERR] Resources directory is not absolute: " << absoluteResourcesDir << '\n';
		std::terminate();
	}
    auto relativeResourceFile = absoluteResourceFile.lexically_relative(absoluteResourcesDir);
	auto [targetConfig, targetConfigKey] = config.GetConfigurationFor(absoluteResourcesDir, absoluteResourceFile);
	if(targetConfig.ignores(relativeResourceFile)) return;

	std::cout
		<< "[ProcessFile] [INF] Processing with config '" << targetConfigKey << "' : " << relativeResourceFile;

    std::string identifier = GenerateIdentifierFromPath(relativeResourceFile, targetConfig.includeExtensions());
    std::string outputFilename = GenerateFilenameFromPath(relativeResourceFile, targetConfig.includeExtensions());
    Path outputFilePath = outputDirectory / relativeResourceFile.parent_path() / outputFilename;

	int backtrackDepth = DirectoryNestingDepth(absoluteResourcesDir, absoluteResourceFile, true);
	size_t embedSizeInBinary;
    auto result = GenerateHeaderToFile(absoluteResourceFile, outputFilePath, identifier, targetConfig, backtrackDepth, &embedSizeInBinary);
	switch(result)
	{
		case GenerateHeaderToFileResult::CannotOpenInputFile:
			std::cout << "\n\n[ProcessFile] [ERR] Cannot open input file (" << absoluteResourceFile << ")\n";
			std::terminate();
			break;
		case GenerateHeaderToFileResult::CannotOpenOutputFile:
			std::cout << "\n\n[ProcessFile] [ERR] Cannot write output file (" << outputFilePath << ")\n";
			std::terminate();
			break;
		case GenerateHeaderToFileResult::CompressionFailed:
			std::cout << "\n\n[ProcessFile] [ERR] File compression failed for (" << absoluteResourceFile << ")\n";
			std::terminate();
			break;
		case GenerateHeaderToFileResult::Ok:
			std::cout << " [" 
				<< embedSizeInBinary << " B / "
				<< auto_format{((double)embedSizeInBinary/1024)} << " KB / "
				<< auto_format{((double)embedSizeInBinary/1024/1024)} << " MB "
				<< " embeded]\n";
			break;
	}
	*outProcessed = true;
	*outEmbedSizeInBinary = embedSizeInBinary;
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

// Pass custom compression levels:
// 0  -> No Compression (Raw constexpr arrays with direct std::string_view access)
// 1  -> Fastest Compression
// 6  -> Default Compression level
// 9  -> Maximum Compression
void ProcessDirectory(PathPass absoluteResourcesDir, PathPass outputDirectory, const Configuration& config)
{
	size_t totalEmbededBytesInBinary = 0;
	size_t embededFileCount = 0;
	ExploreTreeFiles(absoluteResourcesDir, [&](PathPass absoluteResourceFile)
	{
		size_t currentFileEmbededByteCount = 0;
		bool processed;
		ProcessFile(absoluteResourcesDir, absoluteResourceFile, outputDirectory, config, &currentFileEmbededByteCount, &processed);
		totalEmbededBytesInBinary += currentFileEmbededByteCount;
		embededFileCount += (int)processed;
	});

	std::cout << "\n[ProcessDirectory] [OK ] Files embeded succesfully.\n";
	std::cout
		<< "Files     : " << embededFileCount << "\n"
		<< "Bytes     : " << totalEmbededBytesInBinary << "\n"
		<< "KiloBytes : " << auto_format{(totalEmbededBytesInBinary/1024.0)} << "\n"
		<< "MegaBytes : " << auto_format{(totalEmbededBytesInBinary/1024.0/1024.0)} << "\n"
		;
}

bool ExportMiniz(PathPass outputDirectory)
{
	try
	{
		Path minizCopyFile = outputDirectory / "miniz.h";
		if(!std::filesystem::exists(minizCopyFile))
		{
			std::filesystem::copy_file("zip_file.hpp", minizCopyFile);
		}
		std::cout << "\n[ExportMiniz] [OK ] Miniz library exported for runtime decompression.\n";
	}
	catch(std::exception e)
	{
		std::cout
			<< "\n[ExportMiniz] [ERR ] Unable to export Miniz library for runtime decompression: '" 
			<< e.what()
			<< "'.\n";
		return false;
	}
	return true;
}


int main(int argc, const char** argv)
{
    auto absoluteResourcesDir = std::filesystem::current_path();
	Path outputDirectory = "../test/resources/embeds/";
	Configuration config = ConfigurationParser::ParseFromFile("resources.json");

	ProcessDirectory(absoluteResourcesDir, outputDirectory, config);
	printf("\n"); config.Print();
	if(!ExportMiniz(outputDirectory)) return -1;

}


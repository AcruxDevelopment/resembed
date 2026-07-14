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
#include "NamingConventionConverter.h"
#include "zip_file.hpp" // Or "miniz.h" directly

using PathPass = const std::filesystem::path&;
using Path = const std::filesystem::path;

enum class GenerateHeaderToFileResult
{
    Ok,
    CannotOpenInputFile,
    CannotOpenOutputFile,
    CompressionFailed
};

std::string GenerateIdentifierFromText(std::string_view text)
{
    return ConvertToNamingConvention(text, NameCase::ScreamingSnakeCase);
}

std::string GenerateIdentifierFromPath(PathPass path)
{
    return GenerateIdentifierFromText(path.c_str());
}

GenerateHeaderToFileResult GenerateHeaderToFile(
    PathPass inputFilePath, 
    PathPass outputFilePath, 
    std::string_view identifier, 
    int compressionLevel)
{
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
        << "#include <cstddef>\n";

    if (compressionLevel <= 0)
    {
        // === UNCOMPRESSED / READ-ONLY DIRECT ACCESS MODE ===
        outputFile
            << "#include <string_view>\n\n"
            << "namespace Resources::Embeds::" << identifier << " {\n"
            << "\tnamespace Internal {\n"
            << "\t\tinline constexpr uint8_t " << identifier << "_DATA[] = {\n";

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

        outputFile
            << std::dec
            << (columns != 0 ? "\n" : "")
            << "\t\t};\n"
            << "\t}\n\n"
            << "\tinline constexpr size_t Size() { return sizeof(Internal::" << identifier << "_DATA); }\n"
            << "\tinline constexpr const uint8_t* Data() { return Internal::" << identifier << "_DATA; }\n"
            << "\tinline constexpr std::string_view StringView()\n"
            << "\t{\n"
            << "\t\treturn { reinterpret_cast<const char*>(Internal::" << identifier << "_DATA), sizeof(Internal::" << identifier << "_DATA) };\n"
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
            compressionLevel
        );

        if (status != MZ_OK) 
        {
            return GenerateHeaderToFileResult::CompressionFailed;
        }
        compressedData.resize(compressedSize);

        outputFile
            << "#include \"miniz.h\" // Required for mz_uncompress and mz_stream at runtime\n"
            << "#include <fstream>\n"
            << "#include <filesystem>\n\n"
            << "namespace Resources::Embeds::" << identifier << " {\n"
            << "\tnamespace Internal {\n"
            << "\t\tinline constexpr size_t " << identifier << "_ORIGINAL_SIZE = " << fileSize << ";\n"
            << "\t\tinline constexpr size_t " << identifier << "_COMPRESSED_SIZE = " << compressedSize << ";\n"
            << "\t\tinline constexpr uint8_t " << identifier << "_DATA[] = {\n";

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

        outputFile
            << std::dec
            << (columns != 0 ? "\n" : "")
            << "\t\t};\n"
            << "\t}\n\n"
            << "\tinline constexpr size_t UncompressedSize() { return Internal::" << identifier << "_ORIGINAL_SIZE; }\n"
            << "\tinline constexpr size_t CompressedSize() { return Internal::" << identifier << "_COMPRESSED_SIZE; }\n"
            << "\tinline constexpr const uint8_t* CompressedData() { return Internal::" << identifier << "_DATA; }\n\n"
            
            // METHOD 1: Memory Decompression
            << "\tinline bool Decompress(uint8_t* destBuffer, size_t destBufferSize)\n"
            << "\t{\n"
            << "\t\tif (destBufferSize < Internal::" << identifier << "_ORIGINAL_SIZE) return false;\n"
            << "\t\tunsigned long destLen = destBufferSize;\n"
            << "\t\tint status = mz_uncompress(\n"
            << "\t\t\tdestBuffer, \n"
            << "\t\t\t&destLen, \n"
            << "\t\t\tInternal::" << identifier << "_DATA, \n"
            << "\t\t\tInternal::" << identifier << "_COMPRESSED_SIZE\n"
            << "\t\t);\n"
            << "\t\treturn status == MZ_OK;\n"
            << "\t}\n\n"
            
            // METHOD 2: Direct File Stream Decompression (Uses 32KB fixed stack chunk)
            << "\tinline bool Decompress(std::ofstream& file)\n"
            << "\t{\n"
            << "\t\tif (!file.is_open()) return false;\n"
            << "\t\tmz_stream stream = {0};\n"
            << "\t\tstream.next_in = Internal::" << identifier << "_DATA;\n"
            << "\t\tstream.avail_in = Internal::" << identifier << "_COMPRESSED_SIZE;\n"
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
            << "\t}\n"

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

void ProcessFile(PathPass absoluteResourcesDir, PathPass absoluteResourceFile, PathPass outputDirectory, int compressionLevel)
{
    if(absoluteResourceFile.is_relative()) std::terminate();
    if(absoluteResourcesDir.is_relative()) std::terminate();
    auto relativeResourceFile = absoluteResourceFile.lexically_relative(absoluteResourcesDir);

    std::string identifier = GenerateIdentifierFromPath(relativeResourceFile);
    Path outputFilePath = outputDirectory / relativeResourceFile.parent_path() / (identifier+".h");

    auto result = GenerateHeaderToFile(absoluteResourceFile, outputFilePath, identifier, compressionLevel);
    std::cout << (int)result << '\n';
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
void ProcessDirectory(PathPass absoluteResourcesDir, PathPass outputDirectory, int compressionLevel)
{
	ExploreTreeFiles(absoluteResourcesDir, [&](PathPass absoluteResourceFile)
	{
		ProcessFile(absoluteResourcesDir, absoluteResourceFile, outputDirectory, compressionLevel);
	});
}

void ExportMiniz(PathPass outputDirectory)
{
	Path minizCopyFile = outputDirectory / "miniz.h";
	if(!std::filesystem::exists(minizCopyFile))
	{
		std::filesystem::copy_file("zip_file.hpp", minizCopyFile);
	}
}

int main(int argc, const char** argv)
{
    auto absoluteResourcesDir = std::filesystem::current_path();
	Path outputDirectory = "../test/resources/embeds/";

	ProcessDirectory(absoluteResourcesDir, outputDirectory, 9);
	ExportMiniz(outputDirectory);
}

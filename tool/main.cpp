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
            << "namespace Resources::Embeds {\n"
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
            << "\tinline constexpr size_t Get_Size_" << identifier << "() { return sizeof(Internal::" << identifier << "_DATA); }\n"
            << "\tinline constexpr const uint8_t* Get_Data_" << identifier << "() { return Internal::" << identifier << "_DATA; }\n"
            << "\tinline constexpr std::string_view Get_StringView_" << identifier << "()\n"
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
        
        // mz_compress2 allows choosing custom levels (1 = fast, 9 = maximum, -1 = default)
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
            << "#include \"miniz.h\" // Required for mz_uncompress at runtime\n\n"
            << "namespace Resources::Embeds {\n"
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
            << "\tinline constexpr size_t Get_Original_Size_" << identifier << "() { return Internal::" << identifier << "_ORIGINAL_SIZE; }\n"
            << "\tinline constexpr size_t Get_Compressed_Size_" << identifier << "() { return Internal::" << identifier << "_COMPRESSED_SIZE; }\n"
            << "\tinline constexpr const uint8_t* Get_Compressed_Data_" << identifier << "() { return Internal::" << identifier << "_DATA; }\n\n"
            << "\tinline bool Decompress_" << identifier << "(uint8_t* destBuffer, size_t destBufferSize)\n"
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

int main(int argc, const char** argv)
{
    auto absoluteResourcesDir = std::filesystem::current_path();
    auto absoluteResourceFile = absoluteResourcesDir / "main.cpp";
	Path outputDirectory = "../test/resources/embeds/";
	Path minizCopyFile = outputDirectory / "miniz.h";
    
    // Pass custom compression levels:
    // 0  -> No Compression (Raw constexpr arrays with direct std::string_view access)
    // 1  -> Fastest Compression
    // 6  -> Default Compression level
    // 9  -> Maximum Compression
	int compressionLevel = 6; 
    
    ProcessFile(absoluteResourcesDir, absoluteResourceFile, outputDirectory, compressionLevel);
	std::filesystem::copy_file("zip_file.hpp", minizCopyFile);
}

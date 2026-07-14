#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <string>
#include <string_view>
#include "NamingConventionConverter.h"

using PathPass = const std::filesystem::path&;
using Path = const std::filesystem::path;

enum class GenerateHeaderToFileResult
{
	Ok,
	CannotOpenInputFile,
	CannotOpenOutputFile
};

std::string GenerateIdentifierFromText(std::string_view text)
{
	return ConvertToNamingConvention(text, NameCase::ScreamingSnakeCase);
}

std::string GenerateIdentifierFromPath(PathPass path)
{
	return GenerateIdentifierFromText(path.c_str());
}

GenerateHeaderToFileResult GenerateHeaderToFile(PathPass inputFilePath, PathPass outputFilePath, std::string_view identifier)
{
	std::filesystem::create_directories(inputFilePath.parent_path());
	std::filesystem::create_directories(outputFilePath.parent_path());

	std::ifstream inputFile(inputFilePath, std::ios::binary);
	std::ofstream outputFile(outputFilePath);

	if(!inputFile) return GenerateHeaderToFileResult::CannotOpenInputFile;
	if(!outputFile) return GenerateHeaderToFileResult::CannotOpenOutputFile;

	// header
	outputFile
		<< "#pragma once\n"
		<< "#include <cstdint>\n"
		<< "namespace Resources::Embeds {\n";

	// binary
	outputFile
		<< "\tinline constexpr uint8_t " << identifier << "[] = {\n";

	constexpr size_t BUFFER_SIZE = 4096; // Read in 4KB chunks
    char buffer[BUFFER_SIZE];
    size_t binarySize = 0;
    int columns = 0;

    // Set hex formatting
    outputFile << std::hex << std::setfill('0');

    while (inputFile.read(buffer, BUFFER_SIZE) || inputFile.gcount() > 0)
	{
		std::streamsize bytesRead = inputFile.gcount();
        for (std::streamsize i = 0; i < bytesRead; ++i)
		{
            outputFile << "0x" << std::setw(2) << static_cast<int>(static_cast<unsigned char>(buffer[i])) << ",";
            binarySize++;
            
            // Standard formatting: 16 bytes per line
            if (++columns >= 16) {
                outputFile << "\n";
                columns = 0;
            }
        }
    }

	// footer
	outputFile
		<< std::dec
		<< "\n\t};\n"	//binary
		<< "}";			//namespace

	return GenerateHeaderToFileResult::Ok;
}

void ProcessFile(PathPass absoluteResourcesDir, PathPass absoluteResourceFile, PathPass outputDirectory)
{
	if(absoluteResourceFile.is_relative()) std::terminate();
	if(absoluteResourcesDir.is_relative()) std::terminate();
	auto relativeResourceFile = absoluteResourceFile.lexically_relative(absoluteResourcesDir);

	std::string identifier = GenerateIdentifierFromPath(relativeResourceFile);
	Path outputFilePath = outputDirectory / relativeResourceFile.parent_path() / (identifier+".h");

	auto result = GenerateHeaderToFile(absoluteResourceFile, outputFilePath, identifier);
	std::cout << (int)result << '\n';
}

int main(int argc, const char** argv)
{
	auto absoluteResourcesDir = std::filesystem::current_path();
	auto absoluteResourceFile = absoluteResourcesDir / "main.cpp";
	ProcessFile(absoluteResourcesDir, absoluteResourceFile, "../test/resources/embeds/");
}

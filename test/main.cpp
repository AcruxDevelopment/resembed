#include "resources/embeds/MAIN_CPP.h"
#include <cstdint>
#include <filesystem>
#include <fstream>

int main()
{
	std::filesystem::create_directories("extracted");
	std::ofstream file("extracted/main.cpp");

	constexpr size_t size = Resources::Embeds::Get_Original_Size_MAIN_CPP();
    uint8_t buffer[size]; 
    
    if (Resources::Embeds::Decompress_MAIN_CPP(buffer, size))
    {
		file.write((const char*)buffer, size);
	}
}

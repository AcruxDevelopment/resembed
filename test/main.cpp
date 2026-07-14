#include "resources/embeds/MAIN_CPP.h"
#include <filesystem>
#include <fstream>

int main()
{
	std::filesystem::create_directories("extracted");
	std::ofstream file("extracted/main.cpp");
	file.write((const char*)Resources::Embeds::MAIN_CPP, sizeof(Resources::Embeds::MAIN_CPP));
}

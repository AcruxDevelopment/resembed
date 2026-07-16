#pragma once
#include <cstdint>
#include <cstddef>
#include <memory>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace Resources::Embeds::Text::Message {
	namespace Internal {
		inline constexpr uint8_t DATA[] = {
			0x57,0x65,0x6c,0x63,0x6f,0x6d,0x65,0x20,0x54,0x6f,0x20,0x52,0x65,0x73,0x65,0x6d,
			0x62,0x65,0x64,0x21,0x0a,0x41,0x63,0x72,0x75,0x78,0x20,0x44,0x65,0x76,0x65,0x6c,
			0x6f,0x70,0x6d,0x65,0x6e,0x74,0x0a,
		};
	}

	inline constexpr size_t Size() { return sizeof(Internal::DATA); }
	inline constexpr const uint8_t* Data() { return Internal::DATA; }
	inline std::string_view StringView()
	{
		return { reinterpret_cast<const char*>(Internal::DATA), sizeof(Internal::DATA) };
	}
	inline bool Extract(std::ofstream& file)
	{
		try{
			file.write(StringView().data(), Size());
		} catch (std::exception) { return false; }
		return true;
	}

	inline bool Extract(const std::filesystem::path& outputPath)
	{
		std::ofstream file(outputPath, std::ios::binary);
		return Extract(file);
	}

}

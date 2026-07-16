#pragma once

enum class Compression
{
	None = 0,
	Fastest = 1,
	Balanced = 6,
	Maximum = 9
};

namespace detail
{
	inline constexpr const char* CompressionNames[] =
	{
		"None",
		"Fastest",
		"2",
		"3",
		"4",
		"5",
		"Balanced",
		"7",
		"8",
		"Maximum"
	};
}

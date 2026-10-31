// SPDX-License-Identifier: GPL-2.0-or-later
// Vita adaptation; see license.txt and MODIFICATIONS.md.
/*
 * Part of VCMI engine, licensed under GNU GPL v2.0 or later.
 * See license.txt in the project root.
 */
#pragma once

#include <array>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

class VitaLegacyNumberParser
{
	class CommaPunct : public std::numpunct<char>
	{
		char do_decimal_point() const override { return ','; }
	};
	static const std::locale & commaLocale()
	{
		static const std::locale value(std::locale(), new CommaPunct());
		return value;
	}
	std::istringstream stream;
	uint64_t calls = 0, elapsedUs = 0;

public:
	VitaLegacyNumberParser()
	{
		constexpr std::array samples{
			"", " ", "\t", "0", "-0", "+0", "1", "-1", "+1", "12345",
			"1.25", "1,25", "-2,5", "1e2", "1e-4", "1e9999", "1e-9999",
			"abc", "12abc", ".5", ",5", "-", ".", "1,234.56", " \t-42 ",
			"16777217", "2147483648", "nan", "inf", "0x10", "3.4028235e38",
			"-0.0", "-0,0", "1e", "+.5", "1 234"
		};
		for(const std::string input : samples)
		{
			std::istringstream reference(input);
			if(input.find(',') != std::string::npos)
				reference.imbue(commaLocale());
			float expected;
			if(!(reference >> expected))
				expected = 0;
			if(std::bit_cast<uint32_t>(parse(input)) != std::bit_cast<uint32_t>(expected))
				throw std::runtime_error("Vita legacy number stream reuse self-test failed");
		}
		calls = elapsedUs = 0;
		std::fprintf(stderr, "Vita legacy numbers: stream reuse self-test PASS cases=%u\n", static_cast<unsigned>(samples.size()));
	}
	~VitaLegacyNumberParser()
	{
		std::fprintf(stderr, "Vita legacy numbers: calls=%llu parse_us=%llu\n",
			static_cast<unsigned long long>(calls), static_cast<unsigned long long>(elapsedUs));
	}
	float parse(const std::string & input)
	{
		const auto started = std::chrono::steady_clock::now();
		// Reset failed/eof state and positions before reusing the string buffer.
		stream.clear();
		stream.str(input);
		const auto locale = input.find(',') != std::string::npos ? commaLocale() : std::locale();
		if(stream.getloc() != locale)
			stream.imbue(locale);
		float result;
		if(!(stream >> result))
			result = 0;
		++calls;
		elapsedUs += std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count();
		return result;
	}
};

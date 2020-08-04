#pragma once
#include "stdint.h"

namespace guid {

	struct BinaryGUID {
		uint8_t bytes[16];

		bool operator==(const BinaryGUID& rhs) const{
			
			for (int i = 0; i < 16; i++) {
				if (bytes[i] != rhs.bytes[i]) return false;
			}
			return true;
		}
	};

	struct TextGUID {
		char letters[37];

		bool operator==(const TextGUID& rhs) const {

			for (int i = 0; i < 36; i++) {
				if (letters[i] != rhs.letters[i]) return false;
			}
			return true;
		}
	};

	TextGUID convert_to_text(const BinaryGUID& binGuid);
	BinaryGUID convert_to_binary(const TextGUID& txGuid);
	BinaryGUID convert_to_binary(const char* txt);

	

	class GUIDGenerator {

		class Xorshiro {
		public:

			void seed(uint64_t seed[4]) {
				for (int i = 0; i < 4; i++) {
					s[i] = seed[i];
				}
			}
			uint64_t next(void) {
				const uint64_t result = rotl(s[1] * 5, 7) * 9;

				const uint64_t t = s[1] << 17;

				s[2] ^= s[0];
				s[3] ^= s[1];
				s[1] ^= s[2];
				s[0] ^= s[3];

				s[2] ^= t;

				s[3] = rotl(s[3], 45);

				return result;
			}
		private:
			static inline uint64_t rotl(const uint64_t x, int k) {
				return (x << k) | (x >> (64 - k));
			}

			uint64_t s[4];
		};

	public:
		void init();

		BinaryGUID make_binary_guid();

		
	private:

		Xorshiro rng;
	};
}
#pragma optimize( "", off )

namespace std {
	template <class _Kty>
	struct hash;

	template<>
	struct hash<guid::BinaryGUID> {
	public:
		size_t operator()(const guid::BinaryGUID& s) const
		{
			size_t accumulator = 0;
			const size_t* ptr = (size_t*)&s.bytes[0];
			const size_t* end = (size_t*)((&s) + 1);
			
			while (ptr != end) {
				accumulator ^= *ptr;
				ptr++;
			}

			return accumulator;
		}
	};
}


/* unoptimized code section */
#pragma optimize( "", on )
#ifdef MICROGUID_IMPLEMENTATION

#include <random>

namespace guid {
	char byte_to_hex(uint8_t byte)
	{
		uint8_t low_nibble = byte & 0x0f;

		if (low_nibble >= 10)
		{
			low_nibble -= 10;
			return 'A' + low_nibble;
		}
		else {
			return '0' + low_nibble;
		}
	}

	uint8_t hex_to_byte(char hex)
	{
		uint8_t byte = 0;

		if (hex >= 'A') {
			byte = hex - 'A' +10;
			
		}
		else {
			byte = hex - '0';
		}

		return byte;
	}

	BinaryGUID convert_to_binary(const char* txt)
	{
		TextGUID txGuid;
		for (int i = 0; i < 36; i++)
		{
			txGuid.letters[i] = txt[i];
		}
		txGuid.letters[36] = 0;
		return convert_to_binary(txGuid);
	}

	BinaryGUID convert_to_binary(const TextGUID& txGuid)
	{
		BinaryGUID newguid;

		int l = 0;
		int i = 0;
		for (i; i < 4; i++) {
			uint8_t ba = hex_to_byte(txGuid.letters[l+1]);
			uint8_t bb = hex_to_byte(txGuid.letters[l]);

			newguid.bytes[i] = ba | bb << 4;
			l += 2;
		}

		
		l++;
		for (i; i < 6; i++) {
			uint8_t ba = hex_to_byte(txGuid.letters[l + 1]);
			uint8_t bb = hex_to_byte(txGuid.letters[l]);

			newguid.bytes[i] = ba | bb << 4;
			l += 2;
		}
		
		l++;
		for (i; i < 8; i++) {

			uint8_t ba = hex_to_byte(txGuid.letters[l + 1]);
			uint8_t bb = hex_to_byte(txGuid.letters[l]);

			newguid.bytes[i] = ba | bb << 4;
			l += 2;
		}
	
		l++;
		for (i; i < 10; i++) {

			uint8_t ba = hex_to_byte(txGuid.letters[l + 1]);
			uint8_t bb = hex_to_byte(txGuid.letters[l]);

			newguid.bytes[i] = ba | bb << 4;
			l += 2;
		}

		l++;
		for (i; i < 16; i++) {

			uint8_t ba = hex_to_byte(txGuid.letters[l + 1]);
			uint8_t bb = hex_to_byte(txGuid.letters[l]);

			newguid.bytes[i] = ba | bb << 4;
			l += 2;
		}

		return newguid;
	}

	TextGUID convert_to_text(const BinaryGUID& binGuid)
	{
		TextGUID newguid;

		//123e4567-e89b-42d3-a456-426614174000
		int l = 0;
		int i = 0;
		for (i; i < 4; i++) {

			newguid.letters[l] = byte_to_hex(binGuid.bytes[i] >> 4);
			newguid.letters[l + 1] = byte_to_hex(binGuid.bytes[i]);

			l += 2;
		}

		newguid.letters[l] = '-';
		l++;
		for (i; i < 6; i++) {

			newguid.letters[l] = byte_to_hex(binGuid.bytes[i] >> 4);
			newguid.letters[l + 1] = byte_to_hex(binGuid.bytes[i]);

			l += 2;
		}
		newguid.letters[l] = '-';
		l++;
		for (i; i < 8; i++) {

			newguid.letters[l] = byte_to_hex(binGuid.bytes[i] >> 4);
			newguid.letters[l + 1] = byte_to_hex(binGuid.bytes[i]);

			l += 2;
		}
		newguid.letters[l] = '-';
		l++;
		for (i; i < 10; i++) {

			newguid.letters[l] = byte_to_hex(binGuid.bytes[i] >> 4);
			newguid.letters[l + 1] = byte_to_hex(binGuid.bytes[i]);

			l += 2;
		}
		newguid.letters[l] = '-';
		l++;
		for (i; i < 16; i++) {

			newguid.letters[l] = byte_to_hex(binGuid.bytes[i] >> 4);
			newguid.letters[l + 1] = byte_to_hex(binGuid.bytes[i]);

			l += 2;
		}
		//null terminate
		newguid.letters[l] = 0;
		return newguid;
	}
#pragma optimize( "", off )

	void GUIDGenerator::init()
	{
		std::random_device generator;

		uint64_t seed[4];
		for (int i = 0; i < 4; i++) {
			uint64_t n = generator(); //first 32 bits
			n = (n << 32) | generator(); //second 32 bits

			seed[i] = n;
		}
		rng.seed(seed);

		auto testguid = make_binary_guid();

		auto textversion = guid::convert_to_text(testguid);

		auto otherbin = guid::convert_to_binary(textversion);
		if (otherbin != testguid)
		{
			std::abort();
		}
	}
	/* unoptimized code section */
#pragma optimize( "", on )
	BinaryGUID GUIDGenerator::make_binary_guid()
	{
		uint64_t rng_bytes[2];

		rng_bytes[0] = rng.next();
		rng_bytes[1] = rng.next();

		BinaryGUID new_guid;

		memcpy(&new_guid, rng_bytes, sizeof(BinaryGUID));

		//clear the 4th most significant bytes
		new_guid.bytes[6] &= 0xf;
		//set 4 most significant bytes to type 4
		new_guid.bytes[6] |= 4 << 4;

		//clear the 2th most significant bytes
		new_guid.bytes[8] &= 0x3f;
		//set 2 most significant bytes to 2
		new_guid.bytes[8] |= 2 << 6;

		return new_guid;
	}	
}

#endif MICROGUID_IMPLEMENTATION
 
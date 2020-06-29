#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <intrin.h>
#include <memory.h>


template<typename T>
std::string print_type( T a )
{
	const char table_hex[] = {
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F', 
	};
	
	char *const buffer = (char *)alloca( sizeof(a) * 2 + 1 );
	char *pbuffer = buffer;
	
	unsigned char *p = (unsigned char *)&a+sizeof(a)-1;
	for ( int i=0; i<sizeof(a); i++ ) {
		*pbuffer++ = table_hex[ *p >> 4 ];
		*pbuffer++ = table_hex[ *p & 0x0F ];
		p--;
	}
	*pbuffer = '\0';
	return std::string( buffer, sizeof(a)*2 );
}


/**
 * 分岐なしに __m256i を16進文字列に変換する
 * __m256i は 256bit = 32byte -> 64文字 = 64byte : 32byte(_m256i) x 2
 * 	FF -> 0F0F に分けてから '0' か 'A'-10 を足す
  */
std::string m256i_string( const __m256i original_value )
{
	// const __m256i original_value = _mm256_set_epi32( 0xFEDCBA98, 0x76543210, 0x8642FDB9, 0x87654321, 0,0xFF,0xFE,1 );

	char hex_str_buffer[ sizeof(__m256i) * 2 ];
	const __m256i v0 = _mm256_set1_epi8( '0' );
	const __m256i vA = _mm256_set1_epi8( 'A'-10 );
	const __m256i v9 = _mm256_set1_epi8( 9 );


	//	original_value 32byte を 64文字64byte文字列に変換する
	//		32文字 32byte x 2 に分けて処理する
	//		上位から変換して、出力時に逆転させる必要があるが、2文字単位で 
	//		0x123456789ABCDEF0_1122334455667788_99AABBCCDDEEFF00_FDB9753102468ACE
	//		'123456789ABCDEF0112233445566778899AABBCCDDEEFF00FDB9753102468ACE'
	//		0x31,32,34,44,... 0x41,43,45
	
	const __m256i shuffle_index = _mm256_set_epi8( 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 );
	
	// ニブルをバイトに展開する
	// 	ABCD
	// 	0A0B0C0D
	const uint64_t expand_nibble_mask = 0x0F0F0F0F0F0F0F0F;
	
	// 上位128bit を ニブル->バイト 256bit に展開、上下をスワップ
	__m256i high_nibbled = _mm256_set_epi64x(
		_pdep_u64( _mm256_extract_epi32( original_value, 5 ), expand_nibble_mask ),
		_pdep_u64( _mm256_extract_epi32( original_value, 4 ), expand_nibble_mask ),
		_pdep_u64( _mm256_extract_epi32( original_value, 7 ), expand_nibble_mask ),
		_pdep_u64( _mm256_extract_epi32( original_value, 6 ), expand_nibble_mask )
	);

	//	バイト順を逆転
	high_nibbled = _mm256_shuffle_epi8( high_nibbled, shuffle_index );

	// 1バイトずつ 9 と比較する。10以上なら 0xFF
	__m256i ans_compare = _mm256_cmpgt_epi8( high_nibbled, v9 );

	// 0x00 のバイトは v0 , 0xFF のバイトは vA, が入る
	// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX2&expand=339,453
	__m256i hex_base = _mm256_blendv_epi8( v0, vA, ans_compare );

	// 0～9 + '0' / 10～15 + 'A'-10
	__m256i hex_string_high = _mm256_add_epi16( hex_base, high_nibbled );

	memcpy( hex_str_buffer, &hex_string_high, sizeof( hex_string_high ) );

	// // ここから下位
	// 下位128bit を 256bit に展開、上下をスワップ
	__m256i low_nibbled = _mm256_set_epi64x(
		_pdep_u64( _mm256_extract_epi32( original_value, 1 ), expand_nibble_mask ),
		_pdep_u64( _mm256_extract_epi32( original_value, 0 ), expand_nibble_mask ),
		_pdep_u64( _mm256_extract_epi32( original_value, 3 ), expand_nibble_mask ),
		_pdep_u64( _mm256_extract_epi32( original_value, 2 ), expand_nibble_mask )
	);

	//	バイト順を逆転
	low_nibbled = _mm256_shuffle_epi8( low_nibbled, shuffle_index );
	
	// 1バイトずつ 9 と比較する。10以上なら 0xFF
	ans_compare = _mm256_cmpgt_epi8( low_nibbled, v9 );

	// 0x00 のバイトは v0 , 0xFF のバイトは vA, が入る
	// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#techs=AVX2&expand=339,453
	hex_base = _mm256_blendv_epi8( v0, vA, ans_compare );

	// 0～9 + '0' / 10～15 + 'A'-10
	__m256i hex_string_low = _mm256_add_epi16( hex_base, low_nibbled );

	memcpy( hex_str_buffer + sizeof(__m256i), &hex_string_low, sizeof( hex_string_low ) );
	
	return std::string( (const char *)&hex_str_buffer, sizeof( __m256i ) * 2 );	
}

int main() {
	uint64_t uint64 = 0xFEDC'BA98'7654'3210;
	std::string s = print_type( uint64 );
	printf("%s\n",s.c_str());
	
	const __m256i original_value = _mm256_set_epi32( 0xFEDCBA98, 0x76543210, 0x8642FDB9, 0x87654321, 0,0xFF,0xFE,1 );

	std::string hex_string = print_type( original_value );
	printf("%s\n", hex_string.c_str());
	
	hex_string = m256i_string( original_value );
	printf("%s\n",hex_string.c_str());

}


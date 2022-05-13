#ifndef BASE64_WRAPPER_H
#define BASE64_WRAPPER_H

#include "Arduino.h"
#include <WString.h>

extern "C" {
#include "libb64/cdecode.h"
#include "libb64/cencode.h"
}

class base64
{
public:
	/**
	 * convert input data to base64
	 * @param data const uint8_t *
	 * @param length size_t
	 * @return String
	 */
    static String encode(const uint8_t * data, size_t length)
	{
		size_t size = base64_encode_expected_len(length) + 1;
		char * buffer = (char *) malloc(size);
		if(buffer) {
			base64_encodestate _state;
			base64_init_encodestate(&_state);
			int len = base64_encode_block((const char *) &data[0], length, &buffer[0], &_state);
			len = base64_encode_blockend((buffer + len), &_state);

			String base64 = String(buffer);
			free(buffer);
			return base64;
		}
		return String("-FAIL-");
	}

	
	/**
	 * convert input data to base64
	 * @param text const String&
	 * @return String
	 */
    static String encode(const String& text)
	{
		return base64::encode((uint8_t *) text.c_str(), text.length());
	}
	
	
	/**
	 * convert input base64 to plain text
	 * @param base64 const char*
	 * @return String
	 */
	static String decode(const char* base64)
	{
		int len = strlen(base64);
		char plainText[len];
		
		base64_decode_chars(base64, len, plainText);
		return plainText;	
	}
	
	/**
	 * convert input base64 to plain text
	 * @param base64 const String&
	 * @return String
	 */
	static String decode(const String& base64)
	{
		return decode(base64.c_str());
	}
	
private:
};

#endif
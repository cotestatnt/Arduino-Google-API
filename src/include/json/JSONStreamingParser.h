//////////////////////////////////////////////////////////////////////////////////////
//																					//
//							JSONStreamingParser.h									//
//				JSON parser library for low resources controllers					//
//					Created by Massimo Del Fedele, 2015								//
//																					//
//  Copyright (c) 2016 and 2017 Massimo Del Fedele.  All rights reserved.			//
//																					//
//	Redistribution and use in source and binary forms, with or without				//
//	modification, are permitted provided that the following conditions are met:		//
//																					//
//	- Redistributions of source code must retain the above copyright notice,		//
//	  this list of conditions and the following disclaimer.							//
//	- Redistributions in binary form must reproduce the above copyright notice,		//
//	  this list of conditions and the following disclaimer in the documentation		//
//	  and/or other materials provided with the distribution.						//
//																					//	
//	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"		//
//	AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE		//
//	IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE		//
//	ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE		//
//	LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR				//
//	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF			//
//	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS		//
//	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN			//
//	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)			//
//	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE		//
//	POSSIBILITY OF SUCH DAMAGE.														//
//																					//
//	VERSION 1.0.0 - INITIAL VERSION													//
//	VERSION 7.0.1 - 09/10/2017 - ACCEPT EMPTY OBJECTS {} AND ARRAYS {}				//
//	VERSION 7.1.0 - 20/10/2017 - ACCEPT EMPTY OBJECTS {} AND ARRAYS {}				//
//	Version 8.0.0 - 26/07/2020 - UPDATED FOR FIRMWARE 8.0.0							//
//																					//
//////////////////////////////////////////////////////////////////////////////////////
#ifndef _JsonStreamingParser_h_
#define _JsonStreamingParser_h_

#include <stdint.h>
#include <stdlib.h>


// callback on match
typedef void (*JSONCallback)(uint8_t filter, uint8_t level, const char *name, const char *value, void *cbObj);

typedef struct {
	char func[16];
	char keyword[16];
} searchKey;

// minimum name and value sizes (gets incremented up to maximum)
#define JSON_MIN_DATA_LEN			120
#define JSON_DATA_LEN_INCREMENT		32

class JSONStreamingParser
{
	private:
	
		// parser state
		uint8_t state;
		
		// parser flags definitions
		enum
		{
			IN_QUOTES			= 0x01,
			IN_DQUOTES			= 0x02,
			IN_BACKSLASH		= 0x04,
		} PARSER_FLAGS;
		
		// parser flags
		uint8_t flags;
		
		// parsing depth level
		uint8_t level;
		
		// level flags : bit 1 means array, bit 0 means list
		// used to keep track of nested elements
		// here we allow max 32 nesting levels, NOT checked
		uint32_t levels;
		
		// callback on data got from stream
		JSONCallback callback;
		
		// object parameter passed to callback, if needed
		void *callbackObject;
		
		// limits on names and values length
		uint16_t maxDataLen;
		
		// current name and value fields
		char *name;
		uint16_t nameAlloc;
		
		char *value;
		uint16_t valueAlloc;
		
	protected:
	
		// append to name field
		void appendName(char c);
		
		// append to value field
		void appendValue(char c);
		
		// clear name and value fields
		void clearName(void);
		void clearValue(void);
		
		// execute callback and clear name/value
		void doCallback(const char *val = 0);
		char * valueOut;
		

	public:	
		searchKey filter;	
		void setSearchKey(const char* func, const char* key );
		void setOutput(const char* _value, size_t len);
		const char* getOutput();
		const char* getCallFunction();
		const char* getCallKeyword();

		// parser states definitions
		enum
		{
			PARSER_IDLE				= 0x00,
			PARSER_WAIT_SEPARATOR,
			PARSER_WAIT_NAME,
			PARSER_NAME,
			PARSER_WAIT_SEMICOLON,
			PARSER_WAIT_VALUE,
			PARSER_VALUE,
			PARSER_FINISHED,
			PARSER_ERROR			= 0xff
		} PARSER_STATE;
		
		// constructors
		JSONStreamingParser();
		
		// destructor
		~JSONStreamingParser();
		
		// set the callback
		JSONStreamingParser &setCallback(JSONCallback cb, void *cbObj);
		
		// reset the parser
		JSONStreamingParser &reset(void);
		
		// set the maximum allowed data length
		JSONStreamingParser &setMaxDataLen(uint16_t len = -1);
		
		// feed the parser with a char
		// return value:
		// 1	if needs more chars
		// 0	if finished
		// -1	if error
		uint8_t feed(char c);
		
		// get parser state
		uint8_t getState(void);
		
		// check if finished
		bool isFinished(void);
		
		// check if error
		bool isError(void);
		
		// a couple of helpers to be used into callbacks
		static bool isListEnd(const char *s) { return s && *s == '}' && !*(s+1); }
		static bool isArrayEnd(const char *s) { return s && *s == ']' && !*(s+1); }
		static bool isSeqEnd(const char *s) { return s && (*s == '}' || *s == ']') && !*(s+1); }
};

#endif

//////////////////////////////////////////////////////////////////////////////////////
//																					//
//							JSONStreamingParser.cpp									//
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
#include "JSONStreamingParser.h"


#include <stdlib.h>
#include <ctype.h>
#include <string.h>

// constructors
JSONStreamingParser::JSONStreamingParser()
{
	callback = NULL;
	
	// clear name and value
	name = value = NULL;
	nameAlloc = valueAlloc = 0;
	
	// no limits on name and value lengths
	maxDataLen = -1;
	
	// resets the parser
	reset();
}

// destructor
JSONStreamingParser::~JSONStreamingParser()
{
	if(name)
		free(name);
	if(value)
		free(value);
}
		


// set the callback
JSONStreamingParser &JSONStreamingParser::setCallback(JSONCallback cb, void *cbObj)
{
	callback = cb;
	callbackObject = cbObj;
	return *this;
}

// reset the parser
JSONStreamingParser &JSONStreamingParser::reset(void)
{
	state = PARSER_IDLE;
	flags = 0;
	level = 0;
	levels = 0;
	
	// clear stored name and value
	clearName();
	clearValue();
	
	if(valueOut){
		free(valueOut);
		valueOut = NULL;
	}

	return *this;
}

// set the maximum allowed data length
JSONStreamingParser &JSONStreamingParser::setMaxDataLen(uint16_t len)
{
	maxDataLen = len;
	return *this;
}
		
void JSONStreamingParser::setOutput(const char* _value, size_t len){
    valueOut = (char*)realloc( valueOut, sizeof(char *) * (len+1) );
	memcpy( valueOut , _value, len);
	valueOut[len] = '\0';
}

const char* JSONStreamingParser::getOutput(){
	return valueOut;
}
		 
void JSONStreamingParser::setSearchKey(const char* func, const char* key ){
	strcpy( filter.func, func);
	strcpy( filter.keyword, key);
	if(valueOut)
		free(valueOut);
	valueOut = NULL;
}

const char* JSONStreamingParser::getCallFunction(){
	return filter.func;
}

const char* JSONStreamingParser::getCallKeyword(){
	return filter.keyword;
}

// append to name field
void JSONStreamingParser::appendName(char c)
{
	if(!name)
	{
		name = (char *)malloc(JSON_MIN_DATA_LEN + 1);
		nameAlloc = JSON_MIN_DATA_LEN + 1;
		*name = 0;
	}
	uint16_t len = strlen(name);
	
	if(len + 1 >= nameAlloc)
	{
		nameAlloc += JSON_DATA_LEN_INCREMENT;
		if(nameAlloc > maxDataLen)
			nameAlloc = maxDataLen;
		name = (char *)realloc(name, nameAlloc);
	}
	if(len + 1 >= nameAlloc)
		return;
	name[len++] = c;
	name[len] = 0;
}

// append to value field
void JSONStreamingParser::appendValue(char c)
{
	if(!value)
	{
		value = (char *)malloc(JSON_MIN_DATA_LEN + 1);
		valueAlloc = JSON_MIN_DATA_LEN + 1;
		*value = 0;
	}
	uint16_t len = strlen(value);
	
	if(len + 1 >= valueAlloc)
	{
		valueAlloc += JSON_DATA_LEN_INCREMENT;
		if(valueAlloc > maxDataLen)
			valueAlloc = maxDataLen;
		value = (char *)realloc(value, valueAlloc);
	}
	if(len + 1 >= valueAlloc)
		return;
	value[len++] = c;
	value[len] = 0;
}

// clear name and value fields
void JSONStreamingParser::clearName(void)
{
	if(name)
		free(name);
	nameAlloc = 0;
	name = NULL;
}

void JSONStreamingParser::clearValue(void)
{
	if(value)
		free(value);
	valueAlloc = 0;
	value = NULL;
}
	
// execute callback and clear name/value
void JSONStreamingParser::doCallback(const char *val)
{
	if(callback)
	{
		const char *nam = name;
		if(!nam)
			nam = "";
		if(!val)
			val = value;
		if(!val)
			val = "";
		callback(0, level, nam, val, callbackObject);
	}
	clearName();
	clearValue();
}
	

// some constant strings
static const char *_listVal = "{";
static const char *_endListVal = "}";
static const char *_arrVal = "[";
static const char *_endArrVal = "]";

// feed the parser with a char
// return value:
// >0	if needs more chars
// 0	if finished
// -1	if error
uint8_t JSONStreamingParser::feed(char c)
{
	switch(state)
	{
		// on error, just drop chars and return state
		case PARSER_ERROR:
		case PARSER_FINISHED:
			return state;
			
		// on idle, espect { opening character, skipping blanks and others chars until
		case PARSER_IDLE:
			if(isspace(c))
				return state;
			if(c != '{')
			{
				//state = PARSER_ERROR;
				return state;
			}
			state = PARSER_WAIT_NAME;
			flags = 0;
			return state;
			
		case PARSER_WAIT_NAME:
			// skip spaces
			if(isspace(c))
				return state;
			
			// an empty list {} could get here after opening list
			// so we check for it (fixed bug on 2017/10/09
			if(c == '}'|| c == ']')
			{
				if(level <= 0)
				{
					if(c == '}')
						state = PARSER_FINISHED;
					else
						state = PARSER_ERROR;
					return state;
				}

				// run callback just to signal we're out of this level
				callback(0, level, "", c == '}' ? _endListVal : _endArrVal, callbackObject);
				
				level--;
				levels = levels >> 1;
				state = PARSER_WAIT_SEPARATOR;
				return state;
			}
			
			// we can have quotes or double quotes here
			if(c != '"' && c != '\'')
			{
				state = PARSER_ERROR;
				return state;
			}
			if(c == '"')
				flags = IN_DQUOTES;
			else
				flags = IN_QUOTES;
			
			// switch to reading name
			clearName();
			state = PARSER_NAME;
			return state;
			
		case PARSER_NAME:
		
			// first, look at special state after a backslash
			if(flags & IN_BACKSLASH)
			{
				appendName(c);
				flags &= ~IN_BACKSLASH;
				return state;
			}
			else if(c == '\\')
			{
				flags |= IN_BACKSLASH;
				return state;
			}
				
			switch(c)
			{
				case '"':
					if((flags & IN_QUOTES))
					{
						appendName(c);
						return state;
					}
					// switch to semicolon state
					state = PARSER_WAIT_SEMICOLON;
					flags = 0;
					return state;
					
				case '\'':
					if((flags & IN_DQUOTES))
					{
						appendName(c);
						return state;
					}
					// switch to semicolon state
					state = PARSER_WAIT_SEMICOLON;
					flags = 0;
					return state;
					
				case ':':
					if( (flags & IN_QUOTES) || (flags & IN_DQUOTES))
					{
						appendName(c);
						return state;
					}
					state = PARSER_WAIT_VALUE;
					flags = 0;
					return state;
					
				case 0:
					state = PARSER_ERROR;
					return state;
					
				default:
					appendName(c);
					return state;
			} // switch(c)
			
		case PARSER_WAIT_SEMICOLON:
			if(isspace(c))
				return state;
			if(c != ':')
			{
				state = PARSER_ERROR;
				return state;
			}
			state = PARSER_WAIT_VALUE;
			flags = 0;
			return state;
			
		case PARSER_WAIT_VALUE:
			if(isspace(c))
				return state;
			
			// value can be a simple type, a list or an array
			// check latter 2 before
			if(c == '{')
			{
				doCallback(_listVal);
				level++;
				
				// mark current level as list
				levels = levels << 1;
				
				clearName();
				state = PARSER_WAIT_NAME;
				flags = 0;
				return state;
			}
			else if(c == '[')
			{
				doCallback(_arrVal);
				level++;

				// mark current level as array
				levels = (levels << 1) | 0x01;
				
				clearName();
				state = PARSER_WAIT_VALUE;
				flags = 0;
				return state;
			}
			else if(c == '"')
			{
				flags = IN_DQUOTES;
				clearValue();
				//appendValue(c);
				state = PARSER_VALUE;

				return state;
			}
			else if(c == '\'')
			{
				flags = IN_QUOTES;
				clearValue();
				//appendValue(c);
				state = PARSER_VALUE;
				return state;
			}
			else if(c == '}' || c == ']')
			{
				if(level <= 0)
				{
					if(c == '}')
						state = PARSER_FINISHED;
					else
						state = PARSER_ERROR;
					return state;
				}

				// run callback just to signal we're out of this level
				callback(0, level, "", c == '}' ? _endListVal : _endArrVal, callbackObject);
				
				level--;
				levels >>= 1;
				state = PARSER_WAIT_SEPARATOR;
				
				return state;
			}
			else
			{
				flags = 0;
				clearValue();
				appendValue(c);
				state = PARSER_VALUE;
				return state;
			}
			
		case PARSER_VALUE:
			
			if(flags & IN_BACKSLASH)
			{
				flags &= ~IN_BACKSLASH;
				appendValue(c);
				return state;
			}
			else if(c == '\\')
			{
				flags |= IN_BACKSLASH;
				return state;
			}
			else if( ((c == '"') && (flags & IN_DQUOTES)) || ((c == '\'') && (flags & IN_QUOTES)))
			{
				flags = 0;
				//appendValue(c);
				doCallback();
				state = PARSER_WAIT_SEPARATOR;
				return state;
			}
			else if( (flags & IN_QUOTES) || (flags & IN_DQUOTES))
			{
				appendValue(c);
				return state;
			}
			else if(isalnum(c) || c == '_' || c == '.' || c == '-')
			{
				appendValue(c);
				return state;
			}
			else if(c == ',')
			{
				flags = 0;
				doCallback();
				if(levels &0x01)
					state = PARSER_WAIT_VALUE;
				else
					state = PARSER_WAIT_NAME;
				return state;
			}
			else if(isspace(c))
			{
				flags = 0;
				doCallback();
				state = PARSER_WAIT_SEPARATOR;
				return state;
			}
			else if(c == '}' || c == ']')
			{
				if(level <= 0)
				{
					if(c == '}')
						state = PARSER_FINISHED;
					else
						state = PARSER_ERROR;
					return state;
				}
				doCallback();

				// run callback just to signal we're out of this level
				callback(0, level, "", c == '}' ? _endListVal : _endArrVal, callbackObject);
				
				level--;
				levels = levels >> 1;
				state = PARSER_WAIT_SEPARATOR;
				return state;
			}
			else
			{
				state = PARSER_ERROR;
				return state;
			}
			
		case PARSER_WAIT_SEPARATOR:
			if(isspace(c))
				return state;
			else if(c == ',')
			{
				flags = 0;
				if(levels &0x01)
					state = PARSER_WAIT_VALUE;
				else
					state = PARSER_WAIT_NAME;
				return state;
			}
			else if(c == '}' || c == ']')
			{
				if(level <= 0)
				{
					if(c == '}')
						state = PARSER_FINISHED;
					else
						state = PARSER_ERROR;
					return state;
				}
				// run callback just to signal we're out of this level
				callback(0, level, "", c == '}' ? _endListVal : _endArrVal, callbackObject);
				
				level--;
				levels = levels >> 1;
				return state;
			}
			else
			{
				state = PARSER_ERROR;
				return state;
			}
			
		default:
			state = PARSER_ERROR;
			return state;
	} // switch
}
	
// get parser state
uint8_t JSONStreamingParser::getState(void)
{
	return state;
}

// check if finished
bool JSONStreamingParser::isFinished(void)
{
	return state == PARSER_FINISHED || state == PARSER_ERROR;
}

// check if error
bool JSONStreamingParser::isError(void)
{
	return state == PARSER_ERROR;
}

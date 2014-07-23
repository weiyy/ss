#pragma once

#include "ListNode.h"
#include "JsonValue.h"
const int  _MAX_WRITE_SIZE_ = 4096;
const int  _MAX_NAME_SIZE_ = 256;


class JsonBase
{
public:
	JsonBase(void);
	~JsonBase(void);

	bool parse(const char* src, JsonValue &root);
	const char* write(const JsonValue &root, int &n);
	int write(const JsonValue &root, char *document);
	void writeValue( const JsonValue &value, int &n);
	void writeValue(const JsonValue &value, int& n, char *document);
private:
	char* document_;
	ListNode nodes_;
	const char* begin_;
	const char* end_;
	const char* current_;
	const char* lastValueEnd_;
	JsonValue *lastValue_;
	enum TokenType
	{
		tokenEndOfStream = 0,
		tokenObjectBegin,
		tokenObjectEnd,
		tokenArrayBegin,
		tokenArrayEnd,
		tokenString,
		tokenNumber,
		tokenTrue,
		tokenFalse,
		tokenNull,
		tokenArraySeparator,
		tokenMemberSeparator,
		tokenComment,
		tokenError
	};

	class Token
	{
	public:
		TokenType type_;
		const char* start_;
		const char* end_;
	};

	bool expectToken( TokenType type, Token &token, const char *message );
	bool readToken( Token &token );
	void skipSpaces();
	bool match( const char* pattern, 
		int patternLength );
	void readNumber();
	bool readString();
	bool decodeNumber( Token &token );


	 bool decodeDouble( Token &token )
	{
		double value = 0;
		char* buffer = NULL;
		int count;
		int length = int(token.end_ - token.start_);
		buffer = static_cast<char *>( g_application_mem_pool->Alloc(length+1));
		if(buffer)
		{
			memset(buffer, 0, length+1);
			memcpy( buffer, token.start_, length );
			//buffer[length] = 0;
			count = sscanf( buffer, "%lf", &value );

			g_application_mem_pool->Free((void*)buffer);
			if ( count != 1 )
				return false;
			currentValue().setValue(value);
			return true;
		}
		return false;
	}


	 bool decodeString( Token &token )
	{
		//modify by zwj
		int decoded_len = token.end_ - token.start_; //- 2
		char* decoded=static_cast<char *>( g_application_mem_pool->Alloc(decoded_len+1));
		if(decoded)
		{
			memset(decoded, 0, decoded_len+1);
			if ( !decodeString( token, decoded, decoded_len) )
			{
				g_application_mem_pool->Free((void*)decoded);
				return false;
			}
			currentValue().setValue(decoded);
			g_application_mem_pool->Free((void*)decoded);
			return true;
		}
		return false;
	}

	bool readValue();
	bool readObject(Token &token);
	bool readArray( Token &token );
	bool decodeString( Token &token, char* decoded, int dlen);
	bool decodeUnicodeCodePoint( Token &token, 
		const char* &current, 
		const char* end, 
		unsigned int &unicode );
	bool decodeUnicodeEscapeSequence( Token &token, 
		const char* &current, 
		const char* end, 
		unsigned int &unicode );
	void skipUntilSpace();
	JsonValue &currentValue();
	char getNextChar();
	bool parse( const char *beginDoc, const char *endDoc,JsonValue &root);
};

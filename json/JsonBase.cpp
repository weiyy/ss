#include "JsonBase.h"

JsonBase::JsonBase(void)
{
	document_ = static_cast<char *>( g_application_mem_pool->Alloc(_MAX_WRITE_SIZE_));
}

JsonBase::~JsonBase(void)
{
	if(document_!=NULL)
	{
		g_application_mem_pool->Free((void*)document_);
		document_=NULL;
	}
}

bool JsonBase::parse(const char* src, JsonValue &root)
{
	assert(src!=NULL);
	int len=strlen(src);
	const char* pbuf=strchr(src,EOF);

	if(pbuf!=NULL)
	{
		len=pbuf-src;
	}

	memset(document_, 0, _MAX_WRITE_SIZE_);
	memcpy(document_, src, len);
	const char *begin = document_;
	const char *end = begin + len;
	return parse(begin, end, root);
}

bool JsonBase::readToken( Token &token )
{
	skipSpaces();
	token.start_ = current_;
	char c = getNextChar();
	bool ok = true;
	//printf("c: %c\t%d\n", c, c);
	switch ( c )
	{
	case '{':
		token.type_ = tokenObjectBegin;
		break;
	case '}':
		token.type_ = tokenObjectEnd;
		break;
	case '[':
		token.type_ = tokenArrayBegin;
		break;
	case ']':
		token.type_ = tokenArrayEnd;
		break;
	case '"':
		token.type_ = tokenString;
		ok = readString();
		break;
	case '/':
		//token.type_ = tokenComment;
		//ok = readComment();
		break;
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '-':
		token.type_ = tokenNumber;
		readNumber();
		break;
	case 't':
		token.type_ = tokenTrue;
		ok = match( "rue", 3 );
		break;
	case 'f':
		token.type_ = tokenFalse;
		ok = match( "alse", 4 );
		break;
	case 'n':
		token.type_ = tokenNull;
		ok = match( "ull", 3 );
		break;
	case ',':
		token.type_ = tokenArraySeparator;
		break;
	case ':':
		token.type_ = tokenMemberSeparator;
		break;
	case 0:
		token.type_ = tokenEndOfStream;
		break;
	default:
		ok = false;
		break;
	}
	if ( !ok )
		token.type_ = tokenError;
	token.end_ = current_;
	return true;
}



bool JsonBase::readValue()
{
	Token token;
	readToken( token );
	bool successful = true;

	switch ( token.type_ )
	{
	case tokenObjectBegin:
		//modify by zwj
		successful = readObject( token );
		break;
	case tokenArrayBegin:
		successful = readArray( token );
		break;
	case tokenNumber:
		successful = decodeNumber( token );
		break;
	case tokenString:
		//printf("decodeString( token );\tcurrentValue().isString(): %d\tcurrentValue().isBool(): %d\n", currentValue().isString(), currentValue().isBool());
		successful = decodeString( token );
		//printf("currentValue().asString(): %s\ttoken.type_: %d\n", currentValue().asString(), token.type_);
		break;
		break;
	case tokenTrue:
		//printf("tokenTrue currentValue().setValue(true);\tcurrentValue().isString(): %d\tcurrentValue().isBool(): %d\n", currentValue().isString(), currentValue().isBool());
		currentValue().setValue(true);
		//printf("tokenTrue currentValue().asBool(): %d\ttoken.type_: %d\n", currentValue().asBool(), token.type_);
		break;
	case tokenFalse:
		//printf("tokenFalse currentValue().setValue(true);\tcurrentValue().isString(): %d\tcurrentValue().isBool(): %d\n", currentValue().isString(), currentValue().isBool());
		currentValue().setValue(false);
		//printf("tokenFalse currentValue().asBool(): %d\ttoken.type_: %d\n", currentValue().asBool(), token.type_);
		break;
	case tokenNull:
		currentValue().setValue();
		break;
	default:
		return false;
	}


	return successful;
}

bool JsonBase::parse( const char *beginDoc, const char *endDoc,JsonValue &root)
{
	begin_ = beginDoc;
	end_ = endDoc;
	current_ = begin_;
	lastValueEnd_ = 0;
	lastValue_ = 0;

	while ( !nodes_.empty() )
	{
		nodes_.PopFront();
	}

	PNode node = (PNode)( g_application_mem_pool->Alloc(sizeof(Node) ) );
	node->value = (void*)&root;
	nodes_.PushFront(node);
	bool successful = readValue();
	node = nodes_.PopFront();
	if(node)
		g_application_mem_pool->Free((void*)node);
	Token token;
	readToken( token );


	return successful;
}

bool JsonBase::expectToken( TokenType type, Token &token, const char *message )
{
	readToken( token );
	if ( token.type_ != type )
		return false;
	return true;
}



void JsonBase::skipSpaces()
{
	while ( current_ != end_ )
	{
		char c = *current_;
		if ( c == ' '  ||  c == '\t'  ||  c == '\r'  ||  c == '\n' )
			++current_;
		else
			break;
	}
}


bool JsonBase::match( const char* pattern, 
				 int patternLength )
{
	if ( end_ - current_ < patternLength )
		return false;
	int index = patternLength;
	while ( index-- )
		if ( current_[index] != pattern[index] )
			return false;
	current_ += patternLength;
	return true;
}


void JsonBase::readNumber()
{
	while ( current_ != end_ )
	{
		if ( !(*current_ >= '0'  &&  *current_ <= '9')  &&
			(( *current_!='.') && ( *current_!='e') && ( *current_!='E') && ( *current_!='+') && ( *current_!= '-') ) )
			break;
		++current_;
	}
}

bool JsonBase::readString()
{
	char c = 0;
	while ( current_ != end_ )
	{
		c = getNextChar();
		if ( c == '\\' )
			getNextChar();
		else if ( c == '"' )
			break;
	}
	return c == '"';
}



bool JsonBase::readObject(Token &tokenStart)
{
	Token tokenName;
	char name[_MAX_NAME_SIZE_] = {0};

	while ( readToken( tokenName ) )
	{
		bool initialTokenOk = true;
		while ( tokenName.type_ == tokenComment  &&  initialTokenOk )
			initialTokenOk = readToken( tokenName );
		if  ( !initialTokenOk )
			break;
		if ( tokenName.type_ == tokenObjectEnd  &&  name[0]=='\0' )  // empty object
		{
			return true;
		}
		if ( tokenName.type_ != tokenString )
			break;

		memset(name,0,_MAX_NAME_SIZE_);
		if ( !decodeString( tokenName, name, _MAX_NAME_SIZE_) )
		{
			return false;
		}

		Token colon;
		if ( !readToken( colon ) ||  colon.type_ != tokenMemberSeparator )
		{
			return false;
		}

		const JsonValue *value = currentValue().insert(name, JsonValue::null);
		PNode node = (PNode)( g_application_mem_pool->Alloc(sizeof(Node) ) );
		node->value = (void*)value;
		nodes_.PushFront(node);
		bool ok = readValue();
		node = nodes_.PopFront();
		if(node)
			g_application_mem_pool->Free((void*)node);
		if ( !ok ) // error already set
		{
			return false;
		}

		Token comma;
		if ( !readToken( comma )
			||  ( comma.type_ != tokenObjectEnd  &&  
			comma.type_ != tokenArraySeparator &&
			comma.type_ != tokenComment ) )
		{
			return false;
		}

		bool finalizeTokenOk = true;
		while ( comma.type_ == tokenComment &&
			finalizeTokenOk )
			finalizeTokenOk = readToken( comma );
		if ( comma.type_ == tokenObjectEnd )
		{
			return true;
		}
	}
	return false;
}

bool JsonBase::readArray( Token &tokenStart )
{
	skipSpaces();
	if ( *current_ == ']' ) // empty array
	{
		Token endArray;
		readToken( endArray );
		return true;
	}
	int index = 0;
	while ( true )
	{
		const JsonValue *value = currentValue().append(JsonValue::null);
		PNode node = (PNode)( g_application_mem_pool->Alloc(sizeof(Node) ) );
		node->value = (void*)value;
		nodes_.PushFront(node);
		bool ok = readValue();
		node = nodes_.PopFront();
		if(node)
			g_application_mem_pool->Free((void*)node);
		if ( !ok ) // error already set
			return false;

		Token token;
		// Accept Comment after last item in the array.
		ok = readToken( token );
		while ( token.type_ == tokenComment  &&  ok )
		{
			ok = readToken( token );
		}
		bool badTokenType = ( token.type_ == tokenArraySeparator  &&  
			token.type_ == tokenArrayEnd );
		if ( !ok  ||  badTokenType )
		{
			return false;
		}
		if ( token.type_ == tokenArrayEnd )
			break;
		index++;
	}
	return true;
}


bool JsonBase::decodeNumber( Token &token )
{
	bool isDouble = false;
	for ( const char* inspect = token.start_; inspect != token.end_; ++inspect )
	{
		isDouble = isDouble  
			||  ( *inspect == '.' || *inspect ==  'e' || *inspect ==  'E' || *inspect ==  '+' )  
			||  ( *inspect == '-'  &&  inspect != token.start_ );
	}
	if ( isDouble )
		return decodeDouble( token );
	const char* current = token.start_;
	bool isNegative = *current == '-';
	if ( isNegative )
		++current;
	unsigned int threshold;
	if(isNegative)
	{
		threshold=(unsigned int)(-JsonValue::minInt)/ 10 ;
	}
	else
	{
		threshold=JsonValue::maxUInt / 10;
	}
	unsigned int value = 0;
	while ( current < token.end_ )
	{
		char c = *current++;
		if ( c < '0'  ||  c > '9' )
			return false;
		if ( value >= threshold )
			return decodeDouble( token );
		value = value * 10 + (unsigned int)(c - '0');
	}
	if ( isNegative )
		currentValue().setValue(-int( value ));
	else if ( value <= (unsigned int)(JsonValue::maxInt) )
		currentValue().setValue(int( value ));
	else
		currentValue().setValue(int( value ));
	return true;
}



static const char* codePointToUTF8(unsigned int cp)
{
	char* result = NULL;
	int len = 1;

	if (cp <= 0x7f) 
	{
		result = static_cast<char *>( g_application_mem_pool->Alloc(len+1));
		result[0] = static_cast<char>(cp);
	} 
	else if (cp <= 0x7FF) 
	{
		len = 2;
		result = static_cast<char *>( g_application_mem_pool->Alloc(len+1));
		result[1] = static_cast<char>(0x80 | (0x3f & cp));
		result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
	} 
	else if (cp <= 0xFFFF) 
	{
		len = 3;
		result = static_cast<char *>( g_application_mem_pool->Alloc(len+1));
		result[2] = static_cast<char>(0x80 | (0x3f & cp));
		result[1] = 0x80 | static_cast<char>((0x3f & (cp >> 6)));
		result[0] = 0xE0 | static_cast<char>((0xf & (cp >> 12)));
	}
	else if (cp <= 0x10FFFF) 
	{
		len = 4;
		result = static_cast<char *>( g_application_mem_pool->Alloc(len+1));
		result[3] = static_cast<char>(0x80 | (0x3f & cp));
		result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
		result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
		result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
	}
	if(result!=NULL)
	{
		result[len] = '\0';
	}

	return result;
}

//add by zwj
bool JsonBase::decodeString(Token &token, char* decoded, int dlen)
{
	const char* current = token.start_ + 1; // skip '"'
	const char* end = token.end_ - 1;      // do not include '"'
	int n=0;
	while ( current != end && n < dlen)
	{
		char c = *current++;

		if ( c == '"' )
			break;
		else if ( c == '\\' )
		{
			if ( current == end )
				return false;
			char escape = *current++;
			switch ( escape )
			{
			case '"': *(decoded+n)='"';++n;break;
			case '/': *(decoded+n)='/';++n;break;
			case '\\': *(decoded+n)='\\';++n;break;
			case 'b':*(decoded+n)='\b';++n;break;
			case 'f': *(decoded+n)='\f';++n;break;
			case 'n':*(decoded+n)='\n';++n;break;
			case 'r': *(decoded+n)='\r';++n;break;
			case 't': *(decoded+n)='\t';++n;break;
			case 'u':
				{
					unsigned int unicode;
					if ( !decodeUnicodeCodePoint( token, current, end, unicode ) )
						return false;
					const char* point = codePointToUTF8(unicode); 
					if(point != NULL)
					{
						int point_len = strlen(point);
						if(n+point_len < dlen)
						{
							memcpy(decoded+n, point, point_len);
							n+=point_len;
						}
						g_application_mem_pool->Free((void*)point);
					}
				}
				break;
			default:
				return false;
			}
		}
		else
		{
			*(decoded+n) = c;
			++n;
		}
	}
	return true;
}

bool
JsonBase::decodeUnicodeCodePoint( Token &token, 
								  const char* &current, 
								  const char* end, 
								  unsigned int &unicode )
{

	if ( !decodeUnicodeEscapeSequence( token, current, end, unicode ) )
		return false;
	if (unicode >= 0xD800 && unicode <= 0xDBFF)
	{
		// surrogate pairs
		if (end - current < 6)
			return false;
		unsigned int surrogatePair;
		if (*(current++) == '\\' && *(current++) == 'u')
		{
			if (decodeUnicodeEscapeSequence( token, current, end, surrogatePair ))
			{
				unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
			} 
			else
				return false;
		} 
		else
			return false;
	}
	return true;
}

bool 
JsonBase::decodeUnicodeEscapeSequence( Token &token, 
									   const char* &current, 
									   const char* end, 
									   unsigned int &unicode )
{
	if ( end - current < 4 )
		return false;
	unicode = 0;
	for ( int index =0; index < 4; ++index )
	{
		char c = *current++;
		unicode *= 16;
		if ( c >= '0'  &&  c <= '9' )
			unicode += c - '0';
		else if ( c >= 'a'  &&  c <= 'f' )
			unicode += c - 'a' + 10;
		else if ( c >= 'A'  &&  c <= 'F' )
			unicode += c - 'A' + 10;
		else
			return false;
	}
	return true;
}




JsonValue &
JsonBase::currentValue()
{
	JsonValue* v = (JsonValue*)(nodes_.GetHead()->value);
	JsonValue& value = *v;
	return value;
}



char 
JsonBase::getNextChar()
{
	if ( current_ == end_ )
		return 0;
	return *current_++;
}

const char* JsonBase::write(const JsonValue &root, int &n)
{	
	memset(document_,0,_MAX_WRITE_SIZE_);
	n=0;
	writeValue(root, n);
	return document_;
}

//add by zwj
int JsonBase::write(const JsonValue &root, char *document)
{	
	int n=0;
	writeValue(root, n, document);
	return n;
}

static bool isControlCharacter(char ch)
{
	return ch > 0 && ch <= 0x1F;
}

static bool containsControlCharacter( const char* str )
{
	while ( *str ) 
	{
		if ( isControlCharacter( *(str++) ) )
			return true;
	}
	return false;
}

static void uintToString( unsigned int value, char *&current )
{
	*--current = 0;
	do
	{
		*--current = (value % 10) + '0';
		value /= 10;
	}
	while ( value != 0 );
}

void valueToString(uint value, char* document_, int& n)
{
	char buffer[32] = {0};
    char *current = buffer + sizeof(buffer);
	uintToString( value, current );
	int len = strlen(current);
	memcpy(document_+n, current, len);
	n+=len;
	return;
}

void valueToString(int value, char* document_, int& n)
{
	char buffer[32] = "\0";
    char *current = buffer + sizeof(buffer);
	bool isNegative = value < 0;
	if ( isNegative )
	{
		value = -value;
	}
	uintToString( value, current );
	if ( isNegative )
	{
		*--current = '-';
	}
	int len = strlen(current);
	memcpy(document_+n, current,len);
	n+=len;
	return;
}

void valueToString( double value, char* document_, int& n)
{
	char buffer[32];
	sprintf(buffer, "%#.16g", value); 
	char* ch = buffer + strlen(buffer) - 1;
	int len = strlen(buffer);

	if (*ch != '0') 
	{
		memcpy(document_+n,buffer,len);// nothing to truncate, so save time
		n+=len;
		return;
	}

	while(ch > buffer && *ch == '0')
	{
		--ch;
	}
	char* last_nonzero = ch;
	while(ch >= buffer)
	{
		switch(*ch)
		{
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			--ch;
			continue;
		case '.':
			// Truncate zeroes to save bytes in output, but keep one.
		 {
			*(last_nonzero+2) = '\0';
			 len=strlen(buffer);
			 memcpy(document_+n,buffer,len);// nothing to truncate, so save time
			 n+=len;
			 return;
		 }
		default:
		 {
			 memcpy(document_+n,buffer,len);// nothing to truncate, so save time
			 n+=len;
			 return;
		 }
		}
	}
	memcpy(document_+n,buffer,len);
	n+=len;
}


void valueToQuotedString(const char *value, char* document_, int& n)
{
	int len = 0;
	// Not sure how to handle unicode...
	if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL && !containsControlCharacter( value ))
	{
		*(document_+n) = '\"';
		++n;
		len = strlen(value);	
		memcpy(document_+n, value,len);
		n+=len;
		*(document_+n) = '\"';
		++n;
		return;
	}

	*(document_+n) = '\"';
	++n;
	for (const char* c = value; *c != 0; ++c)
	{
		switch(*c)
		{
		case '\"':	
			len = strlen("\\\"");
			memcpy(document_+n, "\\\"", len);
			n+=len;
			break;
		case '\\':
			len = strlen("\\\\");
			memcpy(document_+n, "\\\\", len);
			n+=len;
			break;
		case '\b':
			len = strlen("\\b");
			memcpy(document_+n, "\\b", len);
			n+=len;
			break;
		case '\f':
			len=strlen("\\f");
			memcpy(document_+n,"\\f", len);
			n+=len;
			break;
		case '\n':
			len = strlen("\\n");
			memcpy(document_+n, "\\n", len);
			n+=len;
			break;
		case '\r':
			len = strlen("\\r");
			memcpy(document_+n, "\\r", len);
			n+=len;
			break;
		case '\t':
			len = strlen("\\t");
			memcpy(document_+n, "\\t", len);
			n+=len;
			break;
			//case '/':
			// Even though \/ is considered a legal escape in JSON, a bare
			// slash is also legal, so I see no reason to escape it.
			// (I hope I am not misunderstanding something.
			// blep notes: actually escaping \/ may be useful in javascript to avoid </ 
			// sequence.
			// Should add a flag to allow this compatibility mode and prevent this 
			// sequence from occurring.
		default:
			if ( isControlCharacter( *c ) )
			{
				char dest[9] = {0};
				sprintf(dest, "\\u%04x", static_cast<int>(*c));
				len = strlen(dest);
				memcpy(document_+n, dest, len);
				n+=len;
			}
			else
			{
				*(document_+n) = *c;
				++n;
			}
			break;
		}
	}
	*(document_+n)='\"';;
	++n;
}

//add by zwj
void JsonBase::writeValue(const JsonValue &value, int& n, char *document)
{
	int len=0;
	switch ( value.type() )
	{
	case nullValue:
		memcpy(document+n, "null", 4);
		n += 4;
		break;
	case booleanValue:
		if(value.asBool())
		{
			memcpy(document_+n, "true", 4);
			n += 4;
		}
		else{
			memcpy(document_+n, "false", 5);
			n += 5;
		}
		break;
	case intValue:
		valueToString(value.asInt(), document, n);
		break;
	case uintValue:
		valueToString(value.asUInt(), document, n);
		break;
	case realValue:
		valueToString(value.asDouble(), document, n);
		break;
	case stringValue:
		valueToQuotedString(value.asString(), document, n);
		break;
	case arrayValue:
		{
			*(document+n) = '[';
			++n;
			int size = value.size();
			for ( int index =0; index < size; ++index )
			{
				if ( index > 0 )
				{
					*(document+n) = ',';
					++n;
				}
				writeValue( *(value[index]), n, document);
			}
			*(document+n) = ']';
			++n;
		}
		break;
	case objectValue:
		{
			*(document+n) = '{';
			++n;
			ObjectValues* mapValue = value.getObject();
			if(mapValue != NULL)
			{
				_RB_tree_node<const char*,JsonValue*>* node = mapValue->first();
				int i = 0;
				for(; node!=mapValue->nil(); ++i)
				{
					if(i > 0)
					{
						*(document+n) = ',';
						++n;
					}

					*(document+n) = '\"';
					++n;
					len = strlen(node->_key);
					memcpy(document+n, node->_key, len);
					n += len;
					*(document+n) = '\"';
					++n;
					*(document+n) = ':';
					++n;
					writeValue(*(node->_value), n, document);
					node=mapValue->next(node);
				}
			}
			*(document+n) = '}';
			++n;
		}
		break;
	}
}

void JsonBase::writeValue( const JsonValue &value, int& n)
{
	int len = 0;
	switch ( value.type() )
	{
	case nullValue:
		memcpy(document_+n, "null", 4);
		n += 4;
		break;
	case booleanValue:
		if(value.asBool())
		{
			memcpy(document_+n, "true", 4);
			n += 4;
		}
		else{
			memcpy(document_+n, "false", 5);
			n += 5;
		}
		break;
	case intValue:
		valueToString(value.asInt(), document_, n);
		break;
	case realValue:
		valueToString(value.asDouble(), document_, n);
		break;
	case stringValue:
		valueToQuotedString(value.asString(), document_, n);
		break;
	case arrayValue:
		{
			*(document_+n) = '[';
			++n;
			int size = value.size();
			for ( int index =0; index < size; ++index )
			{
				if ( index > 0 )
				{
					*(document_+n) = ',';
					++n;		
				}
				writeValue( *(value[index]), n);
			}
			*(document_+n) = ']';
			++n;
		}
		break;
	case objectValue:
		{
			*(document_+n) = '{';
			++n;
			ObjectValues* mapValue = value.getObject();
			if(mapValue != NULL)
			{
				_RB_tree_node<const char*,JsonValue*>* node = mapValue->first();
				int i = 0;
				for(; node!=mapValue->nil(); ++i)
				{
					if(i > 0)
					{
						*(document_+n) = ',';
						++n;
					}

					*(document_+n) = '\"';
					++n;
					len = strlen(node->_key);
					memcpy(document_+n, node->_key, len);
					n+=len;
					*(document_+n) = '\"';
					++n;
					*(document_+n) = ':';
					++n;
					writeValue(*(node->_value), n);
					node = mapValue->next(node);
				}
			}
			*(document_+n) = '}';
			++n;
		}
		break;
	}
}

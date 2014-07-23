#include "JsonValue.h"

const JsonValue JsonValue::null;
const int JsonValue::minInt = int( ~((unsigned int)(-1)/2) );
const int JsonValue::maxInt = int( (unsigned int)(-1)/2 );
const unsigned int JsonValue::maxUInt = (unsigned int)(-1);

char * duplicateStringValue( const char *value, unsigned int length )
{
	//changed by zwj
	if ( value==NULL )//  ||  value[0] == 0
	{
		return 0;
	}

	if ( length == 0 )
	{
		length = (unsigned int)strlen(value);
	}

	char *newString = static_cast<char *>( g_application_mem_pool->Alloc( length + 1 ) );
	memset( newString, 0, length+1 );
	memcpy( newString, value, length );
	return newString;
}


char * duplicateStringValue(int value)
{
	int length =10;
	char *newString = static_cast<char *>( g_application_mem_pool->Alloc( length + 1 ) );
	memset( newString, 0, length+1 );
	sprintf( newString, "%d", value );
	return newString;
}

void releaseStringValue(const char *value )
{
	if ( value != NULL)
	{
		g_application_mem_pool->Free((void*)value);
	}
}

JsonValue::JsonValue(void)
{
	type_ = nullValue;
	allocated_ = 0;
}

const JsonValue * JsonValue::operator[]( const char *key ) const
{
	assert( type_ == nullValue || type_ == objectValue ||  type_ == arrayValue);
	//printf("key: %s\n", key);
	//printf("type_: %d\n", type_);
	if ((type_ != objectValue &&  type_ != arrayValue) || value_.map_ == NULL)	//changed by zwj
	{
		return &null;
	}

	_RB_tree_node<const char *, JsonValue*> * pNode = value_.map_->find(key);
	if(pNode != value_.map_->nil()) 
	{
		return pNode->value();
	}
	return &null;
}

const JsonValue *JsonValue::operator[](int index ) const
{
	assert( type_ == nullValue  ||  type_ == arrayValue );
	
	if (type_ != arrayValue || value_.map_ == NULL)
	{
		return &null;
	}

	char *key = duplicateStringValue(index);
	_RB_tree_node<const char *, JsonValue*> * pNode = value_.map_->find(key);
	JsonValue *value = NULL;
	if(pNode != value_.map_->nil())
	{
		value = pNode->_value;
	}
	releaseStringValue(key);
	return value;
}

JsonValue::~JsonValue(void)
{
	switch ( type_ )
	{
	case nullValue:
	case intValue:
	case realValue:
	case booleanValue:
		break;
	case stringValue:
		if ( allocated_)
		{
			releaseStringValue( value_.string_ );
			value_.string_ = NULL;
			allocated_ = 0;
		}
		break;
	case arrayValue:
	case objectValue:
		release_map();
		break;
	default:
		break;
	}
}

ValueType JsonValue::type() const
{
	return type_;
}


JsonValue &JsonValue::operator=( const JsonValue &other )
{
	if(this == &other)
	{
		return *this;
	}

	if(type_ == arrayValue || type_ == objectValue)
	{
		release_map();
	}

	if ( type_ == stringValue && allocated_ )
	{
		releaseStringValue(value_.string_);
		value_.string_ = NULL;
		allocated_ = 0;
	}

	type_ = other.type_;
	_RB_tree_node<const char*, JsonValue*>* node = NULL;
	JsonValue* pVal = NULL;
	char* key = NULL;
	
	switch ( type_ )
	{
	case nullValue:
	case intValue:
	case realValue:
	case booleanValue:
		value_ = other.value_;
		break;
	case stringValue:
		if ( other.allocated_)
		{
			value_.string_ = duplicateStringValue(other.value_.string_,0);
			if(value_.string_)
				allocated_ = 1;
		}
		else
		{
			value_.string_ = 0;
			allocated_=0;
		}
		break;
	case arrayValue:
	case objectValue:
		value_.map_ = new ObjectValues();
		node = other.value_.map_->first();
		for(;node != other.value_.map_->nil();)
		{
			key = duplicateStringValue(node->_key, 0);
			if(key)
			{
				pVal=static_cast<JsonValue*>( g_application_mem_pool->Alloc(sizeof(JsonValue)) );
				pVal->allocated_ = 0;
				pVal->type_ = nullValue;
				*pVal = *(JsonValue*)node->_value;
				value_.map_->insert_direct(key, pVal);
			}
			node = other.value_.map_->next(node);
		}
		break;
	default:
		//assert(1==2);
		break;
	}
	
	return *this;
}


const JsonValue* JsonValue::insert(ObjectValues* pMap, const char* key, int value)
{
	_RB_tree_node<const char *,JsonValue*> * pNode = pMap->find(key);
	JsonValue* pVal = NULL;
	if(pNode == pMap->nil())
	{
		pVal=static_cast<JsonValue*>( g_application_mem_pool->Alloc(sizeof(JsonValue)) );
		char *pkey = duplicateStringValue(key, 0);
		//changed by zwj
		if(pkey)
			pNode = pMap->insert_direct(pkey,pVal);
		else
			return &null;
	}
	else if(pNode->_value->allocated_)
	{
		releaseStringValue(pNode->_value->value_.string_ );
		pNode->_value->value_.string_ = NULL;
		pNode->_value->allocated_ = 0;
	}

	pNode->_value->type_ = intValue;
	pNode->_value->value_.int_ = value;
	pNode->_value->allocated_ = 0;
	return pNode->_value;
}

const JsonValue* JsonValue::insert(ObjectValues* pMap, const char* key, double value)
{
	_RB_tree_node<const char *,JsonValue*> * pNode = pMap->find(key);
	JsonValue* pVal=NULL;
	if(pNode == pMap->nil())
	{
		pVal = static_cast<JsonValue*>( g_application_mem_pool->Alloc(sizeof(JsonValue)) );
		char *pkey = duplicateStringValue(key,0);
		//changed by zwj
		if(pkey)
			pNode = pMap->insert_direct(pkey,pVal);
		else
			return &null;
	}
	else if(pNode->_value->allocated_)
	{
		releaseStringValue(pNode->_value->value_.string_ );
		pNode->_value->value_.string_ = NULL;
		pNode->_value->allocated_ = 0;
	}

	pNode->_value->type_ = realValue;
	pNode->_value->value_.real_ = value;
	pNode->_value->allocated_ = 0;
	return pNode->_value;
}

const JsonValue* JsonValue::insert(ObjectValues* pMap, const char* key, const char* value)
{
	_RB_tree_node<const char *,JsonValue*> * pNode = pMap->find(key);
	JsonValue* pVal=NULL;
	if(pNode == pMap->nil())
	{
		pVal = static_cast<JsonValue*>( g_application_mem_pool->Alloc(sizeof(JsonValue)) );
		char *pkey = duplicateStringValue(key,0);
		//changed by zwj
		if(pkey)
			pNode = pMap->insert_direct(pkey,pVal);
		else
			return &null;
	}
	else if(pNode->_value->allocated_)
	{
		releaseStringValue(pNode->_value->value_.string_ );
		pNode->_value->value_.string_ = NULL;
		pNode->_value->allocated_ = 0;
	}

	pNode->_value->type_ = stringValue;
	pNode->_value->value_.string_ = duplicateStringValue((char*)value,0);
	if(pNode->_value->value_.string_)
		pNode->_value->allocated_ = 1;
	return pNode->_value;
}

const JsonValue* JsonValue::insert(ObjectValues* pMap, const char* key, const JsonValue& value)
{
	_RB_tree_node<const char *,JsonValue*> * pNode = pMap->find(key);
	JsonValue* pVal = NULL;
	if(pNode == pMap->nil())
	{
		pVal = static_cast<JsonValue*>( g_application_mem_pool->Alloc(sizeof(JsonValue)) );
		pVal->allocated_ = 0;
		pVal->type_ = nullValue;
		*pVal=value;
		char *pkey = duplicateStringValue(key, 0);
		//changed by zwj
		if(pkey)
			pNode = pMap->insert_direct(pkey, pVal);
		else
			return &null;
	}
	else
	{
		*(pNode->_value) = value;
	}

	return pNode->_value;
}

const JsonValue* JsonValue::insert(const char* key, const JsonValue& value)
{
	if ( type_ == nullValue )
	{
		type_ = objectValue;
		value_.map_ = new ObjectValues();
	}

	return insert(value_.map_, key, value);
}



const JsonValue* JsonValue::append(const JsonValue& value)
{
	if ( type_ == nullValue )
	{
		type_ = arrayValue;
		value_.map_ = new ObjectValues();
	}
	char *key = duplicateStringValue(size());
	const JsonValue* v = insert(value_.map_, key, value);
	releaseStringValue(key);
	return v;
}

void JsonValue::release_map()
{
	_RB_tree_node<const char*,JsonValue*>* node = value_.map_->first();
	for(;node != value_.map_->nil();)
	{
		releaseStringValue(node->_key);
		if ( node->_value->allocated_)
		{
			releaseStringValue(node->_value->value_.string_);
			node->_value->value_.string_ = NULL;
			node->_value->allocated_ = 0;
		}
		g_application_mem_pool->Free((void*)node->_value);
		node = value_.map_->next(node);
	}
	value_.map_->removeall();
	delete value_.map_;
	value_.map_ = NULL;
}

void JsonValue::print_map()
{
	_RB_tree_node<const char*,JsonValue*>* node = value_.map_->first();
	int iCount = 0;
	while (node != value_.map_->nil())
	{
		iCount ++;
		printf("iCount: %d\t key: %s\n", iCount, node->key());
		node = value_.map_->next(node);
	}
}

int JsonValue::size() const
{
	switch ( type_ )
	{
	case nullValue:
	case intValue:
	case realValue:
	case stringValue:
		return 0;
	case arrayValue:
	case objectValue:
		return value_.map_->size();
	default:
		return 0;
	}
	return 0; 
}

bool JsonValue::empty() const
{
	if ( isNull() || isArray() || isObject() )
		return size() == 0u;
	else
		return false;
}

void JsonValue::clear()
{
	assert( type_ == nullValue  ||  type_ == arrayValue  || type_ == objectValue );

	switch ( type_ )
	{
   case arrayValue:
   case objectValue:
	   value_.map_->removeall();
	   break;
   default:
	   break;
	}
}

bool JsonValue::isNull() const
{
	return type_ == nullValue;
}

bool JsonValue::isInt() const
{
   return type_ == intValue;
}

bool JsonValue::isDouble() const
{
   return type_ == realValue;
}

bool JsonValue::isBool() const
{
   return type_ == booleanValue;
}

bool JsonValue::isUInt() const
{
   return type_ == uintValue;
}

bool JsonValue::isIntegral() const
{
   return type_ == intValue  
          ||  type_ == uintValue  
          ||  type_ == booleanValue;
}

bool JsonValue::isNumeric() const
{
   return isInt() || isDouble();
}

bool JsonValue::isString() const
{
   return type_ == stringValue;
}

bool JsonValue::isArray() const
{
   return type_ == nullValue  ||  type_ == arrayValue;
}

bool JsonValue::isObject() const
{
   return type_ == nullValue  ||  type_ == objectValue;
}

const char* JsonValue::asString() const
{
	//printf("type_: %d\n", type_);
	assert(type_==nullValue || type_==stringValue);
	switch ( type_ )
	{
	case nullValue:
		return "";
	case stringValue:
		if(allocated_ && value_.string_ != NULL)
		{
			return value_.string_;
		}
		return "";
	case booleanValue:
		return value_.bool_ ? "true" : "false";
	}
	return ""; // unreachable
}

int JsonValue::asInt() const
{
	assert(type_ == nullValue || type_ == intValue || type_ == uintValue || type_ == booleanValue);
	switch ( type_ )
	{
	case nullValue:
		return 0;
	case intValue:
		return value_.int_;
	case uintValue:
		if( value_.uint_ < (unsigned)maxInt )
			return value_.uint_;
		else 
			return 0;
	case realValue:
		if( value_.real_ >= minInt  &&  value_.real_ <= maxInt )
			return value_.real_;
		else
			return 0;
	case booleanValue:
		return value_.bool_ ? 1 : 0;
	}
	return 0; // unreachable
}

double JsonValue::asDouble() const
{
	assert(type_ == nullValue || type_ == intValue || type_ == uintValue || type_ == realValue || type_ == booleanValue);
	switch ( type_ )
	{
	case nullValue:
		return 0.0;
	case intValue:
		return value_.int_;
	case uintValue:
		return value_.uint_;
	case realValue:
		return value_.real_;
	case booleanValue:
		return value_.bool_ ? 1.0 : 0.0;
	}
	return 0; // unreachable;
}

bool JsonValue::asBool() const
{
	//printf("type_: %d\n", type_);
	assert(type_ == nullValue || type_ == intValue || type_ == uintValue || type_ == realValue || type_ == booleanValue);
	switch ( type_ )
	{
	case nullValue:
		return false;
	case intValue:
	case uintValue:
		return value_.int_ != 0;
	case realValue:
		return value_.real_ != 0.0;
	case booleanValue:
		//printf("value_.bool_: %d  type_: %d\n", value_.bool_, type_);
		return value_.bool_;
	case stringValue:
		return value_.string_  &&  value_.string_[0] != 0;
	}
	return false; // unreachable;
}

uint JsonValue::asUInt() const
{
	switch ( type_ )
	{
	case nullValue:
		return 0;
	case intValue:
		if( value_.int_ >= 0 )
			return value_.int_;
		else
			return 0;
	case uintValue:
		return value_.uint_;
	case realValue:
		if( value_.real_ >= 0  &&  value_.real_ <= maxUInt)
			return value_.real_;
		else
			return 0;
	case booleanValue:
		return value_.bool_ ? 1 : 0;
	}
	return 0; // unreachable;
}


ObjectValues* JsonValue::getObject() const
{
	return value_.map_;
}

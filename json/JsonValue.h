#pragma once
#include <assert.h>
#include "func.h"
#include "RB_tree.h"
#include "MemPool.h"

extern CMemPool* g_application_mem_pool;

enum ValueType
{
	nullValue = 0, ///< 'null' value
	intValue,      ///< signed integer value
	uintValue,     ///< unsigned integer value
	realValue,     ///< double value
	stringValue,   ///< UTF-8 string value
	booleanValue,  ///< bool value
	arrayValue,    ///< array value (ordered list)
	objectValue    ///< object value (collection of name/value pairs).
};



char * duplicateStringValue( const char *value, unsigned int length );
char * duplicateStringValue(int value);
void releaseStringValue(const char *value );

class rbtree_value_alloctor {
public:
	void *alloc(size_t n)
	{
		return g_application_mem_pool->Alloc(n);
	}

	void dealloc(void *p)
	{
		g_application_mem_pool->Free(p);
	}
};

class JsonValue;
typedef _RB_tree<const char*, JsonValue*, map_compare_string, rbtree_value_alloctor> ObjectValues;

class JsonValue 
{
public:
	static const JsonValue null;
	static const int minInt;
	static const int maxInt;
	static const unsigned int maxUInt;
public:
	JsonValue();
	~JsonValue();

	

	const JsonValue* insert(const char* key,int value)
	{
		if ( type_ == nullValue )
		{
			type_=objectValue;
			value_.map_ = new ObjectValues();
		}

		return insert(value_.map_,key,value);
	}

	const JsonValue* insert(const char* key,double value)
	{
		if ( type_ == nullValue )
		{
			type_=objectValue;
			value_.map_ = new ObjectValues();
		}

		return insert(value_.map_,key,value);
	}


	const JsonValue* insert(const char* key,const char* value)
	{
		if ( type_ == nullValue )
		{
			type_=objectValue;
			value_.map_ = new ObjectValues();
		}

		return insert(value_.map_,key,value);
	}

	const JsonValue* insert(const char* key, const JsonValue& value);
	const JsonValue* append(int value)
	{
		if ( type_ == nullValue )
		{
			type_=arrayValue;
			value_.map_ = new ObjectValues();
		}
		char *key = duplicateStringValue(size());
		const JsonValue* v = insert(value_.map_, key, value);
		releaseStringValue(key);
		return v;
	}

	const JsonValue* append(double value)
	{
		if ( type_ == nullValue )
		{
			type_=arrayValue;
			value_.map_ = new ObjectValues();
		}
		char *key = duplicateStringValue(size());
		const JsonValue* v=insert(value_.map_,key,value);
		releaseStringValue(key);
		return v;
	}

	const JsonValue* append(const char* value)
	{
		if ( type_ == nullValue )
		{
			type_=arrayValue;
			value_.map_ = new ObjectValues();
		}
		char *key = duplicateStringValue(size());
		const JsonValue* v=insert(value_.map_,key,value);
		releaseStringValue(key);
		return v;
	}

	const JsonValue* append(const JsonValue& value);
	void setValue()
	{
		type_ = nullValue;
		allocated_ = 0;

	}

	void setValue(int value)
	{
		if ( allocated_)
		{
			releaseStringValue(value_.string_ );
			value_.string_ = NULL;
			allocated_ = 0;
		}

		type_ = intValue;
		value_.int_ = value;
		allocated_ = 0;
	}

	void setValue(uint value)
	{
		if ( allocated_)
		{
			releaseStringValue(value_.string_ );
			value_.string_ = NULL;
			allocated_ = 0;
		}

		type_ = intValue;
		value_.uint_ = value;
		allocated_ = 0;
	}

	void setValue(double value)
	{
		if ( allocated_)
		{
			releaseStringValue(value_.string_ );
			value_.string_ = NULL;
			allocated_ = 0;
		}

		type_ = realValue;
		value_.real_ = value;
		allocated_ = 0;
	}

	void setValue(bool value)
	{
		if ( allocated_)
		{
			releaseStringValue(value_.string_ );
			value_.string_ = NULL;
			allocated_ = 0;
		}
		
		type_ = booleanValue;
		value_.bool_ = value;
		allocated_ = 0;
	}

	void setValue(const char* value)
	{
		if ( allocated_)
		{
			releaseStringValue(value_.string_ );
		}

		type_ = stringValue;
		value_.string_ = duplicateStringValue(value, 0);
		allocated_ = 1;
	}

	ValueType type() const;
	int size() const;
	bool empty() const;
	void clear();
	bool isNull() const;
	bool isInt() const;
	bool isBool() const;
	bool isUInt() const;
	bool isIntegral() const;
	bool isDouble() const;
	bool isNumeric() const;
	bool isString() const;
	bool isArray() const;
	bool isObject() const;

	void print_map();
	JsonValue & operator=( const JsonValue &other );
	const JsonValue * operator[]( const char *key ) const;
	const JsonValue * operator[]( int index ) const;
	int asInt() const;
	double asDouble() const;
	bool asBool() const;
	uint asUInt() const;
	const char* asString() const;
	ObjectValues* getObject() const;

private:
	const JsonValue* insert(ObjectValues* pMap,const char* key,int value);
	const JsonValue* insert(ObjectValues* pMap,const char* key,double value);
	const JsonValue* insert(ObjectValues* pMap,const char* key,const char* value);
	const JsonValue* insert(ObjectValues* pMap,const char* key,const JsonValue& value);	

	void release_map();
	JsonValue( const JsonValue &other );
	union ValueHolder
	{
		int int_;
		uint uint_;
		double real_;
		bool bool_;
		char *string_;
		ObjectValues *map_;
	}value_;
	ValueType type_ : 8; 
	int allocated_ : 1; 	  
};


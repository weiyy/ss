/*

Program by Shasure
2011.5

-=-=- History -=-=-

[2013.2.21]
1.修复了当<>括号不完整时导致的死循环问题
2.修复了对注释识别错误的问题


[2011.5]
1.创建代码

*/




#pragma once
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#define PLIST_UNKNOWN					0xFF

#define PLIST_ROOT						0x00
#define PLIST_DICTIONARY				0x31
#define PLIST_ARRAY						0x32
#define PLIST_BOOL_TRUE					0x33
#define PLIST_BOOL_FALSE				0x34
#define PLIST_NUMBER					0x35
#define PLIST_STRING					0x36
#define PLIST_DATE						0x37
#define PLIST_DATA						0x38
#define PLIST_REAL						0x39

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
/* modify_option */
#define PLIST_MODIFY_NAME				0x01
#define PLIST_MODIFY_VALUE				0x02
#define PLIST_MODIFY_TYPE				0x04
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class PLIST_NODE
{
public:
	unsigned char node_type;
	char * name;
	char * value;

	PLIST_NODE * first_child_node;
	PLIST_NODE * parent_node;
	PLIST_NODE * prev_node;
	PLIST_NODE * next_node;

public:
	/*
	*/
	PLIST_NODE()
	{
		node_type = PLIST_UNKNOWN;
		name = NULL;
		value = NULL;
		first_child_node = NULL;
		parent_node = NULL;
		prev_node = NULL;
		next_node = NULL;
	}

	/*
	*/
	void Modify(char * name_str, char * value_str, int type, unsigned int modify_option)
	{
		if(PLIST_MODIFY_NAME & modify_option)
		{
			if(name)
			{
				delete name;
			}

			if(name_str)
			{
				name = new char[strlen(name_str)+1];
				strcpy(name, name_str);
			}
			else
			{
				name = NULL;
			}
		}

		if(PLIST_MODIFY_VALUE & modify_option)
		{
			if(value)
			{
				delete value;
			}

			if(value_str)
			{
				value = new char[strlen(value_str)+1];
				strcpy(value, value_str);
			}
			else
			{
				value = NULL;
			}
		}

		if(PLIST_MODIFY_TYPE & modify_option)
		{
			node_type = type;
		}
	}

	/*
	*/
	PLIST_NODE * FindChild(char * name_str)
	{
		PLIST_NODE * it = first_child_node;
		while(it)
		{
			if(0 == strcasecmp(it->name, name_str))
			{
				return it;
			}

			it = it->next_node;
		}

		return NULL;
	}

	/*
	*/
	PLIST_NODE * InsertChildNode(int type, char * name_str, char * value_str)
	{
		PLIST_NODE * node = first_child_node;
		if(!node)
		{
			node = new PLIST_NODE;
			first_child_node = node;
			node->prev_node = NULL;
		}
		else
		{
			while(node->next_node)
			{
				node = node->next_node;
			}

			PLIST_NODE * new_node = new PLIST_NODE;
			new_node->prev_node = node;
			node->next_node = new_node;
			node = new_node;
		}

		node->first_child_node = NULL;
		if(PLIST_DICTIONARY == node_type)
		{
			node->name = new char[strlen(name_str) + 1];
			strcpy(node->name, name_str);
		}
		else
		{
			node->name = NULL;
		}
		node->next_node = NULL;
		node->node_type = type;
		node->parent_node = this;
		if(value_str)
		{
			node->value = new char[strlen(value_str) + 1];
			strcpy(node->value, value_str);
		}
		else
		{
			node->value = NULL;
		}

		return node;
	}

};
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class PLIST
{
private:
	struct NODE_STACK_ITEM
	{
		PLIST_NODE * node;

		NODE_STACK_ITEM * prev;
		NODE_STACK_ITEM * next;
	};

private:
	PLIST_NODE * root_node;

public:
	PLIST()
	{
//		root_node = NULL;
 		root_node = new PLIST_NODE;
 		root_node->node_type = PLIST_ROOT;
	}

	~PLIST()
	{
		del_node(root_node);
	}

	/*
	*/
	int Load(char * file_name)
	{
		int ret = 0;

		FILE * f = fopen(file_name, "rb");
		if(f)
		{
			fseek(f, 0, SEEK_END);
			long file_size = ftell(f);
			if(file_size > 0)
			{
				char * buff = new char[file_size];

				fseek(f, 0, SEEK_SET);
				fread(buff, file_size, 1, f);

				ret = Load(buff, file_size);

				delete buff;
			}

			fclose(f);

			return ret;
		}

		return -1;
	}

	/*
	*/
	int Load(char * buff, int buff_len)
	{
		int ret = 0;
		int pos = -1;

		for(int i=0; i<buff_len-1; i++)
		{
			if(buff[i] == '<' && (i+12 < buff_len && 0 == memcmp(&buff[i+1], "plist", 5) ) )
			{
				for(int j=i; j<buff_len-1; j++)
				{
					if(buff[j] == '>')
					{
						pos = j+1;
						break;
					}
				}
			}
		}

		if(-1 == pos)
			return -1;

		del_node(root_node);
		//root_node = NULL;
		root_node = new PLIST_NODE;
		root_node->node_type = PLIST_ROOT;

		PLIST_NODE * current_node = root_node;

		NODE_STACK_ITEM * stack_first = new NODE_STACK_ITEM;
		stack_first->next = NULL;
		stack_first->prev = NULL;
		stack_first->node = root_node;
		NODE_STACK_ITEM * stack_curr = stack_first;

		int after_key = 0;

		int in_where = 0;
		while(pos < buff_len)
		{
			if('<' == buff[pos])
			{
				pos ++;

				if(pos < buff_len && '/' == buff[pos])
				{
					pos ++;
					in_where = 3;
				}
				else if(pos < buff_len && '!' == buff[pos])
				{
					while( pos < buff_len )
					{
						if( (buff[pos] == '>') && (*(unsigned short*)&buff[pos-2] == *(unsigned short*)"--") )
						{
							pos++;
							break;
						}
						else
						{
							pos++;
						}
					}
				}
				else
				{
					in_where = 1;

					if(0 == after_key)
					{
						PLIST_NODE * new_node = new PLIST_NODE;

						if(PLIST_ROOT == stack_curr->node->node_type || PLIST_ARRAY == stack_curr->node->node_type || PLIST_DICTIONARY == stack_curr->node->node_type)
						{
							new_node->parent_node = stack_curr->node;
							if(stack_curr->node->first_child_node)
							{
								PLIST_NODE * temp = stack_curr->node->first_child_node;
								while(temp->next_node)
								{
									temp = temp->next_node;
								}

								temp->next_node = new_node;
								new_node->prev_node = temp;
							}
							else
							{
								stack_curr->node->first_child_node = new_node;
							}
						}
						else
						{
							new_node->parent_node = stack_curr->node->parent_node;
							if(stack_curr->node->next_node)
							{
								PLIST_NODE * temp = stack_curr->node->next_node;
								while(temp->next_node)
								{
									temp = temp->next_node;
								}

								temp->next_node = new_node;
								new_node->prev_node = temp;
							}
							else
							{
								stack_curr->node->next_node = new_node;
								new_node->prev_node = stack_curr->node;
							}
						}

						current_node = new_node;

						NODE_STACK_ITEM * new_stack = new NODE_STACK_ITEM;
						new_stack->next = NULL;
						new_stack->node = current_node;
						new_stack->prev = stack_curr;
						stack_curr->next = new_stack;
						stack_curr = new_stack;
					}

					if(1 == after_key)
					{
						after_key = 0;
					}
				}
			}
			else if(pos + 1 < buff_len && '/' == buff[pos] && '>' == buff[pos+1])
			{
				pos += 2;

				in_where = 0;

				if(0 == after_key && stack_curr->prev)
				{
					NODE_STACK_ITEM * del_item = stack_curr;
					stack_curr->prev->next = NULL;
					stack_curr = stack_curr->prev;
					delete del_item;
				}
			}
			else if('>' == buff[pos])
			{
				pos ++;

				in_where = (1 == in_where ? 2 : 0);

				if(0 == in_where && 0 == after_key)
				{
					if(stack_curr->prev)
					{
						NODE_STACK_ITEM * del_item = stack_curr;
						stack_curr->prev->next = NULL;
						stack_curr = stack_curr->prev;
						delete del_item;
					}
				}
			}
			else	//other character
			{
				if(1 == in_where)
				{
					bool is_matched = false;

					for(int i=pos; i<buff_len; i++)
					{
						if('>' == buff[i] || ('/' == buff[i] && '>' == buff[i+1]))
						{
							if(0 == memcmp(&buff[pos], "dict", i-pos))
							{
								current_node->node_type = PLIST_DICTIONARY;
							}
							else if(0 == memcmp(&buff[pos], "array", i-pos))
							{
								current_node->node_type = PLIST_ARRAY;
							}
							else if(0 == memcmp(&buff[pos], "string", i-pos))
							{
								current_node->node_type = PLIST_STRING;
							}
							else if(0 == memcmp(&buff[pos], "data", i-pos))
							{
								current_node->node_type = PLIST_DATA;
							}
							else if(0 == memcmp(&buff[pos], "date", i-pos))
							{
								current_node->node_type = PLIST_DATE;
							}
							else if(0 == memcmp(&buff[pos], "integer", i-pos))
							{
								current_node->node_type = PLIST_NUMBER;
							}
							else if(0 == memcmp(&buff[pos], "real", i-pos))
							{
								current_node->node_type = PLIST_REAL;
							}
							else if(0 == memcmp(&buff[pos], "true", i-pos))
							{
								current_node->node_type = PLIST_BOOL_TRUE;
							}
							else if(0 == memcmp(&buff[pos], "false", i-pos))
							{
								current_node->node_type = PLIST_BOOL_FALSE;
							}
							else if(0 == memcmp(&buff[pos], "key", i-pos))
							{
								after_key = 1;
							}
							else
							{
								clear_all_node_stack_item(stack_first);
								del_node(root_node);
								root_node = NULL;
								return -2; //unknown data
							}

							pos = i;

							is_matched = true;
							break;
						}
					}

					if(!is_matched)
					{
						return -10;
					}
				}
				else if(3 == in_where)
				{
					bool is_matched = false;

					for(int i=pos; i<buff_len; i++)
					{
						if('>' == buff[i])
						{
							pos = i;

							is_matched = true;
							break;
						}
					}

					if(!is_matched)
					{
						return -10;
					}
				}
				else if(0 == in_where)
				{
					pos ++;
				}
			}

			if(2 == in_where)
			{
				bool is_matched = false;

				for(int i=pos; i<buff_len; i++)
				{
					if('<' == buff[i])
					{
						if(1 == after_key && current_node->parent_node->node_type == PLIST_DICTIONARY)
						{
							current_node->name = new char[i - pos + 1];
							memcpy(current_node->name, &buff[pos], i-pos);
							current_node->name[i - pos] = 0;
						}
						else if(current_node->node_type == PLIST_STRING
							||current_node->node_type == PLIST_DATA
							||current_node->node_type == PLIST_DATE
							||current_node->node_type == PLIST_NUMBER
							||current_node->node_type == PLIST_REAL
							)
						{
							if(current_node->parent_node->node_type == PLIST_DICTIONARY)
							{
								current_node->value = new char[i - pos + 1];
								memcpy(current_node->value, &buff[pos], i-pos);
								current_node->value[i - pos] = 0;
								
								if(current_node->node_type == PLIST_DATA)
								{
									datafield_trim(current_node->value);
								}
								
								after_key = 0;
							}
							else
							{
								current_node->value = new char[i - pos + 1];
								memcpy(current_node->value, &buff[pos], i-pos);
								current_node->value[i - pos] = 0;
								
								if(current_node->node_type == PLIST_DATA)
								{
									datafield_trim(current_node->value);
								}
							}
						}
						
						pos = i;
						is_matched = true;
						break;
					}
				}

				if(!is_matched)
				{
					return -10;
				}
			}

		}

		clear_all_node_stack_item(stack_first);

		return ret;
	}

	/*
	*/
	char* GetBuffer()
	{
		char * out_buff;
		GetBuffer(&out_buff);
		return out_buff;
	}

	/*
	*/
	int GetBuffer(char ** out_buff)
	{
		int curr_buffer_size = 64 * 1024;

		*out_buff = new char[curr_buffer_size];

		int pos = 0;

		char * plist_header = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n";

		strcpy(*out_buff, plist_header);
		pos = strlen(plist_header);


		enum_node(root_node, out_buff, curr_buffer_size, pos);

		check_and_inc_mem(out_buff, curr_buffer_size, pos, 9);
		memcpy(&(*out_buff)[pos], "</plist>\0", 9);
		pos += 9;

		return pos;
	}

	/*
	*/
	void ReleaseBuffer(char * buff)
	{
		delete buff;
	}

	/*
	*/
	int Save(char * file_name)
	{
		char * buff;
		int buff_size = GetBuffer(&buff) - 1;

		FILE *f = fopen(file_name, "wb");
		if(f)
		{
			fwrite(buff, buff_size, 1, f);
			fclose(f);
		}
		else
		{
			ReleaseBuffer(buff);
			return -1;
		}

		ReleaseBuffer(buff);
		return 0;
	}

	/*
	*/
	PLIST_NODE * Root()
	{
		return root_node;
	}

	/*
	*/
	void DeleteNode(PLIST_NODE * node)
	{
		bool is_root = false;
		if(node->node_type == PLIST_ROOT)
			is_root = true;

		if(node->parent_node && node->parent_node->first_child_node == node)
		{
			node->parent_node->first_child_node = node->next_node;
		}

		if(node->next_node)
		{
			node->next_node->prev_node = node->prev_node;
		}

		if(node->prev_node)
		{
			node->prev_node->next_node = node->next_node;
		}

		del_node(node);

		if(is_root)
		{
			PLIST();
		}
	}

private:
	void clear_all_node_stack_item(NODE_STACK_ITEM * nsi)
	{
		while(nsi)
		{
			NODE_STACK_ITEM * del_item = nsi;
			nsi = nsi->next;
			delete del_item;
		}
	}

	void datafield_trim(char * str)
	{
		if(!str)
			return;

		char * ret_str = str;

		for(; 0 != *str; str++)
		{
			if( (*str != '\x20') && (*str != '\t') && (*str != '\r') && (*str != '\n') )
			{
				*ret_str = *str;
				ret_str ++;
			}
		}

		*ret_str = 0;
	}

	char * datafield_format(char * str, int deep)
	{
		int str_len = strlen(str);

		if(str_len / 68 < 1)
			return NULL;

		deep -= 2;
		if(deep < 0)
			deep = 0;

		int inc_size = (str_len / 68 + (str_len % 68 == 0 ? 0 : 1)) * (1 + deep) + 1;

		int new_size = (str_len + 1) +  inc_size;

		char * new_str = new char[new_size];

		char * new_str_pointer = new_str;

		*new_str = '\n';
		new_str ++;
		for(int i=0; i<str_len;)
		{
			for(int n=0; n<deep; n++)
			{
				*new_str = '\t';
				new_str ++;
			}

			int copy_bytes_size = str_len - i >= 68 ? 68 : str_len - i;
			memcpy(new_str, &str[i], copy_bytes_size);
			
			i += copy_bytes_size;
			new_str += copy_bytes_size;

			*new_str = '\n';
			new_str ++;
		}
		*new_str = 0;

		return new_str_pointer;
	}

	int check_and_inc_mem(char** mem, int old_size, int curr_position, int new_data_len)
	{
		int inc_size = 0;

		while(old_size - curr_position + inc_size < new_data_len)
		{
			inc_size += 64*1024;
		}

		if(inc_size > 0)
		{
			char * new_mem = new char[old_size + inc_size];
			memcpy(new_mem, *mem, curr_position);
			delete *mem;
			*mem = new_mem;
		}

		return old_size + inc_size;
	}

	int enum_node(PLIST_NODE * node, char ** out_buff, int &buff_size, int &pos, int deep = 0)
	{
		deep ++;

		PLIST_NODE * it = node;

		while(it)
		{
			if(it->parent_node)
			{
				for(int d=2; d<deep; d++)
				{
					buff_size = check_and_inc_mem(out_buff, buff_size, pos, 1);
					(*out_buff)[pos++] = '\t';
				}

				bool is_dict_item = false;
				if(PLIST_DICTIONARY == it->parent_node->node_type)
				{
					is_dict_item = true;

					buff_size = check_and_inc_mem(out_buff, buff_size, pos, 5);
					memcpy(&(*out_buff)[pos], "<key>", 5);
					pos += 5;

					int len = strlen(it->name);
					buff_size = check_and_inc_mem(out_buff, buff_size, pos, len);
					memcpy(&(*out_buff)[pos], it->name, len);
					pos += len;

					buff_size = check_and_inc_mem(out_buff, buff_size, pos, 7);
					memcpy(&(*out_buff)[pos], "</key>\n", 7);
					pos += 7;

					for(int d=2; d<deep; d++)
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 1);
						(*out_buff)[pos++] = '\t';
					}
				}

				switch(it->node_type)
				{
				case PLIST_DICTIONARY:
					{
						if(it->first_child_node)
						{
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, 7);
							memcpy(&(*out_buff)[pos], "<dict>\n", 7);
							pos += 7;
						}
						else
						{
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
							memcpy(&(*out_buff)[pos], "<dict/>\n", 8);
							pos += 8;
						}
					}
					break;
				case PLIST_ARRAY:
					{
						if(it->first_child_node)
						{
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
							memcpy(&(*out_buff)[pos], "<array>\n", 8);
							pos += 8;
						}
						else
						{
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, 9);
							memcpy(&(*out_buff)[pos], "<array/>\n", 9);
							pos += 9;
						}
					}
					break;
				case PLIST_BOOL_TRUE:
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
						memcpy(&(*out_buff)[pos], "<true/>\n", 8);
						pos += 8;
					}
					break;
				case PLIST_BOOL_FALSE:
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 9);
						memcpy(&(*out_buff)[pos], "<false/>\n", 9);
						pos += 9;
					}
					break;
				case PLIST_NUMBER:
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 9);
						memcpy(&(*out_buff)[pos], "<integer>", 9);
						pos += 9;
						if(it->value)
						{
							int len = strlen(it->value);
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, len);
							memcpy(&(*out_buff)[pos], it->value, len);
							pos += len;
						}
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 11);
						memcpy(&(*out_buff)[pos], "</integer>\n", 11);
						pos += 11;
					}
					break;
				case PLIST_REAL:
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 6);
						memcpy(&(*out_buff)[pos], "<real>", 6);
						pos += 6;
						if(it->value)
						{
							int len = strlen(it->value);
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, len);
							memcpy(&(*out_buff)[pos], it->value, len);
							pos += len;
						}
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
						memcpy(&(*out_buff)[pos], "</real>\n", 8);
						pos += 8;
					}
					break;
				case PLIST_STRING:
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
						memcpy(&(*out_buff)[pos], "<string>", 8);
						pos += 8;
						if(it->value)
						{
							int len = strlen(it->value);
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, len);
							memcpy(&(*out_buff)[pos], it->value, len);
							pos += len;
						}
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 10);
						memcpy(&(*out_buff)[pos], "</string>\n", 10);
						pos += 10;
					}
					break;
				case PLIST_DATA:
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 6);
						memcpy(&(*out_buff)[pos], "<data>", 6);
						pos += 6;
						if(it->value)
						{
							int len = strlen(it->value);

							char * new_str = datafield_format(it->value, deep);
							if(new_str)
							{
								len = strlen(new_str);
								buff_size = check_and_inc_mem(out_buff, buff_size, pos, len);
								memcpy(&(*out_buff)[pos], new_str, len);
								delete new_str;
								pos += len;

								for(int d=2; d<deep; d++)
								{
									buff_size = check_and_inc_mem(out_buff, buff_size, pos, 1);
									(*out_buff)[pos++] = '\t';
								}
							}
							else
							{
								buff_size = check_and_inc_mem(out_buff, buff_size, pos, len);
								memcpy(&(*out_buff)[pos], it->value, len);
								pos += len;
							}
						}
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
						memcpy(&(*out_buff)[pos], "</data>\n", 8);
						pos += 8;
					}
					break;
				case PLIST_DATE:
					{
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 6);
						memcpy(&(*out_buff)[pos], "<date>", 6);
						pos += 6;
						if(it->value)
						{
							int len = strlen(it->value);
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, len);
							memcpy(&(*out_buff)[pos], it->value, len);
							pos += len;
						}
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
						memcpy(&(*out_buff)[pos], "</date>\n", 8);
						pos += 8;
					}
					break;
				}
			}


			if(it->first_child_node)
			{
				enum_node(it->first_child_node, out_buff, buff_size, pos, deep);
			}

			switch(it->node_type)
			{
			case PLIST_DICTIONARY:
				{
					if(it->first_child_node)
					{
						for(int d=2; d<deep; d++)
						{
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, 1);
							(*out_buff)[pos++] = '\t';
						}
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 8);
						memcpy(&(*out_buff)[pos], "</dict>\n", 8);
						pos += 8;
					}
				}
				break;
			case PLIST_ARRAY:
				{
					if(it->first_child_node)
					{
						for(int d=2; d<deep; d++)
						{
							buff_size = check_and_inc_mem(out_buff, buff_size, pos, 1);
							(*out_buff)[pos++] = '\t';
						}
						buff_size = check_and_inc_mem(out_buff, buff_size, pos, 9);
						memcpy(&(*out_buff)[pos], "</array>\n", 9);
						pos += 9;
					}
				}
				break;
			}

			it = it->next_node;
		}

		return 0;
	}

	void del_child_node(PLIST_NODE * node)
	{
		PLIST_NODE * it = node;

		while(it)
		{
			if(it->first_child_node)
			{
				del_child_node(it->first_child_node);
			}

			PLIST_NODE * del_it = it;

			it = it->next_node;

			if(del_it->name)
				delete del_it->name;
			if(del_it->value)
				delete del_it->value;

			delete del_it;
		}
	}

	void del_node(PLIST_NODE * node)
	{
		PLIST_NODE * it = node;

		if(it)
		{
			if(it->first_child_node)
			{
				del_child_node(it->first_child_node);
			}

			if(it->name)
				delete it->name;
			if(it->value)
				delete it->value;

			delete it;
		}
	}
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

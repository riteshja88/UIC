/*
 * =====================================================================================
 *
 *       Filename:  string.cpp
 *
 *    Description:  Implementation for string class
 *
 *        Version:  1.0
 *        Created:  04/05/2013 02:30:14 AM
 *       Revision:  none
 *       Compiler:  g++
 *
 *         Author:  Ritesh Agarwal (Omi), ragarw8@uic.edu
 *        Company:  University of Illinois, Chicago
 *
 * =====================================================================================
 */

#include "string.h"
#include <string.h>
void string_t::allocate_and_copy_from_cstring(const char * const str)
{
    size = strlen(str);
    value = new char[size + 1];
    strncpy(value, str, size); /* Does not make sense to use strncpy if str is not NULL terminated. since size will be detected using a NULL character */
    value[size] = '\0';
}

void string_t::allocate_and_copy_from_string(const string_t &str)
{
    size = str.size;
    value = new char[size + 1];
    strncpy(value, str.value, size);
    value[size] = '\0';
}

string_t::string_t()
{
    value = new char[1];
    size = 0;
    value[0] = '\0';
}

string_t::string_t(const string_t& str)
{
    allocate_and_copy_from_string(str);
}

string_t::string_t(const char * const str)
{
    allocate_and_copy_from_cstring(str);
}


string_t& string_t::operator=(const string_t& str)
{
    delete[] value;
    allocate_and_copy_from_string(str);
    return *this;
}

string_t& string_t::operator=(const char * const str)
{
    delete[] value;
    allocate_and_copy_from_cstring(str);
    return *this;
}

string_t string_t::operator+(const string_t& str)
{
    string_t result;
    delete[] result.value;
    result.size = size + str.size;
    result.value = new char [result.size + 1];
    strncpy(result.value, value, size);
    result.value[size] = '\0';
    strncat(result.value, str.value, str.size); 
    return result;
}

string_t string_t::operator+(const char * const str)
{
    string_t result;
    delete[] result.value;
    result.size = size + strlen(str);
    result.value = new char [result.size + 1];
    strncpy(result.value, value, size);
    result.value[size] = '\0';
    strncat(result.value, str, strlen(str)); /* Does not make sense to use strncat if str is not NULL terminated. since size will be detected using a NULL character */
    return result;
}

#define PLUS_EQUALS(dest,src) dest = dest + src

string_t& string_t::operator+=(const string_t& str)
{
    PLUS_EQUALS(*this, str);
    return *this;
}

string_t& string_t::operator+=(const char * const str)
{
    PLUS_EQUALS(*this, str);
    return *this;
}

bool string_t::operator==(const string_t& str) const
{
    if(size == str.size && strncmp(value,str.value,size)==0)
    {
        return true;
    }
    return false;
}

bool string_t::operator==(const char * const str) const
{
    if(size == strlen(str) && strncmp(value,str,size)==0)
    {
        return true;
    }
    return false;
}

#define NOT_EQUAL(a,b) !(a == b)

bool string_t::operator!=(const string_t& str) const
{
    return NOT_EQUAL(*this,str);
}

bool string_t::operator!=(const char * const str) const
{
    return NOT_EQUAL(*this,str);
}

bool string_t::operator<(const string_t& str) const
{
    if(strncmp(value,str.value,(size>str.size?size:str.size)) < 0)
    {
        return true;
    }
    return false;

}

bool string_t::operator<(const char * const str) const
{
    if(strncmp(value,str,(size>strlen(str)?size:strlen(str))) < 0)
    {
        return true;
    }
    return false;
}

#define LESS_EQUAL(a,b) (a<b || a ==b)

bool string_t::operator<=(const string_t& str) const
{
    return LESS_EQUAL(*this,str);
}

bool string_t::operator<=(const char * const str) const
{
    return LESS_EQUAL(*this,str);
}

#define GREATER(a,b) !(a<=b)
bool string_t::operator>(const string_t& str) const
{
    return GREATER(*this,str);
}

bool string_t::operator>(const char * const str) const
{
    return GREATER(*this,str);
}

#define GREATER_EQUAL(a,b) (a>b || a==b)
bool string_t::operator>=(const string_t& str) const
{
    return GREATER_EQUAL(*this, str);
}

bool string_t::operator>=(const char * const str) const
{
    return GREATER_EQUAL(*this, str);
}


string_t::~string_t()
{
    delete[] value;
}

ostream& operator<<(ostream& cout, const string_t& str)
{
    cout<<str.value<<str.size;
    return cout;
}

istream& operator>>(istream& cin, string_t& str)
{
    char tmp[100];
    cin>>tmp;
    str=tmp;
    return cin;
}

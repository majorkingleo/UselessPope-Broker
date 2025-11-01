#pragma once

#include <set>
#include <string>

class IsOneOf
{
private:
	std::set<wchar_t> m_characters {};

public:
	IsOneOf( std::initializer_list<wchar_t> il )
	: m_characters{ il }
	{}

	IsOneOf( const std::wstring & str )
	{
		for( wchar_t c : str ) {
			m_characters.insert(c);
		}
	}

	bool operator()( wchar_t c ) const
	{
		return m_characters.find( c ) != m_characters.end();
	}
};

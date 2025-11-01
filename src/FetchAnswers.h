#pragma once

#include <string>
#include "BasicThread.h"
#include "IsOneOf.h"
#include <vector>

class FetchAnswers : public BasicThread
{
	class Reaction
	{
		std::wstring m_song_title {};
		std::wstring m_answer {};
		std::vector<std::wstring> key_words {};

	public:
		Reaction( const std::wstring & song_title );

		void add_answer( const std::wstring & answer ) {
			m_answer = answer;
		}

	private:
		void strip_file_extension( std::wstring & file_name );
		void strip_file_name_from_path( std::wstring & file_name );
	};

private:
	const IsOneOf is_space{ L" \t\r\n" };

	std::vector<Reaction> m_reactions;

public:
    FetchAnswers();

    void run() override;

    void fetch_from_file( const std::string & file );

protected:
};

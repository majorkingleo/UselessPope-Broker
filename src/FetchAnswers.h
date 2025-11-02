#pragma once

#include <string>
#include "BasicThread.h"
#include "IsOneOf.h"
#include <vector>
#include <algorithm>

class FetchAnswers : public BasicThread
{
public:
	class KeyWords
	{
	public:
		using set_t = std::set<std::wstring>;

	private:
		set_t m_key_words {};

	public:
		KeyWords() = default;

		KeyWords( KeyWords && other )
		: m_key_words( std::move( other.m_key_words ) )
		{
		}

		KeyWords( const KeyWords & other )
		: m_key_words( other.m_key_words )
		{
		}

		auto & operator=( KeyWords && other ) {
			m_key_words = std::move( other.m_key_words );
			return *this;
		}

		auto & operator=( const KeyWords & other ) {
			m_key_words = other.m_key_words;
			return *this;
		}

		auto & operator=( set_t && other ) {
			m_key_words = std::move( other );
			return *this;
		}

		void insert( std::wstring && key_word ) {
			m_key_words.insert( std::move(key_word) );
		}

		void insert( const std::wstring & key_word ) {
			m_key_words.insert( key_word );
		}

		const auto & get_key_words() const {
			return m_key_words;
		}

		unsigned get_max_match_points() const {
			return m_key_words.size();
		}

		unsigned match( const KeyWords & other ) const {

			unsigned match_count = 0;
			const auto & other_words = other.get_key_words();

			auto count_matches = [&match_count,this]( const auto & word ) {
				match_count += m_key_words.count(word);
			};

			std::for_each( other_words.begin(), other_words.end(), count_matches );

			return match_count;
		}
	};

	class Reaction
	{
		std::wstring m_song_title {};
		std::wstring m_answer {};
		KeyWords	 m_key_words {};

	public:
		Reaction( const std::wstring & song_title );

		void add_answer( const std::wstring & answer ) {
			m_answer = answer;
		}

		const auto & get_key_words() const {
			return m_key_words;
		}

		const auto & get_title() const {
			return m_song_title;
		}

		const auto & get_answer() const {
			return m_answer;
		}

		static std::set<std::wstring> get_key_words_from_title( const std::wstring & title );

		static std::wstring strip_file_name( const std::wstring & file_name );

	private:
		static void strip_file_extension( std::wstring & file_name );
		static void strip_file_name_from_path( std::wstring & file_name );
	};

private:
	const IsOneOf is_space{ L" \t\r\n" };

	std::vector<Reaction> m_reactions;

public:
    FetchAnswers();

    void run() override;

    void fetch_from_file( const std::string & file );
    void get_reaction_from_song( const std::string & file );

protected:
    void fetch_last_played_chunks();
};

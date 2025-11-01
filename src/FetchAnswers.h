#pragma once

#include <string>

class FetchAnswers : public BasicThread
{

public:
    FetchAnswers();

    void run() override;

    void fetch_from_file( const std::string & file );

protected:

};

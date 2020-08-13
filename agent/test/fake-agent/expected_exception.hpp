#ifndef YELLA_EXPECTED_EXCEPTION_HPP__
#define YELLA_EXPECTED_EXCEPTION_HPP__

#include <exception>
#include <unicode/unistr.h>
#include <memory>
#include <sstream>

namespace yella
{

namespace test
{

class expected : public std::exception
{
public:
    expected(const std::string& ctx, const icu::UnicodeString& exp, const icu::UnicodeString& got);
    expected(const std::string& ctx, std::size_t exp, std::size_t got);
    expected(const std::string& ctx, const void* exp, const void* got);

    virtual const char* what() const noexcept override;

private:
    std::string msg_;
};

}

}

#endif

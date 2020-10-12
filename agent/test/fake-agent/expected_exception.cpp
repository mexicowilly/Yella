#include "expected_exception.hpp"

namespace yella
{

namespace test
{

expected::expected(const std::string& ctx, const icu::UnicodeString& exp, const icu::UnicodeString& got)
{
    std::string exp8;
    exp.toUTF8String(exp8);
    std::string got8;
    got.toUTF8String(got8);
    std::ostringstream stream;
    stream << ctx << ": Expected '" << exp8 << "' but got '" << got8 << "'";
    msg_ = stream.str();
}

expected::expected(const std::string& ctx, std::size_t exp, std::size_t got)
{
    std::ostringstream stream;
    stream << ctx << ": Expected " << exp << " but got " << got;
    msg_ = stream.str();
}

expected::expected(const std::string& ctx, const void* exp, const void* got)
{
    std::ostringstream stream;
    stream << ctx << ": Expected " << exp << " got " << got;
    msg_ = stream.str();
}

expected::expected(const std::string& ctx, const std::string& exp, const std::string& got)
{
    std::ostringstream stream;
    stream << ctx << ": Expected '" << exp << "' got '" << got << "'";
    msg_ = stream.str();
}

const char* expected::what() const noexcept
{
    return msg_.c_str();
}

}

}
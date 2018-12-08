#include "plugin/file/file_name_matcher.h"
#include "common/file.h"
#include <unicode/ustring.h>

static bool file_name_matches_impl(const UChar *name, const UChar *pattern)
{
    UChar p_ch;
    bool match_slash;
    UChar n_ch;
    const UChar* prev_p;
    const UChar* slash;

    for ( ; (p_ch = *pattern) != 0; name++, pattern++)
    {
        if ((n_ch = *name) == 0 && p_ch != u'*')
            return false;
        switch (p_ch)
        {
        case u'\\':
            p_ch = *++pattern;
            /* fall through */

        default:
            if (n_ch != p_ch)
                return false;
            continue;

        case u'?':
            if (n_ch == YELLA_DIR_SEP[0])
                return false;
            continue;

        case u'*':
            if (*++pattern == u'*')
            {
                prev_p = pattern - 2;
                while (*++pattern == u'*') {}
                if ((prev_p < pattern || *prev_p == YELLA_DIR_SEP[0]) &&
                    (*pattern == 0 || *pattern == YELLA_DIR_SEP[0] || (*pattern == u'\\' && pattern[1] == YELLA_DIR_SEP[0])))
                {
                    if (*pattern == YELLA_DIR_SEP[0] && file_name_matches_impl(name, pattern + 1))
                        return true;
                    match_slash = true;
                }
                else
                {
                    match_slash = false;
                }
            }
            else
            {
                match_slash = false;
            }
            if (*pattern == 0)
            {
                if (!match_slash)
                {
                    if (u_strchr(name, YELLA_DIR_SEP[0]) != NULL)
                        return false;
                }
                return true;
            }
            else if (!match_slash && *pattern == YELLA_DIR_SEP[0])
            {
                slash = u_strchr(name, YELLA_DIR_SEP[0]);
                if (slash == NULL)
                    return false;
                name = slash;
                break;
            }
            while (true)
            {
                if (n_ch == 0)
                    break;
                if (u_strchr(u"*?\\", *pattern) == NULL)
                {
                    p_ch = *pattern;
                    while ((n_ch = *name) != 0 && (match_slash || n_ch != YELLA_DIR_SEP[0]))
                    {
                        if (n_ch == p_ch)
                            break;
                        name++;
                    }
                    if (n_ch != p_ch)
                        return false;
                }
                if (file_name_matches_impl(name, pattern))
                    return true;
                else if (!match_slash && n_ch == YELLA_DIR_SEP[0])
                    return false;
                n_ch = *++name;
            }
            return false;
        }
    }
    return *name == 0;
}

bool file_name_matches(const UChar* name, const UChar* pattern)
{
    return file_name_matches_impl(name, pattern);
}

const UChar* first_unescaped_special_char(const UChar* const pattern)
{
    UChar* first;
    UChar* slash;
    size_t num;

    first = (UChar*)pattern;
    while (true)
    {
        first = u_strpbrk(first, u"*?");
        if (first == NULL)
            break;
        slash = first;
        num = 0;
        while (slash > pattern && *(--slash) == u'\\')
            ++num;
        if ((num & 1) == 0)
            break;
        ++first;
    }
    return first;
}

uds unescape_pattern(const UChar* const pattern)
{
    uds result;
    const UChar* cur;

    if (u_strchr(pattern, u'\\') == NULL)
    {
        result = udsnew(pattern);
    }
    else
    {
        result = udsempty();
        for (cur = pattern; *cur != 0; cur++)
        {
            if (*cur == u'\\')
                ++cur;
            result = udscatlen(result, cur, 1);
        }
    }
    return result;
}

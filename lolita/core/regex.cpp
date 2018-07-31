#include "core/regex.h"
#include "core/errors.h"
#include "text/text-utils.h"
#include <optional>
#include <vector>

using namespace std;
using namespace eds;
using namespace eds::text;

namespace eds::loli::regex
{
    // Error Helpers
    //

    void RegexParsingAssert(bool condition, const string& msg)
    {
        if (!condition)
        {
            throw ParserConstructionError(msg);
        }
    }

    static constexpr auto kMsgUnexpectedEof       = "regex: unexpected eof";
    static constexpr auto kMsgEmptyExpressionBody = "regex: empty expression body is not allowed";
    static constexpr auto kMsgInvalidClosure      = "regex: invalid closure is not allowed";

    using RegexExprVec = vector<RegexExpr::Ptr>;

    // merge expressions in seq into any in cascade(SequenceExpr)
    void MergeSequence(RegexExprVec& any, RegexExprVec& seq)
    {
        assert(!seq.empty());

        auto vec = RegexExprVec(seq.size());
        move(seq.begin(), seq.end(), vec.begin()), seq.clear();

        if (vec.size() == 1)
        {
            any.push_back(
                move(vec.front()));
        }
        else
        {
            any.push_back(
                make_unique<SequenceExpr>(move(vec)));
        }
    }

    int EscapeRawCharacter(int ch)
    {
        switch (ch)
        {
        case 'a':
            return '\a';
        case 'b':
            return '\b';
        case 't':
            return '\t';
        case 'r':
            return '\r';
        case 'v':
            return '\v';
        case 'f':
            return '\f';
        case 'n':
            return '\n';
        default:
            return ch;
        }
    }

    // parse a character from str, content of which would be consumed
    int ParseCharacter(zstring& str)
    {
        int result;
        auto p = str;
        if (ConsumeIf(p, '\\'))
        {
            RegexParsingAssert(*p != '\0', kMsgUnexpectedEof);

            result = EscapeRawCharacter(Consume(p));
        }
        else
        {
            result = Consume(p);
        }

        str = p;
        return result;
    }

    RegexExpr::Ptr ParseEscapedExpr(zstring& str)
    {
        auto rg = CharRange{EscapeRawCharacter(Consume(str))};

        return make_unique<EntityExpr>(rg);
    }

    RegexExpr::Ptr ParseCharClass(zstring& str)
    {
        // ASSUME leading '[' pre-consumed

        auto p = str;
        // check leading '^'
        bool reverse = ConsumeIf(p, '^');

        // load char class
        optional<int> last_ch;
        vector<CharRange> ranges;

        while (*p && *p != ']')
        {
            if (last_ch) // if a value pending in last_ch
            {
                if (ConsumeIf(p, '-')) // if a mark for range
                {
                    RegexParsingAssert(*p != '\0', kMsgUnexpectedEof);

                    // copy last_ch and disable it
                    auto ch = *last_ch;
                    last_ch = nullopt;

                    // if '-' is the last character in char class
                    // it's treated as a raw character
                    if (*p == ']')
                    {
                        ranges.push_back(CharRange{'-'});
                        break; // early break for closing bracket
                    }
                    else
                    {
                        int min = ch;
                        int max = ParseCharacter(p);
                        if (max < min)
                            std::swap(min, max);

                        ranges.push_back(CharRange{min, max});
                    }
                }
                else // replace the last_ch
                {
                    ranges.push_back(CharRange{*last_ch});

                    last_ch = ParseCharacter(p);
                }
            }
            else
            {
                last_ch = ParseCharacter(p); // don't save it into ranges yet
            }
        }

        if (last_ch)
            ranges.push_back(CharRange{*last_ch});

        RegexParsingAssert(ConsumeIf(p, ']'), kMsgUnexpectedEof);
        RegexParsingAssert(!ranges.empty(), kMsgEmptyExpressionBody);

        str = p;

        // merge ranges
        sort(ranges.begin(), ranges.end(), [](auto lhs, auto rhs) { return lhs.Min() < rhs.Min(); });

        vector<CharRange> merged_ranges{ranges.front()};
        for (auto rg : ranges)
        {
            auto last_rg = merged_ranges.back();
            if (rg.Min() > last_rg.Min())
            {
                merged_ranges.push_back(rg);
            }
            else
            {
                auto new_min = last_rg.Min();
                auto new_max = max(rg.Max(), last_rg.Max());

                merged_ranges.back() = CharRange{new_min, new_max};
            }
        }

        // reverse if required
        if (reverse)
        {
            vector<CharRange> tmp;
            if (merged_ranges.front().Min() > 0)
                tmp.push_back(CharRange{0, merged_ranges.front().Min() - 1});

            for (auto it = merged_ranges.begin(); it != merged_ranges.end(); ++it)
            {
                auto next_it = next(it);
                if (next_it != merged_ranges.end())
                {
                    auto new_min = it->Max() + 1;
                    auto new_max = next_it->Min() - 1;

                    if (new_min <= new_max)
                    {
                        tmp.push_back(CharRange{new_min, new_max});
                    }
                }
                else
                {
                    if (it->Max() < 127)
                    {
                        tmp.push_back(CharRange{it->Max() + 1, 127});
                    }
                }
            }

            merged_ranges = tmp;
        }

        // construct expressions
        RegexExprVec result;
        for (auto rg : merged_ranges)
        {
            result.push_back(make_unique<EntityExpr>(rg));
        }

        return result.size() == 1
                   ? move(result.front())
                   : make_unique<ChoiceExpr>(move(result));
    }

    RegexExpr::Ptr ParseRegexInternal(zstring& str, char term)
    {
        RegexExprVec any{};
        RegexExprVec seq{};

        bool allow_closure = false;
        auto p             = str;
        for (; *p && *p != term;)
        {
            if (ConsumeIf(p, '|'))
            {
                RegexParsingAssert(!seq.empty(), kMsgEmptyExpressionBody);

                allow_closure = false;

                MergeSequence(any, seq);
            }
            else if (ConsumeIf(p, '('))
            {
                allow_closure = true;

                seq.push_back(
                    ParseRegexInternal(p, ')'));
            }
            else if (*p == '*' || *p == '+' || *p == '?')
            {
                RegexParsingAssert(!seq.empty(), kMsgEmptyExpressionBody);
                RegexParsingAssert(allow_closure, kMsgInvalidClosure);

                allow_closure = false;

                RepetitionMode strategy;
                switch (Consume(p))
                {
                case '?':
                    strategy = RepetitionMode::Optional;
                    break;
                case '*':
                    strategy = RepetitionMode::Star;
                    break;
                case '+':
                    strategy = RepetitionMode::Plus;
                    break;
                }

                auto to_repeat = move(seq.back());
                seq.pop_back();

                seq.push_back(
                    make_unique<ClosureExpr>(move(to_repeat), strategy));
            }
            else if (ConsumeIf(p, '['))
            {
                allow_closure = true;

                seq.push_back(
                    ParseCharClass(p));
            }
            else if (ConsumeIf(p, '\\'))
            {
                allow_closure = true;

                seq.push_back(
                    ParseEscapedExpr(p));
            }
            else
            {
                allow_closure = true;

                seq.push_back(
                    make_unique<EntityExpr>(CharRange{Consume(p)}));
            }
        }

        RegexParsingAssert(!seq.empty(), kMsgEmptyExpressionBody);
        RegexParsingAssert(!(term && !ConsumeIf(p, term)), kMsgUnexpectedEof);

        str = p;
        MergeSequence(any, seq);

        return any.size() == 1
                   ? move(any.front())
                   : make_unique<ChoiceExpr>(move(any));
    }

    unique_ptr<RootExpr> ParseRegex(const string& regex)
    {
        auto str = regex.c_str();

        return make_unique<RootExpr>(ParseRegexInternal(str, '\0'));
    }
}
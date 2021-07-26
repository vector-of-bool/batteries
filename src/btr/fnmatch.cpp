#include "./fnmatch.hpp"

#include <cassert>

#include <neo/ufmt.hpp>
#include <neo/utf8.hpp>

using char32ptr = const char32_t*;
using std::string_view;
using std::u32string_view;

using namespace btr;

using u8_cp_range = btr::codepoint_range<std::u8string_view>;
using u8_iter     = u8_cp_range::iterator;

namespace {

enum match_result : int {
    yes,
    no,
    never,
};

class base_pattern_elem {
public:
    virtual ~base_pattern_elem() = default;

    // Attempt to match the given string
    virtual match_result match(u8_iter first, u8_iter last) const noexcept = 0;

    std::unique_ptr<base_pattern_elem> next;
};

class rt_star : public base_pattern_elem {
    match_result match(u8_iter first, u8_iter last) const noexcept {
        while (first != last) {
            auto did_match = next->match(first, last);
            if (did_match == yes) {
                return yes;
            } else if (did_match == never) {
                return never;
            }
            ++first;
        }
        // We're at the end. Try once more
        auto did_match = next->match(first, last);
        if (did_match == no) {
            // No matter how far the caller advances, we will never find a match
            return never;
        }
        return did_match;
    }
};

/// This is for a "*" pattern that matches anything at all
class rt_always : public base_pattern_elem {
    match_result match(u8_iter, u8_iter) const noexcept override { return yes; }
};

/// This is for a '*foo' pattern, where we only need to check that the string has the correct suffix
class rt_endswith : public base_pattern_elem {
    std::u32string suffix;

public:
    rt_endswith(u32string_view s)
        : suffix(s) {}

    match_result match(u8_iter first, u8_iter last) const noexcept override {
        auto in_len = std::distance(first, last);
        if (in_len < static_cast<std::ptrdiff_t>(suffix.size())) {
            return never;
        }
        auto skip = in_len - suffix.size();
        std::advance(first, skip);
        auto eq = std::equal(first, last, suffix.begin());
        return eq ? yes : never;
    }
};

class rt_any_char : public base_pattern_elem {
    match_result match(u8_iter first, u8_iter last) const noexcept {
        if (first == last) {
            return no;
        }
        return next->match(std::next(first), last);
    }
};  // namespace

class rt_oneof : public base_pattern_elem {
    bool           _negative;
    std::u32string _chars;

    match_result match(u8_iter first, u8_iter last) const noexcept {
        if (first == last) {
            return never;
        }
        auto       idx         = _chars.find(*first);
        const bool is_in_group = idx != _chars.npos;
        if (is_in_group == _negative) {
            // Either: We do not want any of the chars but we found one, OR
            //         we wanted one of the chars but did not find it
            return no;
        } else {
            return next->match(std::next(first), last);
        }
    }

public:
    explicit rt_oneof(std::u32string&& chars, bool negative)
        : _negative(negative)
        , _chars(std::move(chars)) {}

};  // namespace

class rt_lit : public base_pattern_elem {
    std::u32string _lit;

    match_result match(u8_iter first, u8_iter last) const noexcept {
        auto        probe     = first;
        std::size_t remaining = 0;
        while (probe != last and remaining < _lit.size()) {
            ++probe;
            ++remaining;
        }
        if (remaining < _lit.size()) {
            return never;
        }
        auto eq = std::equal(first, probe, _lit.begin());
        if (!eq) {
            return no;
        }
        return next->match(probe, last);
    }

public:
    explicit rt_lit(std::u32string&& lit)
        : _lit(std::move(lit)) {}
};

class rt_end : public base_pattern_elem {
    match_result match(u8_iter first, u8_iter last) const noexcept {
        return first == last ? yes : no;
    }
};

}  // namespace

class btr::fnmatch_pattern::impl {
    std::unique_ptr<base_pattern_elem>  _head;
    std::unique_ptr<base_pattern_elem>* _next_to_compile = &_head;

    std::string    _spelling;
    std::u32string _unicode = btr::transcode_string<char32_t>(_spelling);

    template <typename T, typename... Args>
    void _add_elem(Args&&... args) {
        *_next_to_compile = std::make_unique<T>(std::forward<Args>(args)...);
        _next_to_compile  = &(*_next_to_compile)->next;
    }

    u32string_view _compile_oneof(u32string_view tail) {
        std::u32string chars;
        bool           negate = false;
        if (tail.front() == '!') {
            if (tail.starts_with(U"!]")) {
                // Special '[!]' matches a single exclamation point
                _add_elem<rt_oneof>(U"!", false);
                return tail.substr(2);
            }
            negate = true;
            tail.remove_prefix(1);
        }
        if (tail.starts_with(U"]]")) {
            _add_elem<rt_oneof>(U"]", negate);
            return tail.substr(2);
        }
        while (!tail.empty()) {
            auto c = tail.front();
            if (c == ']') {
                // We've reached the end of the group
                _add_elem<rt_oneof>(std::move(chars), negate);
                return tail.substr(1);
            } else {
                chars.push_back(c);
            }
            tail.remove_prefix(1);
        }
        throw bad_fnmatch_pattern(_spelling, "Unterminated [group] in pattern");
    }

    u32string_view _compile_lit(u32string_view tail) {
        std::u32string lit;
        while (!tail.empty()) {
            auto c = tail.front();
            if (c == '*' || c == '[' || c == '?') {
                break;
            } else {
                lit.push_back(c);
            }
            tail.remove_prefix(1);
        }
        _add_elem<rt_lit>(std::move(lit));
        return tail;
    }

    void _compile_next(u32string_view tail) {
        if (tail.empty()) {
            return;
        }
        auto c = tail.front();
        if (c == '*') {
            if (tail.size() == 1) {
                _add_elem<rt_always>();
                return;
            } else if (tail.substr(1).find_first_of(U"*?[") == tail.npos) {
                _add_elem<rt_endswith>(tail.substr(1));
                return;
            } else {
                _add_elem<rt_star>();
                _compile_next(tail.substr(1));
            }
        } else if (c == '[') {
            tail = _compile_oneof(tail.substr(1));
            _compile_next(tail);
        } else if (c == '?') {
            _add_elem<rt_any_char>();
            _compile_next(tail.substr(1));
        } else {
            // Literal string
            tail = _compile_lit(tail);
            _compile_next(tail);
        }
    }

public:
    impl(u8view str_)
        : _spelling(str_) {
        if (_unicode.starts_with(U"!")) {
            throw bad_fnmatch_pattern(_spelling,
                                      "Patterns starting with a literal exclamation point '!' "
                                      "are reserved. Escape with square brackets [!]");
        }
        _compile_next(_unicode);
        // Set the tail of the list to be an rt_end to detect end-of-string
        _add_elem<rt_end>();
    }

    bool match(u8_iter first, u8_iter last) const noexcept {
        assert(_head);
        return _head->match(first, last) == yes;
    }

    bool match(auto range) const {
        assert(_head);
        return _head->match(range.begin(), range.end());
    }

    const std::string& spelling() const noexcept { return _spelling; }
};

btr::fnmatch_pattern btr::fnmatch_pattern::compile(u8view pat) {
    return fnmatch_pattern{std::make_shared<impl>(pat)};
}

const std::string& btr::fnmatch_pattern::literal_spelling() const noexcept {
    assert(_impl);
    return _impl->spelling();
}

btr::bad_fnmatch_pattern::bad_fnmatch_pattern(std::string pat, std::string reason) noexcept
    : runtime_error{neo::ufmt("The given fnmatch pattern string '{}' is invalid: {}", pat, reason)}
    , _pat(std::move(pat))
    , _reason(std::move(reason)) {}

bool btr::fnmatch_pattern::_test(u8view sv) const noexcept {
    assert(_impl);
    btr::codepoint_range chars{sv.u8string_view()};
    return _impl->match(chars.begin(), chars.end());
}

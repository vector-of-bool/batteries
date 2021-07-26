#include "./glob.hpp"

#include "./fnmatch.hpp"

#include <neo/switch_coro.hpp>

#include <optional>
#include <set>
#include <variant>

using namespace btr;
namespace fs    = std::filesystem;
using path_iter = fs::path::const_iterator;

namespace {

struct rglob_pattern {};

// A glob is a list of sub-patterns, which are either regular fnmatch patterns, or rglob ("**")
// patterns
using glob_element_type = std::variant<fnmatch_pattern, rglob_pattern>;

bool is_rglob(const glob_element_type& el) noexcept {
    return std::holds_alternative<rglob_pattern>(el);
}

const fnmatch_pattern& as_fnmatch(const glob_element_type& el) noexcept {
    return *std::get_if<fnmatch_pattern>(&el);
}

}  // namespace

struct glob::impl {
    std::string spelling;

    std::vector<glob_element_type> elements;

    using pattern_it = std::vector<glob_element_type>::const_iterator;

    bool check_matches(path_iter        elem_it,
                       const path_iter  elem_stop,
                       pattern_it       pat_it,
                       const pattern_it pat_stop) const noexcept {
        if (elem_it == elem_stop && pat_it == pat_stop) {
            // We have iterated each element and each pattern, so we match
            return true;
        }
        if (elem_it == elem_stop || pat_it == pat_stop) {
            // We hit the end of the path, or we hit the end of the pattern list
            return false;
        }
        // Check this path element
        if (!is_rglob(*pat_it)) {
            // This is a regular pattern (not a '**' part)
            if (!(as_fnmatch(*pat_it).test(elem_it->string()))) {
                // This element did not match, so we don't need to check any of the remainder
                return false;
            }
            // Check that the remainder of the pattern and path match
            return check_matches(std::next(elem_it), elem_stop, std::next(pat_it), pat_stop);
        } else {
            // This is a recursive-glob pattern '**'
            const auto next_pat = std::next(pat_it);
            if (next_pat == pat_stop) {
                // It is a final '**', which means the remainder of the path does not matter
                return true;
            }
            // Continue peeling individual elements until we find a match
            for (; elem_it != elem_stop; ++elem_it) {
                if (check_matches(elem_it, elem_stop, next_pat, pat_stop)) {
                    return true;
                }
            }
            return false;
        }
    }
};

glob glob::compile(const fs::path& pattern) {
    glob ret;

    glob::impl acc{};
    acc.spelling = pattern.string();

    for (fs::path elem : pattern) {
        auto str = elem.u8string();
        if (str == u8"**") {
            // Only append an rglob if the pattern list doesn't already end with an rglob
            // (consecutive rglobs are folded)
            if (acc.elements.empty() || !is_rglob(acc.elements.back())) {
                acc.elements.emplace_back(rglob_pattern{});
            }
        } else {
            acc.elements.push_back(btr::fnmatch_pattern::compile(str));
        }
    }

    ret._impl = std::make_shared<impl>(std::move(acc));
    return ret;
}

bool glob::test(const fs::path& filepath) const noexcept {
    return _impl->check_matches(filepath.begin(),
                                filepath.end(),
                                _impl->elements.cbegin(),
                                _impl->elements.cend());
}

struct glob::iterator::state {
    fs::path          my_root;
    const glob::impl& impl;

    const impl::pattern_it pat_iter        = impl.elements.begin();
    const bool             is_leaf_pattern = std::next(pat_iter) == impl.elements.end();

    std::set<fs::path> yielded{};

    fs::directory_entry    entry{};
    fs::directory_iterator dir_iter{my_root};
    std::unique_ptr<state> next_state{};
    int                    _coro = 0;

    const fs::directory_entry& get_entry() const noexcept {
        if (next_state) {
            return next_state->get_entry();
        }
        return entry;
    }

    enum coro_ret {
        reenter_again,
        yield_value,
        done,
    };

    /**
     * Globbing is implemented as a simple coroutine.
     *
     * Each iterator::state handles a single element of the pattern sequence that
     * is defined by a glob. The state has a root directory on which it is scanning,
     * and a single pattern with which it is associated.
     *
     * If this state is a '**' pattern, it is an "rglob" pattern. It recursively
     * walks the directory heirarchy searching for matches for the remainder of
     * the pattern sequence.
     *
     * If this state is not the leaf pattern, then it scans for directories
     * that match its pattern. If it finds a match, then a new state is pushed
     * onto the stack and we re-enter the coroutine into the new state.
     *
     * If this state is a leaf, then it should yield files (including directories)
     * that it finds that match its pattern.
     */
    coro_ret resume() {
        if (next_state) {
            auto st = next_state->resume();
            if (st == done) {
                next_state.reset();
                return reenter_again;
            }
            return st;
        }

        const auto& cur_pattern = *pat_iter;

        const bool dir_done     = dir_iter == fs::directory_iterator();
        auto       next_pattern = std::next(pat_iter);

        NEO_CORO_BEGIN(_coro);

        if (dir_done) {
            NEO_CORO_YIELD(done);
            neo_assert(invariant, false, "Re-entered a glob iterator after it was finished");
        }

        // Inspect the next entry in the directory
        entry = *dir_iter++;

        if (is_rglob(cur_pattern)) {
            // We are a '**' element.
            if (is_leaf_pattern) {
                // We are the final '**' element, so we match everything and should immediately
                // yield what we just found
                NEO_CORO_YIELD(yield_value);
            } else if (as_fnmatch(*next_pattern).test(fs::path(entry).filename().string())) {
                // We are not the final element, and the next pattern will match the entry we just
                // found.
                if (entry.is_directory()) {
                    // The entry is a directory, so we recurse into it.
                    next_state.reset(new state{entry, impl, std::next(next_pattern)});
                    NEO_CORO_YIELD(reenter_again);
                } else {
                    // The entry is a file, so yield that
                    NEO_CORO_YIELD(yield_value);
                }
            }
            if (entry.is_directory()) {
                // Recurse into directories
                next_state.reset(new state{entry, impl, pat_iter});
                NEO_CORO_YIELD(reenter_again);
            } else {
                // This is a non-directory file matching an '**' pattern. Ignore it
            }
        } else {
            if (as_fnmatch(cur_pattern).test(fs::path(entry).filename().string())) {
                // We match this entry
                if (is_leaf_pattern) {
                    NEO_CORO_YIELD(yield_value);
                } else if (entry.is_directory()) {
                    next_state.reset(new state{entry, impl, next_pattern});
                    NEO_CORO_YIELD(reenter_again);
                }
            }
        }

        NEO_CORO_END;

        _coro = 0;
        return reenter_again;
    }
};

glob::iterator::iterator(const glob& glb, const fs::path& dirpath)
    : _impl(glb._impl)
    , _done(false) {
    _state = std::make_shared<state>(state{dirpath, *_impl});
    increment();
}

fs::directory_entry glob::iterator::dereference() const { return _state->get_entry(); }
void                glob::iterator::increment() {
    auto st = state::reenter_again;
    while (st == state::reenter_again) {
        st = _state->resume();
        if (st == state::yield_value) {
            // Keep track of which paths we have already yielded to the caller. Don't yield the same
            // file more than once.
            auto& entry_path = _state->get_entry().path();
            if (_state->yielded.contains(entry_path)) {
                // We've already yielded this file. Reenter the glob search.
                st = state::reenter_again;
            } else {
                _state->yielded.insert(entry_path);
            }
        }
    }
    _done = st == state::done;
}

std::vector<fs::directory_entry> glob::iterator::to_vector() {
    std::vector<fs::directory_entry> ret;
    while (!at_end()) {
        ret.push_back(dereference());
        increment();
    }
    return ret;
}
#pragma once

namespace btr {

/**
 * @brief The spawn() options for creating a subprocess.
 */
struct subprocess_spawn_options;

/**
 * @brief Exception that represents a subprocess not existing normally with successful exit status
 *
 * Derived from `std::runtime_error`
 */
class subprocess_failure;

/**
 * @brief The process-exit information of some subprocess.
 */
struct subprocess_exit;
/**
 * @brief The accumulation of output data from the subprocess's standard IO
 * pipes.
 */
struct subprocess_output;
/**
 * @brief The combined result of a subprocess's exit status and output
 */
struct subprocess_result;

/**
 * @brief A handle to a spawned subprocess
 */
class subprocess;

}  // namespace btr

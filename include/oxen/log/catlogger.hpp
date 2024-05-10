#pragma once

#include <spdlog/sinks/dist_sink.h>
#include <atomic>
#include <string>
#include <functional>

#include "internal.hpp"
#include "level.hpp"

namespace oxen::log {

namespace detail {

    // Internal function that sets the internal variable for default log level of new cat loggers;
    // must be called with the loggers mutex held (i.e. in a call to `for_each_cat_logger`).
    // External callers should use the methods in log.hpp instead.
    void set_default_catlogger_level(Level level);

    // Internal function to retrieve the current default.
    Level get_default_catlogger_level();

}  // namespace detail

/// Runs a function on each existing logger and then runs the `and_then` callback (if given), all
/// while holding a mutex that blocks new categories from being created.  There is no particular
/// order in which the individual loggers are passed to the function.
void for_each_cat_logger(
        std::function<void(const std::string& name, spdlog::logger& logger)> f,
        std::function<void()> and_then = nullptr);

/// Wrapper class for a categorized logger.  This wrapper is provided rather than using a direct
/// logger_ptr because, in some cases, we need construction to happen during static initialization,
/// but actually setting up the category needs to be deferred until later, i.e.  once the logging
/// system is properly initialized.
struct CategoryLogger {
  private:
    std::atomic<bool> have_logger = false;
    logger_ptr logger;

    static std::unordered_map<std::string, logger_ptr> loggers_;
    static std::mutex loggers_mutex_;
    static Level loggers_default_level_;
    friend void for_each_cat_logger(
            std::function<void(const std::string&, spdlog::logger&)>, std::function<void()>);
    friend void detail::set_default_catlogger_level(Level);
    friend Level detail::get_default_catlogger_level();

    void find_or_make_logger();

  public:
    /// The category name.
    const std::string name;

    /// Constructor: this stores the name; actually categorized logger initialization is deferred to
    /// the first call to `operator const logger_ptr&` or dereference.
    explicit CategoryLogger(std::string name) : name{std::move(name)} {}

    /// Returns a shared_ptr to a spdlog::logger for this logging category.  The first time this is
    /// called the logger is initialized: either finding an existing logger (if one with the same
    /// name has already be created) or setting up a new one and attaching it to the global sink.
    operator const logger_ptr&() {
        if (have_logger)
            return logger;
        find_or_make_logger();
        return logger;
    }

    /// Accesses the underlying spd::logger.  Creates it if necessary.
    spdlog::logger& operator*() { return *static_cast<const logger_ptr&>(*this); }

    /// Member pointer dereference into the underlying spd::logger.  Creates it if necessary.
    spdlog::logger* operator->() { return static_cast<const logger_ptr&>(*this).get(); }

    // Our master sink where all log output goes; we add sub-sinks into this as desired, but this
    // master sink stays around forever.
    static const std::shared_ptr<spdlog::sinks::dist_sink_mt> master_sink;
};

/// Shortcut for constructing a CategoryLogger with the given name.
inline CategoryLogger Cat(std::string cat) {
    return CategoryLogger(std::move(cat));
}

}  // namespace oxen::log

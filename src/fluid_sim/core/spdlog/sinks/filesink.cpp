/**
 * @file   filesink.cpp
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2019.10.28
 *
 * @brief  Implementation of a spdlog file sink that rotates files after program restart.
 * A lot of code is copied from the original rotating_file_sink in spdlog.
 */

#include "core/spdlog/sinks/filesink.h"
#include <cassert>
#include <spdlog/details/os.h>

namespace mysh::core::spdlog::sinks {

    template<typename Mutex>
    inline rotating_open_file_sink<Mutex>::rotating_open_file_sink(::spdlog::filename_t base_filename,
                                                                   std::size_t max_files)
        : m_base_filename(std::move(base_filename)), m_max_files(max_files)
    {
        m_file_helper.open(calc_filename(m_base_filename, 0));
        auto current_size = m_file_helper.size(); // expensive. called only once
        if (current_size > 0) { rotate_(); }
    }

    template<typename Mutex>
    ::spdlog::filename_t rotating_open_file_sink<Mutex>::calc_filename(const ::spdlog::filename_t& filename,
                                                                       std::size_t index)
    {
        if (index == 0u) { return filename; }
        auto [basename, ext] = ::spdlog::details::file_helper::split_by_extension(filename);
        return fmt::format(SPDLOG_FILENAME_T("{}.{}{}"), basename, index, ext);
    }

    template<typename Mutex>::spdlog::filename_t rotating_open_file_sink<Mutex>::filename()
    {
        const std::lock_guard<Mutex> lock(::spdlog::sinks::base_sink<Mutex>::mutex_);
        return m_file_helper.filename();
    }

    template<typename Mutex> void rotating_open_file_sink<Mutex>::sink_it_(const ::spdlog::details::log_msg& msg)
    {
        ::spdlog::memory_buf_t formatted;
        ::spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        m_file_helper.write(formatted);
    }

    template<typename Mutex> void rotating_open_file_sink<Mutex>::flush_() { m_file_helper.flush(); }

    template<typename Mutex> void rotating_open_file_sink<Mutex>::rotate_()
    {
        using ::spdlog::details::os::filename_to_str;
        using ::spdlog::details::os::path_exists;
        m_file_helper.close();
        for (auto i = m_max_files; i > 0; --i) {
            const ::spdlog::filename_t src = calc_filename(m_base_filename, i - 1);
            if (!path_exists(src)) { continue; }
            const ::spdlog::filename_t target = calc_filename(m_base_filename, i);

            if (!rename_file_(src, target)) {
                // if failed try again after a small delay.
                // this is a workaround to a windows issue, where very high rotation
                // rates can cause the rename to fail with permission denied (because of antivirus?).
                ::spdlog::details::os::sleep_for_millis(100);
                if (!rename_file_(src, target)) {
                    m_file_helper.reopen(true); // truncate the log file anyway to prevent it to grow beyond its limit!
                    SPDLOG_THROW(::spdlog::spdlog_ex("rotating_file_sink: failed renaming " + filename_to_str(src)
                                                         + " to " + filename_to_str(target),
                                                     errno));
                }
            }
        }
        m_file_helper.reopen(true);
    }

    template<typename Mutex>
    bool rotating_open_file_sink<Mutex>::rename_file_(const ::spdlog::filename_t& src_filename,
                                                      const ::spdlog::filename_t& target_filename)
    {
        (void)::spdlog::details::os::remove(target_filename);
        return ::spdlog::details::os::rename(src_filename, target_filename) == 0;
    }

    template class rotating_open_file_sink<std::mutex>;
    template class rotating_open_file_sink<::spdlog::details::null_mutex>;

    template std::shared_ptr<::spdlog::logger>
    rotating_open_logger_mt<::spdlog::synchronous_factory>(const std::string& logger_name,
                                                           const ::spdlog::filename_t& filename, std::size_t max_files);

    template std::shared_ptr<::spdlog::logger>
    rotating_open_logger_st<::spdlog::synchronous_factory>(const std::string& logger_name,
                                                           const ::spdlog::filename_t& filename, std::size_t max_files);
}

/**
 * @file   boundary_span.h
 * @author Sebastian Maisch <sebastian.maisch@googlemail.com>
 * @date   2023.10.07
 *
 * @brief  A span with boundary conditions.
 */

#pragma once

#include <functional>
#include <span>

namespace fluid::utils
{
    template<typename T>
    class boundary_span
    {
    public:
        using content_type = std::span<T>;
        using boundary_fn = std::function<T(const content_type&, std::size_t)>;

        constexpr boundary_span(content_type content, const boundary_fn& handle_boundary)
            : m_content{content}
            , m_handle_boundary{handle_boundary}
        {
        }

        constexpr content_type& get_content() { return m_content; }
        constexpr const content_type& get_content() const { return m_content; }

        constexpr T operator[](std::size_t idx) const
        {
            if (idx < 0 || idx >= m_content.size()) return m_handle_boundary(m_content, idx);
            return m_content[idx];
        }

    private:
        content_type m_content;
        boundary_fn m_handle_boundary;
    };
}

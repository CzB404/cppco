// Copyright(C) 2024 by Balazs Cziraki <balazs.cziraki@gmail.com>
//
// Permission to use, copy, modify, and /or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above copyright
// notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS.IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
// OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
// TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
// THIS SOFTWARE.

#ifndef CO_HPP_INCLUDE_GUARD
#define CO_HPP_INCLUDE_GUARD

#include <libco.h>
#include <memory>
#include <functional>

namespace co {

class thread;
class thread_ref;

class thread_create_failure;
class thread_return_failure;

thread_ref current_thread() noexcept;

class thread_create_failure : public std::runtime_error
{
public:
	thread_create_failure() noexcept;
};

class thread_return_failure : public std::runtime_error
{
public:
	thread_return_failure() noexcept;
};

namespace detail {

template<typename T>
class thread_impl;

class thread_base
{
protected:
	thread_base() noexcept = default;
	thread_base(const thread_base& other) noexcept = default;
	thread_base(thread_base&& other) noexcept = default;
	thread_base& operator=(const thread_base& other) noexcept = default;
	thread_base& operator=(thread_base&& other) noexcept = default;
	~thread_base() noexcept = default;

	class thread_stopping : public std::exception
	{
	};

	struct thread_deleter
	{
		void operator()(cothread_t p) const noexcept;
	};

	struct thread_failure
	{
		thread* failing_thread = nullptr;
		std::exception_ptr exception = nullptr;

		thread_failure() = default;
		thread_failure(std::nullptr_t) {};

		explicit operator bool() const noexcept;
		friend bool operator==(const thread_failure& lhs, std::nullptr_t) noexcept
		{
			return !lhs;
		}
		friend bool operator==(std::nullptr_t, const thread_failure& rhs) noexcept
		{
			return !rhs;
		}
		friend bool operator!=(const thread_failure& lhs, std::nullptr_t) noexcept
		{
			return static_cast<bool>(lhs);
		}
		friend bool operator!=(std::nullptr_t, const thread_failure& rhs) noexcept
		{
			return static_cast<bool>(rhs);
		}
	};

	struct thread_status
	{
		cothread_t current_thread;
		thread* current_this;
		thread_failure current_failure;
	};

	static thread_local std::unique_ptr<thread_status> tl_status;
};

template<typename T>
class thread_impl : private thread_base
{
public:
	explicit operator bool() const noexcept;
	void switch_to() const;

protected:
	using thread_base::thread_deleter;
	using thread_base::thread_stopping;
	thread_impl() = default;

	friend class ::co::thread;
	friend class ::co::thread_ref;
	cothread_t get_thread() const noexcept;

	using thread_base::tl_status;
};

} // namespace detail

class thread_ref : private detail::thread_impl<thread_ref>
{
public:
	thread_ref() = default;
	using detail::thread_impl<thread_ref>::operator bool;
	using detail::thread_impl<thread_ref>::switch_to;
private:
	explicit thread_ref(cothread_t thread) noexcept;
	friend cothread_t detail::thread_impl<thread_ref>::get_thread() const noexcept;
	friend class thread;
	friend thread_ref current_thread() noexcept;
	cothread_t get_thread() const noexcept;
	cothread_t m_thread = nullptr;
};

class thread : private detail::thread_impl<thread>
{
public:
	using entry_t = std::function<void()>;

	// libco's recommendation for the default stack size is 1 MB on 32 bit systems, and to define the stack size in pointer size.
	inline static constexpr size_t default_stack_size = 1 * 1024 * 1024 / 4 * sizeof(void*);

	using detail::thread_impl<thread>::operator bool;
	using detail::thread_impl<thread>::switch_to;
	void reset() noexcept;
	void reset(entry_t entry) noexcept;
	void set_failure_thread(thread_ref failure_thread) noexcept;

	operator thread_ref() const noexcept;
	
	explicit thread(entry_t entry, thread_ref failure_thread = current_thread());
	explicit thread(entry_t entry, size_t stack_size, thread_ref failure_thread = current_thread());

	thread() = delete;
	thread(const thread& other) = delete;
	thread(thread&& other) = delete;
	thread& operator=(const thread& other) = delete;
	thread& operator=(thread&& other) = delete;
	~thread();

private:
	void setup();
	static void entry_wrapper() noexcept;

	using thread_ptr = std::unique_ptr<void, thread_deleter>;

	friend cothread_t detail::thread_impl<thread>::get_thread() const noexcept;
	template<typename T>
	friend void detail::thread_impl<T>::switch_to() const;

	cothread_t get_thread() const noexcept;
	void stop() const noexcept;

	thread_ptr m_thread;
	thread_ref m_failure_thread;
	entry_t m_entry;
	size_t m_stack_size;
};

} // namespace co

#include "co.ipp"

#endif // CO_HPP_INCLUDE_GUARD

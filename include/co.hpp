#ifndef CO_HPP_INCLUDE_GUARD
#define CO_HPP_INCLUDE_GUARD

#include <libco.h>
#include <memory>
#include <functional>

namespace co {

class thread;
class thread_ref;

namespace detail {

template<typename T>
class thread_impl;

class thread_base
{
private:
	thread_base() noexcept = default;
	thread_base(const thread_base& other) noexcept = default;
	thread_base(thread_base&& other) noexcept = default;
	thread_base& operator=(const thread_base& other) noexcept = default;
	thread_base& operator=(thread_base&& other) noexcept = default;
	~thread_base() noexcept = default;

	friend class ::co::thread;
	friend class ::co::detail::thread_impl<thread>;
	friend class ::co::thread_ref;
	friend class ::co::detail::thread_impl<thread_ref>;

	struct thread_deletion : public std::exception
	{
	};

	static thread_local cothread_t tl_current_thread;
};

template<typename T>
class thread_impl : private thread_base
{
public:
	explicit operator bool() const noexcept;
	void switch_to() const;
protected:
	thread_impl() = default;
private:
	friend class ::co::thread;
	friend class ::co::thread_ref;
	cothread_t get_thread() const noexcept;
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
	cothread_t get_thread() const noexcept;
	cothread_t m_thread = nullptr;
};

class thread : private detail::thread_impl<thread>
{
public:
	using entry_t = std::function<void()>;

	using detail::thread_impl<thread>::operator bool;
	using detail::thread_impl<thread>::switch_to;
	static thread_ref current() noexcept;

	thread();
	thread(const thread& other) = delete;
	thread(thread&& other) = delete;
	thread& operator=(const thread& other) = delete;
	thread& operator=(thread&& other) = delete;
	~thread();

	thread(entry_t entry, size_t stack_size = 1 * 1024 * 1024 * 1024);
private:
	static void entry_wrapper() noexcept;

	struct thread_deleter
	{
		void operator()(cothread_t p) const noexcept;
	};

	using thread_ptr = std::unique_ptr<void, thread_deleter>;
	cothread_t get_thread() const noexcept;
	friend cothread_t detail::thread_impl<thread>::get_thread() const noexcept;
	thread_ptr m_thread;
	entry_t m_entry;
	std::exception_ptr m_exception;

	static thread_local thread* tl_current_this;
};

} // namespace co

#include "co.ipp"

#endif // CO_HPP_INCLUDE_GUARD

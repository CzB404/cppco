#ifndef CO_HPP_INCLUDE_GUARD
#define CO_HPP_INCLUDE_GUARD

#include <libco.h>
#include <memory>

namespace co {

template<typename Entry>
class thread;
class thread_ref;

thread_ref current_thread() noexcept;

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

	template<typename Entry>
	friend class ::co::thread;

	template<typename T>
	friend class ::co::detail::thread_impl;
	friend class ::co::thread_ref;

	struct thread_deletion : public std::exception
	{
	};

	struct thread_deleter
	{
		void operator()(cothread_t p) const noexcept;
	};

	static thread_local cothread_t tl_current_thread;
	static thread_local thread_base* tl_current_this;
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
	template<typename Entry>
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
	friend thread_ref current_thread() noexcept;
	cothread_t get_thread() const noexcept;
	cothread_t m_thread = nullptr;
};

template<typename Entry>
class thread : private detail::thread_impl<thread<Entry>>
{
	using entry_t = Entry;

public:
	using detail::thread_impl<thread>::operator bool;
	using detail::thread_impl<thread>::switch_to;
	void reset() noexcept;

	thread();
	thread(const thread& other) = delete;
	thread(thread&& other) = delete;
	thread& operator=(const thread& other) = delete;
	thread& operator=(thread&& other) = delete;
	~thread();

	thread(entry_t entry, size_t stack_size = 1 * 1024 * 1024 * 1024);
private:
	static void entry_wrapper() noexcept;

	using thread_ptr = std::unique_ptr<void, detail::thread_base::thread_deleter>;
	cothread_t get_thread() const noexcept;
	friend cothread_t detail::thread_impl<thread>::get_thread() const noexcept;
	void signal_destruction() const noexcept;
	thread_ptr m_thread;
	entry_t m_entry;
	std::exception_ptr m_exception;
};

} // namespace co

#include "co.ipp"

#endif // CO_HPP_INCLUDE_GUARD

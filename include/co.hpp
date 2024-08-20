#ifndef CO_HPP_INCLUDE_GUARD
#define CO_HPP_INCLUDE_GUARD

#include <libco.h>
#include <memory>

namespace co {

template<typename Entry>
class thread;
class thread_ref;

class thread_create_failure;

thread_ref current_thread() noexcept;

namespace detail {

template<typename T>
class thread_impl;

class thread_owning_impl;

class thread_base
{
protected:
	thread_base() noexcept = default;
	thread_base(const thread_base& other) noexcept = default;
	thread_base(thread_base&& other) noexcept = default;
	thread_base& operator=(const thread_base& other) noexcept = default;
	thread_base& operator=(thread_base&& other) noexcept = default;
	~thread_base() noexcept = default;

	class thread_deletion : public std::exception
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
	using thread_base::thread_deleter;
	using thread_base::thread_deletion;
	thread_impl() = default;
	cothread_t get_thread() const noexcept;

	using thread_base::tl_current_thread;
	using thread_base::tl_current_this;
};

class thread_owning_impl : private thread_impl<thread_owning_impl>
{
public:
	// libco's recommendation for the default stack size is 1 MB on 32 bit systems, and to define the stack size in pointer size.
	inline static constexpr size_t default_stack_size = 1 * 1024 * 1024 / 4 * sizeof(void*);

	using thread_impl<thread_owning_impl>::operator bool;
	using thread_impl<thread_owning_impl>::switch_to;
	void reset() noexcept;

	operator thread_ref() const noexcept;

protected:
	using thread_ptr = std::unique_ptr<void, thread_deleter>;
	using thread_impl<thread_owning_impl>::thread_deletion;

	friend cothread_t detail::thread_impl<thread_owning_impl>::get_thread() const noexcept;

	cothread_t get_thread() const noexcept;
	thread_owning_impl() noexcept = default;
	void signal_destruction() const noexcept;

	thread_ptr m_thread;
	std::exception_ptr m_exception;

	using thread_impl<thread_owning_impl>::tl_current_thread;
	using thread_impl<thread_owning_impl>::tl_current_this;
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
	friend class detail::thread_owning_impl;
	template<typename Entry>
	friend class thread;
	friend thread_ref current_thread() noexcept;
	cothread_t get_thread() const noexcept;
	cothread_t m_thread = nullptr;
};

class thread_create_failure : public std::runtime_error
{
public:
	thread_create_failure() noexcept;
};

template<typename Entry>
class thread : private detail::thread_owning_impl
{
	using entry_t = Entry;

public:
	using detail::thread_owning_impl::operator bool;
	using detail::thread_owning_impl::switch_to;
	using detail::thread_owning_impl::reset;
	using detail::thread_owning_impl::operator thread_ref;

	thread();
	thread(const thread& other) = delete;
	thread(thread&& other) = delete;
	thread& operator=(const thread& other) = delete;
	thread& operator=(thread&& other) = delete;
	~thread();

	using detail::thread_owning_impl::default_stack_size;
	thread(entry_t entry, size_t stack_size = default_stack_size);
private:
	static void entry_wrapper() noexcept;

	entry_t m_entry;
};

} // namespace co

#include "co.ipp"

#endif // CO_HPP_INCLUDE_GUARD

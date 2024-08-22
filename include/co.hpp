#ifndef CO_HPP_INCLUDE_GUARD
#define CO_HPP_INCLUDE_GUARD

#include <libco.h>
#include <memory>
#include <functional>

namespace co {

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

	struct thread_failure
	{
		thread_owning_impl* failing_thread = nullptr;
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

	static thread_local cothread_t tl_current_thread;
	static thread_local thread_base* tl_current_this;
	static thread_local thread_failure tl_current_failure;
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
	friend class thread_owning_impl;
	friend class ::co::thread_ref;
	cothread_t get_thread() const noexcept;

	using thread_base::tl_current_thread;
	using thread_base::tl_current_this;
	using thread_base::tl_current_failure;
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
	friend class thread;
	friend thread_ref current_thread() noexcept;
	cothread_t get_thread() const noexcept;
	cothread_t m_thread = nullptr;
};

namespace detail {

class thread_owning_impl : private thread_impl<thread_owning_impl>
{
public:
	using entry_t = std::function<void()>;

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
	template<typename T>
	friend void detail::thread_impl<T>::switch_to() const;

	cothread_t get_thread() const noexcept;
	thread_owning_impl(thread_ref failure_thread) noexcept;
	void signal_destruction() const noexcept;

	thread_ptr m_thread;
	thread_ref m_failure_thread;

	using thread_impl<thread_owning_impl>::tl_current_thread;
	using thread_impl<thread_owning_impl>::tl_current_this;
	using thread_impl<thread_owning_impl>::tl_current_failure;
};

} // namespace detail

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

class thread : private detail::thread_owning_impl
{
public:
	using detail::thread_owning_impl::entry_t;

	using detail::thread_owning_impl::operator bool;
	using detail::thread_owning_impl::switch_to;
	using detail::thread_owning_impl::reset;
	using detail::thread_owning_impl::operator thread_ref;

	thread() = delete;
	thread(const thread& other) = delete;
	thread(thread&& other) = delete;
	thread& operator=(const thread& other) = delete;
	thread& operator=(thread&& other) = delete;
	~thread();

	using detail::thread_owning_impl::default_stack_size;
	explicit thread(entry_t entry, size_t stack_size, thread_ref failure_thread = current_thread());
	explicit thread(entry_t entry, thread_ref failure_thread = current_thread());
private:
	static void entry_wrapper() noexcept;

	entry_t m_entry;
};

} // namespace co

#include "co.ipp"

#endif // CO_HPP_INCLUDE_GUARD

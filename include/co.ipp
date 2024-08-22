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

#ifndef CO_IPP_INCLUDE_GUARD
#define CO_IPP_INCLUDE_GUARD

#include <stdexcept>
#include <type_traits>
#include <cassert>

namespace co {

template<typename T>
inline cothread_t detail::thread_impl<T>::get_thread() const noexcept
{
	static_assert(std::is_base_of_v<thread_impl<T>, T>, "co::detail::thread_impl<T> is a CRTP class, T should inherit from co::detail::thread_impl<T>!");
	return static_cast<const T*>(this)->get_thread();
}

inline cothread_t thread_ref::get_thread() const noexcept
{
	return m_thread;
}

inline cothread_t detail::thread_owning_impl::get_thread() const noexcept
{
	return m_thread.get();
}

inline void detail::thread_owning_impl::reset() noexcept
{
	signal_destruction();
	m_thread.reset();
}

template<typename T>
inline detail::thread_impl<T>::operator bool() const noexcept
{
	return get_thread() != nullptr;
}

template<typename T>
inline void detail::thread_impl<T>::switch_to() const
{
	co_switch(get_thread());
	// If the current thread variable is set while switch_to is called then it's the destructor that's issuing the call and the entry function stack has to be destroyed. Propagate an exception to achieve that.
	if (tl_current_thread)
	{
		throw thread_deletion();
	}
	// If there is an active failure then this is the callback to the failure handling cothread. Rethrow the failure.
	if (auto failure = std::exchange(tl_current_failure, nullptr))
	{
		failure.failing_thread->m_thread.reset(); // The exception killed the cothread's scope.
		std::rethrow_exception(failure.exception);
	}
}

inline thread_ref current_thread() noexcept
{
	return thread_ref(co_active());
}

inline thread_ref::thread_ref(cothread_t thread) noexcept
	: m_thread{ thread }
{
}

inline detail::thread_owning_impl::operator thread_ref() const noexcept
{
	return thread_ref(get_thread());
}

inline void detail::thread_base::thread_deleter::operator()(cothread_t p) const noexcept
{
	co_delete(p);
}

inline void detail::thread_owning_impl::signal_destruction() const noexcept
{
	if (!m_thread)
	{
		return;
	}
	assert(tl_current_thread == nullptr);
	tl_current_thread = co_active();
	switch_to();
}

inline thread::~thread()
{
	signal_destruction();
}

inline thread_create_failure::thread_create_failure() noexcept
	: std::runtime_error("Failed to create co::thread")
{
}

inline thread_return_failure::thread_return_failure() noexcept
	: std::runtime_error("Return from entry function given to co::thread")
{
}

inline detail::thread_owning_impl::thread_owning_impl(thread_ref failure_thread) noexcept
	: m_failure_thread{ failure_thread }
{
}

inline thread::thread(thread::entry_t entry, size_t stack_size, thread_ref failure_thread)
	: detail::thread_owning_impl{ failure_thread }
	, m_entry{ std::move(entry) }
{
	assert(tl_current_this == nullptr);
	assert(tl_current_thread == nullptr);
	// Parameters to the true libco entry function have to be passed as "globals".
	tl_current_this = (detail::thread_base*)this;
	tl_current_thread = co_active();
	m_thread.reset(co_create(static_cast<unsigned int>(stack_size), &entry_wrapper));
	if (m_thread == nullptr)
	{
		tl_current_this = nullptr;
		tl_current_thread = nullptr;
		throw thread_create_failure();
	}
	// Handing execution over to the entry wrapper to let it store the parameters on its stack.
	co_switch(m_thread.get());
}

inline thread::thread(thread::entry_t entry, thread_ref failure_thread)
	: thread(std::move(entry), default_stack_size, failure_thread)
{
}

inline thread_local detail::thread_base* detail::thread_base::tl_current_this = nullptr;
inline thread_local cothread_t detail::thread_base::tl_current_thread = nullptr;
inline thread_local detail::thread_base::thread_failure detail::thread_base::tl_current_failure{};

inline detail::thread_base::thread_failure::operator bool() const noexcept
{
	assert(!exception == !failing_thread);
	return exception && failing_thread;
}

inline void thread::entry_wrapper() noexcept
{
	assert(tl_current_this != nullptr);
	assert(tl_current_thread != nullptr);
	// Acquire parameters from constructor
	auto* self = (thread*)(std::exchange(tl_current_this, nullptr));
	auto creating_thread = thread_ref(std::exchange(tl_current_thread, nullptr));
	try
	{
		// Hand control back to the creating cothread.
		// This has to be done in the try block in case this cothread is destroyed before use.
		creating_thread.switch_to();
		(self->m_entry)();
	}
	catch (const thread_deletion&)
	{
		assert(tl_current_thread != nullptr);
		auto* deleting_thread = std::exchange(tl_current_thread, nullptr);
		co_switch(deleting_thread);
	}
	catch(...)
	{
		assert(tl_current_failure == nullptr);
		tl_current_failure.failing_thread = self;
		tl_current_failure.exception = std::current_exception();
		co_switch(self->m_failure_thread.get_thread());
	}
	// Handling entry function return
	assert(tl_current_failure == nullptr);
	tl_current_failure.failing_thread = self;
	tl_current_failure.exception = std::make_exception_ptr(thread_return_failure());
	co_switch(self->m_failure_thread.get_thread());
}

} // namespace co

#endif // CO_IPP_INCLUDE_GUARD

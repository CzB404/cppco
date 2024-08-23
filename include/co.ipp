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

inline thread_local std::unique_ptr<detail::thread_base::thread_status> detail::thread_base::tl_status = std::make_unique<detail::thread_base::thread_status>();

inline thread_create_failure::thread_create_failure() noexcept
	: std::runtime_error("Failed to create co::thread")
{
}

inline thread_return_failure::thread_return_failure() noexcept
	: std::runtime_error("Return from entry function given to co::thread")
{
}

inline thread_ref current_thread() noexcept
{
	return thread_ref(co_active());
}

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

inline cothread_t thread::get_thread() const noexcept
{
	return m_thread.get();
}

inline thread::operator thread_ref() const noexcept
{
	return thread_ref(get_thread());
}

template<typename T>
inline detail::thread_impl<T>::operator bool() const noexcept
{
	return get_thread() != nullptr;
}

inline void thread::set_failure_thread(thread_ref failure_thread) noexcept
{
	m_failure_thread = failure_thread;
}

inline void thread::reset() noexcept
{
	stop();
	m_thread.reset();
}

inline void thread::reset(thread::entry_t entry) noexcept
{
	stop();
	m_entry = std::move(entry);
	setup();
}

template<typename T>
inline void detail::thread_impl<T>::switch_to() const
{
	co_switch(get_thread());
	// If the current thread variable is set while switch_to is called then it's the stop function that's issuing the call and the entry function stack has to be destroyed. Propagate an exception to achieve that.
	if (tl_status->current_thread)
	{
		throw thread_stopping();
	}
	// If there is an active failure then this is the callback to the failure handling cothread. Rethrow the failure.
	if (auto failure = std::exchange(tl_status->current_failure, nullptr))
	{
		failure.failing_thread->m_thread.reset(); // The exception killed the cothread's scope.
		std::rethrow_exception(failure.exception);
	}
}

inline thread_ref::thread_ref(cothread_t thread) noexcept
	: m_thread{ thread }
{
}

inline void detail::thread_base::thread_deleter::operator()(cothread_t p) const noexcept
{
	co_delete(p);
}

inline void thread::stop() const noexcept
{
	if (!m_thread)
	{
		return;
	}
	assert(tl_status->current_thread == nullptr);
	tl_status->current_thread = co_active();
	switch_to();
}

inline thread::~thread()
{
	stop();
}

inline thread::thread(thread::entry_t entry, thread_ref failure_thread)
	: thread(std::move(entry), default_stack_size, failure_thread)
{
}

inline thread::thread(thread::entry_t entry, size_t stack_size, thread_ref failure_thread)
	: m_failure_thread{ failure_thread }
	, m_entry{ std::move(entry) }
	, m_stack_size{ stack_size }
{
	setup();
}

inline void thread::setup()
{
	assert(tl_status->current_this == nullptr);
	assert(tl_status->current_thread == nullptr);
	// Parameters to the true libco entry function have to be passed as "globals".
	tl_status->current_this = this;
	tl_status->current_thread = co_active();
	if (!m_thread)
	{
		m_thread.reset(co_create(static_cast<unsigned int>(m_stack_size), &entry_wrapper));
	}
	if (m_thread == nullptr)
	{
		tl_status->current_this = nullptr;
		tl_status->current_thread = nullptr;
		throw thread_create_failure();
	}
	// Handing execution over to the entry wrapper to let it store the parameters on its stack.
	co_switch(m_thread.get());
}

inline detail::thread_base::thread_failure::operator bool() const noexcept
{
	assert(!exception == !failing_thread);
	return exception && failing_thread;
}

inline void thread::entry_wrapper() noexcept
{
	while (true) // Reuse
	{
		assert(tl_status->current_this != nullptr);
		assert(tl_status->current_thread != nullptr);
		// Acquire parameters from setup
		auto* self = std::exchange(tl_status->current_this, nullptr);
		auto creating_thread = thread_ref(std::exchange(tl_status->current_thread, nullptr));
		try
		{
			// Hand control back to the cothread that called setup.
			// This has to be done in the try block in case this cothread is stopped before use.
			creating_thread.switch_to();
			(self->m_entry)();
			// Handling entry function return
			throw thread_return_failure();
		}
		catch (const thread_stopping&)
		{
			assert(tl_status->current_thread != nullptr);
			auto* stopping_thread = std::exchange(tl_status->current_thread, nullptr);
			co_switch(stopping_thread); // Stop
		}
		catch (...)
		{
			assert(tl_status->current_failure == nullptr);
			tl_status->current_failure.failing_thread = self;
			tl_status->current_failure.exception = std::current_exception();
			co_switch(self->m_failure_thread.get_thread()); // Failure
		}
	}
}

} // namespace co

#endif // CO_IPP_INCLUDE_GUARD

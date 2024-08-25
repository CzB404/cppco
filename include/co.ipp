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
	: thread_failure("Failed to create co::thread")
{
}

inline thread_return_failure::thread_return_failure() noexcept
	: thread_failure("Return from entry function given to co::thread")
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
	return *this ? m_thread.get() : nullptr;
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

inline thread_ref::thread_ref(cothread_t thread) noexcept
	: m_thread{ thread }
{
}

inline bool operator==(const thread_ref lhs, const thread_ref rhs) noexcept
{
	return lhs.m_thread == rhs.m_thread;
}

inline bool operator!=(const thread_ref lhs, const thread_ref rhs) noexcept
{
	return !(lhs == rhs);
}

inline size_t thread::get_stack_size() const noexcept
{
	return m_stack_size;
}
inline void thread::set_stack_size(size_t stack_size) noexcept
{
	m_stack_size = stack_size;
	if (!*this)
	{
		return;
	}
	stop();
	m_thread.reset();
	setup();
}

inline void thread::set_failure_thread(thread_ref failure_thread) noexcept
{
	assert(failure_thread);
	m_failure_thread = failure_thread;
}

inline thread_ref thread::get_failure_thread() const noexcept
{
	return m_failure_thread;
}

inline void detail::thread_base::thread_deleter::operator()(cothread_t p) const noexcept
{
	assert(p);
	co_delete(p);
}

inline void thread::reset()
{
	stop();
	m_entry = nullptr;
	m_thread.reset();
}

inline void thread::reset(thread::entry_t entry)
{
	stop();
	m_entry = std::move(entry);
	setup();
}

inline void thread::rewind()
{
	stop();
	setup();
}

template<typename T>
inline void detail::thread_impl<T>::switch_to() const
{
	auto* thread = get_thread();
	assert(thread != nullptr);
	co_switch(thread);
	// If the current thread variable is set while switch_to is called then it's the stop function that's issuing the call and the entry function stack has to be destroyed. Propagate an exception to achieve that.
 	if (tl_status->current_thread)
	{
		throw thread_stopping();
	}
	// If there is an active exception then this is the callback to the failure handling cothread. Rethrow the exception.
	if (auto exception = std::exchange(tl_status->current_exception, nullptr))
	{
		std::rethrow_exception(exception);
	}
}

inline void thread::stop() const noexcept
{
	if (!*this)
	{
		return;
	}
	assert(tl_status->current_thread == nullptr);
	tl_status->current_thread = co_active();
	switch_to();
}

inline thread::thread(size_t stack_size)
	: thread(current_thread(), stack_size) 
{
}

inline thread::thread(thread_ref failure_thread, size_t stack_size)
	: m_failure_thread{ failure_thread }
	, m_stack_size{ stack_size }
{
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

inline thread::operator bool() const noexcept
{
	return static_cast<bool>(m_active);
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
		self->m_active = true;
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
			self->m_active = false;
			co_switch(stopping_thread); // Stop
		}
		catch (...)
		{
			assert(tl_status->current_exception == nullptr);
			tl_status->current_exception = std::current_exception();
			self->m_active = false;
			co_switch(self->m_failure_thread.get_thread()); // Failure
		}
	}
}

} // namespace co

#endif // CO_IPP_INCLUDE_GUARD

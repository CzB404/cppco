// Copyright(C) 2024 by Balazs Cziraki <balazs.cziraki@gmail.com>
//
// Permission to use, copy, modify, and /or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above copyright
// notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
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

inline thread_local std::unique_ptr<thread::thread_status> thread::tl_status = std::make_unique<thread::thread_status>();

inline thread::thread_status::thread_status() noexcept
	: main{ std::make_unique<thread>(co_active(), private_token) }
	, current_active{ main.get() }
{
}

inline thread_create_failure::thread_create_failure() noexcept
	: thread_failure("Failed to create co::thread")
{
}

inline thread_return_failure::thread_return_failure() noexcept
	: thread_failure("Return from entry function given to co::thread")
{
}

inline const thread& active() noexcept
{
	return *thread::tl_status->current_active;
}

inline cothread_t thread::get_thread() const noexcept
{
	return *this ? m_thread.get() : nullptr;
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

inline void thread::set_parent(const thread& parent) noexcept
{
	assert(parent);
	m_parent = &parent;
}

inline const thread& thread::get_parent() const noexcept
{
	return *m_parent;
}

inline void thread::thread_deleter::operator()(cothread_t p) const noexcept
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

inline void thread::switch_to() const
{
	auto* thread = get_thread();
	assert(thread != nullptr);
	tl_status->current_active = this;
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
	tl_status->current_thread = &active();
	switch_to();
}

inline thread::thread(size_t stack_size)
	: thread(active(), stack_size) 
{
}

inline thread::thread(const thread& parent, size_t stack_size)
	: m_parent{ &parent }
	, m_stack_size{ stack_size }
{
}

inline thread::~thread()
{
	stop();
	if (m_parent == nullptr)
	{
		m_thread.release(); // The main cothread has no owner.
	}
}

inline thread::thread(cothread_t cothread, thread::private_token_t) noexcept
	: m_thread{ cothread }
	, m_active{ true }
{
}

inline thread::thread(thread::entry_t entry, const thread& parent)
	: thread(std::move(entry), default_stack_size, parent)
{
}

inline thread::thread(thread::entry_t entry, size_t stack_size, const thread& parent)
	: m_parent{ &parent }
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
	tl_status->current_thread = &active();
	if (!m_thread)
	{
		auto cothread = cothread_t{};
		auto int_stack_size = static_cast<unsigned int>(m_stack_size);
#ifdef CPPCO_FLB_LIBCO
		size_t real_stack_size;
		cothread = co_create(int_stack_size, &entry_wrapper, &real_stack_size);
#else // CPPCO_FLB_LIBCO
		cothread = co_create(int_stack_size, &entry_wrapper);
#endif // CPPCO_FLB_LIBCO
		m_thread.reset(cothread);
	}
	if (m_thread == nullptr)
	{
		tl_status->current_this = nullptr;
		tl_status->current_thread = nullptr;
		throw thread_create_failure();
	}
	// Handing execution over to the entry wrapper to let it store the parameters on its stack.
	tl_status->current_active = this;
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
		auto&& creating_thread = *std::exchange(tl_status->current_thread, nullptr);
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
			auto&& stopping_thread = *std::exchange(tl_status->current_thread, nullptr);
			self->m_active = false;
			tl_status->current_active = &stopping_thread;
			co_switch(stopping_thread.get_thread()); // Stop
		}
		catch (...)
		{
			assert(tl_status->current_exception == nullptr);
			tl_status->current_exception = std::current_exception();
			self->m_active = false;
			tl_status->current_active = self->m_parent;
			co_switch(self->m_parent->get_thread()); // Failure
		}
	}
}

} // namespace co

#endif // CO_IPP_INCLUDE_GUARD

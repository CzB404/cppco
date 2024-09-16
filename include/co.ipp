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

#include <cassert>

namespace co {

#ifdef __GNUC__
constexpr size_t thread::default_stack_size __attribute__((weak));
constexpr thread::private_token_t thread::private_token __attribute__((weak));
#endif // __GNUC__

#ifndef CPPCO_CUSTOM_STATUS
inline thread::thread_status& thread::status()
{
	static thread_local std::unique_ptr<thread::thread_status> instance = std::make_unique<thread::thread_status>();
	return *instance;
}
#endif // CPPCO_CUSTOM_STATUS

inline thread::thread_status::thread_status() noexcept
	: main{ thread(co_active(), private_token) }
	, current_active{ &main }
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

#ifdef CPPCO_LIBCO_INTEROP
inline void init() noexcept
{
	thread::status();
}

inline bool thread::thread_status::thread_order::operator()(const thread& lhs, const thread& rhs) const noexcept
{
	return lhs.get_thread() < rhs.get_thread();
}

inline thread::thread_status::registry& thread::thread_status::get_registry()
{
	static registry instance;
	return instance;
}

inline thread* thread::thread_status::registry::find(cothread_t cothread) noexcept
{
	assert(cothread != nullptr);
	std::lock_guard<std::recursive_mutex> guard(mutex);
	auto it = data.find(cothread);
	if (it == data.end())
	{
		return nullptr;
	}
	return it->second;
}

inline void thread::thread_status::registry::insert(cothread_t cothread, thread* pthread) noexcept
{
	if (cothread == nullptr)
	{
		return;
	}
	std::lock_guard<std::recursive_mutex> guard(mutex);
	auto success = data.insert(std::make_pair(cothread, pthread));
	static_cast<void>(success); // Suppress unused variable warning because `assert` does not.
	assert(success.second == true);
}

inline void thread::thread_status::registry::exchange(cothread_t cothread, thread* pthread) noexcept
{
	if (cothread == nullptr)
	{
		return;
	}
	std::lock_guard<std::recursive_mutex> guard(mutex);
	auto it = data.find(cothread);
	assert(it != data.end());
	it->second = pthread;
}

inline void thread::thread_status::registry::erase(cothread_t cothread) noexcept
{
	if (cothread == nullptr)
	{
		return;
	}
	std::lock_guard<std::recursive_mutex> guard(mutex);
	auto count = data.erase(cothread);
	static_cast<void>(count); // Suppress unused variable warning because `assert` does not.
	assert(count == 1);
}

inline void set_active_as_main() noexcept
{
	if (co::active().get_thread() == co::main().get_thread())
	{
		return;
	}

	auto&& status = thread::status();
	status.main = thread(co_active(), thread::private_token);
	status.current_active = &status.main;
}


inline void clear_external() noexcept
{
	auto&& registry = thread::thread_status::get_registry();
	std::lock_guard<std::recursive_mutex> guard(registry.mutex);
	registry.external.clear();
}
#endif // CPPCO_LIBCO_INTEROP

inline const thread& active() noexcept
{
	auto&& status = thread::status();
#ifdef CPPCO_LIBCO_INTEROP
	auto active_cothread = co_active();
	if (status.current_active->get_thread() == active_cothread)
	{
		return *status.current_active;
	}
	auto&& registry = thread::thread_status::get_registry();
	std::lock_guard<std::recursive_mutex> guard(registry.mutex);
	auto* cothread = registry.find(active_cothread);
	if (cothread != nullptr)
	{
		status.current_active = cothread;
		return *status.current_active;
	}
	auto success = registry.external.insert(thread(active_cothread, thread::private_token));
	assert(success.second == true);
	assert(registry.find(active_cothread) == &*success.first);
	status.current_active = &*success.first;
	return *status.current_active;
#else // CPPCO_LIBCO_INTEROP
	assert(status.current_active->get_thread() == co_active());
	return *status.current_active;
#endif // CPPCO_LIBCO_INTEROP
}

inline const thread& main() noexcept
{
	return thread::status().main;
}

inline cothread_t thread::get_thread() const noexcept
{
	return m_thread.get();
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
#ifdef CPPCO_LIBCO_INTEROP
	thread_status::get_registry().erase(get_thread());
#endif // CPPCO_LIBCO_INTEROP
	m_thread.reset();
	setup();
}

inline void thread::set_parent(const thread& parent) noexcept
{
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
#ifdef CPPCO_LIBCO_INTEROP
	thread_status::get_registry().erase(get_thread());
#endif // CPPCO_LIBCO_INTEROP
	m_thread.reset();
}

inline void thread::reset(thread::entry_t entry)
{
	stop();
	m_entry = std::make_unique<entry_t>(std::move(entry));
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
	status().current_active = this;
	co_switch(thread);
	// If the current thread variable is set while switch_to is called then it's the stop function that's issuing the call and the entry function stack has to be destroyed. Propagate an exception to achieve that.
 	if (status().current_thread)
	{
		throw thread_stopping();
	}
	// If there is an active exception then this is the callback to the failure handling cothread. Rethrow the exception.
	if (auto exception = std::exchange(status().current_exception, nullptr))
	{
		std::rethrow_exception(exception);
	}
}

inline void thread::stop() const noexcept
{
	if (!*this || m_parent == nullptr)
	{
		return;
	}
	assert(status().current_thread == nullptr);
	status().current_thread = &active();
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
#ifdef CPPCO_LIBCO_INTEROP
	thread_status::get_registry().erase(get_thread());
#endif // CPPCO_LIBCO_INTEROP
	if (m_parent == nullptr)
	{
		m_thread.release(); // `cppco` does not own external cothreads.
	}

}

inline thread::thread(thread&& other) noexcept
	: m_thread{ std::move(other.m_thread) }
	, m_parent{ std::exchange(other.m_parent, &co::active()) }
	, m_entry{ std::move(other.m_entry) }
	, m_stack_size{ std::exchange(other.m_stack_size, default_stack_size) }
	, m_active{ std::exchange(other.m_active, false) }
{
#ifdef CPPCO_LIBCO_INTEROP
	thread_status::get_registry().exchange(get_thread(), this);
#endif // CPPCO_LIBCO_INTEROP
}

inline thread& thread::operator=(thread&& other) noexcept
{
	m_thread = std::move(other.m_thread);
	m_parent = std::exchange(other.m_parent, &co::active());
	m_entry = std::move(other.m_entry);
	m_stack_size = std::exchange(other.m_stack_size, default_stack_size);
	m_active = std::exchange(other.m_active, false);
#ifdef CPPCO_LIBCO_INTEROP
	thread_status::get_registry().exchange(get_thread(), this);
#endif // CPPCO_LIBCO_INTEROP
	return *this;
}

inline thread::thread(cothread_t cothread, thread::private_token_t) noexcept
	: m_thread{ cothread }
	, m_active{ true }
{
#ifdef CPPCO_LIBCO_INTEROP
	thread_status::get_registry().insert(get_thread(), this);
#endif // CPPCO_LIBCO_INTEROP
}

inline thread::thread(thread::entry_t entry, const thread& parent)
	: thread(std::move(entry), default_stack_size, parent)
{
}

inline thread::thread(thread::entry_t entry, size_t stack_size, const thread& parent)
	: m_parent{ &parent }
	, m_entry{ std::make_unique<entry_t>(std::move(entry)) }
	, m_stack_size{ stack_size }
{
	setup();
}

inline void thread::setup()
{
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
#ifdef CPPCO_LIBCO_INTEROP
		auto&& registry = thread_status::get_registry();
		std::lock_guard<std::recursive_mutex> guard(registry.mutex);
		registry.erase(get_thread());
#endif // CPPCO_LIBCO_INTEROP
		m_thread.reset(cothread);
#ifdef CPPCO_LIBCO_INTEROP
		registry.insert(get_thread(), this);
#endif // CPPCO_LIBCO_INTEROP
	}
	if (m_thread == nullptr)
	{
		status().current_thread = nullptr;
		throw thread_create_failure();
	}
}

inline thread::operator bool() const noexcept
{
	return static_cast<bool>(m_active);
}

inline void thread::entry_wrapper() noexcept
{
	while (true) // Reuse
	{
		co::active().m_active = true;
		try
		{
			(*co::active().m_entry)();
			// Handling entry function return
			throw thread_return_failure();
		}
		catch (const thread_stopping&)
		{
			assert(status().current_thread != nullptr);
			auto&& stopping_thread = *std::exchange(status().current_thread, nullptr);
			co::active().m_active = false;
			status().current_active = &stopping_thread;
			co_switch(stopping_thread.get_thread()); // Stop
		}
		catch (...)
		{
			assert(status().current_exception == nullptr);
			status().current_exception = std::current_exception();
			co::active().m_active = false;
			status().current_active = co::active().m_parent;
			co_switch(status().current_active->get_thread()); // Failure
		}
	}
}

} // namespace co

#endif // CO_IPP_INCLUDE_GUARD

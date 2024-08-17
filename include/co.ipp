#ifndef CO_IPP_INCLUDE_GUARD
#define CO_IPP_INCLUDE_GUARD

#include <stdexcept>
#include <type_traits>

namespace co {

template<typename T>
cothread_t detail::thread_impl<T>::get_thread() const noexcept
{
	static_assert(std::is_base_of_v<thread_impl<T>, T>, "co::detail::thread_impl<T> is a CRTP class, T should inherit from co::detail::thread_impl<T>!");
	return static_cast<const T*>(this)->get_thread();
}

cothread_t thread_ref::get_thread() const noexcept
{
	return m_thread;
}

cothread_t thread::get_thread() const noexcept
{
	return m_thread.get();
}

template<typename T>
detail::thread_impl<T>::operator bool() const noexcept
{
	return get_thread() != nullptr;
}

template<typename T>
void detail::thread_impl<T>::switch_to() const
{
	co_switch(get_thread());
	if (tl_current_thread)
	{
		throw thread_deletion();
	}
}

thread_ref thread::current() noexcept
{
	return thread_ref(co_active());
}

thread_ref::thread_ref(cothread_t thread) noexcept
	: m_thread{ thread }
{
}

inline void thread::thread_deleter::operator()(cothread_t p) const noexcept
{
	co_delete(p);
}

inline thread::thread() = default;
inline thread::~thread()
{
	tl_current_thread = co_active();
	switch_to();
}

thread::thread(thread::entry_t entry, size_t stack_size)
	: m_entry{ std::move(entry) }
{
	tl_current_this = this;
	tl_current_thread = co_active();
	m_thread.reset(co_create(static_cast<unsigned int>(stack_size), &entry_wrapper));
	co_switch(m_thread.get());
}

inline thread_local thread* thread::tl_current_this = nullptr;
inline thread_local cothread_t thread::tl_current_thread = nullptr;

void thread::entry_wrapper() noexcept
{
	auto* self = std::exchange(tl_current_this, nullptr);
	auto* creating_thread = std::exchange(tl_current_thread, nullptr);
	co_switch(creating_thread);
	try
	{
		(self->m_entry)();
	}
	catch (const thread_deletion&)
	{
		auto* deleting_thread = std::exchange(tl_current_thread, nullptr);
		co_switch(deleting_thread);
	}
	catch(...)
	{
		self->m_exception = std::current_exception();
		co_switch(creating_thread);
	}
}

} // namespace co

#endif // CO_IPP_INCLUDE_GUARD

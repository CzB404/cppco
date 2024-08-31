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

/// \file co.hpp
/// `co.hpp` is the main header file for `cppco`.
///
/// `cppco` has customization macros that affect the behavior of the code
/// defined in this file.
/// 
/// - `CPPCO_CUSTOM_STATUS`: If defined the private static function definition
///   `thread::thread_status& thread::status()` will be omitted. This is useful
///   for clients that want to handle `thread_local` storage directly. The
///   custom status function may only call the constructor of
///   `thread::thread_status` and has to keep it alive throughout the runtime of
///   the application.
/// - `CPPCO_FLB_LIBCO`: If defined then the three argument `co_create` function
///   will be used as it is defined in the `libco`'s `flb_libco` fork.

#ifndef CO_HPP_INCLUDE_GUARD
#define CO_HPP_INCLUDE_GUARD

#include <libco.h>
#include <memory>
#include <functional>

namespace co {

class thread;

class thread_failure;
class thread_create_failure;
class thread_return_failure;

/// `co::active()` returns the currently active `co::thread`.
///
/// It can also be used from the main cothread, which will return an instance
/// that represents the main cothread but otherwise has no ownership of it.
const thread& active() noexcept;

/// `co::thread_failure` is the base exception of the `cppco` library.
class thread_failure : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

/// `co::thread_create_failure` signals that a `co::thread` failed to initialize.
class thread_create_failure : public thread_failure
{
public:
	thread_create_failure() noexcept;
};

/// `co::thread_return_failure` signals that a `co::thread` tried to return.
class thread_return_failure : public thread_failure
{
public:
	thread_return_failure() noexcept;
};

/// `co::thread_stopping` is used to signal that the entry functor needs to exit as soon as possible.
///
/// Catching this exception is only permitted if it is rethrown in the same catch block.
class thread_stopping
{
};

/// `co::thread` is a class that represents and handles so-called "cothreads".
class thread
{
public:
	friend const thread& active() noexcept;

	/// `co::thread::entry_t` is the functor type for the entry functions for cothreads.
	using entry_t = std::function<void()>;

	/// The recommended size for the stack is 1 MB on 32 bit systems, and to define the stack size in pointer size.
	///
	/// Source: <https://github.com/higan-emu/libco/blob/9b76ff4c5c7680555d27c869ae90aa399d3cd0f2/doc/usage.md#co_create>
	static constexpr size_t default_stack_size = 1 * 1024 * 1024 / 4 * sizeof(void*);

	/// `co::thread` is considered to be running when the user supplied entry functor has been entered.
	///
	/// \return Boolean whether the `co::thread` is running (`true`) or not (`false`).
	explicit operator bool() const noexcept;

	/// Switches to this `co::thread`.
	///
	/// The previously active `co::thread` will resume from where it called this function.
	void switch_to() const;

	/// Releases all resources held by this `co::thread`.
	///
	/// Also stops the running entry functor.
	void reset();

	/// Sets a new entry functor.
	///
	/// Also stops the previous entry functor.
	///
	/// \param entry  The new entry functor for this `co::thread`.
	void reset(entry_t entry);

	/// Rewinds the entry functor's execution to its initial state.
	void rewind();

	/// Gets the stack size of this `co::thread`.
	///
	/// \return The current stack size.
	size_t get_stack_size() const noexcept;
	/// Sets the stack size of this `co::thread`.
	/// 
	/// Also stops the running entry functor, as there is no other way to change the stack size.
	///
	/// \param stack_size  The new stack size.
	void set_stack_size(size_t stack_size) noexcept;

	/// Gets the parent `co::thread` of this `co::thread`.
	///
	/// It is undefined behavior to get the parent of the main cothread.
	/// 
	/// \return The parent `co::thread` of this `co::thread`.
	const thread& get_parent() const noexcept;
	/// Sets the parent `co::thread` of this `co::thread`.
	///
	/// \param parent The new parent for this `co::thread`.
	void set_parent(const thread& parent) noexcept;

	/// Constructs an empty `co::thread`.
	///
	/// It will need an entry functor assigned to it via `reset(entry_t entry)`.
	/// The parent is set to be the calling `co::thread`.
	///
	/// \param stack_size The stack size. Defaults to `co::thread::default_stack_size`.
	explicit thread(size_t stack_size = default_stack_size);
	/// Constructs an empty `co::thread`.
	///
	/// It will need an entry functor assigned to it via `reset(entry_t entry)`.
	///
	/// \param parent      The explicitly specified parent for this `co::thread`.
	/// \param stack_size  The stack size. Defaults to `co::thread::default_stack_size`.
	explicit thread(const thread& parent, size_t stack_size = default_stack_size);

	/// Constructs a `co::thread` with `entry` as its entry functor.
	/// 
	/// The stack size will be `co::thread::default_stack_size`.
	///
	/// \param entry   The entry functor that will begin execution when the `co::thread` starts running.
	/// \param parent  The explicitly specified parent for this `co::thread`. Defaults to the calling `co::thread`.
	explicit thread(entry_t entry, const thread& parent = active());
	/// Constructs a `co::thread` with `entry` as its entry functor.
	///
	/// \param entry       The entry functor that will begin execution when the `co::thread` starts running.
	/// \param stack_size  The stack size.
	/// \param parent      The explicitly specified parent for this `co::thread`. Defaults to the calling `co::thread`.
	explicit thread(entry_t entry, size_t stack_size, const thread& parent = active());

	/// Move constructor.
	thread(thread&& other) noexcept;
	/// Move assignment operator.
	thread& operator=(thread&& other) noexcept;
	/// Destructor.
	~thread();

	thread(const thread& other) = delete;
	thread& operator=(const thread& other) = delete;

private:
	class private_token_t {};
	static constexpr private_token_t private_token{};
public:
	explicit thread(cothread_t cothread, private_token_t) noexcept;

private:

	struct thread_deleter
	{
		void operator()(cothread_t p) const noexcept;
	};

	struct thread_status
	{
		std::unique_ptr<thread> main;
		const thread* current_active = nullptr;
		const thread* current_thread = nullptr;
		std::exception_ptr current_exception;

		thread_status() noexcept;
	};

	void setup();
	static void entry_wrapper() noexcept;

	using thread_ptr = std::unique_ptr<void, thread_deleter>;

	cothread_t get_thread() const noexcept;
	void stop() const noexcept;

	static thread_status& status();

	thread_ptr m_thread;
	const thread* m_parent = nullptr;
	std::unique_ptr<entry_t> m_entry;
	size_t m_stack_size = 0;
	mutable bool m_active = false;
};

} // namespace co

#include "co.ipp"

#endif // CO_HPP_INCLUDE_GUARD

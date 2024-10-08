`cppco`
=======

`cppco` is a C++11 wrapper library for the cooperative multithreading C library
[`libco`](https://github.com/higan-emu/libco), originally written by
[Byuu](https://en.wikipedia.org/wiki/Near_(programmer)) for
[higan](https://github.com/higan-emu/higan).

The "cooperative multithreading" `libco` implements could also be called a
[coroutine](https://en.wikipedia.org/wiki/Coroutine) or a
[fiber](https://en.wikipedia.org/wiki/Fiber_(computer_science)) implementation.

However, in order to preserve `libco`'s terminology this library will refer to
the concept defined by `libco` as "cothread" and introduces the class
`co::thread` to represent it.

Getting started
---------------

`cppco` is a header only library, so only the `include` directory needs to be
specified as an include path for the compiler, then include the header
`<co.hpp>`.

You also need to use the `libco` library. This repository defines a CMake file
to build and link it.

The simplest "Hello World!" program using `cppco` is the following:

```cpp
#include <iostream>
#include <co.hpp>

int main()
{
    using namespace std;

    // Create the cothread.
    auto cothread = co::thread([]()
    {
        // Printing on `cothread`
        cout << "Hello World!" << endl;

        // Switch back to the parent.
        co::active().get_parent().switch_to();
    });

    // Switch to `cothread`.
    cothread.switch_to();

    // Execution will resume here when `cothread` switches back to its parent.
    return 0;
}
```

Rationale
---------

`libco` provides a
[C API](https://github.com/higan-emu/libco/blob/master/doc/usage.md) which is
not safe to use directly in C++ as it doesn't handle C++ features such as
destructors or exceptions, leading to resource leaks and crashes unless special
care is made to avoid them.

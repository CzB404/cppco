#include <stdexcept>
#include <cstdio>
#include <libco.h>

cothread_t parent;

void entry()
{
	try
	{
		throw std::runtime_error("Catch me!");
	}
	catch (const std::exception& e)
	{
		std::printf("%s\n", e.what());
		co_switch(parent);
	}
}

int main()
{
	parent = co_active();
	auto cothread = co_create(262144 * sizeof(void*), &entry);
	co_switch(cothread);
	return 0;
}

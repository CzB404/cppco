#include <co.hpp>
#include <iostream>

int main()
{
    using namespace std;

    // Create the cothread.
    auto cothread = co::thread([&]() // []()
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

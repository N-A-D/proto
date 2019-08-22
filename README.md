## Proto

### Overview

A dependency free, single-header C++17 signals and slots implementation.

Signals and slots are a way of decoupling a sender and zero or more receivers.
A signal contains a collection of zero or more callback functions that are each
invoked whenever the signal has data to send.
 
I created the library as a tool for implementing the Observer pattern in a clean
way. I wanted a familiar implementation that I could easily place into a project
without having to configure anything.

### Summary
- Dependency free, single-header C++17 signals and slots library
- Learned about the [Observer pattern](https://en.wikipedia.org/wiki/Observer_pattern)

### TODO
Multithreading suppoort

### Usage

#### Connections
A `proto::signal` can connect any free function or lambda that matches its
invocation parameters.
```cpp
    void function() {
        std::cout << "Hello from free function!" << std::endl;    
    }

    // A signal that accepts functions with void return value
    // and an empty parameter list.
    proto::signal<void()> signal;

    // Connects a lambda to the signal.
    proto::connection conn0 = signal.connect([](){});

    // Connects a free function to the signal.
    proto::connection conn1 = signal.connect(function);

    signal();
```

Whenever a free function or lambda is connected to a signal, the signal returns
an instance of `proto::connection`, which represents the *connection* between
the signal and the slot (connected function). A function is disconnected from a signal
by invoking the returned `proto::connection`'s `proto::connection::close` member 
function.

Signals are also capable of connecting both const and non-const member functions. 
However, before a class instance connects one of its member functions to a signal, 
the class must itself must inherit from `proto::receiver`. The purpose of 
`proto::receiver` is to provide automatic management of signal connections. Any
derived class of `proto::receiver` is given access to the `proto::receiver::num_connections`
member function to query how many signal connections it has.

```cpp
    struct some_receiver : public proto::receiver {
        void function0() {
            std::cout << "Hello from non-const member function!" << std::endl;
        }

        void function1() const {
            std::cout << "Hello from const member function!" << std::endl;
        }
    };

    proto::signal<void()> signal;

    some_reciver receiver;

    signal.connect(&receiver, &some_receiver::function0);
    signal.connect(&receiver, &some_receiver::function1);
    receiver.num_connections(); // Returns 2

    signal();
```

**NOTE** Any class deriving from `proto::receiver` becomes non copyable and non movable.

#### Scoped connections

A `proto::scoped_connection` is just like a `proto::connection` except that it
closes the connection between slot and signal when it goes out of scope. This is useful
whenever you have to connect function(s) infrequently.

```cpp
    void some_rarely_invoked_function() {
        // logic...
    }

    proto::signal<void()> signal;
    if (some_rare_condition)
    {
        proto::scoped_connection = signal.connect(some_rarely_invoked_function);
        signal();
    }
```

#### Return value collection

Clients that require the output of slots can *collect* them from a signal by invoking the
`proto::signal::collect` member function. The function requires an output iterator to
a container. Moreover, the function raises a static assertion error if the slot return 
value is void.

```cpp
    int function0() {
        return std::rand();
    }
    
    int function1() {
        return std::rand();
    }

    int function2() {
        return std::rand();
    }
    
    proto::signal<int()> signal;
    signal.connect(function0);   
    signal.connect(function1);   
    signal.connect(function2);   

    // Output container
    std::vector<int> values;
    
    // Slot return values are 'collected' into the container
    signal.collect(std::back_inserter(values));

    std::cout << *std::max_element(values.begin(), values.end()) << std::endl;

```

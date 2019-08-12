## proto

### Overview

A single-header C++17 signals and slots implementation.

Signals and slots are a way of decoupling a sender and zero or more receivers.
A signal contains a collection of zero or more callback functions that are each
invoked whenever the signal has data to send.
 
I created the library as a tool for implementing the Observer pattern in a clean
way. I wanted a familiar implementation that I could easily place into a project
without having to configure anything.

### Summary
- Single-header C++17 signals and slots library
- I learned about the Observer pattern
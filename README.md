# RealBoy Monitor

This is a client program using the libemu IPC library to control
execution of emulators.
It offers a simple UI through ncurses.

This program is intended to be used alongside [RealBoy] and
[libemu] as a teaching device for introducing low-level and
systems-level programming.

Take a look [here] for more details on the purpose and goals of the
project.

## Installation

### Compiling from Source

Dependencies:

* meson (for building)
* ncurses
* libemu

Run these commands:

    meson setup build/
    ninja -C build/
    sudo ninja -C build/ install


[RealBoy]: https://github.com/sergio-gdr/realboy
[libemu]: https://github.com/sergio-gdr/libemu
[here]: https://raw.githubusercontent.com/sergio-gdr/realboy-book/refs/heads/main/book.txt

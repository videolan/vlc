In this directory you will find a Microsoft Visual Studio C++ 6 project
to build vlc.

You should be aware that this project should mainly be used for debugging vlc.
Most of the hardware accelerated plugins can't be built from MSVC as they are
programmed in assembly using the GCC asm syntax and MSVC doesn't understand
this.

Also you shouldn't expect this project to be working out of the box as you
need to configure it to include the GTK headers and libraries.
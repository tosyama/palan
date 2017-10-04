# Palan
Palan is aiming simpler, safer and more enjoyable programming language alternative C.
Current palan is very draft.

## Quick Start
### Environment:
* Ubuntu 16.04.2 LTS (64bit)
* g++ (gcc 5.4.0) or later
* GNU Make 4.1 or later
* curl
* git

### Build Palan Compiler:
```sh
$ sudo make package
$ make
```
Note: `make package` installs [Boost], [Bison], [Flex] and [Catch].

[Boost]: http://boost.org
[Bison]: https://www.gnu.org/software/bison/
[Flex]: https://github.com/westes/flex
[Catch]: https://github.com/philsquared/Catch 

### Coding:
```sh
$ vi helloworld.pa
```
```c
syscall 1: write();
write(1, "Hello World!\n", 13);
```

### Build and Run:
```sh
$ ./pac helloworld.pa -o a.out
$ ./a.out
```

## Basic Types
sbyte, byte  
int16, uint16 
int32, uint32  
int64, uint64  
  
char
string  
object  

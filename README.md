# Palan

## Quick Start
### Environment:
* Ubuntu 16.04.2 LTS
* g++ (gcc 5.4.0) or later
* GNU Make 4.1 or later
* curl
* git

### Build Palan Compiler:
```sh
$ sudo make package
$ make
```

### Coding:
```sh
$ vi helloworld.pa
```
```
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

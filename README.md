# Palan
[![Build Status](https://travis-ci.org/tosyama/palan.svg?branch=master)](https://travis-ci.org/tosyama/palan)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/8224c75046a04172a3798c29dd3aedd0)](https://www.codacy.com/app/tosyama/palan?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=tosyama/palan&amp;utm_campaign=Badge_Grade)
[![Coverage Status](https://img.shields.io/coveralls/github/tosyama/palan/master.svg)](https://coveralls.io/github/tosyama/palan?branch=master)  
Palan is aiming simpler, safer and more enjoyable programming language alternative C.
Current palan is very draft.

## Quick Start
### Environment
*   Ubuntu 14.04.5 LTS (x86-64) or later(18.04.1 LTS)
*   g++ (gcc 5.5.0/as,ld) or later(gcc 7.3.0)
*   GNU Make
*   curl
*   git

### Build Palan Compiler
```console
$ git clone --recursive https://github.com/tosyama/palan.git
$ sudo make package
$ make
```
Note: `make package` installs [Boost][boost], [Bison][bison], [Flex][flex] and [Catch][catch]. You might need to install libfl-dev.

### Write Code
```console
$ vi helloworld.pa
```
```go
ccall printf(...);
"Hello World!\n" -> printf();
```
Note: write "ccall int32 printf();" if the version 0.2 or less.

### Build and Run
```console
$ ./pac helloworld.pa -o a.out
$ ./a.out
Hello World!
```

## Reference
See wiki [Palan 0.2 Language Reference](https://github.com/tosyama/palan/wiki/Palan-0.2-Language-Reference).

[boost]: http://boost.org
[bison]: https://www.gnu.org/software/bison/
[flex]: https://github.com/westes/flex
[catch]: https://github.com/philsquared/Catch 

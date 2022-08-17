# What is Palan?
Palan is a programming language (compiler) that is developing little by little, aiming at a language that can be an alternative to C language, which is simpler and safer. 

Main added features of ver. 0.4 are object can be allocate stack or struct member directly. You can develop some practical programs by using Palan ver. 0.4. (e.g. Tetris on console)

Although it run faster than the gcc -O0 in many cases, because it compiles to the assembler and optimize performance by register allocation for integer.

# Design philosophy and policy
The aim of language is that makes both clarity and simplicity compatible. Also, although it is just sense, my feeling when programming by Palan is important.

# Sample program and explanation
Although Palan ver. 0.4 has limited language features, you can develop a simple quick sort program like below.

```
ccall printf(...);

// init data.
const N = 10;
[N]int16 data = [0,4,8,3,7,2,6,1,5,0];

printf("before:");
show(data);

func show([N]int16 data)
{
    i=0;
    while i<N {
        printf(" %d", data[i]);
        i+1 -> i;
    }
    printf("\n");
}

printf("after:");
data ->> quicksort(0, N-1)
    ->> data
    -> show();

func quicksort([N]int16 >>data, int32 left, right)
    -> [N]int16 data
{
    if left >= right { return }
    var mid, i = left, left +1;

    while i <= right {
        if data[i] < data[left] {
            mid++;
            data[mid], data[i] -> data[i], data[mid];
        }
        i++;
    }
    data[left], data[mid] -> data[mid], data[left];

    data ->> quicksort(left, mid-1)
        ->> quicksort(mid+1, right)
        ->> data;
}
```

You can compile with the pac command on console to create an executable file as shown below. When you run the executable file, the sequence of numbers before and after of sorting will be displayed.

```
$ bin/pac quicksort.pa -o a.out
linking: a.out
$ ./a.out
before: 0 4 8 3 7 2 6 1 5 0
after: 0 0 1 2 3 4 5 6 7 8
```

If you do not specify an option, it will be executed immediately after compilation.

```
$ bin/pac quicksort.pa
before: 0 4 8 3 7 2 6 1 5 0
after: 0 0 1 2 3 4 5 6 7 8
```

## Explanation of the program
The extension of Palan's file is *.pa*.
There is no start function like main, the program is basically executed in order from the top.

```
ccall printf(...);
```

In the Palan ver. 0.4, it links with the standard C library by default. By declaring with `ccall`, you can use preferred functions of the library. `...` represents a variable length argument.

```
// init data.
const N = 10;
```

Constant declaration of the number of elements of the array to be sorted. Constants are used anywhere within the scope. The values can be specified a literal or calculation between literals.
`//` is a comment, ignored until the end of the line.

```
[N]int16  data  =  [0,4,8,3,7,2,6,1,5,0];
```
  
Declaration and initialization of array variables to be sorted. int16 represents a 16-bit integer, and `[] ` represents an array. An array can be initialized by numerical values separated by `,` written in `[]`. In here, the array is initialized with the numbers want to be sorted.

```
printf("before:");
show(data);
```

Call `printf` of C standard library and display characters on the console. `show()` is a Palan function call that displays numbers in the array on the console. The definition of the show function is later. As long as it is within the scope, the order of describing function definitions and calls can be changed.

```
func show([N]int16 data)
{
```

It defines a function to display inside an array and a function to perform sorting.
Since a parameter defined as `[N]int16 data` pass the copied array to the argument, changing the array in the function has no effect on the array of the caller. 

```
i=0;
while i<N {
    printf(" %d", data[i]);
    i+1 -> i;
}
printf("\n");
```

`while` is a loop statement, and repeats statements in `{}` as long as condition `i<N` is true. `->` represents assignment, assigning the value on the left to the variable on the right. Although other languages often use `=` for assignment, in Palan `=` is only at initialization, and using `->` for assignment. This is one of characteristic grammar in Palan.

```
printf("after:");
data ->> quicksort(0, N-1)
    ->> data
    -> show();
```

Execute the sort by passing the data array in `quicksort()` , obtain and display the result. You can call a function by specified argument using `->` like the assignment. Also, assignments and function calls can be connected with `->`, and in Palan they are called chain call. `->>` moves ownership of the array. Variables that have passed ownership can not be used until obtain new ownership.

```
func quicksort([N]int16 >>data, int32 left, right)
    -> [N]int16 data
{
```

A parameter defined as `[N]int16 >>data` require ownership, so copying does not occur because the reference is passed. `-> [N]int16 data` is the definition of the return value, and when returning from the function it returns a reference to the variable `data`.

```
   if left >= right { return }
```

`if` statement executes the processes in {} only when the condition is true. Here, if left is more than right, the `return` statement ends the function and back to the caller.

The `;` at the end of the line can be omitted when the sentence is last in `{}`.

```
   data[mid], data[i] -> data[i], data[mid];
```

Multiple variables can be assigned by separating them with `,`. Here we use this multiple assignment to swap the elements of the array.

I will finish the explanation of the quick sort program so far. How was the programming of Palan?
# Environment
The Palan compiler pac works on the following environment.
- CPU: X86-64 Intel base CPU
- Memory: 2GB
- OS: Ubuntu 16.04.5 LTS (64bit) or later (18.04.1 LTS) 
- Library: g++ (gcc 5.5.0/as/ld) or later(gcc 7.3.0), libboost-program-options

# Installation
Palan ver. 0.4 does not provide a binary file for installation. You need to clone and build the source from github. Please read README.md for the build method.

# Command line reference
The Palan compiler (pac) is the command line tool on the console.
- Display help

```
$ pac -h
```
You can check command line options.

- Display version information

```
$ pac -v
```
- Compile and show assembly
```
$ pac -S foo.pa
```
The assembly code is output to standard output.

- Compile and create object file
```
$ pac -c foo.pa
```
*foo.o* is created in the current directory. The object file name is the file name excluding the extension of the source code + *.o*.  
Note: Object files can be made into executable files by linking with ld command. However, combining of multiple object files can not be supported by the current Palan.

- Compile and create executable file
```
$ pac -o foo foo.pa
```
In the above example, an object file *foo.o* and an executable file *foo* are generated.

- Compile and run immediately
```
$ pac foo.pa
```
Generate and execute the executable file named *a.out*. After execution, the object file and executable file are deleted.

# Comment
After `//` description, it becomes a comment. The description is ignored until the end of the line and is not regarded as a program.

```
// This is a comment.
```

# Primitive types
In Palan ver. 0.4, following integer types and floating point number types are available.  

|Type name|Description|Minimum|Maximum|
|:--------|:----------|:------|:------|
|`sbyte`|signed 8bit integer|-128|127|
|`byte`|unsigned 8bit integer|0|255|
|`int16`|signed 16bit integer|-32768|-32767|
|`uint16`|unsigned 16bit integer|0|65535|
|`int32`|signed 32bit integer|-2147483648|2147483647|
|`uint32`|unsigned 32bit integer|0|4294967295|
|`int64`|singed 64bit integer|-9223372036854775808|9223372036854775807|
|`uint64`|unsigned 64bit integer|0|18446744073709551615|
|`flo32`|32bit floating point number|(+-)1.175494e-38|(+-)3.402823E+38|
|`flo64`|64bit floating point number|(+-)2.225074e-308|(+-) 1.797693e+308|

# Variable

## Variable declaration
Variables must be declared in advance before they can be used.
Declare the variable by writing the variable name after the type name. Variables can be used in the scope (area enclosed by `{` and `}` or top level).

```
int32 i;     // Declaration of 32bit integer variable i. 
int16 x, y;  // Declaration of 16bit integer variable x and y. 
byte b, sbyte sb;  // Declaration of unsigned 8bit integer variable b and signed 8bit integer sb. 
```
You can initialize variables with with` =` in declaration. To declare  multiple variable, write an initial values on the right side of `=` with separated by `,`.
```
int32 i = 0;
int16 x, y = 3, 4;
byte b, sbyte sb = 1u, -1;

x + y -> i;  // i is result of 3+4
```

## Type inference
At initializing the variable declaration, you can specify the type as `var`, it make the type the same as the right side. If you declare one variable, you can also omit `var`.

```
int32 i = 10;
var i2 = i;	// i2 is int32.
i3 = 10;	// i3 is int64.
f = 1.23;	// f is flo64.
var x, y = 1.0, i;	// x and y are flo64.
var xx, var yy = 1.0, i;	// xx is flo64, yy is int32.
```

## Array variable
Fixed-length arrays is available . Array variables must be declared before they can be used. Describe the number of elements in `[]`. You can also declare multidimensional arrays by separating them with `,`. The number of elements specified must be a fixed value, and it is limited to integer literals, constants or computations between them.
When using, specify an index from 0 to the number of elements-1 in `[]`. An preferred expression can be specified as an index, but do not exceed the range. If the array index beyond the range, it will cause of a crash.

```
[10]int32 a1, a2; // Declaration  array variable of 10 32bit integers a1, a2.
[3,4]int32 m;     //3x4 multidimensional array. 3 array items that has 4 int32 items.

// Access array item.
1 -> a1[0]; 10 -> a2[9];
12 -> m[2,3];
```

## Array of array
Array of array can also be used by connecting `[]`. Unlike multidimensional arrays, normal arrays and array of their reference are generated.
```
[10][3,4]int32 arrayOfMat; // 
Array of 10 elements with [3,4]int32 as elements

123 -> arrayOfMat[9][2,3];

[3,4]int32 m = arrayOfMat[9];  // Treat as 2 dimension array.
```

## Array description
Array description allow you to initialize an array or create an anonymous array. Array description is a series of values separated by `,` and enclosed by `[]`. You can not assign values to an array description, or obtain ownership from it.

Multidimensional arrays can be represented by placing array descriptions side by side. You can also write in nested form.

```
[3]int32 a = [1,2,3]; // Initialize a.
i = 10;
[i,i+1,i+2]->a; // Assign an array to a.

[2,3]int32 a2 = [1,2,3][4,5,6]; // Multidimensional array.
[2,3]int32 a3 = [[1,2,3],[4,5,6]]; // Nested form.

[2][3]int32 aa = [1,2,3][4,5,6]; // Array of array.
```

Basically, the type of array description becomes same as the assignment target. In type inference, it is regarded as a single or multidimensional array. It means not an array of array.

```
a = [1,2,3][4,5,6];	// a is [2,3]int64
```

## Number of elements omission
If you omit the number of elements when initializing array variables, it will be the same as the number of elements in the value on the right. However, the number of dimensions must match.

```
[]int32 a = [1,2,3];    // a is [3]int32.
[,]flo32 f = [1,2][3,4];    // f is [2,2]flo32.
[,3]int32 a2 = [1,2,3][4,5,6];  // Partial omission is also possible.
[][]int32 aa = [1,2,3][4,5,6];  // Array of array.
```

## Direct allocation of object elements
In default, objects such as arrays or structures are placed at heap area, and array elements keep referenc of them.
You can place the opject at element area directy by prefixing the element's type name with `#`.

```
[2]#[3]int32 a;   // The Memory allocation is same as [2,3]int32.
```

# Structure
A structure is used to collect multiple data as one variable. To use the structure, you need to define the structure by describing the name and the members in `{}` after `type`.

```
type User {
    int64 id;
    [80]byte name;
    int16 age;
};

User userA;  // The declaration of User variable userA.
```
## Direct allocation of object members
In default, objects such as arrays or structures are placed at heap area, and structure members keep referenc of them.
You can place the opject at member area directy by prefixing the member's type name with `#`.

```
type User {
	int64 id;
	#[80]byte name;	// This object is placed at structure area.
	int16 age;
};
```

## Access to member
You can access the structure member by writing `.` and member name after the variable name.

```
User userA;

// Assign to the member of structure.
101 -> userA.id;
"Alice" -> userA.name;
12 -> userA.age;

// Get and display the values of member.
printf("%d: %s %d\n", userA.id, userA.name, userA.age); // 101: Alice 12
```

## Literal description
You can also use array description for structure assignment and initialization.
```
User userA = [101, "Alice", 12];
User userB;
[102, "Bob", 13] -> userB;
```

# Allocation for stack area
In default, object variables such as arrays or structures are placed at heap area.
This allocation process possible to be bottle neck of performance in cases.
You can optimaize performance by placing the objects on stack area by prefixing type with `#`.
However you should be careful for stack overflow.

```
#[3]int32 a = [1,2,3];
#User u = [101, "Alice", 12];
```


# Type Alias
You can define alias for type. It become easier to understand usage of the type. After `type`, write new name with `=` and the type name you want to alias.

```
type ID = int64;
type Point3D = [3]flo64;

ID id = 10;
Point3D p = [1.2, 1.3, 4.5];
```

# Read-only reference

When passing arrays and structures to functions, etc., it has costs of to allocating the memory and copying data. Using a read-only reference can improve performance because only pass the address of the object. It can also be used as an alternative to C language pointers. Add `@` in front of type name for read-only reference declaration. The value of read-only reference cannot be overwritten after initialization.

```
type struct_tm { int32 tm_sec; };

ccall strlen(@[?]byte str) -> int32;
ccall localtime(@uint64 t) -> @struct_tm;

@[?]byte str = "read only string";
len = strlen(str);
uint64 t = time(0);
@struct_tm tm = localtime(t);
```

# Declaration of type name only
You can also just declare the type name. Variables cannot be directly declared and used, but it's useful for handling C library type by pointer.

```
type FILE;   // Declaration of type name only.
ccall fpoen(@[?]byte filename, mode) -> FILE;
ccall fprintf(@FILE stream, @[?]byte format, ...) -> int32;
ccall fclose(FILE >>stream);

FILE f <<= fopen("file.txt", "w");
f->fprintf("Hello\n");
fclose(f>>);
```

# Expression
The syntax that returns a value is called an expression. Expressions can be concatenated until separated by `;` to form a statement.

## Literal
Fixed values can be written directly in the source code.

|Name|Description|Example|
|:--|:--|:--|
|Integer literal|Fixed integer value. It is treated as signed 64 bit integer. It is written in decimal.|`-12345` `12345`|
|Unsigned integer literal|Fixed value of positive integer. It is treated as an unsigned 64 bit integer. Add u after the decimal number.|`12345u`|
|Floating point literal|Fixed decimal fraction value. It is treated as a 64-bit floating point number. Separate integer part and fractional part by `.` After that, you can also write the exponent that starts with `e`.|`123.4` `1.234e2`|
|String literal|A fixed value for the string. It can used at assignment for byte array, arguments of function and constants. Escape sequences are available.|`Hello World!\n`
|Array literal|A fixed array in which all elements are composed of literals or constants. Unlike regular arrays, they can also be used as  constants or as default values of arguments.|`[1,2,3]`|

## Variable
When you write the declared variable name, the value of that variable is returned. It can be used for substitution, arithmetic operation, function argument, etc.

## Assignment 
Using the assignment operator `->`, you can store the calculation results in variables. Values are copied from the left side to the right side. It can also be assigned to multiple variables.

```
121 + 2 -> i; // Assign 123 to i
i + 3 -> i; // Assign 126 to i

123, 456 -> i, j; // Multiple assignments. Assign 123 for i and 456 for j
f () -> i, j; // Function f returns multiple values, each of which can be assigned.
```
You can also use multiple assignment to swap values.

```
int32 i, j = 1, 2;
i,j -> j,i;  // Swap values. it become i: 2, j: 1
```

If you use an assignment operator for an array variable or array of array, all elements are copied. (Deep copy)

```
int32[10] a, b;
...
a -> b;   // Deep copy
```

## Move ownership
Ownership can be moved for objects such as arrays. Use the move operator `->>` to move ownership. To move to the second and subsequent variables, add `>>` to the destination variable. Since you pass only the reference to the destination variable without copying the contents of the array, you can pass the value faster than assignment.
The variable of ownership transfer source can not be used until ownership is acquired after moving. If you use it, it may cause a crash.

```
int32 [10] a, b, c, d;
...
a ->> b; // Move ownership of the contents of the array from a to b
// 2-> a [2]; // Unusable

b, c -> a, >> d; // Moves ownership from c to d at the same time "a" is regains ownership

int32 [10] e <<= a; // Move ownership and initialize
```

## Arithmetic operation
The following arithmetic operations can be performed on integers and floating point numbers.

|Operator|Description|Example|
|:-------|:----------|:------|
|`+`|Perform addition.|`1 + 2 -> i; // 3`|
|`-`|Perform subtraction.|`4 - 3 -> i; // 1`|
|`*`|Perform multiplication.|`3 * 2 -> i; // 6`|
|`/`|Perform the quotient of division.The decimal point is truncated.|`10 / 3 -> i; // 3`|
|`%`|Perform the remainder of division.|`10 % 3 -> i; // 1`|
|`-`|Returns negative number.|`-i -> i;`|

Operators have priority. `*`, `/`, `%` have higher priority than `+`, `-`.
Basically the calculation of literals are finished in compile time.

## Comparison operation
You can compare integers or floating point numbers using comparison operators. If the comparison result is true, it is 1. If false, it is 0. You can use it wherever an expression can be used, but there are many opportunities to use it as a condition of an if statement or while statement.  
You must always be aware of rounding errors in floating point comparisons.

|Operator|Name|Description|Example|
|:-------|:---|:----------|:------|
|`==`|Equal|Return 1 if the equal left and right values are equal, otherwise return 0 .|`if a == 10 {`...`}`|
|`!=`|Not equal|Return 1 if the left and right values are not equal, return 0 if equal to.|`if a! = 10 {`...`}`|
|`>`|Greater than|Return 1 if the left value is greater than the right value, return 0 if it is equal to or less than.|`if a> 10 {`...`}`|
|`<`|Less than|Return 1 if the left value is less than the right value, return 0 if it is equal or larger.|`if a <10 {`...`}`|
|`>=`|Greater than equal|Return 1 if the left value is greater than or equal to the right value, otherwise return 0.|`if a> = 10 {`...`}`|
|`<=`|Less than equal|Return 1 if the left value is less than or equal to the right value, otherwise return 0.|`if a <= 10 {`...`}`|

## Increment/decrement operation
You can increment/decrement a variable by using `++`/`--` operator.
You cannot use with another expressions because they are statements.

|Operator|Name|Description|Example|
|:-------|:---|:----------|:------|
|`++`|Increment|Increase value of variable by 1.|`i++;`|
|`--`|Decrement|Decrease value of variable by 1.|`i--;`|

## Mixed integers and floating points number operations
When performing operations that mix integers and floating point numbers, calculations and comparisons are performed after promoting integers to 64-bit floating point numbers.

## Integer promotion
Integer types are promoted to 64-bit integers and computations and comparisons are done. Upper bits of the result are truncated at assignment. For signed integer and unsigned integer computation, unsigned integers are computed by treating them as signed integers. Note: Specifications are different from C language.

## Conditional operation 
The following condition operator can be used to judge complicated conditions.

|Operator|Name|Description|Example|
|:-------|:---|:----------|:------|
|`&&`|And|Return 1 if the left value is not 0 and the right value is not 0, otherwise return 0. If the left value is 0, the expression for the right value is not evaluated.|`if a>0 && a <= 10 {`...`}`|
|<code>&#124;&#124;</code>|Or|Return 1 if the left value is not 0 or the right value is not 0, otherwise return 0. If the left value is 1, the expression for the right value is not evaluated.|`if a <0 || a >= 10 {`...`}`|
|`!`|Not|Return 1 if the expression is 0, Return 0 if it is not 0.|`if !(a<0) {`...`}`|

## Function call
You can call functions in the scope defined and declared by specifying arguments in function names and `()`. If it is within the scope, there is no problem even if there is a function definition after calling. Some arguments require ownership. In that case you need to add `>>` after the argument to clarify that the variable will lose ownership. The return value can be received using the assignment operator or the move operator.

Receiving the return value is not mandatory. The memory used by the return value not received is automatically released.

```
int32 x, y;
int32 [10] a;
int32 result;

// Example of function call
foo (); // no argument, do not receive return value.
foo (x, y) -> result; // There is an argument, there is a return value.
foo (a >>, x, y) - >> a, result; // Argument for moving ownership, multiple return values
foo (a) -> a; // Passing by content copy

func foo () {...} // The function definition can be later
func foo (int32 x, y) -> int32 {...}
func foo ([10]int32 >>a, int32 x, y) -> [10]int32 a {...}
func foo ([10]int32 a) {...}
...
```

### Overload
Palan allows you to define functions with the same function name but different arguments (overload). If there is an argument whose type and number exactly match at the time of invocation, that function will be called. If the type does not match exactly, if there is only one matching function after implicit type conversion such as integer etc., that function will be called. If two or more functions are matched, a compile error occurs.

###  Arguments omission
  
If default values are set at arguments in function definition, you can omit the arguments. If the argument is omitted, the default value is used.

```
// A function that can omit x and y arguments.
func foo(int32 x=1, y=2) {  }

foo(3,4);   // normal function call.
foo();  // foo(1,2)
foo(3); // foo(3,2)
foo(,3);    // foo(1,3)
```

## Chain call
You can call function by specifying arguments using assignment or Move operator, and you can apply return value of function to argument of next function call.  

Compared with ordinary function calls, you can describe functions sequentially from left to right, so it is easier to express the flow of data and processing.  

The left value of the assignment operator or the return value of the function are allocated in order from the beginning of the argument and the function is called.

```
int32 a, b;
add(a, 3) -> b; // normal function call
a -> add(3) -> b; // Argument specification with one assignment
a, 3 -> add() -> b; // Argument specification with 2 assignments
add(add(a, 3), 9) -> b; // normal function nested call
a -> add(3) -> add(9) -> b; // Pass the return value of the function to the next function

func add (int32 a, b) -> int32 c
{
	// contents
}
```

The Move operator applies only to the first argument. Palan ver 0.2 do not provide a method to apply to the second and subsequent arguments, so in that case we need to use normal function calls.

```
byte[10] s1, s2;

s1 ->> mix(>>s2) ->> s1, >>s2;
// s1, s2 ->> mix(); // NG: The second argument is considered an assignment

func mix(byte[10] >>a, >>b)
   -> byte[10] a, b
{...}
```

## Constant
By using the const keyword, you can define numerical values or array commonly used in programs as constants. You can specify literal or calculations between literals. Besides using it in expressions, it can also be used in the number of elements of array type declaration.

```
const M, N = 3, 4;
const FORMAT = "bar:% d";
const LEN = M * N; // computing literals
int32 l, z = LEN, Z; // expression. Z is before definition but can be used

func foo (int32 [M, N] m) // declaration of fixed array
{
    const FORMAT = "foo:% d"; // overwriting a constant
    printf (FORMAT, LEN); // Constant LEN defined in the parent block can be used
}
const Z = LEN;
```

Constants can be used before definition if they are within the scope. However, if you use another constant in the definition of a constant, you must define that constant beforehand. In a child block, defining constants of the same name in the parent block overwrites them.

# Control statement
By using control statements such as `if` and `while`, you can control flow by branching and repeating processing.

## Top level
The code written outside the function definition is called the top level. The program runs sequentially from the top level, and it ends when it comes to the end of the top level. A return statement can not be used at the top level. To terminate your program, use the exit function of the standard C library.

## Block
The area enclosed by `{}` is a block. By using blocks you can limit variables, functions, constants and valid areas of that name. Normally, variables declared within a block can be used within that block and child block, not outside the block.  

When processing goes out of the block, all the memory used by the variable declared in the block is released, so if you use the block appropriately, you can save memory.

## If statement
When you want to branch processing depending on conditions, use the `if` statement. The `if` evaluates the conditional expression. If the conditional expression is true (other than 0), the immediately following block is executed.

```
if a == 2 { // Describe conditional expression after if 
    // Execute when conditional expression is true (a is 2)
}
```
To perform another processing when the conditional expression is false (0), put an `else {}`  after the `if {}` .

```
if a == 2 {
     // Execute when conditional expression is true (a is 2)
} else {
     // Execute when the conditional expression is false (a is other than 2)
}
```

Multiple conditional branches can be constructed by connecting `else if {}`.

```
if a == 2 {
     // Execute when conditional expression is true (a is 2)
} else if a == 3 {
     // Execute when conditional expression is true (a is 3)
} else {
     // Execute when all conditional expressions are false (a is other than 2 and 3)
}
```

## While statement 
To repeat the process depending on conditions, use the `while` statement. As long as the conditional statement is true (other than 0), the program will continue to execute repeatedly the following block. It is likely to cause CPU busy due to infinite loop, so coding of conditional expressions needs to be particularly careful.

```
int32 i = 0;
while i <10 {
// Repeat as long as the conditional expression is true (i is smaller than 10)
    printf ("% d", i);
    i + 1 -> i;
}
```

## Loop interruption and skipping
You can use the `break` statement to break the `while` iteration and exit the block. You can also use `continue` to skip the rest and continue the next iteration. Usually, these two statements are used in combination with the `if` statement.

```
i=0;
while i<10 {
    if i==5 { break }
    if i==3 { 4->i; continue }
    printf("%d",i);
    i+1 -> i;
}
// output is "0124"
```

# Function
A function is a collection of processes. It takes an input as an argument, performs processing, then returns the output as a return value. In Palan's function definition, input and output can be expressed clearly.

## Function definition
To define a function, describe the `func` keyword and function name, and then declare parameters to be received from the caller in `()`. If the function returns a value, declare the return value after `->`. Write the processes to be performed in the block after the function declaration.

```
// Function definition with no argument, no return value
func foo ()
{    // Describe processing here
}

// Function definition with argument and no return value
func foo (int32 a, b, int16 c)
{
    printf ("a:%d, b:%d c:%d \ n", a, b, c); // The parameters  can be handled as a normal variable
}

// Function definition with no argument and return value
func bar () -> int 32 a, b, int16 c
{
   // Return value can also be treated as a variable.
   / / The value at the end of the function is returned to the caller.
   1 -> a; 2-> b; 3-> c;
}

// Function definition with argument and return value
// The same variable name a can be specified for argument and return value.
func bar (int32 a, b) -> int32 a, c
{
    1 -> c;
}
```

## Parameter
The value of the argument specified at the function call is passed to the parameter.
In the parameter, type name and variable name are described. To declare multiple parameter, use `,` to connect them. If the previous argument and type are the same, the type name can be omitted.

Normally, the value of the variable of the caller is copied to the parameter. Even if you change the value of a parameter, the value of the variable of the caller does not change.

```
call int32 printf();

func foo(int32 i, j, sbyte b, int32[10] a) // The part enclosed by () is parameter declarations
{
   printf("before: i=%d, a[3]=%dÂĽn", i, a[3]);
   8,9 -> i, j;
   33 -> b;
   88 -> a[3];
   printf("after: i=%d, a[3]=%dÂĽn", i, a[3]);
}

// Variable specified in argument
int32 ii, jj = 3, 4;
sbyte bb = 1;
int32[10] arr;
99 -> arr[3];

foo(ii, jj, bb, arr);  // Call function
printf("original: ii=%dÂĽn, arr[3]=%d", ii, arr[3]);
```

The output is as follows. The original variable has not been changed.

```
before: i=3, a[3]=99
after: i=8, a[3]=88
original: ii=3, arr[3]=99
```

If `>>` is specified before the parameter name, it requests ownership of the variable of the caller, not a copy. Because reference is passed, deep copy does not occur and delivery can be done at high speed. As it is, you can not use the caller variable, so declare the return value that normally returns the changed value to the caller.

Arrangement of parameters that setting the main data place to be used for other functions as the first argument, and leaving the options and so on behind, makes it easier to use with chain calls.

## Default argument
For arguments, you can specify a default value with =. Using default values can reduce the burden of caller arguments. However, overloading should be carefully designed as it causes ambiguity and causes compilation errors. As default values, only fixed values such as literals and constants can be used.

```
// Default values are specified for j, a
func foo(int32 i, j = 3, int32[2] a = [1,2])
{
}
```

## Return value
To return the processing result to the caller, use the return value.

If the previous value and type are the same, the type name can be omitted.

```
func foo() -> int32 i, j, int16 l
{
  1,2,3 -> i, j, l;
}
```

The value when the function exit of the variable specified as the return value is returned to the caller.
```
"%d, %d, %d\n", foo() -> printf(); // => 1, 2, 3
```

You can declare the same variables as parameters as return values. In that case, the value of the argument of the caller is assigned to the variable and it is used as it is for the return value.

```
func foo(int32 j) -> int32 i, j, int16 l // j used for both parameter and return value
{
  j+1 -> j;
  1,2 -> i, l;
}

"%d, %d, %d\n", foo(10) -> printf(); // => 1, 11, 2
```

You can specify only type and not write variable name. In that case, all return values must be type declarations only. When returning the function, it is necessary to describe all the return value with the `return` statement.

```
func foo() -> int32, int32, int16
{
   return 1,2,3;  // must
}

"%d, %d, %d\n", foo() -> printf(); // => 1, 2, 3
```

## Return statement
Use the `return` statement to terminate processing anywhere in the function.  

At the `return` , you can specify the value to be returned to the caller by an expression, but if the variable name is specified in the return value declaration, it is the value of that variable, so the value to return is unnecessary.  

If the declaration of return value is only type name, return value is necessary.
The `return` can not be used in top level processing. To terminate processing, use the `exit()` function of the standard C library.

## Nested function
Function definitions can be nested. Nested functions can only be used within the defined scope (enclosed in `{}`). It can be used when you want to define a function that you do not want to be called from others, such as when dividing a large process into multiple functions.

```
mainfunc(); // OK
// subfunc() // NG

func mainfunc()
{
    subfunc();
    subfunc2();

    func subfunc() {
    }
    func subfunc2() {
    }
}
```

# C function
The program of Palan is linked with the standard C library. By writing `ccall` declaration, you can use functions of standard C library such as `printf`. It is necessary to replace C language type, `int` / `long long` etc. to Palan types `int32`, `int64` etc.. The pointer should be replaced with a reference or output argument. The declaration of C language function basically follows the format below.

`ccall [function name]([input parameters]=>[output parameters]) -> return type : library name`

Since reading C language header files is not supported, you need to write all necessary definitions such as functions, constants and structures by yourself.

## Return value
For the return value, specify a type name after `->`. If there is no return value, the `->` is unnecessary.

```
ccall strlen(@[?]byte str) -> int32;   // has return value.
ccall exit(int32 status);  // no return value.
```

## Using library
The function that requrie linking the library must specify the library name. Howerver, it's not necessary to specify every time because it will be linked when used in either function declaration.

```
ccall sqrt(flo64 x) -> flo64: m; // Use math library
ccall pow(flo64 x, y) -> flo64;
```

## Variable arguments
The variable length argument of the C language function is expressed by `...`. A typical example is the `printf` function.
```
ccall printf(@[?]byte format, ...) -> int32;
```

## Output arguments and placeholders
In C language, input and output parameters were distinguished by the presence of `const` keyword.
In Palan, it is clear that before `=>` are input parameters, and after `=>` is output parameters.
In the case that used for both input and output, it is regarded as output.

If you specify an object such as an array as the output argument, the pointer of array will be passed.
To receive the address of the memory allocated at the callee function (pointer** of the pointer in C language), the Move operator `>>` can be used.

```
// Example of scanf
ccall scanf(@[?]byte format => ...) -> int32;

byte[32] str;
int32 num;
scanf("%31s %d" => str, num);

// Example of sqlite
type sqlite3;
ccall sqlite3_open(@[?]byte filename => sqlite3 db>>) -> int32:sqlite3;
ccall sqlite3_close(sqlite3 >>db) -> int32;

sqlite3 db;
sqlite3_open(":memory:" =>> db);
db->>sqlite3_close();

```
In the case that the function whose output arguments are defined before the input, such as sprintf, then you can use the placeholder `@`.
Arguments after `=>` are treated as specified in order at the placeholder position.

```
// original position of the  dst is placeholder @
ccall sprintf(@, @[?]byte src, ... => [?]byte dst) -> int32;

[10]byte str;
sprintf("%d", 123 => str);
```

## Writable reference
It may be inappropriate to use Move for output arguments that return pointers. In that case you can use the writable reference `@!`.
Writable references should be handled carefully, because it have no access restrictions.

```
ccall strtol(@[?]byte s, @, int32 base => @![?]byte endptr -> int32;

@![?]byte rest_str;
int32 l = strtol("1234 547", 10 => rest_str);
```

## External global variable
You can use global variables such as `stdout` that are used in C language libraries. Declare a global variable with an `extern`.

```
type FILE;
extern @FILE stderr;

stderr->fprintf("Error occured!\n");
```

## System call
Linux (64 bit) system call can be used by `syscall` declaration. A system call can specify an arbitrary name and can be used like a function. At declaration, declare the type of system call number and return value and the corresponding function name.  

There is no parameter declaration. The calling argument is passed to the function as is.

```
syscall 1: int64 write();
syscall 60: exit();
```

Note: Although it is possible to declare and use for the program termination system call `exit`, however in the case of using `printf`, etc, it is better to use the `exit()` function of the standard C library to flush buffer.

# Memory management
Palan has no memory reservation like `new`. Arrays are automatically allocated in the heap area at the time of declaration.

There is no GC or `delete`. The area secured in the heap area is freed when a variable with ownership of the variable goes out of scope or another ownership is moved.

Memory release is done instantaneously, unlike GC. Compared to GC, the processing speed may be reduced by the release processing, but it is always possible to maintain stable performance and minimum memory, and there is nothing that suddenly stops processing with sudden GC.

Arrays allocated at the top level are released from the OS at the same time at the end of the program so that individual release processing is not performed.


Palan Abstract Syntax Tree Specification
========================================

ver 0.3.0a

\* - Required  
\# - Required if first item of list 

Root
----
*   ast\* - AST model
*   errs - Error message list
*   files - Source file list

AST
---
*   funcs - Function definition list
*   stmts - Statement list for top level code

Error Message
-------------
*   msg\* - Error message string
*   loc - Location integer array

Source File
-----------
*   id\* - Source file ID integer
*   name\* - Source file name string
*   dir - Directory path string

Function
--------
*   id\* - Function ID integer

*   name\* - Function name string

*   params\* - Parameter list

*   func-type\* - Function type string: "palan" "ccall" "syscall"
    1.  palan - Palan function definition
        *   rets\* - Return value list
        *   impl\* - Block model for function implemantation

    2.  ccall - C function prototype
        *   ret-type - Return value type string (deprecated)
        *   ret - Return value

    3.  syscall - System call prototype
        *   call-id\* - Integer to set %rax
        *   ret-type - Return value type string (deprecated)
        *   ret - Return value

*   loc - Location integer array

Parameter
---------
*   name\* - Parameter name string
*   var-type\# - Variable type list
*   pass-by\* - Passing way string: "copy", "move", "ro-ref", "read":for variable argument, "write"
*   default-val - Default value expression
*   loc - Location integer array

Parameter (Variable length)
---------
*   name\* - string "..."
*   loc - Location integer array

Return value
------------
1.  Anonymous return value
    *   var-type\* - Variable type list

2.  Standard return value
    *   var-type - Variable type list
    *   name\* - Return value name string

*   loc - Location integer array

Variable Type
-------------
*   name\* - Type name string: "[]" any
    1.  [] - Fixed size array type
        *   sizes\* - Expression list (lit-int:-1 - size inference)

    2.  any - Specified variable type name

Block
------
*   stmts\* - Statement list in the block
*   loc - Location integer array

Statement
---------
*   stmt-type\* - Statement type string:
    "exp" "block" "var-init" "return" "while" "if" "func-def"
    1.  exp - Expression statement
        *   exp\* - Expression model

    2.  block - Block statement
        *   block\* - Block model

    3.  var-init - Variable declaration statement
        *   vars\* - Variable destination list
        *   inits - Variable initialize expression list

    4.  const - Constant value declaration statement
        *   names\* - Constant name string list
        *   values\* - Constant value expression list

    5.  return - Return value statement
        *   ret-vals - Return value expression list

    6.  while - While loop statement
        *   cond\* - Condition expression
        *   block\* - Loop block model

    7.  break - Break statement
        *   label - Loop label to break

    8.  continue - Continue statement
        *   label - Loop label to continue

    9.  if - If statement
        *   cond\* - Condition expression
        *   block\* - If block model
        *   else - Else statement (block/if)

    10. func-def - Function definition link
        *   id - Function id integer

*   loc - Location integer array

Expression
----------
*   exp-type\* - Expression type string:
    "lit-int" "lit-uint" "lit-str" "var" "array-val"
    "asgn" "func-call" "chain-call"
    birary operator ("+" "-" "*" "/" "%" "==" "!=" "<" ">" "<=" ">=" "&&" "||")
    unary operator ("uminus" "not")
    1.  lit-int - Integer(64bit) literal
        *   val\* - Integer

    2.  lit-uint - Unsigned integer(64bit) literal
        *   val\* - Unsigned integer

    3.  lit-float - Floating point number(64bit) literal
        *   val\* - Floating point number

    4.  lit-str - String literal
        *   val\* - String value

    5.  var - Variable/Constant expression
        *   base-var\* - Base variable/ constant vlue name string
        *   opes - Modified operator list

    6.  array-val - Array expression
        *   vals\* - Item value expression list

    7.  asgn - Assignment expression
        *   src-exps\* - Source expression list
        *   dst-vals\* - Destination value list

    8.  func-call - Function call expression
        *   func-name\* - Function name to call
        *   args\* - Argument list
        *   out-args\* - Output argument list

    9.  chain-call - Chain function call expression
        *   func-name\* - Function call expression 
        *   in-args\* - Input argument list
        *   args\* - Rest of argument list
        *   out-args\* - Output argument list

    10. binary operator
        *   lval\* -	Left value expression
        *   rval\* -	Right value expression

    11. unary operator
        *   val\* -	Value expression

*   loc - Location integer array

Destination Value
------------------
*   base-var\* - Base variable name string
*   opes - Modified operator list
*   move - Move ownership flag boolean
*   loc - Location integer array

Modified Operator
-----------------
*   ope-type\* - Operator type string: "index"
    1.  index - Numerical index
        *   indexes - Index expression list

Variable Declaration
--------------------
*   name\* - Variable name string
*   var-type\# - Variable type list (nothing: use pre defined var type, empty: type inference)
*   move - Move ownership flag boolean
*   loc - Location integer array

Argument
--------
*   exp\* - Argument expression
*   move - Move ownership flag boolean

Location Array
--------------
*   0\* - Source file ID integer
*   1\* - Begin line integer
*   2\* - Begin column integer
*   3\* - End line integer
*   4\* - End column integer

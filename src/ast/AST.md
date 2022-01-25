Palan Abstract Syntax Tree Specification
========================================

ver 0.4.0a

\* - Required  
\# - Required if first item of list 

Root
----
*   ast\* - AST model
*   errs - Error message list
*   files - Source file list
*   libs - Library list

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

Library
-----------
*   name\* - Library name

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
        *   ret - Return value

    3.  syscall - System call prototype
        *   call-id\* - Integer to set %rax
        *   ret - Return value

*   loc - Location integer array

Parameter
---------
*   name\* - Parameter name string
*   var-type\# - Variable type list
*   pass-by\* - Passing way string: "copy", "move", "read":for variable argument, "write", "write-ref"
*   io\* - Input/output string: "in", "out"
*   moveto\* - Move owner option: "none", "callee", "caller"
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
        *   sizes\* - Expression list ({exp-type:unknown,info:""} - size inference)

    2.  any - Specified variable type name

*   mode\* - mode string(access right, identity, allocation):
    1.  --- - dafault: depend on type
    2.  rir - read-only reference (@)
    3.  wcr - writable reference (@!)
    4.  wis - writable stack/direct allocation (#)

*   loc - Location integer array

Block
------
*   stmts\* - Statement list in the block
*   loc - Location integer array

Statement
---------
*   stmt-type\* - Statement type string:
    "exp" "block" "var-init" "return" "while" "if"
	"func-def" "extern-var" "ope-asgn"
    1.  exp - Expression statement
        *   exp\* - Expression model

    2.  block - Block statement
        *   block\* - Block model

    3.  var-init - Variable declaration statement
        *   vars\* - Variable declaration list
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

    11. type-def - Type definition
        *   type\* - Type definition type string: "obj-ref", "struct", "alias"
        *   name\* - Type name string
        *   memebers - Struct member list
        *   var-type\* - Original variable type list to alias

    12. extern-var - Import extern global variables
        *   var-type\* - Variable type list
        *   names\* - variable name list

    13. ope-asgn - Operational assignment
        *   dst-val\* - Destination value
        *   ope\* - Operator string : "+"
        *   rval\* - Right value expression

*   loc - Location integer array

Expression
----------
*   exp-type\* - Expression type string:
    "lit-int" "lit-uint" "lit-str" "var" "array-val"
    "asgn" "func-call" "chain-call" "token"
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
        *   var-name - Variable / constant value name string

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

    10. member - struct member expression
        *   base-exp - Base variable expression
        *   member-name - Struct member name string

    11. index - Container item expression
        *   base-exp - Base variable expression
        *   indexes - Index expression list

    12. binary operator
        *   lval\* -	Left value expression
        *   rval\* -	Right value expression

    13. unary operator
        *   val\* -	Value expression

	14. token - Token
	    *   info\* - token information

*   arg-option - Argument option string: "none", "move-owner", "get-owner", "writable-ref"

*   loc - Location integer array

Destination Value
------------------
*   exp - Destination value expression
*   arg-option\* - Option string: "none", "move-owner",  "writable-ref"
*   get-owner\* - get ownership flag boolean
*   loc - Location integer array

Variable Declaration
--------------------
*   name\* - Variable name string
*   var-type\# - Variable type list (nothing: use pre defined var type, empty: type inference)
*   get-owner - get ownership flag boolean
*   loc - Location integer array

Struct member
-------------
*   type\# - Variable type list
*   name\* - Variable name string

Location Array
--------------
*   0\* - Source file ID integer
*   1\* - Begin line integer
*   2\* - Begin column integer
*   3\* - End line integer
*   4\* - End column integer

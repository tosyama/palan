Palan Abstract Syntax Tree Specification
========================================

ver 0.0.0  

\* - Required  
\# - Required if first item of list 

Root
----
* ast\* - AST model
* errs	- Error message list
* files - Source file list

AST
---
* protos - Function prototype list
* funcs - Function definition list
* stmts - Statement list for top level code

Error Message
-------------
* msg\* - Error message string
* loc - Location integer array

Source File
-----------
* id\* - Source file ID integer
* name\* - Source file name string
* dir - Directory path string

Function Prototype
------------------
* func-type\* - Function type string: "palan" "ccall" "syscall"
	1. palan - Palan function prototype
		* name\* - Function name string
		* params\* - Parameter list
		* rets\* - Return value list
	2. ccall - C function prototype
		* name\* - Function name string
		* ret-type - Return value type string
		* params\* - Parameter list
	3. syscall - System call prototype
		* id\* - Integer to set %rax
		* name\* - System call name string
		* ret-type - Return value type string
		* params\* - Parameter list
* loc - Location integer array

Function
--------
* func-type\* - Function type string: "palan"
* name\* - Function name string
* params\* - Parameter list
* rets\* - Return value list
* impl\* - Block model for function implemantation
* loc - Location integer array

Parameter
---------
* name\* - Parameter name string
* var-type\# - Variable type list
* move - Move ownership flag boolean
* default-val - Default value expression
* loc - Location integer array

Return value
------------
1. Anonymous return value
	* var-type\* - Variable type list
2. Standard return value
	* var-type - Variable type list
	* name\* - Return value name string
* loc - Location integer array

Variable Type
-------------
* name\* - Type name string: "[]" any
	1. [] - Fixed size array type
		* sizes\* - Size integer list

Block
------
* stmts\* - Statement list in the block
* loc - Location integer array

Statement
---------
* stmt-type\* - Statement type string:
	"exp" "block" "var-init" "return" "while" "if"
	1. exp - Expression statement
		* exp\* - Expression model
	2. block - Block statement
		* block\* - Block model
	3. var-init - Variable declaration statement
		* vars\* - Variable destination list
		* inits - Variable initialize expression list
	4. return - Return value statement
		* ret-vals - Return value expression list
	5. while - While loop statement
		* cond\* - Condition expression
		* block\* - Loop block model
	6. if - If statement
		* cond\* - Condition expression
		* block\* - If block model
		* else - Else statement (block/if)
* loc - Location integer array

Expression
----------
* exp-type\* - Expression type string:
	"lit-int" "lit-uint" "lit-str" "var"
	"asgn" "func-call" 
	birary operator ("+" "-" "*" "/" "%" "==" "!=" "<" ">" "<=" ">=" "&&" "||")
	unary operator ("uminus" "not")
	1. lit-int - Integer(64bit) literal
		* val\* - Integer
	2. lit-uint - Unsigned integer(64bit) literal
		* val\* - Unsigned integer
	3. lit-str - String literal
		* val\* - String value
	4. var - Variable expression
		* base-var\* - Base variable name string
		* opes - Modified operator list
	5. asgn - Assignment expression
		* src-exps\* - Source expression list
		* dst-vals\* - Destination value list
	6. func-call - Function call expression
		* func-name\* - Function name to call
		* args\* - Argument list
	7. binary operator
		* lval\* -	Left value expression
		* rval\* -	Right value expression
	8. unary operator
		* val\* -	Value expression
* loc - Location integer array

Destination Value
------------------
* base-var\* - Base variable name string
* opes - Modified operator list
* move - Move ownership flag boolean
* loc - Location integer array

Modified Operator
-----------------
* ope-type\* - Operator type string: "index"
	1. index - Numerical index
		* indexes - Index expression list

Variable Declaration
--------------------
* name\* - Variable name string
* var-type\# - Variable type list
* move - Move ownership flag boolean
* loc - Location integer array

Argument
--------
* exp\* - Argument expression
* move - Move ownership flag boolean

Location Array
--------------
* 0\* - Source file ID integer
* 1\* - Begin line integer
* 2\* - Begin column integer
* 3\* - End line integer
* 4\* - End column integer

Palan Hacking Guide & Design Memo
===================

Code Structure
--------------
*palan.cpp*  
	Main function of palan compiler CUI.
	You can start reading code from this source.

*PlnParser.yy*  
	The parser build palan model tree directly from palan code.
	Note: It may be changed to building AST in the future.

*PlnLexer.ll*  
	Lexical analyser.

*PlnDataAllocator.cpp*  
	Provide the mechanism of allcating varibles to stack and register.
	Environment dependent code is separated to /generators/*DataAllocator.cpp 

*PlnGenerator.cpp*  
	Provide the mechanism to generate assembly code.
	Environment dependent code is separated to /generators/*Generator.cpp 
	
*/models*  
	Palan model tree classes. See [Palan Model Tree](#PMT).

*/test*  
	Automatic tests codes.

*/test/pacode*  
	Palan codes for automatic test.
	You can see working palan code here.
	Asm files and execution files will be created under /test/out.

*/sample*  
	A note the idea of palan language for the future.
	Don't use for reference.

Generating Files
----------------
pac - Palan compiler. To display help, run "./pac -h".  
test/tester - Auto test program using Catch framework.  

Main Logic
----------
1. Create Lexer and set input stream of palan code.  
```
PlnLexer	lexer;
lexer.set_filename(fname);
lexer.switch_streams(&f, &cout);
```
2. Create Parser with Lexer, empty Module and ScopeStack.
	* Lexer - Input palan code.
	* Module - Output object that is root of model tree.
	* ScopeStack - Work object. Manage current block level during parsing.
```
PlnModule module;
PlnScopeStack	scopes;
PlnParser parser(lexer, module, scopes);
```
3. Parse and build a model tree.
```
int res = parser.parse();
```
4. Finishing model tree with Data allocator, and set up passing data between models.
	* Data Allocator - Provide allocation data method register and stack.
```
PlnX86_64DataAllocator allocator;
module.finish(allocator);
```
5. Generate assembly data from model tree with Generator.
	* Generator - Generate environment dependent assembly code.
```
PlnX86_64Generator generator(cout);
module.gen(generator);
```
6. Assemble and link with "as" and "ld" command.

Palan Model Tree<a name="PMT"></a>
----------------
Palan model tree represents palan code structure to compile.
The tree is built by PlnParser. Most models has finish() and gen().
The model which have children calls such children's methods recursively.  
The finish() finishes the model by doing such as allocating stack area to use.  
The gen() generates assembly code of the model to output stream.  
Note: dump() is deprecated.

* Typical model
	1. PlnModule - Root of model tree. It includes function definitions and top level block.
	2. PlnFunction - Function definition. Generally, stack/register management is reset by each function.
	3. PlnStatement - An unit to generate assembly code. There is not any passing data directly between statements.
	4. PlnBlock - Block statement includes a series of statements. Block manage local variables in the block.
	5. PlnExpression - Expression represent returning values sentence. Plain PlnExpression class manage Literal or variable.
	Many decendant classes exist like calcuration/assignment expression.
	6. PlnAssignment - Assignment expression. PlnAssignment assign values to variables.

Expression Model
----------------
PlnExpression is base class of all models that are returning values.
Expression class has members *values* and *data_places*.
They are used for passing data between expressions.

The *values* are information of returning values.
It includes information of value type (e.g. literal integer/variable) and lval type.
The lval type is not NO_LVL, the expression is left value (destination). The lval type indicate method of assign like copy or move then.
And *values* also have actual value of literal or pointer for variable objects in *inf* union member.
Generally, *values* are set up in constructor of the model.

The *data_places* manage place and timing of passing data such as register.
Palent expression should set child's data_places before call finish().
Child expression should set source data place by using PlnDataAllocator::pushSrc() in finish().
Parent expression should call PlnDataAllocator::popSrc().
PlnDataAllocator adjusts timing or assigns temporary memory on stack if needed.

Variable Model
--------------

Data Place
----------

Scope Item
----------


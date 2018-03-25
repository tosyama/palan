Palan Hacking Guide & Design Memo
===================

ver 0.0.0

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
	Notes the idea of palan language for the future.
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
The model which have children calls the children's methods recursively.  
The finish() finishes the model. Simulate allocation free stack or register by using Data allocator.  
The gen() generates assembly code of the model to output stream.  
Note: dump() is deprecated.

* Typical models:  
	1. PlnModule - Root of model tree. It includes function definitions and top level block.
	2. PlnFunction - Function definition. Generally, stack/register management is reset by each function.
	3. PlnStatement - An unit to generate assembly code. There is not any passing data directly between statements.
	4. PlnBlock - Block statement includes a series of statements. Block manage local variables in the block.
	5. PlnExpression - Expression represent returning values sentence. Plain PlnExpression class manage Literal or variable.
	Many decendant classes exist like calcuration/assignment expression.
	6. PlnAssignment - Assignment expression. PlnAssignment assign values to variables.

Expression Model
----------------
PlnExpression is base class of the model that has returning values.
Expression class has members *values* and *data_places*.
They are used to exchange data between expressions.

The *values* are information of returning values.
It includes value type (e.g. literal integer/variable) and lval type.
If lval type is not NO_LVL, the expression is left value.
The lval type indicate following method of assignment.  

* LVL_COPY - Assign by memory data copy.
* LVL_MOVE - Move ownership. Assign by memory address copy. Source variable is cleared null.
* LVL_REF - Assign by memory address copy.

And *values* also have actual value of literal or pointer for variable objects in *inf* union member.
Generally, *values* are set up in constructor of the model.

The *data_places* manage place and timing of passing data such as register.
Parent expression set child's data_places before call finish().
Child expression set source data place to own data_places with PlnDataAllocator::pushSrc() in finish().

Variable Model
--------------
PlnVariable represents variable. Not only local variable, also represents indirect object such as array item.
The member *ptr_type* is the flags that indicate following characteristics.

* NO_PTR - Numeric variable.
* PTR_REFERENCE - Variable to keep memory address.
* PTR_OWNERSHIP - Variable that has ownership for indicating address.
* PTR_INDIRECT_ACCESS - Variable that indirect address. e.g.) array item.
* PTR_CLONE - The parameter which need to be clone.

The member *place* is data place of the variable.
However, use alias data place for general use by call getDataPlace() of expression class.

Data Allocator
--------------
PlnDataAllocator(da) manage usage of stack memory and registers.
Each model create appropriate PlnDataPlace(dp) and simulate data operation
using da in their finish(). dp should be created each operation,
because dp has it's active term information.

Use da.allocData() to get dp for allocated stack data.
New dp self or use da.prepare~() to get not allocated dp.
da.allocDp() allocate such dp.
Use da.releaseData() to release data outed the scope.
Use da.funcCalled() to simulate function call.

To exchange data between expressions, use da.pushSrc() and da.popSrc().
da will assign automatically stack space for save if it predict that the using register would destroyed.

Scope Information
-----------------
PlnScopeInfo(si) manage current scope and variables that has ownership during finish().
Owner variables have their lifetime status.
Each model update the lifetime status when update by si.set_lifetime().
Following lifetime status are available currently.

* VLT_UNKOWN - Decleared but is not allocated memory.
* VLT_ALLOCED - Allocated memory. not initialized.
* VLT_INITED - Stored meaningful data.
* VLT_FREED - Losted ownership and cleared already. Don't need to free.

Generator
---------
PlnGenerator(g) output assembly.
Each model output it's assembly by using g.gen~() in gen() method.
PlnGenEntity is require to call many of g.gen~(). 
Entities have environment dependent assembly information.
g.getEntity() create the entity from dp.
Howeverr, g.genSaveSrc() and g.genLoadDp() can be used at the timing of da.pushSrc() and da.popSrc().


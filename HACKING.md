Palan Hacking Guide & Design Memo
===================

ver 0.3.0a

Code Structure
--------------
*src/palan.cpp*  
	Main function of palan CUI compiler.
	You can start reading code from here.

*src/ast/palnast.cpp*  
	Main function of palan paser CUI.

*src/ast/AST.md*  
	Json AST specification.

*src/ast/PlnParser.yy*  
	The parser generate json AST.

*src/ast/PlnLexer.ll*  
	Lexical analyser.

*src/PlnModelTreeBuilder.cpp*  
	Build palan model tree form json AST.

*src/PlnDataAllocator.cpp*  
	Provide the mechanism of allcating varibles to stack and register.
	Environment dependent code is separated to /generators/*DataAllocator.cpp 

*src/PlnGenerator.cpp*  
	Provide the mechanism to generate assembly code.
	Environment dependent code is separated to /generators/*Generator.cpp 
	
*src/models*  
	Palan model tree classes. See [Palan Model Tree](#PMT).

*src/test*  
	Automatic tests codes.

*src/test/pacode*  
	Palan codes for automatic test.
	You can see working palan code here.
	Asm files and execution files will be created under /test/out.

*/idea*  
	Wrote down the idea of palan language for the future.
	Don't use for reference.

Generating Files
----------------
pac - Palan compiler. To display help, type "./pac -h".  
ast/pat - Palan json AST generator. To display help, type "./pat -h".  
test/tester - Auto test program using Catch C++ testing framework.  

Main Logic
----------
1.  Get json AST with "pat" command.
```cpp
	json j;
	string cmd =  cmd_dir + "pat \"" + fname + "\"";
	...
	j = json::parse(pat_output);
```

2.  Build a model tree from json.
```cpp
	PlnModule *module = modelTreeBuilder.buildModule(j["ast"]);
```

3.  Finishing model tree with Data allocator, and set up passing data between models.
    *   Data Allocator - Provide allocation data method register and stack.
```cpp
PlnX86_64DataAllocator allocator;
module.finish(allocator);
```

4.  Generate assembly data from model tree with Generator.
    *   Generator - Generate environment dependent assembly code.
```cpp
PlnX86_64Generator generator(cout);
module.gen(generator);
```

5.  Assemble and link with "as" and "ld" command.

Palan Model Tree<a name="PMT"></a>
----------------
Palan model tree represents palan code structure to compile.
The tree is built by `PlnParser`. Most models has `finish()` and `gen()`.
The model which have children calls the children's methods recursively.  
The `finish()` finishes the model. Simulate allocation of stack or register by using data allocator.  
The `gen()` generates assembly code of the model to output stream.  

### Typical models 
1.  `PlnModule` - Root of model tree. It includes function definitions, type definition and top level block.

2.  `PlnFunction` - Function definition. Generally, stack/register management is reset by each function.

3.  `PlnStatement` - Statement become a unit to generate assembly code. Because there is not any passing data directly between statements.

4.  `PlnBlock` - Block statement includes a series of statements. Block manage local variables and functions in the block.

5.  `PlnExpression` - Expression returns values. Plain PlnExpression class manage Literal or variable.
    Many descendant classes exist such as calculation/assignment expression.

6.  `PlnAssignment` - Assignment expression. It assign values to variables.

Expression Model
----------------
PlnExpression is base class of the model that has returning values.
Expression class has members `values` and `data_places`.
They are used to exchange data between expressions.

The `values` are information of returning values.
It includes value type (e.g. literal integer/variable) and `asgn_type`.
If `asgn_type` is not `NO_ASGN`, the expression is destination of assignment.
`asgn_type` indicate following method of assignment.  

*   `ASGN_COPY` - Assign by coping memory data.
*   `ASGN_MOVE` - Move ownership. Assign by coping memory address. Source variable is cleared null.
*   `ASGN_COPY_REF` - Assign by copying memory address.

`values` also have actual value of literal or pointer for variable objects in `inf` union member.
Generally, `values` are set up at constructor of the model.

The `data_places` manage place and timing of passing data such as processer register.
Parent expression set child's `data_places` before calling `finish()`.

`adjustTypes()` checks compatibilty of type to store and generate and return new expressions if needed.
Throw compile error if types is not compatibe.

Variable Model
--------------
`PlnVariable` represents variable. Not only local variable, also represents indirect object such as array item.
The member `var_type` is data type of the variable.
The member `place` is data place of the variable stored.
However, for general usage, use alias data place with `getDataPlace()` of expression class.

Type information
-----------------
The type information are stored in `PlnVarType` instance.
The member `typeinf` has basic infomation. It has type name and compatibe information.
To check compatibility of between types, use `canCopyFrom()` method. The method used to decide calling function by checking arguments type in `PlnBlock`.

The member `mode` has additional information of the data on the context.
It consist of 3 characters and each charactor indicates access right, identity and allocation.

### Access right
*   *r* - Read-only
*   *w* - Write and read
*   *o* - Write only for output (Reserved)

### Data Identity
*   *m* - Movable ownership
*   *c* - Changeable identity (Reserved)
*   *i* - Keep identity
*   *n* - No identity

### Allocation
*   *h* - Heap allocation
*   *s* - Stack allocation
*   *r* - Reference of the data
*   *i* - Immediate value

The charactor *-* indicates default value. It's replaced to default value of the type by `getVarType()`.

Data Allocator
--------------
`PlnDataAllocator`(`da`) manage usage of stack memory and registers.
Each model create appropriate `PlnDataPlace`(`dp`) and simulate data operation
using da in their `finish()`. `dp` should be created each operation,
because `dp` has it's active period information.

Use `da.allocData()` to get dp for allocated stack data.
New dp self or use `da.prepare*()` to get non allocated `dp`.
`da.allocDp()` allocate such `dp`.
Use `da.releaseData()` to release data which become out of the scope.
Use `da.funcCalled()` to simulate function call.

To simulate and setup the exchange data between expressions, use `da.pushSrc()` and `da.popSrc()`.
da will assign automatically stack space for saving if it predict that the using register would destroyed.

Scope Information
-----------------
`PlnScopeInfo`(`si`) manage current scope and variables that has ownership during `finish()`.
The variables have their lifetime status.
Each model update the lifetime status by `si.set_lifetime()` when update variable.
Followings lifetime status are available currently.

*   `VLT_UNKOWN` - Declared but is not allocated memory.
*   `VLT_ALLOCED` - Allocated the memory, but not initialized.
*   `VLT_INITED` - Stored meaningful data.
*   `VLT_FREED` - Lost ownership and cleared already. Don't need to free.

Generator
---------
`PlnGenerator`(`g`) outputs assembly.
Each model outputs it's assembly by using `g.gen*()` at its `gen()` method.
Many of `g.gen*()` are required `PlnGenEntity` to call.
Entities includes environment dependent assembly information.
`g.getEntity()` create the entity from `dp`.
`g.genSaveSrc()` and `g.genLoadDp()` can be used at the timing of `da.pushSrc()` and `da.popSrc()`.

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


Data Place
----------

Scope Item
---------

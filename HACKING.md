Palan Hacking Guide
===================

Code Structure
--------------
_palan.cpp
	Main function of palan compiler CUI.
	You can start reading code from this source.

PlnParser.yy
	The parser build palan model tree directly from palan code.
	Note: It may be changed to building AST in the future.

PlnLexer.ll
	Lexical analyser.

PlnDataAllocator.cpp
	Provide the mechanism of allcating varibles to stack and register.
	Environment dependent code is separated to /generators/*DataAllocator.cpp 

PlnGenerator.cpp
	Provide the mechanism to generate assembly code.
	Environment dependent code is separated to /generators/*Generator.cpp 
	
/models
	Palan model tree classes. See [Palan Model Tree](#PMT).

/test
	Automatic tests codes.

/test/pacode_
	Palan codes for automatic test.
	You can see working palan code here.

Generating Files
----------------
pac
test/tester

Main Logic
----------

Palan Model Tree
----------------
<a name="#PMT"></a>

Data Place
----------

Scope Item
---------

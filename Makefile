PROGRAM=pac

# NOTICE: Run 'make depend' after update SRCS.
SRCS=palan.cpp \
	models/PlnModule.cpp models/PlnFunction.cpp models/PlnBlock.cpp \
	models/PlnStatement.cpp models/PlnExpression.cpp models/PlnVariable.cpp \
	models/PlnType.cpp models/PlnArray.cpp models/PlnGeneralObject.cpp \
	models/PlnLoopStatement.cpp \
	models/PlnConditionalBranch.cpp \
	models/types/PlnFixedArrayType.cpp \
	models/types/PlnArrayValueType.cpp \
	models/types/PlnStructType.cpp \
	models/expressions/PlnFunctionCall.cpp \
	models/expressions/PlnAddOperation.cpp \
	models/expressions/PlnMulOperation.cpp \
	models/expressions/PlnDivOperation.cpp \
	models/expressions/PlnCmpOperation.cpp \
	models/expressions/PlnBoolOperation.cpp \
	models/expressions/PlnAssignment.cpp \
	models/expressions/PlnArrayItem.cpp \
	models/expressions/PlnStructMember.cpp \
	models/expressions/PlnArrayValue.cpp \
	models/expressions/PlnClone.cpp \
	models/expressions/assignitem/PlnAssignItem.cpp \
	generators/PlnX86_64Generator.cpp \
	generators/PlnX86_64DataAllocator.cpp \
	generators/PlnX86_64RegisterMachine.cpp \
	PlnDataAllocator.cpp PlnGenerator.cpp \
	PlnMessage.cpp PlnTreeBuildHelper.cpp PlnScopeStack.cpp \
	PlnModelTreeBuilder.cpp

OBJS=$(notdir $(SRCS:.cpp=.o))
VPATH=.:objs:models:models/expressions:generators:models/expressions/assignitem:models/types
AST=ast/pat
TEST=test/tester
POST_TEST=test/cuitester
# Workarround of mtrace hang with double free.
MALLOC_CHECK_=1	
export MALLOC_CHECK_

.SUFFIXES: .cpp .o

FORCE: $(PROGRAM)
	@cd test && $(MAKE) post_test
$(PROGRAM): $(OBJS)	$(TEST) $(POST_TEST)
	@cd test && $(MAKE) test
	@echo link $(PROGRAM).
	@$(CXX) $(LDFLAGS) -o $(PROGRAM) $(addprefix objs/,$(OBJS)) -lboost_program_options
.cpp.o:
	@mkdir -p objs
	$(CXX) $(CFLAGS) -std=c++11 -c -g $< -o objs/$@
$(TEST): $(OBJS) test/*.cpp test/pacode/* $(AST)
	@$(MAKE) -C test
$(AST): ast/*.cpp ast/*.yy ast/*.h ast/*.ll
	@$(MAKE) -C ast
depend: $(SRCS)
	-@ $(RM) depend.inc
	-@ for i in $^; do $(CXX) -std=c++11 -MM $$i \
	| sed "s/[_a-zA-Z0-9/][_a-zA-Z0-9/]*\.cpp//g" \
	| sed "s/^\ /\t/g" >> depend.inc; done
-include depend.inc
package:
	-apt-get -y install libboost-dev
	-apt-get -y install libboost-program-options-dev
	-apt-get -y install bison
	-apt-get -y install flex
	-curl -o test/catch.hpp https://raw.githubusercontent.com/catchorg/Catch2/master/single_include/catch2/catch.hpp


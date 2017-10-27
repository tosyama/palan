PROGRAM=pac
SRCS=palan.cpp \
	models/PlnModule.cpp models/PlnFunction.cpp models/PlnBlock.cpp \
	models/PlnStatement.cpp models/PlnExpression.cpp models/PlnVariable.cpp \
	models/PlnType.cpp models/PlnArray.cpp \
	models/expressions/PlnFunctionCall.cpp \
	models/expressions/PlnAddOperation.cpp \
	models/expressions/PlnMulOperation.cpp \
	models/expressions/PlnDivOperation.cpp \
	models/expressions/PlnAssignment.cpp \
	models/expressions/PlnMoveOwnership.cpp \
	models/expressions/PlnClone.cpp \
	generators/PlnX86_64Generator.cpp \
	generators/PlnX86_64DataAllocator.cpp \
	PlnDataAllocator.cpp PlnGenerator.cpp\
	PlnParser.cpp PlnLexer.cpp PlnMessage.cpp

OBJS=$(notdir $(SRCS:.cpp=.o))
VPATH=.:objs:models:models/expressions:generators
TEST=test/tester
# workarround of mtrace hung with double free.
MALLOC_CHECK_=1	
export MALLOC_CHECK_

.SUFFIXES: .cpp .o

$(PROGRAM): $(OBJS)	$(TEST)
	@cd test && $(MAKE) test
	@echo link $(PROGRAM).
	@$(CXX) -o $(PROGRAM) $(addprefix objs/,$(OBJS)) -lboost_program_options
.cpp.o:
	@mkdir -p objs
	$(CXX) -std=c++11 -c -g $< -o objs/$@
PlnParser.cpp: PlnParser.yy
	bison -o $@ $<
PlnParser.hpp: PlnParser.yy
	bison -o PlnParser.cpp -r all --report-file=bison.log $<
PlnLexer.cpp: PlnLexer.ll
	flex -o $@ $<
$(TEST): $(OBJS) test/*.cpp test/pacode/*
	@$(MAKE) -C test
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
	-curl -o test/catch.hpp https://raw.githubusercontent.com/philsquared/Catch/master/single_include/catch.hpp


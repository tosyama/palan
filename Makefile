PROGRAM=pac
SRCS=palan.cpp \
	models/PlnModule.cpp models/PlnFunction.cpp models/PlnBlock.cpp \
	models/PlnStatement.cpp models/PlnExpression.cpp models/PlnVariable.cpp \
	models/expressions/PlnMultiExpression.cpp \
	models/expressions/PlnFunctionCall.cpp \
	models/expressions/PlnAddOperation.cpp \
	models/expressions/PlnAssignment.cpp \
	generators/PlnX86_64Generator.cpp PlnParser.cpp PlnLexer.cpp PlnMessage.cpp
OBJS=$(notdir $(SRCS:.cpp=.o))
VPATH=.:objs:models:models/expressions:generators

.SUFFIXES: .cpp .o

$(PROGRAM): $(OBJS)
	$(CXX) -o $(PROGRAM) $(addprefix objs/,$(OBJS)) -lboost_program_options
.cpp.o:
	@mkdir -p objs
	$(CXX) -std=c++11 -c -g $< -o objs/$@
PlnParser.cpp: PlnParser.yy
	bison -o $@ $<
PlnParser.hpp: PlnParser.yy
	bison -o PlnParser.cpp -r all --report-file=bison.log $<
PlnLexer.cpp: PlnLexer.ll
	flex -o $@ $<
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


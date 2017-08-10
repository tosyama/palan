PROGRAM=pac
OBJS=main.o PlnModule.o PlnFunction.o PlnStatement.o PlnExpression.o \
	PlnX86_64Generator.o PlnValue.o PlnParser.o PlnLexer.o PlnMessage.o

.SUFFIXES: .cpp .o

$(PROGRAM): $(OBJS)
	$(CXX) -o $(PROGRAM) $(OBJS) -lboost_program_options
.cpp.o:
	$(CXX) -std=c++11 -c -g $<
PlnParser.cpp: PlnParser.yy
	bison -o $@ $<
PlnParser.hpp: PlnParser.yy
	bison -o PlnParser.cpp -r all --report-file=bison.log $<
PlnLexer.cpp: PlnLexer.ll
	flex -o $@ $<
depend: $(OBJS:.o=.cpp)
	-@ $(RM) depend.inc
	-@ for i in $^; do $(CXX) -std=c++11 -MM $$i | sed "s/\ [_a-zA-Z0-9][_a-zA-Z0-9]*\.cpp//g" >> depend.inc; done
-include depend.inc
package:
	-apt-get -y install libboost-dev
	-apt-get -y install libboost-program-options-dev
	-apt-get -y install bison
	-apt-get -y install flex


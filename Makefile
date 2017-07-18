PROGRAM=plc
OBJS=main.o PlnModule.o PlnFunction.o PlnStatement.o PlnExpression.o \
	PlnX86_64Generator.o PlnValue.o

.SUFFIXES: .cpp .o

all: $(PROGRAM) parser lexer
$(PROGRAM): $(OBJS)
	$(CXX) -o $(PROGRAM) $(OBJS)
parser: PlnParser.o PlnFlexLexer.o
	$(CXX) -std=c++11 -o $@ $^
.cpp.o:
	$(CXX) -std=c++11 -c -g $<
PlnParser.cpp: PlnParser.yy
	bison -o $@ $<
PlnFlexLexer.cpp: PlnFlexLexer.ll
	flex -o $@ $<
depend: $(OBJS:.o=.cpp)
	-@ $(RM) depend.inc
	-@ for i in $^; do $(CXX) -MM $$i | sed "s/\ [_a-zA-Z0-9][_a-zA-Z0-9]*\.cpp//g" >> depend.inc; done
-include depend.inc
package:
	-apt-get -y install libboost-dev
	-apt-get -y install bison
	-apt-get -y install flex


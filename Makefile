PROGRAM=plc
OBJS=main.o PlnModule.o PlnFunction.o PlnStatement.o PlnExpression.o \
	PlnX86_64Generator.o PlnValue.o

.SUFFIXES: .cpp .o

all: $(PROGRAM) parser
$(PROGRAM): $(OBJS)
	$(CXX) -o $(PROGRAM) $(OBJS)
.cpp.o:
	$(CXX) -std=c++11 -c -g $<
parser: PlnParser.tab.cc
	$(CXX) -std=c++11 -o $@ $<
PlnParser.tab.cc: PlnParser.yy
	bison $<
depend: $(OBJS:.o=.cpp)
	-@ $(RM) depend.inc
	-@ for i in $^; do $(CXX) -MM $$i | sed "s/\ [_a-zA-Z0-9][_a-zA-Z0-9]*\.cpp//g" >> depend.inc; done
-include depend.inc
package:
	-apt-get -y install libboost-dev
	-apt-get -y install bison



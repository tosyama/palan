PROGRAM=pat
SRCS=palanast.cpp PlnParser.cpp PlnLexer.cpp
OBJS=$(notdir $(SRCS:.cpp=.o))
VPATH=.

$(PROGRAM): $(OBJS)
	@$(CXX) -o $(PROGRAM) $(addprefix objs/,$(OBJS))
	./$(PROGRAM)
.cpp.o:
	@mkdir -p objs
	$(CXX) -std=c++11 -c -g $< -o objs/$@
PlnParser.cpp: PlnParser.yy
	bison -o $@ -r all --report-file=bison.log $<
PlnLexer.cpp: PlnLexer.ll
	flex -o $@ $<
palanast.o: PlnParser.cpp

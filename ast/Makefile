PROGRAM=pat
SRCS=palanast.cpp PlnParser.cpp PlnLexer.cpp ../PlnMessage.cpp
OBJS=$(notdir $(SRCS:.cpp=.o))
VPATH=.:objs:..

$(PROGRAM): $(OBJS)
	@echo link $(PROGRAM).
	@$(CXX) $(LDFLAGS) -o $(PROGRAM) $(addprefix objs/,$(OBJS)) -lboost_program_options
	@ln -fs ast/$(PROGRAM) ../$(PROGRAM)
.cpp.o:
	@mkdir -p objs
	$(CXX) $(CFLAGS) -std=c++11 -c -g $< -o objs/$@
PlnParser.cpp: PlnParser.yy
	bison -o $@ -r all --report-file=bison.log $<
PlnLexer.cpp: PlnLexer.ll
	flex -o $@ $<
palanast.o: PlnParser.cpp
PlnParser.o: PlnLexer.h PlnParser.hpp ../PlnMessage.h
palanast.o: PlnLexer.h PlnParser.hpp ../PlnMessage.h

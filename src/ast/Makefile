PROGRAM=pat
SRCS=palanast.cpp PlnParser.cpp PlnLexer.cpp PlnAstMessage.cpp
OBJS=$(notdir $(SRCS:.cpp=.o))
VPATH=.:objs:..
CXX_FLAGS= -g -O0
CXX_RELEASE_FLAGS= -s -O2

$(PROGRAM): $(OBJS)
	@echo link $(PROGRAM).
	@$(CXX) -o $(PROGRAM) $(addprefix objs/,$(OBJS)) -lboost_program_options
	@ln -fs ast/$(PROGRAM) ../$(PROGRAM)
.cpp.o:
	@mkdir -p objs
	$(CXX) -std=c++11 -c $(CXX_FLAGS) $< -o objs/$@
PlnParser.cpp: PlnParser.yy
	bison -o $@ -r all --report-file=bison.log $<
PlnLexer.cpp: PlnLexer.ll
	flex -o $@ $<
palanast.o: PlnParser.cpp
PlnParser.o: PlnLexer.h PlnParser.hpp PlnAstMessage.h
palanast.o: PlnLexer.h PlnParser.hpp PlnAstMessage.h
release: CXX_FLAGS=$(CXX_RELEASE_FLAGS)
release: $(PROGRAM)

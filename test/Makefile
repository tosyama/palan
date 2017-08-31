PROGRAM = tester
TESTOBJS = testMain.o testBase.o basicTest.o stackTest.o \
		dataAllocTest.o
OBJS = $(filter-out ../objs/palan.o, $(wildcard ../objs/*.o))

.SUFFIXES: .cpp .o

$(PROGRAM): $(TESTOBJS) $(OBJS)
	@echo link $(PROGRAM).
	@$(CXX) -o $(PROGRAM) $^

.cpp.o:
	$(CXX) -std=c++11 -c -g $<

test:
	@mkdir -p out
	./$(PROGRAM)

force:
	@$(CXX) -o fpac ../objs/palan.o $(OBJS) -lboost_program_options


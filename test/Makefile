PROGRAM = tester
TESTOBJS = testMain.o testBase.o basicTest.o dataAllocTest.o
OBJS = $(filter-out ../objs/palan.o, $(wildcard ../objs/*.o))

.SUFFIXES: .cpp .o

$(PROGRAM): $(TESTOBJS) $(OBJS) ./pacode/*.pa
	@echo link $(PROGRAM).
	@$(CXX) -o $(PROGRAM) $(filter %.o, $^) -pthread

.cpp.o:
	$(CXX) -std=c++11 -c -g $< -pthread

test:
	@mkdir -p out
	./$(PROGRAM)

force:
	@$(CXX) -o fpac ../objs/palan.o $(OBJS) -lboost_program_options

depend: 
	-@ $(RM) depend.inc
	-@ for i in $(basename $(TESTOBJS)); do $(CXX) -std=c++11 -MM $$i.cpp \
	| sed "s/[_a-zA-Z0-9/][_a-zA-Z0-9/]*\.cpp//g" \
	| sed "s/^\ /\t/g" >> depend.inc; done
-include depend.inc

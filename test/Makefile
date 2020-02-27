CC = gcc
PROGRAM = tester
TESTOBJS = testMain.o testBase.o basicTest.o dataAllocTest.o \
		algorithmTest.o
OBJS = $(filter-out ../objs/palan.o, $(wildcard ../objs/*.o))
AST = ../ast/pat
POST_TESTER = cuitester
POST_OBJS = cuiTestMain.o cuiTestBase.o cuiTest.o
TESTCAPI_OBJ = capi4test.o

.SUFFIXES: .cpp .c .o

$(POST_TESTER): $(POST_OBJS) $(PROGRAM)
	@echo link $(POST_TESTER).
	@$(CXX) $(LDFLAGS) -o $(POST_TESTER) $(filter %.o, $^) -pthread

$(PROGRAM): $(TESTOBJS) $(OBJS) ./pacode/*.pa $(AST)
	@echo link $(PROGRAM).
	@$(CXX) $(LDFLAGS) -o $(PROGRAM) $(filter %.o, $^) -pthread

.cpp.o:
	$(CXX) -std=c++11 -c -g $< -pthread
.c.o: 
	$(CC) -c $<

test: $(TESTCAPI_OBJ)
	@mkdir -p out
	./$(PROGRAM)

post_test:
	@mkdir -p out/cui
	./$(POST_TESTER)

force:
	@$(CXX) -o fpac ../objs/palan.o $(OBJS) -lboost_program_options

depend: 
	-@ $(RM) depend.inc
	-@ for i in $(basename $(TESTOBJS)); do $(CXX) -std=c++11 -MM $$i.cpp \
	| sed "s/[_a-zA-Z0-9/][_a-zA-Z0-9/]*\.cpp//g" \
	| sed "s/^\ /\t/g" >> depend.inc; done
-include depend.inc

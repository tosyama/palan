PROGRAM=a.out
OBJS=main.o PlnModule.o PlnFunction.o PlnX86_64Generator.o

.SUFFIXES: .cpp .o

$(PROGRAM): $(OBJS)
	$(CXX) -o $(PROGRAM) $(OBJS)
.cpp.o:
	$(CXX) -c -g $<


PROGRAM=pac

# NOTICE: Run 'make depend' after update SRCS.
SRCS=palan.cpp \
	models/PlnModule.cpp models/PlnFunction.cpp models/PlnBlock.cpp \
	models/PlnStatement.cpp models/PlnExpression.cpp models/PlnVariable.cpp \
	models/PlnType.cpp models/PlnArray.cpp models/PlnGeneralObject.cpp \
	models/PlnLoopStatement.cpp \
	models/PlnConditionalBranch.cpp \
	models/types/PlnFixedArrayType.cpp \
	models/types/PlnArrayValueType.cpp \
	models/types/PlnStructType.cpp \
	models/expressions/PlnFunctionCall.cpp \
	models/expressions/PlnAddOperation.cpp \
	models/expressions/PlnMulOperation.cpp \
	models/expressions/PlnDivOperation.cpp \
	models/expressions/PlnBoolExpression.cpp \
	models/expressions/PlnCmpOperation.cpp \
	models/expressions/PlnBoolOperation.cpp \
	models/expressions/PlnAssignment.cpp \
	models/expressions/PlnArrayItem.cpp \
	models/expressions/PlnStructMember.cpp \
	models/expressions/PlnReferenceValue.cpp \
	models/expressions/PlnArrayValue.cpp \
	models/expressions/PlnClone.cpp \
	models/expressions/assignitem/PlnAssignItem.cpp \
	generators/PlnX86_64Generator.cpp \
	generators/PlnX86_64DataAllocator.cpp \
	generators/PlnX86_64RegisterMachine.cpp \
	generators/PlnX86_64RegisterSave.cpp \
	generators/PlnX86_64CalcOptimization.cpp\
	PlnDataAllocator.cpp PlnGenerator.cpp \
	PlnMessage.cpp PlnTreeBuildHelper.cpp PlnScopeStack.cpp \
	PlnModelTreeBuilder.cpp

OBJS=$(notdir $(SRCS:.cpp=.o))
VPATH=.:objs:models:models/expressions:generators:models/expressions/assignitem:models/types
AST=ast/pat
TEST=test/tester
POST_TEST=test/cuitester
# Workarround of mtrace hang with double free.
MALLOC_CHECK_=1	
export MALLOC_CHECK_
CXX_FLAGS= -g -O0
CXX_RELEASE_FLAGS= -s -O2
AST_MAKE_FLAGS=

.SUFFIXES: .cpp .o

FORCE: $(PROGRAM)
	@cd test && $(MAKE) post_test
$(PROGRAM): $(OBJS)	$(TEST) $(POST_TEST) test/*.c
	@cd test && $(MAKE) test
	@echo link $(PROGRAM).
	@$(CXX) $(LDFLAGS) -o $(PROGRAM) $(addprefix objs/,$(OBJS)) -lboost_program_options
.cpp.o:
	@mkdir -p objs
	$(CXX) $(CFLAGS) -std=c++11 -c $(CXX_FLAGS) $< -o objs/$@
$(TEST): $(OBJS) test/*.cpp test/pacode/* $(AST)
	@$(MAKE) -C test "LDFLAGS=$(LDFLAGS)"
$(AST): ast/*.cpp ast/*.yy ast/*.h ast/*.ll
	@$(MAKE) -C ast $(AST_MAKE_FLAGS)
clean:
	rm -f objs/*
	rm -f $(AST)
	rm -f ast/objs/*
	rm -f test/*.o
depend: $(SRCS)
	-@ $(RM) depend.inc
	-@ for i in $^; do $(CXX) -std=c++11 -MM $$i \
	| sed "s/[_a-zA-Z0-9/][_a-zA-Z0-9/]*\.cpp//g" \
	| sed "s/^\ /\t/g" >> depend.inc; done
-include depend.inc
.PHONY: release
release: clean
release: CXX_FLAGS=$(CXX_RELEASE_FLAGS)
release: AST_MAKE_FLAGS=release
release: $(PROGRAM)
.PHONY: coverage
coverage: CFLAGS=--coverage -DNDEBUG
coverage: LDFLAGS=--coverage -lgcov
coverage: FORCE


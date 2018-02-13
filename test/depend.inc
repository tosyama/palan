testMain.o:  catch.hpp testBase.h
testBase.o:  testBase.h catch.hpp ../PlnParser.hpp \
	../PlnModel.h ../PlnScopeStack.h ../models/PlnExpression.h ../stack.hh \
	../location.hh ../position.hh ../PlnLexer.h ../models/PlnModule.h \
	../generators/PlnX86_64DataAllocator.h \
	../generators/../PlnDataAllocator.h ../generators/PlnX86_64Generator.h \
	../generators/../PlnGenerator.h
basicTest.o:  testBase.h catch.hpp
dataAllocTest.o:  testBase.h catch.hpp \
	../generators/PlnX86_64DataAllocator.h \
	../generators/../PlnDataAllocator.h ../PlnConstants.h
algorithmTest.o:  testBase.h catch.hpp

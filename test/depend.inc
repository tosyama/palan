testMain.o:  catch.hpp testBase.h
testBase.o:  testBase.h catch.hpp \
	../generators/PlnX86_64DataAllocator.h \
	../generators/../PlnDataAllocator.h ../generators/PlnX86_64Generator.h \
	../generators/../PlnGenerator.h ../models/PlnModule.h \
	../models/PlnExpression.h ../models/../PlnModel.h \
	../PlnModelTreeBuilder.h ../libs/json/single_include/nlohmann/json.hpp
basicTest.o:  testBase.h catch.hpp
dataAllocTest.o:  testBase.h catch.hpp \
	../generators/PlnX86_64DataAllocator.h \
	../generators/../PlnDataAllocator.h ../PlnConstants.h
algorithmTest.o:  testBase.h catch.hpp

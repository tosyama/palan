main.o: PlnParser.hpp stack.hh location.hh position.hh \
 PlnLexer.h PlnModel.h PlnX86_64Generator.h
PlnModule.o: PlnModel.h PlnX86_64Generator.h
PlnFunction.o: PlnModel.h PlnX86_64Generator.h
PlnStatement.o: PlnModel.h
PlnExpression.o: PlnModel.h PlnX86_64Generator.h
PlnX86_64Generator.o: PlnModel.h \
 PlnX86_64Generator.h
PlnValue.o: PlnModel.h PlnX86_64Generator.h
PlnParser.o: PlnParser.hpp stack.hh location.hh position.hh
PlnLexer.o: PlnParser.hpp stack.hh location.hh position.hh \
 PlnLexer.h

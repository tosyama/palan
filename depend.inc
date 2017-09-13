palan.o:  PlnParser.hpp PlnModel.h PlnScopeStack.h stack.hh \
	location.hh position.hh PlnLexer.h models/PlnModule.h \
	models/PlnExpression.h PlnMessage.h generators/PlnX86_64DataAllocator.h \
	generators/../PlnDataAllocator.h generators/PlnX86_64Generator.h \
	generators/../PlnGenerator.h
PlnModule.o:  models/PlnModule.h \
	models/PlnExpression.h models/../PlnModel.h models/PlnFunction.h \
	models/PlnType.h models/PlnVariable.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h
PlnFunction.o:  models/PlnFunction.h \
	models/../PlnModel.h models/PlnModule.h models/PlnExpression.h \
	models/PlnBlock.h models/PlnStatement.h models/PlnType.h \
	models/PlnVariable.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h
PlnBlock.o:  models/PlnFunction.h models/../PlnModel.h \
	models/PlnBlock.h models/PlnStatement.h models/PlnType.h \
	models/PlnVariable.h models/../PlnDataAllocator.h
PlnStatement.o:  models/PlnFunction.h \
	models/../PlnModel.h models/PlnBlock.h models/PlnStatement.h \
	models/PlnVariable.h models/PlnType.h \
	models/expressions/PlnMultiExpression.h \
	models/expressions/../PlnExpression.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h
PlnExpression.o:  models/PlnExpression.h \
	models/../PlnModel.h models/PlnVariable.h models/../PlnGenerator.h
PlnVariable.o:  models/PlnFunction.h \
	models/../PlnModel.h models/PlnBlock.h models/PlnExpression.h \
	models/PlnType.h models/PlnVariable.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h
PlnType.o:  models/PlnType.h models/../PlnModel.h
PlnMultiExpression.o:  \
	models/expressions/PlnMultiExpression.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnGenerator.h
PlnFunctionCall.o:  \
	models/expressions/../../PlnModel.h models/expressions/../PlnFunction.h \
	models/expressions/PlnFunctionCall.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../PlnVariable.h models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h
PlnAddOperation.o:  \
	models/expressions/PlnAddOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnGenerator.h
PlnAssignment.o:  \
	models/expressions/PlnAssignment.h models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnVariable.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h
PlnX86_64Generator.o:  \
	generators/../PlnModel.h generators/PlnX86_64DataAllocator.h \
	generators/../PlnDataAllocator.h generators/PlnX86_64Generator.h \
	generators/../PlnGenerator.h
PlnX86_64DataAllocator.o:  \
	generators/PlnX86_64DataAllocator.h generators/../PlnDataAllocator.h
PlnDataAllocator.o:  PlnDataAllocator.h
PlnParser.o:  models/PlnModule.h models/PlnExpression.h \
	models/../PlnModel.h models/PlnFunction.h models/PlnBlock.h \
	models/PlnStatement.h models/PlnType.h models/PlnVariable.h \
	models/expressions/PlnMultiExpression.h \
	models/expressions/PlnFunctionCall.h \
	models/expressions/PlnAddOperation.h models/expressions/PlnAssignment.h \
	PlnMessage.h PlnParser.hpp PlnScopeStack.h stack.hh location.hh \
	position.hh
PlnLexer.o:  PlnParser.hpp PlnModel.h PlnScopeStack.h \
	stack.hh location.hh position.hh PlnLexer.h
PlnMessage.o:  PlnMessage.h

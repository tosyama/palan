palan.o:  PlnParser.hpp PlnModel.h PlnScopeStack.h \
	models/PlnExpression.h stack.hh location.hh position.hh PlnLexer.h \
	models/PlnModule.h PlnMessage.h generators/PlnX86_64DataAllocator.h \
	generators/../PlnDataAllocator.h generators/PlnX86_64Generator.h \
	generators/../PlnGenerator.h
PlnModule.o:  models/PlnModule.h \
	models/PlnExpression.h models/../PlnModel.h models/PlnBlock.h \
	models/PlnFunction.h models/PlnType.h models/PlnVariable.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/../PlnConstants.h
PlnFunction.o:  models/PlnFunction.h \
	models/../PlnModel.h models/PlnModule.h models/PlnExpression.h \
	models/PlnBlock.h models/PlnStatement.h models/PlnType.h \
	models/PlnVariable.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h models/../PlnConstants.h \
	models/../PlnScopeStack.h
PlnBlock.o:  models/PlnFunction.h models/../PlnModel.h \
	models/PlnBlock.h models/PlnStatement.h models/PlnType.h \
	models/PlnVariable.h models/PlnArray.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h models/../PlnScopeStack.h \
	models/../PlnConstants.h
PlnStatement.o:  models/../PlnConstants.h \
	models/PlnFunction.h models/../PlnModel.h models/PlnBlock.h \
	models/PlnStatement.h models/PlnVariable.h models/PlnType.h \
	models/PlnExpression.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h models/../PlnScopeStack.h
PlnExpression.o:  models/PlnExpression.h \
	models/../PlnModel.h models/PlnType.h models/PlnVariable.h \
	models/../PlnConstants.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h
PlnVariable.o:  models/PlnFunction.h \
	models/../PlnModel.h models/PlnBlock.h models/PlnExpression.h \
	models/PlnType.h models/PlnVariable.h models/PlnArray.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/expressions/PlnClone.h \
	models/expressions/PlnMoveOwnership.h
PlnType.o:  models/PlnType.h models/../PlnModel.h \
	models/../PlnConstants.h
PlnArray.o:  models/PlnArray.h models/../PlnModel.h \
	models/../PlnDataAllocator.h
PlnFunctionCall.o:  \
	models/expressions/PlnFunctionCall.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/PlnMoveOwnership.h \
	models/expressions/../PlnFunction.h models/expressions/../PlnVariable.h \
	models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnConstants.h models/expressions/PlnClone.h
PlnAddOperation.o:  \
	models/expressions/PlnAddOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnConstants.h models/expressions/../PlnType.h
PlnMulOperation.o:  \
	models/expressions/PlnMulOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnConstants.h models/expressions/../PlnType.h
PlnDivOperation.o:  \
	models/expressions/PlnDivOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnConstants.h models/expressions/../PlnType.h
PlnAssignment.o:  \
	models/expressions/PlnAssignment.h models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnVariable.h \
	models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnConstants.h
PlnMoveOwnership.o:  \
	models/expressions/PlnMoveOwnership.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnVariable.h \
	models/expressions/../../PlnGenerator.h
PlnClone.o:  models/expressions/PlnClone.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnType.h \
	models/expressions/../../PlnConstants.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h
PlnArrayItem.o:  \
	models/expressions/PlnArrayItem.h models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnVariable.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnConstants.h
PlnX86_64Generator.o:  \
	generators/../PlnModel.h generators/../PlnConstants.h \
	generators/PlnX86_64DataAllocator.h generators/../PlnDataAllocator.h \
	generators/PlnX86_64Generator.h generators/../PlnGenerator.h
PlnX86_64DataAllocator.o:  \
	generators/../PlnConstants.h generators/../models/PlnVariable.h \
	generators/../models/../PlnModel.h generators/../models/PlnType.h \
	generators/PlnX86_64DataAllocator.h generators/../PlnDataAllocator.h
PlnDataAllocator.o:  PlnDataAllocator.h \
	PlnConstants.h
PlnGenerator.o:  PlnModel.h PlnDataAllocator.h \
	PlnGenerator.h
PlnParser.o:  models/PlnModule.h models/PlnExpression.h \
	models/../PlnModel.h models/PlnFunction.h models/PlnBlock.h \
	models/PlnStatement.h models/PlnType.h models/PlnVariable.h \
	models/PlnArray.h models/expressions/PlnFunctionCall.h \
	models/expressions/PlnAddOperation.h \
	models/expressions/PlnMulOperation.h \
	models/expressions/PlnDivOperation.h models/expressions/PlnArrayItem.h \
	models/expressions/PlnAssignment.h PlnMessage.h PlnConstants.h \
	PlnParser.hpp PlnScopeStack.h stack.hh location.hh position.hh \
	PlnLexer.h
PlnLexer.o:  PlnParser.hpp PlnModel.h PlnScopeStack.h \
	models/PlnExpression.h stack.hh location.hh position.hh PlnLexer.h
PlnMessage.o:  PlnMessage.h

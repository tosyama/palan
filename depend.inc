palan.o:  PlnParser.hpp PlnModel.h PlnScopeStack.h \
	models/PlnExpression.h stack.hh location.hh position.hh PlnLexer.h \
	models/PlnModule.h PlnMessage.h generators/PlnX86_64DataAllocator.h \
	generators/../PlnDataAllocator.h generators/PlnX86_64Generator.h \
	generators/../PlnGenerator.h
PlnModule.o:  models/PlnModule.h \
	models/PlnExpression.h models/../PlnModel.h models/PlnBlock.h \
	models/PlnFunction.h models/PlnType.h models/PlnVariable.h \
	models/PlnStatement.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h models/../PlnScopeStack.h \
	models/../PlnConstants.h models/../PlnBuildTreeHelper.h \
	models/expressions/PlnFunctionCall.h models/expressions/PlnArrayItem.h \
	models/expressions/PlnAssignment.h models/expressions/PlnMemCopy.h
PlnFunction.o:  models/PlnFunction.h \
	models/../PlnModel.h models/PlnModule.h models/PlnExpression.h \
	models/PlnBlock.h models/PlnStatement.h models/PlnType.h \
	models/PlnVariable.h models/../PlnDataAllocator.h \
	models/../PlnGenerator.h models/../PlnConstants.h \
	models/../PlnScopeStack.h
PlnBlock.o:  models/PlnFunction.h models/../PlnModel.h \
	models/PlnBlock.h models/PlnStatement.h models/PlnType.h \
	models/PlnVariable.h models/PlnArray.h models/PlnExpression.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/../PlnConstants.h
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
	models/../PlnScopeStack.h models/../PlnConstants.h \
	models/expressions/PlnClone.h \
	models/expressions/assignitem/PlnAssignItem.h
PlnType.o:  models/PlnType.h models/../PlnModel.h \
	models/PlnVariable.h models/PlnExpression.h models/../PlnConstants.h
PlnArray.o:  models/PlnArray.h models/../PlnModel.h \
	models/../PlnDataAllocator.h
PlnLoopStatement.o:  models/PlnLoopStatement.h \
	models/PlnStatement.h models/../PlnModel.h models/PlnModule.h \
	models/PlnExpression.h models/PlnBlock.h models/../PlnScopeStack.h \
	models/../PlnGenerator.h models/expressions/PlnCmpOperation.h \
	models/expressions/../../PlnConstants.h
PlnConditionalBranch.o:  \
	models/PlnConditionalBranch.h models/PlnStatement.h models/../PlnModel.h \
	models/PlnModule.h models/PlnExpression.h models/PlnBlock.h \
	models/../PlnScopeStack.h models/../PlnGenerator.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/../../PlnConstants.h
PlnFunctionCall.o:  \
	models/expressions/PlnFunctionCall.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnFunction.h \
	models/expressions/../PlnVariable.h models/expressions/../PlnType.h \
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
PlnCmpOperation.o:  \
	models/expressions/PlnCmpOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnConstants.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h models/expressions/../PlnType.h
PlnBoolOperation.o:  \
	models/expressions/PlnBoolOperation.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnConstants.h models/expressions/../PlnType.h \
	models/expressions/../PlnModule.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnScopeStack.h
PlnAssignment.o:  \
	models/expressions/PlnAssignment.h models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnVariable.h \
	models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnConstants.h \
	models/expressions/../../PlnScopeStack.h \
	models/expressions/PlnDivOperation.h \
	models/expressions/assignitem/PlnAssignItem.h
PlnClone.o:  models/expressions/PlnClone.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnType.h \
	models/expressions/../../PlnConstants.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h
PlnArrayItem.o:  \
	models/expressions/PlnArrayItem.h models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/PlnMulOperation.h \
	models/expressions/PlnAddOperation.h models/expressions/../PlnVariable.h \
	models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnConstants.h \
	models/expressions/../../PlnGenerator.h
PlnAssignItem.o:  \
	models/expressions/assignitem/../PlnAssignment.h \
	models/expressions/assignitem/../../PlnExpression.h \
	models/expressions/assignitem/../../../PlnModel.h \
	models/expressions/assignitem/../../PlnVariable.h \
	models/expressions/assignitem/../../PlnType.h \
	models/expressions/assignitem/../../../PlnDataAllocator.h \
	models/expressions/assignitem/../../../PlnGenerator.h \
	models/expressions/assignitem/../../../PlnConstants.h \
	models/expressions/assignitem/../../../PlnScopeStack.h \
	models/expressions/assignitem/../PlnDivOperation.h \
	models/expressions/assignitem/../PlnMemCopy.h \
	models/expressions/assignitem/PlnAssignItem.h \
	models/expressions/assignitem/PlnAssignPrimitiveItem.h \
	models/expressions/assignitem/PlnAssignWorkValsItem.h \
	models/expressions/assignitem/PlnAssignObjectRefItem.h \
	models/expressions/assignitem/PlnDstPrimitiveItem.h \
	models/expressions/assignitem/PlnDstCopyObjectItem.h \
	models/expressions/assignitem/PlnDstMoveObjectItem.h \
	models/expressions/assignitem/PlnDstMoveIndirectObjItem.h
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
	models/PlnStatement.h models/PlnLoopStatement.h \
	models/PlnConditionalBranch.h models/PlnType.h models/PlnVariable.h \
	models/PlnArray.h models/expressions/PlnFunctionCall.h \
	models/expressions/PlnAddOperation.h \
	models/expressions/PlnMulOperation.h \
	models/expressions/PlnDivOperation.h models/expressions/PlnArrayItem.h \
	models/expressions/PlnAssignment.h models/expressions/PlnBoolOperation.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/../../PlnConstants.h PlnMessage.h PlnParser.hpp \
	PlnScopeStack.h stack.hh location.hh position.hh PlnLexer.h
PlnLexer.o:  PlnParser.hpp PlnModel.h PlnScopeStack.h \
	models/PlnExpression.h stack.hh location.hh position.hh PlnLexer.h
PlnMessage.o:  PlnMessage.h
PlnBuildTreeHelper.o:  PlnBuildTreeHelper.h \
	PlnModel.h models/PlnType.h models/PlnBlock.h models/PlnExpression.h \
	models/PlnStatement.h models/PlnVariable.h models/PlnLoopStatement.h \
	models/expressions/PlnAddOperation.h models/expressions/PlnArrayItem.h \
	models/expressions/PlnAssignment.h models/expressions/PlnCmpOperation.h \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnFunctionCall.h
PlnScopeStack.o:  PlnModel.h models/PlnVariable.h \
	PlnScopeStack.h

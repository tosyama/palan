palan.o:  PlnConstants.h PlnMessage.h models/PlnModule.h \
	models/PlnExpression.h models/../PlnModel.h \
	generators/PlnX86_64DataAllocator.h generators/../PlnDataAllocator.h \
	generators/PlnX86_64Generator.h generators/../PlnGenerator.h \
	generators/PlnX86_64RegisterMachine.h PlnModelTreeBuilder.h \
	../libs/json/single_include/nlohmann/json.hpp PlnException.h
PlnModule.o:  models/../PlnConstants.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/../PlnTreeBuildHelper.h \
	models/../PlnModel.h models/PlnModule.h models/PlnExpression.h \
	models/PlnBlock.h models/PlnFunction.h models/PlnVariable.h \
	models/PlnStatement.h models/PlnType.h
PlnFunction.o:  models/../PlnConstants.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/PlnFunction.h models/../PlnModel.h \
	models/PlnModule.h models/PlnExpression.h models/PlnBlock.h \
	models/PlnStatement.h models/PlnType.h models/types/PlnFixedArrayType.h \
	models/types/PlnStructType.h models/PlnVariable.h models/../PlnMessage.h \
	models/../PlnException.h
PlnBlock.o:  models/../PlnConstants.h models/PlnType.h \
	models/../PlnModel.h models/PlnFunction.h models/PlnBlock.h \
	models/PlnExpression.h models/PlnStatement.h models/PlnVariable.h \
	models/types/PlnFixedArrayType.h models/types/PlnStructType.h \
	models/types/PlnAliasType.h models/PlnModule.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/../PlnMessage.h \
	models/expressions/PlnFunctionCall.h models/../PlnException.h
PlnStatement.o:  models/../PlnConstants.h \
	models/PlnFunction.h models/../PlnModel.h models/PlnBlock.h \
	models/PlnExpression.h models/PlnStatement.h models/PlnVariable.h \
	models/PlnType.h models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/../PlnMessage.h \
	models/../PlnException.h models/expressions/PlnClone.h \
	models/expressions/PlnAssignment.h
PlnExpression.o:  models/../PlnConstants.h \
	models/PlnExpression.h models/../PlnModel.h models/PlnType.h \
	models/PlnVariable.h models/expressions/PlnArrayValue.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnMessage.h models/../PlnException.h
PlnVariable.o:  models/../PlnConstants.h \
	models/PlnFunction.h models/../PlnModel.h models/PlnBlock.h \
	models/PlnExpression.h models/PlnType.h models/types/PlnFixedArrayType.h \
	models/types/PlnStructType.h models/PlnVariable.h \
	models/../PlnDataAllocator.h models/../PlnGenerator.h \
	models/../PlnScopeStack.h models/../PlnMessage.h \
	models/../PlnException.h models/expressions/PlnFunctionCall.h \
	models/expressions/assignitem/PlnAssignItem.h
PlnType.o:  models/../PlnConstants.h models/PlnType.h \
	models/../PlnModel.h models/types/PlnFixedArrayType.h \
	models/types/PlnStructType.h models/PlnVariable.h models/PlnExpression.h
PlnLoopStatement.o:  models/../PlnConstants.h \
	models/PlnLoopStatement.h models/PlnStatement.h models/../PlnModel.h \
	models/PlnModule.h models/PlnExpression.h models/PlnBlock.h \
	models/../PlnScopeStack.h models/../PlnGenerator.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/PlnBoolExpression.h
PlnConditionalBranch.o:  \
	models/../PlnConstants.h models/PlnConditionalBranch.h \
	models/PlnStatement.h models/../PlnModel.h models/PlnModule.h \
	models/PlnExpression.h models/PlnBlock.h models/../PlnScopeStack.h \
	models/../PlnGenerator.h models/expressions/PlnCmpOperation.h \
	models/expressions/PlnBoolExpression.h
PlnFixedArrayType.o:  \
	models/types/../../PlnConstants.h models/types/../PlnModule.h \
	models/types/../PlnExpression.h models/types/../../PlnModel.h \
	models/types/../PlnType.h models/types/../PlnStatement.h \
	models/types/../PlnBlock.h models/types/../PlnFunction.h \
	models/types/../expressions/PlnFunctionCall.h \
	models/types/PlnFixedArrayType.h models/types/PlnArrayValueType.h \
	models/types/../../PlnMessage.h models/types/../../PlnException.h \
	models/types/../expressions/PlnMulOperation.h \
	models/types/../expressions/PlnArrayItem.h \
	models/types/../../PlnTreeBuildHelper.h \
	models/types/../expressions/PlnAssignment.h \
	models/types/../PlnConditionalBranch.h models/types/../../PlnGenerator.h \
	models/types/../../PlnDataAllocator.h \
	models/types/../expressions/PlnMemCopy.h
PlnArrayValueType.o:  \
	models/types/../../PlnConstants.h models/types/../PlnType.h \
	models/types/../../PlnModel.h models/types/../PlnBlock.h \
	models/types/../PlnExpression.h models/types/PlnArrayValueType.h \
	models/types/../expressions/PlnArrayValue.h \
	models/types/../../PlnMessage.h models/types/../../PlnException.h
PlnStructType.o:  \
	models/types/../../PlnModel.h models/types/../../PlnConstants.h \
	models/types/../../PlnTreeBuildHelper.h \
	models/types/../../PlnDataAllocator.h models/types/../../PlnGenerator.h \
	models/types/../PlnType.h models/types/../PlnFunction.h \
	models/types/../PlnBlock.h models/types/../PlnExpression.h \
	models/types/../PlnModule.h models/types/../PlnStatement.h \
	models/types/../PlnVariable.h models/types/../PlnConditionalBranch.h \
	models/types/../expressions/PlnStructMember.h \
	models/types/../expressions/PlnAssignment.h \
	models/types/../expressions/PlnMemCopy.h \
	models/types/../expressions/PlnArrayValue.h \
	models/types/../expressions/PlnFunctionCall.h \
	models/types/PlnStructType.h models/types/PlnArrayValueType.h \
	models/types/PlnFixedArrayType.h
PlnFunctionCall.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnFunctionCall.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnFunction.h \
	models/expressions/../PlnVariable.h models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnScopeStack.h models/expressions/PlnClone.h \
	models/expressions/PlnArrayValue.h models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h
PlnAddOperation.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnAddOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h models/expressions/../PlnType.h \
	models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h \
	models/expressions/PlnCalcOperationUtils.h
PlnMulOperation.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnMulOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h models/expressions/../PlnType.h \
	models/expressions/PlnCalcOperationUtils.h
PlnDivOperation.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnDivOperation.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h models/expressions/../PlnType.h \
	models/expressions/PlnCalcOperationUtils.h
PlnBoolExpression.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/PlnBoolExpression.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h
PlnCmpOperation.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/PlnBoolExpression.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h models/expressions/../PlnType.h
PlnBoolOperation.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnBoolOperation.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/PlnBoolExpression.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnType.h \
	models/expressions/../PlnModule.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnScopeStack.h
PlnAssignment.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnAssignment.h models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnScopeStack.h \
	models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h \
	models/expressions/../PlnVariable.h models/expressions/../PlnType.h \
	models/expressions/PlnDivOperation.h \
	models/expressions/assignitem/PlnAssignItem.h \
	models/expressions/PlnArrayItem.h
PlnArrayItem.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnArrayItem.h models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/PlnMulOperation.h \
	models/expressions/PlnAddOperation.h models/expressions/../PlnVariable.h \
	models/expressions/../PlnType.h \
	models/expressions/../types/PlnFixedArrayType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h
PlnStructMember.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h models/expressions/../PlnType.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../types/PlnStructType.h \
	models/expressions/../PlnVariable.h models/expressions/PlnStructMember.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h \
	models/expressions/../types/PlnFixedArrayType.h
PlnReferenceValue.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/PlnReferenceValue.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h \
	models/expressions/../PlnVariable.h models/expressions/../PlnType.h
PlnArrayValue.o:  \
	models/expressions/../../PlnConstants.h models/expressions/../PlnType.h \
	models/expressions/../../PlnModel.h models/expressions/PlnArrayValue.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../types/PlnArrayValueType.h \
	models/expressions/../types/PlnFixedArrayType.h \
	models/expressions/../types/PlnStructType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h \
	models/expressions/../../PlnMessage.h \
	models/expressions/../../PlnException.h
PlnClone.o:  \
	models/expressions/../../PlnConstants.h \
	models/expressions/../PlnExpression.h \
	models/expressions/../../PlnModel.h models/expressions/../PlnVariable.h \
	models/expressions/../PlnType.h \
	models/expressions/../../PlnDataAllocator.h \
	models/expressions/../../PlnGenerator.h models/expressions/PlnClone.h \
	models/expressions/PlnArrayValue.h models/expressions/PlnArrayItem.h \
	models/expressions/PlnStructMember.h \
	models/expressions/assignitem/PlnAssignItem.h
PlnAssignItem.o:  \
	models/expressions/assignitem/../../../PlnConstants.h \
	models/expressions/assignitem/../PlnAssignment.h \
	models/expressions/assignitem/../../PlnExpression.h \
	models/expressions/assignitem/../../../PlnModel.h \
	models/expressions/assignitem/../../PlnVariable.h \
	models/expressions/assignitem/../../PlnType.h \
	models/expressions/assignitem/../../../PlnDataAllocator.h \
	models/expressions/assignitem/../../../PlnGenerator.h \
	models/expressions/assignitem/../../../PlnScopeStack.h \
	models/expressions/assignitem/../../../PlnMessage.h \
	models/expressions/assignitem/../../../PlnException.h \
	models/expressions/assignitem/../PlnDivOperation.h \
	models/expressions/assignitem/../PlnMemCopy.h \
	models/expressions/assignitem/../PlnClone.h \
	models/expressions/assignitem/../PlnArrayValue.h \
	models/expressions/assignitem/../PlnArrayItem.h \
	models/expressions/assignitem/../PlnStructMember.h \
	models/expressions/assignitem/../../types/PlnFixedArrayType.h \
	models/expressions/assignitem/PlnAssignItem.h \
	models/expressions/assignitem/PlnAssignPrimitiveItem.h \
	models/expressions/assignitem/PlnAssignWorkValsItem.h \
	models/expressions/assignitem/PlnAssignObjectRefItem.h \
	models/expressions/assignitem/PlnAssignIndirectObjItem.h \
	models/expressions/assignitem/PlnChainAssignItem.h \
	models/expressions/assignitem/PlnAssignArrayValue.h \
	models/expressions/assignitem/arrval_imp/PlnAssignArrayValue_Static.h \
	models/expressions/assignitem/arrval_imp/PlnAssignArrayValue_Var.h \
	models/expressions/assignitem/arrval_imp/PlnAssignArrayValue_IndirectVar.h \
	models/expressions/assignitem/PlnDstPrimitiveItem.h \
	models/expressions/assignitem/PlnDstCopyObjectItem.h \
	models/expressions/assignitem/PlnDstMoveObjectItem.h \
	models/expressions/assignitem/PlnDstMoveIndirectObjItem.h
PlnX86_64Generator.o:  \
	generators/../PlnModel.h generators/../PlnConstants.h \
	generators/PlnX86_64DataAllocator.h generators/../PlnDataAllocator.h \
	generators/PlnX86_64Generator.h generators/../PlnGenerator.h \
	generators/PlnX86_64RegisterMachine.h \
	generators/PlnX86_64CalcOptimization.h
PlnX86_64DataAllocator.o:  \
	generators/../PlnConstants.h generators/../models/PlnVariable.h \
	generators/../models/../PlnModel.h generators/../models/PlnType.h \
	generators/PlnX86_64DataAllocator.h generators/../PlnDataAllocator.h
PlnX86_64RegisterMachine.o:  \
	generators/../PlnModel.h generators/PlnX86_64DataAllocator.h \
	generators/../PlnDataAllocator.h generators/PlnX86_64Generator.h \
	generators/../PlnGenerator.h generators/PlnX86_64RegisterMachine.h \
	generators/PlnX86_64RegisterMachineImp.h \
	generators/PlnX86_64RegisterSave.h
PlnX86_64RegisterSave.o:  \
	generators/../PlnModel.h generators/PlnX86_64DataAllocator.h \
	generators/../PlnDataAllocator.h generators/PlnX86_64Generator.h \
	generators/../PlnGenerator.h generators/PlnX86_64RegisterMachine.h \
	generators/PlnX86_64RegisterMachineImp.h \
	generators/PlnX86_64RegisterSave.h
PlnX86_64CalcOptimization.o:  \
	generators/../PlnModel.h generators/../PlnConstants.h \
	generators/PlnX86_64DataAllocator.h generators/../PlnDataAllocator.h \
	generators/PlnX86_64Generator.h generators/../PlnGenerator.h \
	generators/PlnX86_64RegisterMachine.h
PlnDataAllocator.o:  PlnDataAllocator.h \
	PlnConstants.h
PlnGenerator.o:  PlnModel.h PlnDataAllocator.h \
	PlnGenerator.h
PlnMessage.o:  PlnMessage.h
PlnTreeBuildHelper.o:  PlnConstants.h \
	PlnTreeBuildHelper.h PlnModel.h models/PlnType.h models/PlnBlock.h \
	models/PlnExpression.h models/PlnStatement.h models/PlnVariable.h \
	models/PlnLoopStatement.h models/expressions/PlnAddOperation.h \
	models/expressions/PlnArrayItem.h models/expressions/PlnAssignment.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/PlnBoolExpression.h \
	models/expressions/PlnFunctionCall.h models/types/PlnFixedArrayType.h
PlnScopeStack.o:  PlnConstants.h PlnModel.h \
	models/PlnType.h models/PlnVariable.h PlnScopeStack.h
PlnModelTreeBuilder.o:  PlnModel.h \
	PlnModelTreeBuilder.h ../libs/json/single_include/nlohmann/json.hpp \
	PlnScopeStack.h PlnConstants.h PlnMessage.h PlnException.h \
	models/PlnType.h models/PlnModule.h models/PlnExpression.h \
	models/PlnFunction.h models/PlnBlock.h models/PlnStatement.h \
	models/PlnVariable.h models/PlnLoopStatement.h \
	models/PlnConditionalBranch.h models/expressions/PlnAssignment.h \
	models/expressions/PlnFunctionCall.h \
	models/expressions/PlnAddOperation.h \
	models/expressions/PlnMulOperation.h \
	models/expressions/PlnDivOperation.h \
	models/expressions/PlnBoolOperation.h \
	models/expressions/PlnCmpOperation.h \
	models/expressions/PlnBoolExpression.h models/expressions/PlnArrayItem.h \
	models/expressions/PlnStructMember.h \
	models/expressions/PlnReferenceValue.h \
	models/expressions/PlnArrayValue.h models/types/PlnFixedArrayType.h \
	models/types/PlnArrayValueType.h models/types/PlnStructType.h

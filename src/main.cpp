#include <iostream>
#include "llvm.hpp"

int main()
{
	llvm::LLVMContext context;
	auto builder = llvm::IRBuilder<>(context);
	auto module = llvm::Module("<module>", context);

	// Add printf declaration
	auto printf_type = llvm::FunctionType::get(builder.getInt32Ty(), { builder.getPtrTy() }, true);
	auto printf_func = llvm::Function::Create(printf_type, llvm::Function::ExternalLinkage, "printf", module);

	// Generate main function declaration
	auto ret_type = builder.getInt32Ty();
	std::vector<llvm::Type *> param_types = {};
	auto function_type = llvm::FunctionType::get(ret_type, param_types, false);
	auto function_name = "main";
	llvm::Function::Create(function_type, llvm::Function::ExternalLinkage, function_name, module);

	// Create main function body
	auto function = module.getFunction(function_name);
	auto entry_block = llvm::BasicBlock::Create(context, "entry", function);
	builder.SetInsertPoint(entry_block);

	// Call printf
	auto fmt = builder.CreateGlobalStringPtr("Hello, World!\n");
	std::vector<llvm::Value *> args = { fmt };
	builder.CreateCall(printf_func, args);

	// Generate return
	auto ret_val = llvm::Constant::getNullValue(builder.getInt32Ty());
	builder.CreateRet(ret_val);

	// Dump module
	module.dump();

	// Setup target machine for object generation
	llvm::InitializeAllTargetInfos();
	llvm::InitializeAllTargets();
	llvm::InitializeAllTargetMCs();
	llvm::InitializeAllAsmParsers();
	llvm::InitializeAllAsmPrinters();
	std::string error;
	auto triple = llvm::sys::getDefaultTargetTriple();
	auto target = llvm::TargetRegistry::lookupTarget(triple, error);
	llvm::TargetOptions opt;
	auto target_machine = target->createTargetMachine(triple, "generic", "", opt, llvm::Reloc::PIC_);
	module.setDataLayout(target_machine->createDataLayout());

	// Generate object
	std::error_code errcode;
	auto output_file = llvm::raw_fd_ostream("output.o", errcode, llvm::sys::fs::OF_None);
	if (errcode) {
		std::cout << "failed to open object file" << std::endl;
		return -1;
	}

	llvm::legacy::PassManager pass;
	if (target_machine->addPassesToEmitFile(pass, output_file, nullptr, llvm::CodeGenFileType::ObjectFile)) {
		std::cout << "failed to add passes to emit file" << std::endl;
		return -1;
	}

	pass.run(module);
	output_file.flush();

	std::cout << "Successfully created object file 'output.o'" << std::endl;

	return 0;
}

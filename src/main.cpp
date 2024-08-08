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

	// Local variable
	auto fmt2 = builder.CreateGlobalStringPtr("Local variable: %d\n");
	llvm::Value *undef = llvm::UndefValue::get(builder.getInt32Ty());
  	auto alloca_ptr = new llvm::BitCastInst(undef, undef->getType(), // placeholder for local variables
                                            "alloca.placeholder", entry_block);
  	auto local_builder = llvm::IRBuilder<>(context);
  	local_builder.SetInsertPoint(alloca_ptr);

	auto local_var = local_builder.CreateAlloca(local_builder.getInt32Ty(), nullptr, "my_number");
	llvm::Value *val = local_builder.getInt32(1337);
	local_builder.CreateStore(val, local_var);

	llvm::Value *loaded_val = dynamic_cast<llvm::Value *>(builder.CreateLoad(builder.getInt32Ty(), local_var, "loaded_my_number"));
	args = { fmt2, loaded_val };
	builder.CreateCall(printf_func, args);

	// Global variable
	auto fmt3 = builder.CreateGlobalStringPtr("Global variable: %.3lf\n");
	auto global_val = llvm::ConstantFP::get(builder.getDoubleTy(), llvm::APFloat(69.420));
	auto global_var = new llvm::GlobalVariable(module, builder.getDoubleTy(), false, llvm::GlobalValue::ExternalLinkage, global_val, "my_global_double");
	loaded_val = dynamic_cast<llvm::Value *>(builder.CreateLoad(builder.getDoubleTy(), global_var, "loaded_double"));
	args = { fmt3, loaded_val };
	builder.CreateCall(printf_func, args);

	// Array
	auto fmt4 = builder.CreateGlobalStringPtr("My Array: [ ");
	auto local_arr = local_builder.CreateAlloca(llvm::ArrayType::get(local_builder.getInt32Ty(), 4), nullptr, "number_arr");
	for (size_t i = 0; i < 4; ++i) {
		auto deref = local_builder.getInt32(0);
		auto index = local_builder.getInt32(i);
		auto ptr = local_builder.CreateGEP(local_arr->getAllocatedType(), local_arr, { deref, index }, "tmp_assign");
		auto value = local_builder.getInt32(i * 10);
		local_builder.CreateStore(value, ptr);
	}

	args = { fmt4 };
	builder.CreateCall(printf_func, args);

	// Loop through array and print
	auto fmt5 = builder.CreateGlobalStringPtr("%d ");
	for (size_t i = 0; i < 4; ++i) {
		auto deref = local_builder.getInt32(0);
		auto index = local_builder.getInt32(i);
		auto ptr = local_builder.CreateGEP(local_arr->getAllocatedType(), local_arr, { deref, index }, "tmp_read");
		auto value = local_builder.CreateLoad(local_builder.getInt32Ty(), ptr, "arr_value");
		args = { fmt5, value };
		builder.CreateCall(printf_func, args);
	}

	auto fmt6 = builder.CreateGlobalStringPtr("]\n");
	args = { fmt6 };
	builder.CreateCall(printf_func, args);

	// Loop/branching
	auto loop_exit = llvm::BasicBlock::Create(context, "loop_exit", function); // Setup loop_exit block with the stuff after the loop

	auto fmt7 = builder.CreateGlobalStringPtr("MyChar: %c\n");
	auto mychar = builder.CreateAlloca(local_builder.getInt8Ty(), nullptr, "mychar");
	local_builder.CreateStore(local_builder.getInt8('A'), mychar);

	auto loop = llvm::BasicBlock::Create(context, "loop", function, loop_exit);
	auto loop_builder = llvm::IRBuilder<>(context);
	loop_builder.SetInsertPoint(loop);

	auto mychar_val = local_builder.CreateLoad(local_builder.getInt8Ty(), mychar);
	args = { fmt7, mychar_val };
	loop_builder.CreateCall(printf_func, args);

	auto addtmp = loop_builder.CreateAdd(mychar, loop_builder.getInt8(1), "addtmp");
	loop_builder.CreateStore(addtmp, mychar);
	// Only go up to 'C'
	// NOTE: 'mychar' is a pointer, so we don't compare it directly
	mychar_val = local_builder.CreateLoad(local_builder.getInt8Ty(), mychar);
	auto cond = loop_builder.CreateICmp(llvm::CmpInst::Predicate::ICMP_SGT, mychar_val, local_builder.getInt8('C'), "cond");
	loop_builder.CreateCondBr(cond, loop, loop_exit);

	// Cleanup
	alloca_ptr->eraseFromParent(); // erase placeholder

	// Generate return
	builder.SetInsertPoint(loop_exit);
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

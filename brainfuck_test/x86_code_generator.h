#pragma once

#include <stdint.h>

#if __cplusplus
extern "C" {
#endif

#define CODEGEN_REGISTER_EAX 0
#define CODEGEN_REGISTER_EBX 3
#define CODEGEN_REGISTER_ECX 1
#define CODEGEN_REGISTER_EDX 2

#define CODEGEN_DATA_SIZE 0x10000
#define CODEGEN_CODE_SIZE 0x10000

struct CodeGeneratorBuffer {
	char* code;
	char* data;
	uint32_t codepos;
	uint32_t tempval;
};

CodeGeneratorBuffer* codegen_alloc();
void codegen_free(CodeGeneratorBuffer* h);
void codegen_run(CodeGeneratorBuffer* h);
void codegen_dump(CodeGeneratorBuffer* h);
void codegen_dump_data(CodeGeneratorBuffer* h, uint32_t len);

int32_t codegen_emit8(CodeGeneratorBuffer* h, uint8_t val);
int32_t codegen_emit16(CodeGeneratorBuffer* h, uint16_t val);
int32_t codegen_emit32(CodeGeneratorBuffer* h, uint32_t val);

int32_t codegen_override32(CodeGeneratorBuffer* h, uint32_t offset, uint32_t val);

int32_t codegen_append_ret(CodeGeneratorBuffer* h);
int32_t codegen_append_inc32(CodeGeneratorBuffer* h, uint8_t reg0);
int32_t codegen_append_dec32(CodeGeneratorBuffer* h, uint8_t reg0);

int32_t codegen_append_push(CodeGeneratorBuffer* h, uint8_t reg0);
int32_t codegen_append_pop(CodeGeneratorBuffer* h, uint8_t reg0);

//add value to eax
int32_t codegen_append_add32(CodeGeneratorBuffer* h, int32_t val);

//add value
int32_t codegen_append_add32_ex(CodeGeneratorBuffer* h, uint8_t reg0, int32_t val);

//addr to eax
int32_t codegen_append_load32(CodeGeneratorBuffer* h, uint32_t* p);

//eax to addr
int32_t codegen_append_store32(CodeGeneratorBuffer* h, uint32_t* p);

//reference to eax.
int32_t codegen_append_load32_ref(CodeGeneratorBuffer* h, uint8_t reg0);

//eax to reference.
int32_t codegen_append_store32_ref(CodeGeneratorBuffer* h, uint8_t reg0);

//set value to register
int32_t codegen_append_set32(CodeGeneratorBuffer* h, uint8_t reg0, uint32_t val);

//compare constant to eax
int32_t codegen_append_cmp(CodeGeneratorBuffer* h, uint32_t val);

//jump
int32_t codegen_append_jmp(CodeGeneratorBuffer* h, uint32_t diff);

//jump if equals
int32_t codegen_append_je(CodeGeneratorBuffer* h, uint32_t diff);

//jump
int32_t codegen_update_jmp(CodeGeneratorBuffer* h, uint32_t offset, uint32_t diff);

//jump if equals
int32_t codegen_update_je(CodeGeneratorBuffer* h, uint32_t offset, uint32_t diff);

//exchange register to eax
int32_t codegen_append_xchg(CodeGeneratorBuffer* h, uint8_t reg0);



#if __cplusplus
}
#endif
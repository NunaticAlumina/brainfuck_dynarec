#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <windows.h>

#include "x86_code_generator.h"

typedef void(*pfunc)(void);

static char* __alloc_execution_block(uint32_t size) {
	return (char*)VirtualAllocEx(GetCurrentProcess(), 0, size, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
}

static int __free_execution_block(char* p) {
	return VirtualFreeEx(GetCurrentProcess(), (LPVOID)p, 0, MEM_DECOMMIT);
}

int32_t codegen_emit8(CodeGeneratorBuffer* h, uint8_t val) {
	(uint8_t&)h->code[h->codepos] = val;
	h->codepos += sizeof(uint8_t);
	return sizeof(uint8_t);
}

int32_t codegen_emit16(CodeGeneratorBuffer* h, uint16_t val) {
	(uint16_t&)h->code[h->codepos] = val;
	h->codepos += sizeof(uint16_t);
	return sizeof(uint16_t);
}

int32_t codegen_emit32(CodeGeneratorBuffer* h, uint32_t val) {
	(uint32_t&)h->code[h->codepos] = val;
	h->codepos += sizeof(uint32_t);
	return sizeof(uint32_t);
}

int32_t codegen_override32(CodeGeneratorBuffer* h, uint32_t offset, uint32_t val) {
	(uint32_t&)h->code[offset] = val;
	return sizeof(uint32_t);
}

CodeGeneratorBuffer* codegen_alloc() {
	CodeGeneratorBuffer* rv = (CodeGeneratorBuffer*)malloc(sizeof(CodeGeneratorBuffer));

	rv->code = __alloc_execution_block(CODEGEN_CODE_SIZE);
	rv->data = (char*)malloc(CODEGEN_DATA_SIZE);
	rv->codepos = 0;
	rv->tempval = 0;

	memset(rv->code, 0, CODEGEN_CODE_SIZE);
	memset(rv->data, 0, CODEGEN_DATA_SIZE);

	return rv;
}

void codegen_free(CodeGeneratorBuffer* h) {
	__free_execution_block(h->code);
	free(h->data);
	free(h);
}

void codegen_run(CodeGeneratorBuffer* h) {
	((pfunc)h->code)();
}

void codegen_dump(CodeGeneratorBuffer* h) {
	printf("Length: %2d # ", h->codepos);
	for (int u = 0; u < h->codepos; ++u) {
		printf("%02X ", 0xFF & h->code[u]);
	}
	putchar('\n');
}

void codegen_dump_data(CodeGeneratorBuffer* h, uint32_t len) {
	for (int u = 0; u < len; ++u) {
		printf("%02X ", 0xFF & h->data[u]);
	}
	putchar('\n');
}


int32_t codegen_append_ret(CodeGeneratorBuffer* h) {
	return codegen_emit8(h, 0xC3);
}

int32_t codegen_append_inc32(CodeGeneratorBuffer* h, uint8_t reg0) {
	return codegen_emit8(h, 0x40 + reg0);
}

int32_t codegen_append_dec32(CodeGeneratorBuffer* h, uint8_t reg0) {
	return codegen_emit8(h, 0x48 + reg0);
}

int32_t codegen_append_push(CodeGeneratorBuffer* h, uint8_t reg0) {
	return codegen_emit8(h, 0x50 + reg0);
}

int32_t codegen_append_pop(CodeGeneratorBuffer* h, uint8_t reg0) {
	return codegen_emit8(h, 0x58 + reg0);
}

int32_t codegen_append_add32(CodeGeneratorBuffer* h, int32_t val) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0x05);
	rv += codegen_emit32(h, val);
	return rv;
}

int32_t codegen_append_add32_ex(CodeGeneratorBuffer* h, uint8_t reg0, int32_t val) {
	if (reg0 == CODEGEN_REGISTER_EAX) return codegen_append_add32(h, val);
	int32_t rv = 0;
	rv += codegen_emit8(h, 0x81);
	rv += codegen_emit8(h, 0xC0 + reg0);
	rv += codegen_emit32(h, val);
	return rv;
}

int32_t codegen_append_load32(CodeGeneratorBuffer* h, uint32_t* p) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0xA1);
	rv += codegen_emit32(h, reinterpret_cast<uint32_t>(p));
	return rv;
}

int32_t codegen_append_store32(CodeGeneratorBuffer* h, uint32_t* p) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0xA3);
	rv += codegen_emit32(h, reinterpret_cast<uint32_t>(p));
	return rv;
}

int32_t codegen_append_load32_ref(CodeGeneratorBuffer* h, uint8_t reg0) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0x8B);
	rv += codegen_emit8(h, reg0);
	return rv;
}

int32_t codegen_append_store32_ref(CodeGeneratorBuffer* h, uint8_t reg0) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0x89);
	rv += codegen_emit8(h, reg0);
	return rv;
}

int32_t codegen_append_set32(CodeGeneratorBuffer* h, uint8_t reg0, uint32_t val) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0xB8 + reg0);
	rv += codegen_emit32(h, val);
	return rv;
}

int32_t codegen_append_cmp(CodeGeneratorBuffer* h, uint32_t val) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0x3D);
	rv += codegen_emit32(h, val);
	return rv;
}

int32_t codegen_append_jmp(CodeGeneratorBuffer* h, uint32_t diff) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0xE9);
	rv += codegen_emit32(h, diff);
	return rv;
}

int32_t codegen_append_je(CodeGeneratorBuffer* h, uint32_t diff) {
	int32_t rv = 0;
	rv += codegen_emit8(h, 0x0F);
	rv += codegen_emit8(h, 0x84);
	rv += codegen_emit32(h, diff);
	return rv;
}

int32_t codegen_update_jmp(CodeGeneratorBuffer* h, uint32_t offset, uint32_t diff) {
	return codegen_override32(h, offset + 1, diff);
}

int32_t codegen_update_je(CodeGeneratorBuffer* h, uint32_t offset, uint32_t diff) {
	return codegen_override32(h, offset + 2, diff);
}

int32_t codegen_append_xchg(CodeGeneratorBuffer* h, uint8_t reg0) {
	return codegen_emit8(h, 0x90 + reg0);
}
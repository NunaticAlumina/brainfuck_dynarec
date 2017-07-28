#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <unordered_map>

#include "x86_code_generator.h"

#define BUFSIZE 0x10000

#define RESULT_SUCCESS 0
#define RESULT_UNEXPECTED_INSTRUCTION 1
#define RESULT_INVALID_LOOP 2

static uint32_t __memory[BUFSIZE];
static uint32_t __output[BUFSIZE];
static clock_t elapsed = 0;

void dump_array_8(const uint32_t* arr, uint32_t len) {
	for (int u = 0; u < len; ++u) {
		printf("%02X ", 0xFF & arr[u]);
	}
	putchar('\n');
}

void dump_array_32(const uint32_t* arr, uint32_t len) {
	for (int u = 0; u < len; ++u) {
		printf("%08X ", arr[u]);
	}
	putchar('\n');
}

void print_array_as_string(const uint32_t* arr) {
	const uint32_t* p = arr;
	while (uint32_t val = *p++)
		putchar(0xFF & val);
}

int brainfuck_find_matching_open(const char* src, int instruction_pointer) {
	int stack = -1;
	int p = instruction_pointer;
	while (stack < 0) {
		--p;
		if (p < 0) return -1;
		char i = src[p];
		switch (i) {
		case '[': ++stack; break;
		case ']': --stack; break;
		}
	}
	return p - 1;
}

int brainfuck_find_matching_close(const char* src, int instruction_pointer) {
	int stack = 1;
	int p = instruction_pointer;
	while (stack > 0) {
		++p;
		char i = src[p];
		switch (i) {
		case '[': ++stack; break;
		case ']': --stack; break;
		case '\0': return -1;
		}
	}
	return p;
}

int brainfuck_interpret(const char* src) {
	clock_t begin = clock();
	
	int32_t jumps[BUFSIZE];

	memset(__memory, 0, BUFSIZE * sizeof(uint32_t));
	memset(__output, 0, BUFSIZE * sizeof(uint32_t));
	memset(jumps, -1, BUFSIZE * sizeof(uint32_t));

	int instruction_pointer = -1;
	int data_pointer = 0;
	int output_pointer = 0;

	while (char instruction = src[++instruction_pointer]) {
		switch (instruction) {
		case '>': //increase data pointer
			++data_pointer; break;
		case '<': //decrease data pointer
			--data_pointer; break;
		case '+': //increase data
			++__memory[data_pointer]; break;
		case '-': //decrease data
			--__memory[data_pointer]; break;
		case '.': //print a byte.
			__output[output_pointer++] = __memory[data_pointer]; break;
		case ',': //read a byte
			return RESULT_UNEXPECTED_INSTRUCTION;
			//__buf[data_pointer] = getchar(); break;
		case '[': //if the byte at the data pointer is zero, jump to matching ]
			if (__memory[data_pointer] != 0) continue;
			instruction_pointer = brainfuck_find_matching_close(src, instruction_pointer);
			if (instruction_pointer == -1) return RESULT_INVALID_LOOP;
			break;
		case ']': //jump to matching [			
			{
				auto p = jumps[instruction_pointer];
				if (p == -1) {
					int last = instruction_pointer;
					instruction_pointer = brainfuck_find_matching_open(src, instruction_pointer);
					if (instruction_pointer == -1) return RESULT_INVALID_LOOP;
					jumps[last] = instruction_pointer;
				}
				else
					instruction_pointer = jumps[instruction_pointer];
			}			
			/*
			instruction_pointer = brainfuck_find_matching_open(src, instruction_pointer);
			if (instruction_pointer == -1) return RESULT_INVALID_LOOP;
			*/
			break;
		default:
			return RESULT_UNEXPECTED_INSTRUCTION;
		}
	}

	elapsed = clock() - begin;
	printf("elapsed: %f\n", elapsed / (double)CLOCKS_PER_SEC);

	print_array_as_string(__output);
	dump_array_32(__memory, 8);
	dump_array_32(__memory + 8, 8);
	return RESULT_SUCCESS;
}

int brainfuck_dynarec(const char* src) {
	clock_t begin = clock();

	std::unordered_map<int, uint32_t> jumps;

	auto h = codegen_alloc();

	//data: eax
	//data pointer: ebx
	//output pointer: ecx

	memset(__output, 0, BUFSIZE);

	codegen_append_push(h, CODEGEN_REGISTER_EAX);
	codegen_append_push(h, CODEGEN_REGISTER_EBX);
	codegen_append_push(h, CODEGEN_REGISTER_ECX);

	codegen_append_set32(h, CODEGEN_REGISTER_EAX, 0);
	codegen_append_set32(h, CODEGEN_REGISTER_EBX, reinterpret_cast<uint32_t>(h->data));
	codegen_append_set32(h, CODEGEN_REGISTER_ECX, reinterpret_cast<uint32_t>(__output));

	int instruction_pointer = -1;

	while (auto instruction = src[++instruction_pointer]) {
		switch (instruction) {
		case '>': //increase data pointer
			codegen_append_add32_ex(h, CODEGEN_REGISTER_EBX, 4);
			if (src[instruction_pointer + 1] != '>') codegen_append_load32_ref(h, CODEGEN_REGISTER_EBX);
			break;
		case '<': //decrease data pointer
			codegen_append_add32_ex(h, CODEGEN_REGISTER_EBX, -4);
			if (src[instruction_pointer + 1] != '<') codegen_append_load32_ref(h, CODEGEN_REGISTER_EBX);
			break;
		case '+': //increase data
			codegen_append_inc32(h, CODEGEN_REGISTER_EAX);
			if (src[instruction_pointer + 1] != '+') codegen_append_store32_ref(h, CODEGEN_REGISTER_EBX);			
			break;
		case '-': //decrease data
			codegen_append_dec32(h, CODEGEN_REGISTER_EAX);
			if (src[instruction_pointer + 1] != '-') codegen_append_store32_ref(h, CODEGEN_REGISTER_EBX);
			break;
		case '.': //print a byte.
			codegen_append_store32_ref(h, CODEGEN_REGISTER_ECX);
			codegen_append_add32_ex(h, CODEGEN_REGISTER_ECX, 4);
			break;
		case ',': //read a byte
			return RESULT_UNEXPECTED_INSTRUCTION;
		case '[': //if the byte at the data pointer is zero, jump to matching ]
			//can't resolve jump target yet.
			jumps.insert(std::make_pair(instruction_pointer, h->codepos));
			codegen_append_cmp(h, 0);
			codegen_append_je(h, 0);
			break;
		case ']': //jump to matching [
			{
				int ip_open = brainfuck_find_matching_open(src, instruction_pointer);
				if (ip_open == -1) return RESULT_INVALID_LOOP;

				auto p = jumps.find(ip_open + 1);
				if (p == jumps.end()) return RESULT_INVALID_LOOP;
				
				int32_t diff = (int32_t)h->codepos - (int32_t)p->second;

				codegen_append_jmp(h, -(diff + 5));
				codegen_update_je(h, p->second + 5, diff - 6);

				jumps.erase(ip_open + 1);
			}
			break;
		default:
			return RESULT_UNEXPECTED_INSTRUCTION;
		}
	}

	codegen_append_pop(h, CODEGEN_REGISTER_ECX);
	codegen_append_pop(h, CODEGEN_REGISTER_EBX);
	codegen_append_pop(h, CODEGEN_REGISTER_EAX);

	codegen_append_ret(h);

	codegen_run(h);

	elapsed = clock() - begin;
	printf("elapsed: %f\n", elapsed / (double)CLOCKS_PER_SEC);

	print_array_as_string(__output);
	dump_array_32((uint32_t*)h->data, 8);
	dump_array_32((uint32_t*)h->data + 8, 8);

	codegen_free(h);

	return RESULT_SUCCESS;
}

#if 1
typedef unsigned char byte;

typedef void(*pfunc)(void);

union funcptr {
	pfunc x;
	byte* y;
};

int dynarec_test() {
	int arg1;
	int arg2;
	int res1;

	byte* buf = (byte*)VirtualAllocEx(GetCurrentProcess(), 0, 1 << 16, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	if (buf == 0) return 0;

	byte* p = buf;

	*p++ = 0x50; // push eax
	*p++ = 0x52; // push edx

	*p++ = 0xA1; // mov eax, [arg2]
	(int*&)p[0] = &arg2; p += sizeof(int*);

	*p++ = 0x92; // xchg edx,eax

	*p++ = 0xA1; // mov eax, [arg1]
	(int*&)p[0] = &arg1; p += sizeof(int*);

	*p++ = 0xF7; *p++ = 0xEA; // imul edx

	*p++ = 0xA3; // mov [res1],eax
	(int*&)p[0] = &res1; p += sizeof(int*);

	*p++ = 0x5A; // pop edx
	*p++ = 0x58; // pop eax
	*p++ = 0xC3; // ret

	funcptr func;
	func.y = buf;

	arg1 = 123; arg2 = 321; res1 = 0;

	func.x(); // call generated code

	printf("arg1=%i arg2=%i arg1*arg2=%i func(arg1,arg2)=%i\n", arg1, arg2, arg1*arg2, res1);

	VirtualFreeEx(GetCurrentProcess(), (LPVOID)buf, 0, MEM_DECOMMIT);
}
#endif

/*
void codegen_test()
{
	auto h = codegen_alloc();
	
	uint32_t* op = &h->tempval;
	*op = 0;
	
	codegen_append_push(h, CODEGEN_REGISTER_EAX);
	codegen_append_push(h, CODEGEN_REGISTER_EBX);

	codegen_append_set32(h, CODEGEN_REGISTER_EAX, 0);
	codegen_append_set32(h, CODEGEN_REGISTER_EBX, 0);

	codegen_append_inc(h, CODEGEN_REGISTER_EBX);
	codegen_append_inc(h, CODEGEN_REGISTER_EBX);
	codegen_append_inc(h, CODEGEN_REGISTER_EBX);
	codegen_append_inc(h, CODEGEN_REGISTER_EBX);
	codegen_append_inc(h, CODEGEN_REGISTER_EBX);
	codegen_append_inc(h, CODEGEN_REGISTER_EAX);

	codegen_append_cmp(h, 2);
	codegen_append_je(h, 5);
	codegen_append_jmp(h, -(5 + 6 + 5 + 6)); // jmp=5, je=6, cmp=5, inc=1

	codegen_append_xchg(h, CODEGEN_REGISTER_EBX);
	codegen_append_store32(h, op);

	codegen_append_pop(h, CODEGEN_REGISTER_EBX);
	codegen_append_pop(h, CODEGEN_REGISTER_EAX);
	codegen_append_ret(h);

	codegen_run(h);
	printf("op: %d\n", *op);

	codegen_free(h);
}

void codegen_test()
{
	auto h = codegen_alloc();

	uint32_t p = 0;
	uint32_t* arr = reinterpret_cast<uint32_t*>(h->data);

	codegen_append_push(h, CODEGEN_REGISTER_EAX);
	codegen_append_push(h, CODEGEN_REGISTER_EBX);

	codegen_append_set32(h, CODEGEN_REGISTER_EAX, 0);
	codegen_append_set32(h, CODEGEN_REGISTER_EBX, reinterpret_cast<uint32_t>(arr));

	codegen_append_inc32(h, CODEGEN_REGISTER_EAX);
	codegen_append_add32_ex(h, CODEGEN_REGISTER_EAX, 1);
	codegen_append_store32(h, &p);
	codegen_append_store32_ref(h, CODEGEN_REGISTER_EBX);

	codegen_append_pop(h, CODEGEN_REGISTER_EBX);
	codegen_append_pop(h, CODEGEN_REGISTER_EAX);
	codegen_append_ret(h);

	codegen_run(h);
	printf("eax: %d\n", p);
	codegen_dump_data(h, 0x10);

	codegen_free(h);
}
*/

int main() {
	//hello world
	//const char* brainfuck_source = "++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.";
	
	//simple loop
	//const char* brainfuck_source = "++[>++<-]";

	//long loop
	const char* brainfuck_source = "++++[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]>[>++++<-]";
		
	//const char* brainfuck_source = "++>++++";
	
	//codegen_test();

	brainfuck_interpret(brainfuck_source);
	brainfuck_dynarec(brainfuck_source);
	//printf("result: %d", result);

	return 0;
}
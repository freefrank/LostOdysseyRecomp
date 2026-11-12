#pragma once
#include <memory>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "tgsi/tgsi_text.h"
#include "tgsi/tgsi_dump.h"

#include "codegen/nv50_ir_driver.h"

#include "glsl_frontend.h"

#include "nv_attributes.h"
#include "nv_shader_header.h"
#include "dksh.h"

class DekoCompiler
{
	pipeline_stage m_stage;
	glsl_program m_glsl;
	const struct tgsi_token* m_tgsi;
	unsigned int m_tgsiNumTokens;
	nv50_ir_prog_info m_info;
	void* m_code;
	uint32_t m_codeSize;
	void* m_data;
	uint32_t m_dataSize;

	NvShaderHeader m_nvsh;
	DkshProgramHeader m_dkph;

	void RetrieveAndPadCode();
	void GenerateHeaders();
	bool CompileGlslUnguarded(const char* glsl);
	bool CompileTgsiOfUnguarded(const DekoCompiler& other);

public:
	DekoCompiler(pipeline_stage stage, int optLevel = 3);
	~DekoCompiler();

	bool CompileGlsl(const char* glsl);
	void OutputDksh(const char* dkshFile);
	void WriteDksh(std::vector<uint8_t>& out) const; // SwitchWakerHD patch 2: OutputDksh's bytes, in memory

	// SwitchWakerHD patch 5: true keeps Mesa's built-in functions and type tables between compilers
	// (built once, on the first compile) instead of releasing them in every destructor
	static void SetFrontendResident(bool resident);

	// banjo-nx (2026-10-18): compiles the GLSL; for a fragment shader with banjo flag 16 (choose), also without
	// flag 8 (the input interpolation cache), without 32 (value numbering over the dominator tree), without both and
	// without 512 (texture fetches clustered) (the codegen again from the same TGSI), and keeps the best: fewer GPRs
	// in steps of 8 (the per-thread allocation unit, which sets how many warps an SM holds), then less code. nullptr
	// when it does not compile.
	static std::unique_ptr<DekoCompiler> CompileBest(pipeline_stage stage, const char* glsl, int optLevel = 3);

	// banjo-nx (2026-10-18): the codegen of another compiler's TGSI (its GLSL program must outlive this one, or be
	// handed over: TakeGlslProgram).
	bool CompileTgsiOf(const DekoCompiler& other);
	void TakeGlslProgram(DekoCompiler& other) { m_glsl = other.m_glsl; other.m_glsl = nullptr; }
	void OutputRawCode(const char* rawFile);
	void OutputTgsi(const char* tgsiFile);
};

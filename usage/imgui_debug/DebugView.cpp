#include "DebugView.hpp"

#include <SDL2/SDL.h>

#include <thirdparty/imgui/imgui.h>
#include <thirdparty/imgui/imgui_internal.h>
#include <thirdparty/imgui/misc/cpp/imgui_stdlib.h>
#include <thirdparty/imgui/backends/imgui_impl_sdl2.h>
#include <thirdparty/imgui/backends/imgui_impl_opengl3.h>
#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>

DebugView::DebugView(std::shared_ptr<psx::video::SdlWindow> win, psx::System* sys) 
	: m_win{ win }, m_psx{ sys }, m_gl_ctx{ nullptr }, 
	m_first_frame{ true }, m_enabled_opts{} {
	m_gl_ctx = m_win->GetGlContext();

	ImGui::CreateContext();

	auto& io = ImGui::GetIO();

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui_ImplSDL2_InitForOpenGL((SDL_Window*)m_win->GetWindowHandle(), 
		m_gl_ctx);
	ImGui_ImplOpenGL3_Init("#version 430");

	m_win->ForwardEventHandler([this](SDL_Event* ev) {
		ImGui_ImplSDL2_ProcessEvent(ev);
	});

	m_enabled_opts.insert(std::pair{ "allow_cpu_state_mod", false });
}

DebugView::~DebugView() {
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void DebugView::Update() {
	m_win->Clear();

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame();
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

	CpuWindow();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	auto& io = ImGui::GetIO();

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		void* gl_current = SDL_GL_GetCurrentContext();
		void* curr_window = SDL_GL_GetCurrentWindow();

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		SDL_GL_MakeCurrent((SDL_Window*)curr_window, gl_current);
	}

	m_win->Present();

	m_first_frame = false;
}

void DebugView::CpuWindow() {
	ImGui::Begin("CPU COP0");

	auto& cpu_ptr = m_psx->GetCPU();
	auto& cop0 = cpu_ptr.m_coprocessor0;

	bool& allow_mods = m_enabled_opts["allow_cpu_state_mod"];

	ImGui::Checkbox("Enable modifications", &allow_mods);

	if (allow_mods) {
		auto show_u32_hex = [&cop0](const char* name, auto* ptr) -> void {
			ImGui::InputScalar(name, ImGuiDataType_U32,
				(void*)ptr, nullptr, nullptr, "%08X",
				ImGuiInputTextFlags_CharsHexadecimal);
		};

		show_u32_hex("BPC (Execute breakpoint)", &cop0.registers.bpc);
		show_u32_hex("BPC Mask", &cop0.registers.bpcm);
		show_u32_hex("BDA (Access breakpoint)", &cop0.registers.bda);
		show_u32_hex("BDA Mask", &cop0.registers.bdam);
		show_u32_hex("BadVaddr", &cop0.registers.badvaddr);
		show_u32_hex("EPC", &cop0.registers.epc);
		show_u32_hex("PRID", &cop0.registers.prid);

		ImGui::BeginChild("Cause reg", ImVec2(0, 0), ImGuiChildFlags_Border);

		show_u32_hex("CAUSE", &cop0.registers.cause.reg);
	
		{
			auto excode = cop0.registers.cause.exception_code;

			auto excode_name = magic_enum::enum_name(excode);
			bool int_req = (bool)((cop0.registers.cause.reg >> 10) & 1);
			bool soft_int1 = (bool)((cop0.registers.cause.reg >> 8) & 1);
			bool soft_int2 = (bool)((cop0.registers.cause.reg >> 9) & 1);
			uint32_t cop_num = cop0.registers.cause.cop_number;
			bool bd = cop0.registers.cause.branch_delay;

			std::string input_text{ excode_name };

			ImGui::InputTextWithHint("Exception code", excode_name.data(),
				&input_text);
			
			auto try_convert = magic_enum::enum_cast<psx::cpu::Excode>(input_text);

			if (try_convert.has_value())
				cop0.registers.cause.exception_code = try_convert.value();

			ImGui::Checkbox("Interrupt request", &int_req);
			ImGui::Checkbox("Software interrupt 1", &soft_int1);
			ImGui::Checkbox("Software interrupt 2", &soft_int2);
			show_u32_hex("Coprocessor of exception", &cop_num);
			ImGui::Checkbox("Branch delay", &bd);

			cop0.registers.cause.reg &= ~(1 << 8);
			cop0.registers.cause.reg &= ~(1 << 9);
			cop0.registers.cause.reg &= ~(1 << 10);

			cop0.registers.cause.reg |= ((uint32_t)int_req << 10);
			cop0.registers.cause.reg |= ((uint32_t)soft_int1 << 8);
			cop0.registers.cause.reg |= ((uint32_t)soft_int2 << 9);
			cop0.registers.cause.cop_number = cop_num;
			cop0.registers.cause.branch_delay = bd;
		}

		ImGui::EndChild();
	}
	else {
		auto show_u32_hex = [&cop0](const char* name, auto* ptr) -> void {
			ImGui::Text("%s  = %08X", name, *ptr);
		};

		show_u32_hex("BPC (Execute breakpoint)", &cop0.registers.bpc);
		show_u32_hex("BPC Mask", &cop0.registers.bpcm);
		show_u32_hex("BDA (Access breakpoint)", &cop0.registers.bda);
		show_u32_hex("BDA Mask", &cop0.registers.bdam);
		show_u32_hex("BadVaddr", &cop0.registers.badvaddr);
		show_u32_hex("EPC", &cop0.registers.epc);
		show_u32_hex("PRID", &cop0.registers.prid);

		ImGui::BeginChild("Cause reg", ImVec2(0, 0), ImGuiChildFlags_Border);

		show_u32_hex("CAUSE", &cop0.registers.cause.reg);

		{
			auto excode = cop0.registers.cause.exception_code;

			auto excode_name = magic_enum::enum_name(excode);
			bool int_req = (bool)((cop0.registers.cause.reg >> 10) & 1);
			bool soft_int1 = (bool)((cop0.registers.cause.reg >> 8) & 1);
			bool soft_int2 = (bool)((cop0.registers.cause.reg >> 9) & 1);
			uint32_t cop_num = cop0.registers.cause.cop_number;
			bool bd = cop0.registers.cause.branch_delay;

			ImGui::Text("Exception Code              : %s", excode_name.data());
			ImGui::Text("Interrupt request           : %s", int_req ? "true" : "false");
			ImGui::Text("Software interrupt 1        : %s", soft_int1 ? "true" : "false");
			ImGui::Text("Software interrupt 2        : %s", soft_int2 ? "true" : "false");
			ImGui::Text("Coprocessor of exception    : %d", cop_num);
			ImGui::Text("Branch delay                : %s", bd ? "true" : "false");
		}

		ImGui::EndChild();
	}

	////////////////////////////

	ImGui::BeginChild("SR register", ImVec2(0, 0), ImGuiChildFlags_Border);

	if (allow_mods) {
		auto show_bit_val = [&cop0](const char* name, uint32_t bitnum) {
			auto& sr = cop0.registers.sr;

			bool bitval = (bool)((sr.reg >> bitnum) & 1);

			ImGui::Checkbox(name, &bitval);

			sr.reg &= ~(1 << bitnum);
			sr.reg |= ((uint32_t)bitval << bitnum);
		};

		ImGui::Text("SR (System status Register)");
		ImGui::InputScalar("SR", ImGuiDataType_U32,
			(void*)&cop0.registers.sr.reg, nullptr, nullptr,
			"%08X", ImGuiInputTextFlags_CharsHexadecimal);

		show_bit_val("Current interrupt enable", 0);
		show_bit_val("Current kernel/user mode (1=User)", 1);
		show_bit_val("Previous interrupt enable", 2);
		show_bit_val("Previous kernel/user mode", 3);
		show_bit_val("Old interrupt enable", 4);
		show_bit_val("Old kernel/user mode", 5);
		show_bit_val("Enable soft interrupt 1", 8);
		show_bit_val("Enable soft interrupt 2", 9);
		show_bit_val("Enable interrupts", 10);
		show_bit_val("Isolate cache", 16);
		show_bit_val("Swap cache", 17);
		show_bit_val("Parity zero", 18);
		show_bit_val("Isolation result", 19);
		show_bit_val("Cache parity error", 20);
		show_bit_val("TLB error", 21);
		show_bit_val("BEV", 22);
		show_bit_val("COP0 enable", 28);
		show_bit_val("COP1 enable", 29);
		show_bit_val("COP2 enable", 30);
		show_bit_val("COP3 enable", 31);
	}
	else {
		auto show_bit_val = [&cop0](const char* name, uint32_t bitnum) {
			auto const& sr = cop0.registers.sr;

			bool bitval = (bool)((sr.reg >> bitnum) & 1);

			ImGui::Text("%s : %s", name, bitval ? "true" : "false");
		};

		ImGui::Text("SR (System status Register) : %08X",
			cop0.registers.sr.reg);
		show_bit_val("Current interrupt enable", 0);
		show_bit_val("Current kernel/user mode (1=User)", 1);
		show_bit_val("Previous interrupt enable", 2);
		show_bit_val("Previous kernel/user mode", 3);
		show_bit_val("Old interrupt enable", 4);
		show_bit_val("Old kernel/user mode", 5);
		show_bit_val("Enable soft interrupt 1", 8);
		show_bit_val("Enable soft interrupt 2", 9);
		show_bit_val("Enable interrupts", 10);
		show_bit_val("Isolate cache", 16);
		show_bit_val("Swap cache", 17);
		show_bit_val("Parity zero", 18);
		show_bit_val("Isolation result", 19);
		show_bit_val("Cache parity error", 20);
		show_bit_val("TLB error", 21);
		show_bit_val("BEV", 22);
		show_bit_val("COP0 enable", 28);
		show_bit_val("COP1 enable", 29);
		show_bit_val("COP2 enable", 30);
		show_bit_val("COP3 enable", 31);
	}

	ImGui::EndChild();


	///////////////////////////

	////////////////////////////

	ImGui::BeginChild("DCIC", ImVec2(0, 0), ImGuiChildFlags_Border);

	if (allow_mods) {
		auto show_bit_val = [&cop0](const char* name, uint32_t bitnum) {
			auto& dcic = cop0.registers.dcic;

			bool bitval = (bool)((dcic.reg >> bitnum) & 1);

			ImGui::Checkbox(name, &bitval);

			dcic.reg &= ~(1 << bitnum);
			dcic.reg |= ((uint32_t)bitval << bitnum);
		};

		ImGui::Text("DCIC (Breakpoint control)");
		ImGui::InputScalar("DCIC", ImGuiDataType_U32,
			(void*)&cop0.registers.dcic.reg, nullptr, nullptr,
			"%08X", ImGuiInputTextFlags_CharsHexadecimal);

		show_bit_val("Did break", 0);
		show_bit_val("BPC Break", 1);
		show_bit_val("BDA Break", 2);
		show_bit_val("BDA Read break", 3);
		show_bit_val("BDA Write break", 4);
		show_bit_val("Jump break", 5);
		show_bit_val("Super master enable 1", 23);
		show_bit_val("Exec   breakpoint", 24);
		show_bit_val("Access breakpoint", 25);
		show_bit_val("Read   breakpoint", 26);
		show_bit_val("Write  breakpoint", 27);
		show_bit_val("Jump   breakpoint", 28);
		show_bit_val("Master enable bit 28", 29);
		show_bit_val("Master enable 24-27", 30);
		show_bit_val("Super master enable 2", 31);
	}
	else {
		auto show_bit_val = [&cop0](const char* name, uint32_t bitnum) {
			auto const& dcic = cop0.registers.dcic;

			bool bitval = (bool)((dcic.reg >> bitnum) & 1);

			ImGui::Text("%s : %s", name, bitval ? "true" : "false");
		};

		ImGui::Text("DCIC (Breakpoint control) : %08X",
			cop0.registers.dcic.reg);

		show_bit_val("Did break", 0);
		show_bit_val("BPC Break", 1);
		show_bit_val("BDA Break", 2);
		show_bit_val("BDA Read break", 3);
		show_bit_val("BDA Write break", 4);
		show_bit_val("Jump break", 5);
		show_bit_val("Super master enable 1", 23);
		show_bit_val("Exec   breakpoint", 24);
		show_bit_val("Access breakpoint", 25);
		show_bit_val("Read   breakpoint", 26);
		show_bit_val("Write  breakpoint", 27);
		show_bit_val("Jump   breakpoint", 28);
		show_bit_val("Master enable bit 28", 29);
		show_bit_val("Master enable 24-27", 30);
		show_bit_val("Super master enable 2", 31);
	}

	ImGui::EndChild();

	///////////////////////////

	ImGui::End();

	///////////////////

	ImGui::Begin("CPU-Internal");

	ImGui::Text("Internal view of the CPU's status");
	ImGui::Text("(Use to debug the emulator)");

	psx::system_status* stat = cpu_ptr.m_sys_status;

	ImGui::Text("Currently in BD : %s", stat->branch_delay ? "true" : "false");
	ImGui::Text("Branch taken    : %s", stat->branch_taken ? "true" : "false");
	ImGui::Text("Branch dest     : %08X", stat->branch_dest);
	ImGui::Text("Curr mode       : %s", stat->curr_mode ? "User" : "Kernel");
	ImGui::Text("Badvaddr        : %08X", stat->badvaddr);
	ImGui::Text("Exception?      : %s", stat->exception ? "true" : "false");

	auto name = magic_enum::enum_name(stat->exception_number);

	ImGui::Text("Exception code  : %s", name.data());

	if (stat->reg_writeback.dest == psx::cpu::InvalidReg)
		ImGui::Text("Last written reg : NONE");
	else
		ImGui::Text("Last written reg : r%d", stat->reg_writeback.dest);

	if (stat->curr_delay.dest == psx::cpu::InvalidReg)
		ImGui::Text("Curr reg delay : NONE");
	else
		ImGui::Text("Curr reg delay : r%d", stat->curr_delay.dest);

	if (stat->next_delay.dest == psx::cpu::InvalidReg)
		ImGui::Text("Next reg delay : NONE");
	else
		ImGui::Text("Next reg delay : r%d", stat->next_delay.dest);

	ImGui::End();
}
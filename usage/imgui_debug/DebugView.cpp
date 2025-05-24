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
	m_first_frame{ true }, m_enabled_opts{}, 
	m_except_init{ false }, m_except_init_hook{ 0xFFFFF } {
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
	m_enabled_opts.insert(std::pair{ "allow_dma_state_mod", false });

	auto& kernel = m_psx->GetKernel();

	m_except_init_hook = kernel.InsertExitHook("InstallExceptionHandlers",
		[this, &kernel](psx::u32 pc, psx::u32 id) {
			m_except_init = true;
			kernel.ScheduleExitHookRemoval(m_except_init_hook);
	}).value();
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
	DmaWindow();
	MemoryConfigWindow();
	TimersWindow();
	GpuWindow();
	KernelWindow();
	DriveWindow();
	MdecWindow();

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

	auto& status_ptr = m_psx->GetStatus();
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

	////////////////////////////

	ImGui::BeginChild("Iterrupt masks", ImVec2(0, 0), ImGuiChildFlags_Border);

	if (allow_mods) {
		auto show_bit_val = [](uint32_t& reg, const char* name, uint32_t bitnum) {
			bool bitval = (bool)((reg >> bitnum) & 1);

			ImGui::Checkbox(name, &bitval);

			reg &= ~(1 << bitnum);
			reg |= ((uint32_t)bitval << bitnum);
		};

		ImGui::Text("Interrupt request");
		ImGui::InputScalar("IREQ", ImGuiDataType_U32,
			(void*)&status_ptr.interrupt_request, nullptr, nullptr,
			"%08X", ImGuiInputTextFlags_CharsHexadecimal);

		show_bit_val(status_ptr.interrupt_request, "VBLANK ", 0);
		show_bit_val(status_ptr.interrupt_request, "GPU    ", 1);
		show_bit_val(status_ptr.interrupt_request, "CDROM  ", 2);
		show_bit_val(status_ptr.interrupt_request, "DMA    ", 3);
		show_bit_val(status_ptr.interrupt_request, "TMR0   ", 4);
		show_bit_val(status_ptr.interrupt_request, "TMR1   ", 5);
		show_bit_val(status_ptr.interrupt_request, "TMR2   ", 6);
		show_bit_val(status_ptr.interrupt_request, "MEMCARD", 7);
		show_bit_val(status_ptr.interrupt_request, "SIO    ", 8);
		show_bit_val(status_ptr.interrupt_request, "SPU    ", 9);
		show_bit_val(status_ptr.interrupt_request, "IRQ10  ", 10);

		ImGui::Text("Interrupt mask");
		ImGui::InputScalar("IMASK", ImGuiDataType_U32,
			(void*)&status_ptr.interrupt_mask, nullptr, nullptr,
			"%08X", ImGuiInputTextFlags_CharsHexadecimal);

		show_bit_val(status_ptr.interrupt_mask, "VBLANK ", 0);
		show_bit_val(status_ptr.interrupt_mask, "GPU    ", 1);
		show_bit_val(status_ptr.interrupt_mask, "CDROM  ", 2);
		show_bit_val(status_ptr.interrupt_mask, "DMA    ", 3);
		show_bit_val(status_ptr.interrupt_mask, "TMR0   ", 4);
		show_bit_val(status_ptr.interrupt_mask, "TMR1   ", 5);
		show_bit_val(status_ptr.interrupt_mask, "TMR2   ", 6);
		show_bit_val(status_ptr.interrupt_mask, "MEMCARD", 7);
		show_bit_val(status_ptr.interrupt_mask, "SIO    ", 8);
		show_bit_val(status_ptr.interrupt_mask, "SPU    ", 9);
		show_bit_val(status_ptr.interrupt_mask, "IRQ10  ", 10);
	}
	else {
		auto show_bit_val = [](uint32_t const& reg, const char* name, uint32_t bitnum) {
			bool bitval = (bool)((reg >> bitnum) & 1);

			ImGui::Text("%s : %s", name, bitval ? "true" : "false");
		};

		ImGui::Text("Interrupt request : %08X",
			status_ptr.interrupt_request);

		show_bit_val(status_ptr.interrupt_request, "VBLANK ", 0);
		show_bit_val(status_ptr.interrupt_request, "GPU    ", 1);
		show_bit_val(status_ptr.interrupt_request, "CDROM  ", 2);
		show_bit_val(status_ptr.interrupt_request, "DMA    ", 3);
		show_bit_val(status_ptr.interrupt_request, "TMR0   ", 4);
		show_bit_val(status_ptr.interrupt_request, "TMR1   ", 5);
		show_bit_val(status_ptr.interrupt_request, "TMR2   ", 6);
		show_bit_val(status_ptr.interrupt_request, "MEMCARD", 7);
		show_bit_val(status_ptr.interrupt_request, "SIO    ", 8);
		show_bit_val(status_ptr.interrupt_request, "SPU    ", 9);
		show_bit_val(status_ptr.interrupt_request, "IRQ10  ", 10);

		ImGui::Text("Interrupt enable : %08X",
			status_ptr.interrupt_mask);

		show_bit_val(status_ptr.interrupt_mask, "VBLANK ", 0);
		show_bit_val(status_ptr.interrupt_mask, "GPU    ", 1);
		show_bit_val(status_ptr.interrupt_mask, "CDROM  ", 2);
		show_bit_val(status_ptr.interrupt_mask, "DMA    ", 3);
		show_bit_val(status_ptr.interrupt_mask, "TMR0   ", 4);
		show_bit_val(status_ptr.interrupt_mask, "TMR1   ", 5);
		show_bit_val(status_ptr.interrupt_mask, "TMR2   ", 6);
		show_bit_val(status_ptr.interrupt_mask, "MEMCARD", 7);
		show_bit_val(status_ptr.interrupt_mask, "SIO    ", 8);
		show_bit_val(status_ptr.interrupt_mask, "SPU    ", 9);
		show_bit_val(status_ptr.interrupt_mask, "IRQ10  ", 10);
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

void DebugView::DmaWindow() {
	auto& status_ptr = m_psx->GetStatus();
	auto& dma_control = status_ptr.sysbus->GetDMAControl();

	bool& allow_mods = m_enabled_opts["allow_dma_state_mod"];

	ImGui::Begin("DMA");

	ImGui::Checkbox("Allow modifications", &allow_mods);

	ImGui::BeginChild("DPCR", ImVec2(0, 400),
		ImGuiChildFlags_Border);

	auto& dpcr = dma_control.m_control;

	ImGui::Text("DPCR");
	ImGui::NewLine();

	if (allow_mods) {
		auto show_dpcr_channel = [&dpcr](uint32_t id, const char* name) {
			int priority = (dpcr.raw >> (id * 4)) & 0x7;
			bool enable = (bool)((dpcr.raw >> (id * 4 + 3)) & 1);

			auto prio = fmt::format("DMA{} Priority", id);
			auto ena =  fmt::format("DMA{} Enable", id);

			ImGui::Text(name);
			ImGui::SliderInt(prio.c_str(), &priority, 0, 7);
			ImGui::Checkbox(ena.c_str(), &enable);

			dpcr.raw &= ~(0x7 << (id * 4));
			dpcr.raw |= ((uint32_t)priority << (id * 4));
			dpcr.raw &= ~(1 << (id * 4 + 3));
			dpcr.raw |= ((uint32_t)enable << (id * 4 + 3));
 		};

		ImGui::InputScalar("DPCR", ImGuiDataType_U32,
			(void*)&dpcr.raw, nullptr, nullptr,
			"%08X", ImGuiInputTextFlags_CharsDecimal);

		ImGui::NewLine();

		show_dpcr_channel(0, "MDECin");
		show_dpcr_channel(1, "MDECout");
		show_dpcr_channel(2, "GPU");
		show_dpcr_channel(3, "CDROM");
		show_dpcr_channel(4, "SPU");
		show_dpcr_channel(5, "PIO");
		show_dpcr_channel(6, "OTC");
	}
	else {
		auto show_dpcr_channel = [&dpcr](uint32_t id, const char* name) {
			int priority = (dpcr.raw >> (id * 4)) & 0x7;
			bool enable = (bool)((dpcr.raw >> (id * 4 + 3)) & 1);

			ImGui::Text(name);
			ImGui::Text("Priority : %d", priority);
			ImGui::Text("Enable   : %s", enable ? "true" : "false");
		};


		ImGui::Text("DPCR (Control Register) : %08X",
			dpcr.raw);

		ImGui::NewLine();

		show_dpcr_channel(0, "MDECin");
		show_dpcr_channel(1, "MDECout");
		show_dpcr_channel(2, "GPU");
		show_dpcr_channel(3, "CDROM");
		show_dpcr_channel(4, "SPU");
		show_dpcr_channel(5, "PIO");
		show_dpcr_channel(6, "OTC");
	}

	ImGui::EndChild();

	////////////////////////

	auto& dicr = dma_control.m_int_control;

	ImGui::BeginChild("DICR", ImVec2(0, 400),
		ImGuiChildFlags_Border);

	if (allow_mods) {
		ImGui::InputScalar("DICR", ImGuiDataType_U32,
			(void*)&dicr.raw, nullptr, nullptr,
			"%08X", ImGuiInputTextFlags_CharsDecimal);

		ImGui::NewLine();

		auto show_bit_val = [&dicr](const char* name, uint32_t bitnum) {
			bool bitval = (bool)((dicr.raw >> bitnum) & 1);

			ImGui::Checkbox(name, &bitval);

			dicr.raw &= ~(1 << bitnum);
			dicr.raw |= ((uint32_t)bitval << bitnum);
		};

		show_bit_val("DMA0 REQ on block end", 0);
		show_bit_val("DMA1 REQ on block end", 1);
		show_bit_val("DMA2 REQ on block end", 2);
		show_bit_val("DMA3 REQ on block end", 3);
		show_bit_val("DMA4 REQ on block end", 4);
		show_bit_val("DMA5 REQ on block end", 5);
		show_bit_val("DMA6 REQ on block end", 6);

		ImGui::NewLine();

		show_bit_val("Bus error", 15);

		ImGui::NewLine();

		show_bit_val("DMA0 Interrupt enable", 16);
		show_bit_val("DMA1 Interrupt enable", 17);
		show_bit_val("DMA2 Interrupt enable", 18);
		show_bit_val("DMA3 Interrupt enable", 19);
		show_bit_val("DMA4 Interrupt enable", 20);
		show_bit_val("DMA5 Interrupt enable", 21);
		show_bit_val("DMA6 Interrupt enable", 22);

		ImGui::NewLine();

		show_bit_val("Master interrupt enable", 23);

		ImGui::NewLine();

		show_bit_val("DMA0 Interrupt req", 24);
		show_bit_val("DMA1 Interrupt req", 25);
		show_bit_val("DMA2 Interrupt req", 26);
		show_bit_val("DMA3 Interrupt req", 27);
		show_bit_val("DMA4 Interrupt req", 28);
		show_bit_val("DMA5 Interrupt req", 29);
		show_bit_val("DMA6 Interrupt req", 30);

		ImGui::NewLine();

		show_bit_val("Master interrupt request", 31);
	}
	else {
		ImGui::Text("DICR (Dma Interrupt Control Register) : %08X", 
			dicr.raw);

		ImGui::NewLine();

		auto show_bit_val = [&dicr](const char* name, uint32_t bitnum) {
			bool bitval = (bool)((dicr.raw >> bitnum) & 1);

			ImGui::Text("%s : %s", name,
				bitval ? "true" : "false");
		};

		show_bit_val("DMA0 REQ on block end", 0);
		show_bit_val("DMA1 REQ on block end", 1);
		show_bit_val("DMA2 REQ on block end", 2);
		show_bit_val("DMA3 REQ on block end", 3);
		show_bit_val("DMA4 REQ on block end", 4);
		show_bit_val("DMA5 REQ on block end", 5);
		show_bit_val("DMA6 REQ on block end", 6);

		ImGui::NewLine();

		show_bit_val("Bus error", 15);

		ImGui::NewLine();

		show_bit_val("DMA0 Interrupt enable", 16);
		show_bit_val("DMA1 Interrupt enable", 17);
		show_bit_val("DMA2 Interrupt enable", 18);
		show_bit_val("DMA3 Interrupt enable", 19);
		show_bit_val("DMA4 Interrupt enable", 20);
		show_bit_val("DMA5 Interrupt enable", 21);
		show_bit_val("DMA6 Interrupt enable", 22);

		ImGui::NewLine();

		show_bit_val("Master interrupt enable", 23);

		ImGui::NewLine();

		show_bit_val("DMA0 Interrupt req", 24);
		show_bit_val("DMA1 Interrupt req", 25);
		show_bit_val("DMA2 Interrupt req", 26);
		show_bit_val("DMA3 Interrupt req", 27);
		show_bit_val("DMA4 Interrupt req", 28);
		show_bit_val("DMA5 Interrupt req", 29);
		show_bit_val("DMA6 Interrupt req", 30);

		ImGui::NewLine();

		show_bit_val("Master interrupt request", 31);
	}

	ImGui::EndChild();

	///////////////////////

	struct DmaDesc {
		psx::u32& madr;
		psx::u32& block_control;
		psx::CHCR& control;
	};

	auto show_dma = [allow_mods](uint32_t id, DmaDesc& desc, const char* name) {
		ImGui::BeginChild(name, ImVec2(0, 300),
			ImGuiChildFlags_Border);
		{
			ImGui::Text(name);

			auto madr = fmt::format("DMA_{} MADR", name);
			auto bcr = fmt::format("DMA_{} BCR", name);
			auto ctrl = fmt::format("DMA_{} CONTROL", name);

			if (allow_mods) {
				ImGui::InputScalar(madr.c_str(),
					ImGuiDataType_U32, (void*)&desc.madr,
					nullptr, nullptr, "%08X",
					ImGuiInputTextFlags_CharsDecimal);

				ImGui::InputScalar(bcr.c_str(),
					ImGuiDataType_U32, (void*)&desc.block_control,
					nullptr, nullptr, "%08X",
					ImGuiInputTextFlags_CharsDecimal);

				ImGui::InputScalar(ctrl.c_str(),
					ImGuiDataType_U32, (void*)&desc.control.raw,
					nullptr, nullptr, "%08X",
					ImGuiInputTextFlags_CharsDecimal);

				const char* prev_value = desc.control.transfer_dir ?
					"RAM -> DEVICE" : "DEVICE -> RAM";

				bool ram_to_dev = false;
				bool dev_to_ram = false;

				if (ImGui::BeginCombo("Transfer direction", prev_value)) {
					if (ImGui::Selectable("RAM -> DEVICE", &ram_to_dev))
						desc.control.transfer_dir = true;
					if (ImGui::Selectable("DEVICE -> RAM", &dev_to_ram))
						desc.control.transfer_dir = false;
					ImGui::EndCombo();
				}

				bool decrement = desc.control.decrement;
				ImGui::Checkbox("Decrement MADR (-4)", &decrement);
				desc.control.decrement = decrement;

				bool chopping = desc.control.chopping;
				ImGui::Checkbox("Chopping mode", &chopping);
				desc.control.chopping = chopping;

				auto curr_sync = magic_enum::enum_name(desc.control.sync);

				if (ImGui::BeginCombo("##sync", curr_sync.data())) {
					auto names = magic_enum::enum_entries<psx::SyncMode>();

					for (auto const& name : names) {
						bool selected = desc.control.sync == name.first;

						if (ImGui::Selectable(name.second.data(), &selected)) {
							desc.control.sync = name.first;
						}
					}

					ImGui::EndCombo();
				}
				
				bool start = desc.control.start_busy;
				ImGui::Checkbox("Start transfer", &start);
				desc.control.start_busy = start;

				bool force = desc.control.force_start;
				ImGui::Checkbox("Force start", &force);
				desc.control.force_start = force;

				bool pause = desc.control.pause;
				ImGui::Checkbox("Pause transfer", &pause);
				desc.control.pause = pause;
			}
			else {
				ImGui::Text("%s = %08X", madr.c_str(), desc.madr);

				if (desc.control.sync == psx::SyncMode::BURST) {
					ImGui::Text("Wordcount : %04X", (uint16_t)desc.block_control);
				}
				else if (desc.control.sync == psx::SyncMode::SLICE) {
					ImGui::Text("Wordcount        : %04X", (uint16_t)desc.block_control);
					ImGui::Text("Number of blocks : %04X", (uint16_t)(desc.block_control >> 16));
				}
				else {
					ImGui::Text("Unused block control");
				}

				ImGui::Text("Transfer direction : %s", desc.control.transfer_dir ?
					"RAM -> DEVICE" : "DEVICE -> RAM");
				ImGui::Text("Increment step : %s", desc.control.decrement ? "-4" : "+4");
				ImGui::Text("Chopping enable : %s", desc.control.chopping ? "true" : "false");

				auto sync_name = magic_enum::enum_name(desc.control.sync);

				ImGui::Text("Sync mode : %s", sync_name.data());
				ImGui::Text("Start transfer : %s", desc.control.start_busy ?
					"stopped/completed" : "start/busy");
				ImGui::Text("Force start : %s", desc.control.force_start ?
					"true" : "false");
				ImGui::Text("Pause (?) : %s", desc.control.pause ? "true" : "false");
				ImGui::Text("Bus snooping : %s", (desc.control.raw >> 30) & 1 ? "true" : "false");
			}

			ImGui::EndChild();
		}
	};

	 {
		auto gpu_dma = dynamic_cast<psx::DmaBase*>(&dma_control.m_gpu_dma);

		DmaDesc dma2{
			.madr = gpu_dma->m_base_address,
			.block_control = gpu_dma->m_block_control,
			.control = gpu_dma->m_control
		};

		show_dma(0, dma2, "GPU");
	}
	
	{
		auto ot_dma = dynamic_cast<psx::DmaBase*>(&dma_control.m_ot_dma);

		DmaDesc dma6{
			.madr = ot_dma->m_base_address,
			.block_control = ot_dma->m_block_control,
			.control = ot_dma->m_control
		};

		show_dma(6, dma6, "OT CLEAR");
	}

	/////////////////////

	ImGui::End();
}

void DebugView::MemoryConfigWindow() {
	ImGui::Begin("Memory configuration");

	auto sysbus = m_psx->GetStatus().sysbus;

	ImGui::Text("EXP1 Base : %08X", sysbus->m_exp1_config.base);
	ImGui::Text("EXP2 Base : %08X", sysbus->m_exp2_config.base);

	auto show_delay_conf = [](psx::RegionConfig const& conf, const char* name) {
		ImGui::NewLine();
		ImGui::NewLine();

		ImGui::Text("%s Delay/Size", name);

		ImGui::Text("Write delay : %02X", conf.delay_size.write_delay);
		ImGui::Text("Read delay : %02X", conf.delay_size.read_delay);
		ImGui::Text("Data bus width : %s", conf.delay_size.bus_width ?
			"16 bits" : "8 bits");
		ImGui::Text("Size : %08X", (1 << conf.delay_size.size_shift));
	};

	show_delay_conf(sysbus->m_bios_config, "BIOS");
	show_delay_conf(sysbus->m_exp1_config, "EXP1");
	show_delay_conf(sysbus->m_exp2_config, "EXP2");
	show_delay_conf(sysbus->m_exp3_config, "EXP3");
	show_delay_conf(sysbus->m_cdrom_config, "CDROM");
	show_delay_conf(sysbus->m_spu_config, "SPU");

	ImGui::NewLine();
	ImGui::NewLine();

	ImGui::Text("COM Delays");
	ImGui::Text("COM0 : %d", sysbus->m_com_delays.com0);;
	ImGui::Text("COM1 : %d", sysbus->m_com_delays.com1);
	ImGui::Text("COM2 : %d", sysbus->m_com_delays.com2);
	ImGui::Text("COM3 : %d", sysbus->m_com_delays.com3);

	ImGui::NewLine();

	const char* ram_sz = "8MB";

	using RamSize = psx::RamSize;

	switch (sysbus->m_curr_ram_sz)
	{
	case RamSize::_1MB_7MB:
		ram_sz = "1MB + 7MB Locked";
		break;
	case RamSize::_4MB_4MB:
		ram_sz = "4MB + 4MB Locked";
		break;
	case RamSize::_1MB_1HIGHZ_6MB:
		ram_sz = "1MB + 1MB HZ + 6MB Locked";
		break;
	case RamSize::_4MB_4HIGHZ:
		ram_sz = "4MB + 4MB HZ";
		break;
	case RamSize::_2MB_6MB:
		ram_sz = "2MB + 6MB Locked";
		break;
	case RamSize::_2MB_2HIGHZ_4MB:
		ram_sz = "2MB + 2MB HZ + 4MB Locked";
		break;
	default:
		break;
	}

	ImGui::Text("Ram size : %s", ram_sz);

	ImGui::NewLine();

	ImGui::Text("Cache Control");
	ImGui::Text("Scratchpad enable 1 : %d", sysbus->m_cache_control.scratch_en1);
	ImGui::Text("Scratchpad enable 2 : %d", sysbus->m_cache_control.scratch_en2);
	ImGui::Text("I-Cache enable : %d", sysbus->m_cache_control.cache_en);

	ImGui::End();
}

void DebugView::ShowTimerImpl(uint32_t tmr_id) {
	psx::RootCounter* tmr = nullptr;

	auto sysbus = m_psx->GetStatus()
		.sysbus;

	switch (tmr_id)
	{
	case 0:
		tmr = &sysbus->m_count1;
		break;
	case 1:
		tmr = &sysbus->m_count2;
		break;
	case 2:
		tmr = &sysbus->m_count3;
		break;
	default:
		return;
		break;
	}

	tmr->UpdateFromTimestamp();

	auto tmr_string = fmt::format("TMR{}", tmr_id);

	if (ImGui::BeginTabItem(tmr_string.c_str())) {
		ImGui::Text("Current timer value : %04X", 
			tmr->m_count_value & 0xFFFF);
		ImGui::Text("Counter target : %04X",
			tmr->m_count_target & 0xFFFF);

		ImGui::NewLine();
		ImGui::Text("Control");
		ImGui::NewLine();

		ImGui::Text("Sync enable : %d", tmr->m_mode.m_sync_enable);

		std::string_view sync_text = "<INVALID>";

		switch (tmr_id)
		{
		case 0: {
			auto sync_mode = static_cast<psx::SyncMode0>(tmr->m_mode.sync_mode);
			sync_text = magic_enum::enum_name(sync_mode);
		}
			break;
		case 1: {
			auto sync_mode = static_cast<psx::SyncMode1>(tmr->m_mode.sync_mode);
			sync_text = magic_enum::enum_name(sync_mode);
		}
			break;
		case 2: {
			auto sync_mode = static_cast<psx::SyncMode2>(tmr->m_mode.sync_mode);
			sync_text = magic_enum::enum_name(sync_mode);
		}
			break;
		default:
			break;
		}

		ImGui::Text("Sync mode : %s", sync_text.data());

		ImGui::Text("Reset counter at target : %d", tmr->m_mode.reset_on_target);
		ImGui::Text("IRQ on target   : %d", tmr->m_mode.irq_on_target);
		ImGui::Text("IRQ on overflow : %d", tmr->m_mode.irq_on_ov);
		ImGui::Text("IRQ repeat      : %d", tmr->m_mode.irq_repeat);
		ImGui::Text("IRQ toggle      : %d", tmr->m_mode.irq_toggle);
		
		std::string_view source_txt = "<INVALID>";

		switch (tmr_id)
		{
		case 0: {
			auto src = static_cast<psx::ClockSource0>(tmr->m_mode.clock_src);
			source_txt = magic_enum::enum_name(src);
		}
			  break;
		case 1: {
			auto src = static_cast<psx::ClockSource1>(tmr->m_mode.clock_src);
			source_txt = magic_enum::enum_name(src);
		}
			  break;
		case 2: {
			auto src = static_cast<psx::ClockSource2>(tmr->m_mode.clock_src);
			source_txt = magic_enum::enum_name(src);
		}
			  break;
		default:
			break;
		}

		ImGui::Text("Clock source : %s", source_txt.data());
		ImGui::Text("Interrupt request : %d", tmr->m_mode.irq);
		ImGui::Text("Reached target : %d", tmr->m_mode.target_reached);
		ImGui::Text("Reached overflow : %d", tmr->m_mode.ov_reached);

		ImGui::EndTabItem();
	}
}

void DebugView::TimersWindow() {
	if (!ImGui::Begin("Timers")) {
		ImGui::End();
		return;
	}

	ImGui::BeginTabBar("##timers");

	ShowTimerImpl(0);
	ShowTimerImpl(1);
	ShowTimerImpl(2);

	ImGui::EndTabBar();

	ImGui::End();
}

void DebugView::GpuWindow() {
	if (!ImGui::Begin("GPU")) {
		ImGui::End();
		return;
	}

	auto& gpu = m_psx->GetStatus()
		.sysbus->m_gpu;

	ImGui::Text("Currently in VBlank : %d", gpu.m_vblank);
	ImGui::Text("Current scanline : %d", gpu.m_scanline);
	ImGui::Text("Current CMD FIFO size : %d", gpu.m_cmd_fifo.len());

	auto curr_cmd_status = magic_enum::enum_name(gpu.m_cmd_status);

	ImGui::Text("Command mode : %s", curr_cmd_status.data());

	auto curr_read_stat = magic_enum::enum_name(gpu.m_read_status);

	ImGui::Text("Read mode : %s", curr_cmd_status.data());
	ImGui::Text("Read latch : %08X", gpu.m_gpu_read_latch);

	ImGui::BeginTabBar("##stat");

	if (ImGui::BeginTabItem("Status")) {
		auto& gpustat = gpu.m_stat;
		ImGui::Text("Tex X page : %d", (int)gpustat.texture_page_x_base * 64);
		ImGui::Text("Tex Y page : %d", (int)gpustat.texture_page_y_base * 256);
		
		auto semi_trans = magic_enum::enum_name(gpustat.semi_transparency);
		ImGui::Text("Semi transparency : %s", semi_trans.data());
		auto texpage_col = magic_enum::enum_name(gpustat.tex_page_colors);
		ImGui::Text("Texpage colors : %s", texpage_col.data());

		ImGui::Text("Dither          : %d", gpustat.dither);
		ImGui::Text("Draw to display : %d", gpustat.draw_to_display);
		ImGui::Text("Set mask        : %d", gpustat.set_mask);
		ImGui::Text("Mask enable     : %d", gpustat.draw_over_mask_disable);
		ImGui::Text("Interlace field : %d", gpustat.interlace_field);
		ImGui::Text("Flip H          : %d", gpustat.flip_screen_hoz);
		ImGui::Text("Tex Y page 2    : %d", (int)gpustat.texture_page_y_base2 * 512);
		ImGui::Text("Vertical interlace : %d", gpustat.vertical_interlace);
		ImGui::Text("Display enable  : %d", gpustat.disp_enable);
		ImGui::Text("IRQ1            : %d", gpustat.irq1);
		ImGui::Text("Dreq            : %d", gpustat.dreq);
		ImGui::Text("Recv cmd word   : %d", gpustat.recv_cmd_word);
		ImGui::Text("Ready for VRAM -> CPU  : %d", gpustat.send_vram_cpu);
		ImGui::Text("Recv DMA        : %d", gpustat.recv_dma);

		auto dma_dir = magic_enum::enum_name(gpustat.dma_dir);
		ImGui::Text("Dma direction   : %s", dma_dir.data());
		ImGui::Text("Drawing odd     : %d", gpustat.drawing_odd);

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Tex window")) {
		ImGui::Text("Mask X : %d", gpu.m_tex_win.mask_x);
		ImGui::Text("Mask Y : %d", gpu.m_tex_win.mask_y);
		ImGui::Text("Offset X : %d", gpu.m_tex_win.offset_x);
		ImGui::Text("Offset Y : %d", gpu.m_tex_win.offset_y);
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Other data")) {
		ImGui::Text("Horizontal resolution : %d", gpu.m_disp_conf.hoz_res);
		ImGui::Text("Vertical resolution   : %d", gpu.m_disp_conf.vert_res);
		ImGui::Text("Draw area top left X : %d, Y : %d",
			gpu.m_x_top_left, gpu.m_y_top_left);
		ImGui::Text("Draw area bottom right X : %d, Y : %d",
			gpu.m_x_bot_right, gpu.m_y_bot_right);
		ImGui::Text("Draw offset X : %d", gpu.m_x_off);
		ImGui::Text("Draw offset Y : %d", gpu.m_y_off);
		ImGui::Text("Display start X : %d", gpu.m_disp_x_start);
		ImGui::Text("Display start Y : %d", gpu.m_disp_y_start);
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	ImGui::End();
}

void DebugView::KernelWindow() {
	if (!ImGui::Begin("Kernel")) {
		ImGui::End();
		return;
	}

	auto const& kernel = m_psx->GetKernel();

	auto bcd = kernel.DumpKernelBcdDate();
	auto maker = kernel.DumpKernelMaker();
	auto version = kernel.DumpKernelVersion();

	ImGui::Text("Kernel BCD Date : %s", bcd.c_str());
	ImGui::Text("Kernel Maker    : %s", maker.data());
	ImGui::Text("Kernel Version  : %s", version.data());

	ImGui::BeginTabBar("##kernel");

	if (ImGui::BeginTabItem("Exceptions")) {
		if (!m_except_init) {
			ImGui::Text("Exception handlers not init!");
		}
		else {
			auto except_chains = kernel.DumpAllExceptionChains();
			ImGui::Text("Total exception chains : %d", except_chains.size());

			for (uint32_t i = 0; auto const& chain : except_chains) {
				auto header_name = fmt::format("Chain {}", i);
				if (ImGui::CollapsingHeader(header_name.c_str())) {
					if (!chain.empty()) {
						uint32_t j = 0;
						for (auto const& entry : chain) {
							ImGui::Indent();

							if (entry.first_function)
								ImGui::Text("First function : %08X", entry.first_function);
							else
								ImGui::Text("First function : <NULL>");

							if (entry.second_function)
								ImGui::Text("Second function : %08X", entry.second_function);
							else
								ImGui::Text("Second function : <NULL>");

							if (entry.next)
								ImGui::Text("Next : %08X", entry.next);
							else
								ImGui::Text("Next : <NULL>");

							j++;
						}

						while (j--)
							ImGui::Unindent();
					}
					else {
						ImGui::Text("Empty chain");
					}
				}
				i++;
			}
		}

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Events")) {
		auto evcbs = kernel.DumpEventControlBlocks();

		ImGui::Text("Total events : %d", evcbs.size());

		for (uint32_t i = 0; auto const& evcb : evcbs) {
			auto name = fmt::format("Event {}", i);
			if (ImGui::CollapsingHeader(name.c_str(), ImGuiTreeNodeFlags_Framed)) {
				auto class_name = psx::kernel::EventClassName(evcb.ev_class);
				auto status_name = psx::kernel::EventStatusName(evcb.status);
				auto spec_name = psx::kernel::EventSpecName(evcb.spec);
				auto mode_name = psx::kernel::EventModeName(evcb.mode);

				ImGui::Indent();
				ImGui::Text("Class    : %s", class_name.data());
				ImGui::Text("Status   : %s", status_name.data());
				ImGui::Text("Spec     : %s", spec_name.data());
				ImGui::Text("Mode     : %s", mode_name.data());
				ImGui::Text("Function : %08X", evcb.func_pointer);
				ImGui::Unindent();
			}
			i++;
		}

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Threads")) {
		ImGui::Text("Current TCB : %08X", kernel.GetCurrentThread());

		auto tcbs = kernel.DumpThreadControlBlocks();

		for (uint32_t i = 0; auto const& tcb : tcbs) {
			auto tcb_name = fmt::format("Thread {}", i);

			if (ImGui::CollapsingHeader(tcb_name.c_str())) {
				ImGui::Indent();

				if (ImGui::CollapsingHeader("GP Registers")) {
					ImGui::Indent();
					for (uint32_t index = 0; index < 32; index++)
						ImGui::Text("r%d = %08X", index, tcb.regs[index]);
					ImGui::Unindent();
				}

				ImGui::Text("EPC    : %08X", tcb.epc);
				ImGui::Text("HI     : %08X", tcb.hi);
				ImGui::Text("LO     : %08X", tcb.lo);
				ImGui::Text("SR     : %08X", tcb.sr);
				ImGui::Text("CAUSE  : %08X", tcb.cause);

				ImGui::Unindent();
			}

			i++;
		}

		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem("Devices")) {
		auto dcbs = kernel.DumpDeviceControlBlocks();

		ImGui::Text("Device control blocks : %d", dcbs.size());

		auto ram_base = m_psx->GetStatus()
			.sysbus->GetGuestBase();

		for (uint32_t i = 0; auto const& dcb : dcbs) {
			auto dcb_name = fmt::format("Device {}", i);
			if (ImGui::CollapsingHeader(dcb_name.c_str())) {
				ImGui::Indent();
				ImGui::Text("Lowercase name : %s", ram_base + dcb.lowercase_name_ptr);

				switch (dcb.flags)
				{
				case psx::kernel::DeviceFlags::CDROM_BU:
					ImGui::Text("Flags : CDROM/BU");
					break;
				case psx::kernel::DeviceFlags::TTY_DUART:
					ImGui::Text("Flags : TTY DUART");
					break;
				case psx::kernel::DeviceFlags::TTY_DUMMY:
					ImGui::Text("Flags : TTY DUMMY");
					break;
				default:
					ImGui::Text("Flags : <INVALID>");
					break;
				}

				ImGui::Text("Sector size : %X", dcb.sect_size);
				ImGui::Text("Uppercase name : %s", ram_base + dcb.uppercase_name_ptr);

				ImGui::Unindent();
			}
			i++;
		}

		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	ImGui::End();
}

void DebugView::DriveWindow() {
	if (!ImGui::Begin("CD-DRIVE")) {
		ImGui::End();
		return;
	}

	auto& drive = m_psx->GetStatus().sysbus->m_cdrom;

	auto yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

	ImGui::TextColored(yellow, "IDLE         :");
	ImGui::SameLine();
	ImGui::Text("%d", drive.m_idle);
	ImGui::TextColored(yellow, "Cmd pending  :");
	ImGui::SameLine();
	ImGui::Text("%d", drive.m_has_next_cmd);
	ImGui::TextColored(yellow, "Curr command :");
	ImGui::SameLine();
	ImGui::Text("%d", drive.m_curr_cmd);
	ImGui::TextColored(yellow, "Next command :");
	ImGui::SameLine();
	ImGui::Text("%d", drive.m_new_cmd);

	if (ImGui::CollapsingHeader("Index register")) {
		ImGui::TextColored(yellow, "Selected index      :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_index_reg.index);
		ImGui::TextColored(yellow, "XADPCM fifo empty   :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_index_reg.xa_adpcm_fifo_empty);
		ImGui::TextColored(yellow, "Param fifo empty    :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_index_reg.param_fifo_empty);
		ImGui::TextColored(yellow, "Param fifo full     :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_index_reg.param_fifo_full);
		ImGui::TextColored(yellow, "Response fifo empty :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_index_reg.response_fifo_empty);
		ImGui::TextColored(yellow, "Data fifo empty     :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_index_reg.data_fifo_empty);
		ImGui::TextColored(yellow, "Transmission busy   :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_index_reg.transmission_busy);
	}

	if (ImGui::CollapsingHeader("Interrupt control")) {
		ImGui::TextColored(yellow, "Interrupt enable  :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_int_enable.enable_bits);
		ImGui::TextColored(yellow, "Current interrupt :");
		auto int_text = magic_enum::enum_name(drive.m_int_flag.irq);
		ImGui::SameLine();
		ImGui::Text("%s", int_text.data());
		ImGui::TextColored(yellow, "Cmd start irq     :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_int_flag.cmd_start);
		ImGui::TextColored(yellow, "Want cmd start irq:");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_cmd_start_interrupt);
	}

	if (ImGui::CollapsingHeader("Mode")) {
		ImGui::TextColored(yellow, "Allow read CD-DA    :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.allow_cd_da);
		ImGui::TextColored(yellow, "Pause on track end     :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.autopause);
		ImGui::TextColored(yellow, "Report                 :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.report);
		ImGui::TextColored(yellow, "XA filter enable       :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.xa_filter_enable);
		ImGui::TextColored(yellow, "(undocumented) Ignore  :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.ignore);
		ImGui::TextColored(yellow, "Read whole sector      :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.read_whole_sector);
		ImGui::TextColored(yellow, "Enable XA-ADPCM        :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.enable_xa_adpcm);
		ImGui::TextColored(yellow, "Motor double speed     :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_mode.double_speed);
	}

	if (ImGui::CollapsingHeader("Stat")) {
		ImGui::TextColored(yellow, "General error    :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.err);
		ImGui::TextColored(yellow, "Motor on         :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.motor_on);
		ImGui::TextColored(yellow, "Seek error       :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.seek_err);
		ImGui::TextColored(yellow, "GetID() error    :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.id_err);
		ImGui::TextColored(yellow, "Shell open       :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.shell_open);
		ImGui::TextColored(yellow, "Reading          :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.reading);
		ImGui::TextColored(yellow, "Seeking          :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.seeking);
		ImGui::TextColored(yellow, "Playing          :");
		ImGui::SameLine();
		ImGui::Text("%d", drive.m_stat.playing);
	}

	if (ImGui::CollapsingHeader("Command history")) {
		ImGui::BeginChild("#cmds", ImVec2(0, 400), ImGuiChildFlags_Border);
		bool clear = ImGui::Button("Clear", ImVec2(60, 30));

		if (clear) {
			drive.m_history.clear();
		}

		ImGui::Checkbox("Enable history", &drive.m_keep_history);

		for (uint64_t i = 0; auto const& cmd : drive.m_history) {
			auto cmd_header = fmt::format("{} - {}", i, cmd.command_name);
			if (ImGui::CollapsingHeader(cmd_header.c_str())) {
				ImGui::Text("Command ID: %d", cmd.command_id);
				ImGui::Text("Timestamp : %ld", cmd.issue_timestamp);
				ShowDriveCommand(&cmd);
			}
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

void DebugView::MdecWindow() {
	if (!ImGui::Begin("MDEC")) {
		ImGui::End();
		return;
	}

	auto& mdec = m_psx->GetStatus().sysbus->m_mdec;
	auto stat = mdec.m_stat;

	ImGui::Text("Current command: %s", magic_enum::enum_name(mdec.m_curr_cmd)
		.data());

	ImGui::Text("Missing params : %d", stat.missing_params);
	ImGui::Text("Curr block     : %s", magic_enum::enum_name(stat.curr_block)
		.data());
	ImGui::Text("Set mask bit   : %s", stat.data_out_bit15 ? "true" : "false");
	ImGui::Text("Signed output  : %s", stat.data_out_signed ? "true" : "false");
	ImGui::Text("Color depth    : %s", magic_enum::enum_name(stat.out_depth)
		.data());
	ImGui::Text("DMA out        : %s", stat.data_out_request ? "true" : "false");
	ImGui::Text("DMA in         : %s", stat.data_in_request ? "true" : "false");
	ImGui::Text("CMD Busy       : %s", stat.cmd_busy ? "true" : "false");
	ImGui::Text("Data in full   : %s", stat.data_in_full ? "true" : "false");
	ImGui::Text("Data out empty : %s", stat.data_out_empty ? "true" : "false");

	ImGui::End();
}

static void ShowTestCommand(psx::DriveCommand const* cmd) {
	const auto yellow = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);

	switch (cmd->params[0])
	{
	case 0x20:
	{
		ImGui::TextColored(yellow, "Bios date : %X/%X/%X, Version : %X",
			cmd->responses[0].fifo.fifo[0], cmd->responses[0].fifo.fifo[1], 
			cmd->responses[0].fifo.fifo[2], cmd->responses[0].fifo.fifo[3]);
	}
		break;
	default:
		ImGui::TextColored(yellow, "Unknown test command");
		break;
	}
}

void DebugView::ShowDriveCommand(psx::DriveCommand const* cmd) {
	switch (cmd->command_id)
	{
	case 0x1A: {
		//Test commands
		ShowTestCommand(cmd);
	}
		break;
	default:
		break;
	}
}
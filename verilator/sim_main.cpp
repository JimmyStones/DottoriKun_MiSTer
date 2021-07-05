#include <verilated.h>
#include "Vtop.h"

#include "imgui.h"
#ifndef _MSC_VER
#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>
#else
#define WIN32
#include <dinput.h>
#endif

#include "sim_console.h"
#include "sim_bus.h"
#include "sim_video.h"
#include "sim_input.h"
#include "sim_clock.h"

#include "../imgui/imgui_memory_editor.h"

// Debug GUI 
// ---------
const char* windowTitle = "Verilator Sim: Arcade-DottoriKun";
bool showDebugWindow = true;
const char* debugWindowTitle = "Virtual Dev Board v1.0";
DebugConsole console;
MemoryEditor memoryEditor_hs;

// HPS emulator
// ------------
SimBus bus(console);

// Input handling
// --------------
SimInput input(12);
const int input_right = 0;
const int input_left = 1;
const int input_down = 2;
const int input_up = 3;
const int input_fire1 = 4;
const int input_fire2 = 5;
const int input_start_1 = 6;
const int input_start_2 = 7;
const int input_coin_1 = 8;
const int input_coin_2 = 9;
const int input_coin_3 = 10;
const int input_pause = 11;

// Video
// -----
#define VGA_WIDTH 256
#define VGA_HEIGHT 192
#define VGA_ROTATE 0 
#define VGA_SCALE_X 4.0
#define VGA_SCALE_Y 3.0
SimVideo video(VGA_WIDTH, VGA_HEIGHT, VGA_ROTATE);

// Simulation control
// ------------------
int initialReset = 48;
bool run_enable = 1;
int batchSize = 25000000 / 10000;
bool single_step = 0;
bool multi_step = 0;
int multi_step_amount = 1024;

// Verilog module
// --------------
Vtop* top = NULL;

vluint64_t main_time = 0;	// Current simulation time.
double sc_time_stamp() {	// Called by $time in Verilog.
	return main_time;
}

int clockSpeed = 8; // This is not used, just a reminder for the dividers below
SimClock clk_sys(1); // 4mhz
SimClock clk_pix(0); // 4mhz

void resetSim() {
	main_time = 0;
	top->reset = 1;
	clk_sys.Reset();
	clk_pix.Reset();
}

int verilate() {

	if (!Verilated::gotFinish()) {

		// Assert reset during startup
		if (main_time < initialReset) { top->reset = 1; }
		// Deassert reset after startup
		if (main_time == initialReset) { top->reset = 0; }

		// Clock dividers
		clk_sys.Tick();
		clk_pix.Tick();

		// Set system clock in core
		top->clk_4 = clk_sys.clk;

		// Update console with current cycle for logging
		console.prefix = "(" + std::to_string(main_time) + ") ";

		// Output pixels on rising edge of pixel clock
		if (clk_pix.clk && !clk_pix.old) {
			uint32_t colour = 0xFF000000 | top->VGA_B << 16 | top->VGA_G << 8 | top->VGA_R;
			video.Clock(top->VGA_HB, top->VGA_VB, colour);
		}

		//if (top->VGA_HS && top->VGA_VS) {
		//	console.AddLog("hsync x %x", video.count_pixel);
		//	console.AddLog("Vsync y %x", video.count_line);
		//}

		// Simulate both edges of system clock
		if (clk_sys.clk != clk_sys.old) {
			if (clk_sys.clk) { bus.BeforeEval(); }
			top->eval();
			if (clk_sys.clk) { bus.AfterEval(); }
		}

		main_time++;

		return 1;
	}
	// Stop verilating and cleanup
	top->final();
	delete top;
	exit(0);
	return 0;
}

int main(int argc, char** argv, char** env) {

	// Create core and initialise
	top = new Vtop();
	Verilated::commandArgs(argc, argv);

#ifdef WIN32
	// Attach debug console to the verilated code
	Verilated::setDebug(console);
#endif

	// Attach bus
	bus.ioctl_addr = &top->ioctl_addr;
	bus.ioctl_index = &top->ioctl_index;
	bus.ioctl_wait = &top->ioctl_wait;
	bus.ioctl_download = &top->ioctl_download;
	bus.ioctl_upload = &top->ioctl_upload;
	bus.ioctl_wr = &top->ioctl_wr;
	bus.ioctl_dout = &top->ioctl_dout;
	bus.ioctl_din = &top->ioctl_din;

	// Set up input module
	input.Initialise();
#ifdef WIN32
	input.SetMapping(input_up, DIK_UP);
	input.SetMapping(input_right, DIK_RIGHT);
	input.SetMapping(input_down, DIK_DOWN);
	input.SetMapping(input_left, DIK_LEFT);
	input.SetMapping(input_fire1, DIK_LCONTROL);
	input.SetMapping(input_fire2, DIK_LALT);
	input.SetMapping(input_start_1, DIK_1);
	input.SetMapping(input_coin_1, DIK_5);
#else
	input.SetMapping(input_up, SDL_SCANCODE_UP);
	input.SetMapping(input_right, SDL_SCANCODE_RIGHT);
	input.SetMapping(input_down, SDL_SCANCODE_DOWN);
	input.SetMapping(input_left, SDL_SCANCODE_LEFT);
	input.SetMapping(input_fire1, SDL_SCANCODE_LCONTROL);
	input.SetMapping(input_fire2, SDL_SCANCODE_LALT);
	input.SetMapping(input_start_1, SDL_SCANCODE_1);
	input.SetMapping(input_coin_1, SDL_SCANCODE_3);
#endif
	// Setup video output
	if (video.Initialise(windowTitle) == 1) { return 1; }

	// Reset simulation 
	resetSim();

	// Stage roms for this core
	//bus.QueueDownload("roms/dotriman/14479a.mpr", 0);
	bus.QueueDownload("roms/14479a.mpr", 0);
	//bus.QueueDownload("roms/14479a.103", 0);


#ifdef WIN32
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			continue;
		}
#else
	bool done = false;
	while (!done)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
		}
#endif
		video.StartFrame();

		input.Read();


		// Draw GUI
		// --------
		ImGui::NewFrame();

		console.Draw("Debug Log", &showDebugWindow);
		ImGui::Begin(debugWindowTitle);
		/*ImGui::SetWindowPos(debugWindowTitle, ImVec2(580, 10), ImGuiCond_Once);
		ImGui::SetWindowSize(debugWindowTitle, ImVec2(1000, 1000), ImGuiCond_Once);*/

		if (ImGui::Button("RESET")) { resetSim(); } ImGui::SameLine();
		if (ImGui::Button("START")) { run_enable = 1; } ImGui::SameLine();
		if (ImGui::Button("STOP")) { run_enable = 0; } ImGui::SameLine();
		ImGui::Checkbox("RUN", &run_enable);
		ImGui::SliderInt("Batch size", &batchSize, 1, 250000);

		if (single_step == 1) { single_step = 0; }
		if (ImGui::Button("Single Step")) { run_enable = 0; single_step = 1; }
		ImGui::SameLine();
		if (multi_step == 1) { multi_step = 0; }
		if (ImGui::Button("Multi Step")) { run_enable = 0; multi_step = 1; }
		ImGui::SameLine();
		ImGui::SliderInt("Step amount", &multi_step_amount, 8, 1024);

		ImGui::SliderInt("Rotate", &video.output_rotate, -1, 1); ImGui::SameLine();
		ImGui::Checkbox("Flip V", &video.output_vflip);

		ImGui::Text("main_time: %d frame_count: %d sim FPS: %f", main_time, video.count_frame, video.stats_fps); 
		ImGui::Text("pixel: %d line: %d", video.count_pixel, video.count_line);

		// Draw VGA output
		ImGui::Image(video.texture_id, ImVec2(video.output_width * VGA_SCALE_X, video.output_height * VGA_SCALE_Y));
		ImGui::End();

		ImGui::Begin("RAM");
		memoryEditor_hs.DrawContents(&top->top__DOT__dottori__DOT__MEM_RAM__DOT__ram, 4096, 0);
		ImGui::End();

		ImGui::Begin("ROM");
		memoryEditor_hs.DrawContents(&top->top__DOT__dottori__DOT__MEM_ROM__DOT__ram, 4096, 0);
		ImGui::End();

		video.UpdateTexture();

		// Pass inputs to sim
		top->inputs = 0;
		for (int i = 0; i < input.inputCount; i++)
		{
			if (input.inputs[i]) { top->inputs |= (1 << i); }
		}
				
		// Run simulation
		if (run_enable) {
			for (int step = 0; step < batchSize; step++) { verilate(); }
		}
		else {
			if (single_step) { verilate(); }
			if (multi_step) {
				for (int step = 0; step < multi_step_amount; step++) { verilate(); }
			}
		}
	}

	// Clean up before exit
	// --------------------

	video.CleanUp();
	input.CleanUp();

	return 0;
}

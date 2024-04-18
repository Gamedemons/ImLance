#include "gui.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_stdlib.h"
#include "../lib/tinyfiledialogs.h"
#include "../lib/json.hpp"
#include <string>
#include <regex>
#include <filesystem>
#include <shlobj.h>
#include <windows.h>
#include <sys/stat.h>
#include <fstream>
#include <chrono>
#include "lance_utils.h"
#include "lance_ini.h"

using std::filesystem::directory_iterator;
using std::filesystem::path;
using namespace std::chrono;
using std::string;
using std::to_string;
using std::vector;
using std::ifstream;
using std::ofstream;
using json = nlohmann::json;

#define IM_CLAMP(V, MN, MX)     ((V) < (MN) ? (MN) : (V) > (MX) ? (MX) : (V))
#define IM_NEWLINE  "\n"


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND window, UINT message, WPARAM wideParameter, LPARAM longParameter
);

typedef struct RgbColor
{
	unsigned char r;
	unsigned char g;
	unsigned char b;
} RgbColor;
typedef struct HsvColor
{
	unsigned char h;
	unsigned char s;
	unsigned char v;
} HsvColor;

long __stdcall WindowProcess(HWND window, UINT message, WPARAM wideParameter, LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}
void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WIDTH,
		HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}
void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}
bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}
void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}
void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}
void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}
void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}
void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}
void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}
void gui::Render(int iniTheme) noexcept
{
	// Static 
	static bool firstTimeRun = true;
	static bool show_app_style_editor = false;
	static bool show_app_settings = false;
	static bool show_app_console = true;
	static bool show_stat = false;
	static bool aura_breathing = false;
	static bool progressbar_anim = false;
	static bool show_path_for_chaptername_previews = false;

	static short int xPos = 10;
	static short int yPos = 60;
	static short int lGap = 35;
	static short int leftLayoutWidth = 350;
					
	static char outputFileName[250] = "";
	static std::vector outputFileExtensions = { ".txt", ".md" };
	static char seperator[1000] = "\n\n\n\n\n";
	static std::vector<std::string> realFileNames;
	static std::vector<std::string> oldFileNames;										// Old File Names
	static std::vector<std::string> newFileNames;										// New File Names
	static char outputPreviewText[9000000] = "";										// Main Output Text
	static char chapterText[500000] = "";												// Split Output Text by Chapters
	static char msgLabel[500] = "";

	static bool enable_markdown_format = true;

	static long stat_cumulative_file_size = 0;
	static int stat_selected_chapter_index = 0;
	static std::string stat_selected_chapter_name = "";
	static std::string stat_selected_chapter_firstline = "";
	static long stat_selected_chapter_size = 0;
	static long stat_selected_chapter_word_count = 0;
	static long stat_selected_chapter_line_count = 0;

	// Global Style Vars
	static int theme = 1;

	if (firstTimeRun) {
		theme = iniTheme;
	}

	ImGuiStyle& style = ImGui::GetStyle();
	{
		style.FrameRounding = 3.0f;
		style.WindowPadding = ImVec2(0, 0);
		style.FramePadding = ImVec2(4.0, 6.0);
		style.GrabRounding = 1.0;
		//style.ScrollbarRounding = 7.0;
		if (theme == 0)	// Default Theme
		{
			style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255, 255);
			style.Colors[ImGuiCol_TextDisabled] = ImColor(128, 128, 128, 255);
			style.Colors[ImGuiCol_WindowBg] = ImColor(15, 15, 15, 240);
			style.Colors[ImGuiCol_PopupBg] = ImColor(20, 20, 20, 240);
			style.Colors[ImGuiCol_FrameBg] = ImColor(41, 74, 122, 138);
			style.Colors[ImGuiCol_TitleBg] = ImColor(10, 10, 10, 255);
			style.Colors[ImGuiCol_TitleBgActive] = ImColor(41, 74, 122, 255);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(5, 5, 5, 135);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(79, 79, 79, 255);
			style.Colors[ImGuiCol_Tab] = ImColor(46, 89, 148, 220);
			style.Colors[ImGuiCol_TabActive] = ImColor(51, 105, 173, 255);
			style.Colors[ImGuiCol_PlotHistogram] = ImColor(230, 179, 0);
		}
		else if (theme == 1) // Dracula Theme
		{
			style.Colors[ImGuiCol_Text] = ImColor(255, 255, 255, 255);
			style.Colors[ImGuiCol_TextDisabled] = ImColor(128, 128, 128, 255);
			style.Colors[ImGuiCol_WindowBg] = ImColor(37, 37, 37);
			style.Colors[ImGuiCol_PopupBg] = ImColor(20, 20, 20, 240);
			style.Colors[ImGuiCol_FrameBg] = ImColor(41, 74, 122, 138);
			style.Colors[ImGuiCol_TitleBg] = ImColor(10, 10, 10, 255);
			style.Colors[ImGuiCol_TitleBgActive] = ImColor(41, 74, 122, 255);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(5, 5, 5, 135);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(79, 79, 79, 255);
			style.Colors[ImGuiCol_Tab] = ImColor(46, 89, 148, 220);
			style.Colors[ImGuiCol_TabActive] = ImColor(51, 105, 173, 255);
			style.Colors[ImGuiCol_PlotHistogram] = ImColor(117, 255, 121);
		}
		else if (theme == 2) // Violet Candy
		{
			style.Colors[ImGuiCol_Text] = ImColor(0, 0, 0, 255);
			style.Colors[ImGuiCol_TextDisabled] = ImColor(55, 55, 55, 255);
			style.Colors[ImGuiCol_WindowBg] = ImColor(211, 208, 255, 255);
			style.Colors[ImGuiCol_PopupBg] = ImColor(215, 203, 255, 255);
			style.Colors[ImGuiCol_FrameBg] = ImColor(255, 255, 255, 255);
			style.Colors[ImGuiCol_TitleBg] = ImColor(122, 102, 255, 255);
			style.Colors[ImGuiCol_TitleBgActive] = ImColor(177, 129, 255, 255);
			style.Colors[ImGuiCol_ScrollbarBg] = ImColor(255, 255, 255, 0);
			style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(89, 89, 89, 255);
			style.Colors[ImGuiCol_Tab] = ImColor(131, 184, 255, 220);
			style.Colors[ImGuiCol_TabActive] = ImColor(104, 133, 255, 255);
			style.Colors[ImGuiCol_PlotHistogram] = ImColor(132, 90, 255, 255);
		}
	}

	// Window Settings
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WIDTH, HEIGHT }); // 1280 x 720
	ImGui::Begin(
		"Lance",
		&isRunning,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoBringToFrontOnFocus
	);

	// Code here -------------------------------------------------------------------------------------------------------------------------|


	// Frame Rate Label - 6 for half of 24 (button height)
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( WIDTH - 200.0 ),
		30 + 0
	));
	ImGui::Text("RT : %.1f ms/fr (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);


	// Task Completion Label - 6 for half of 24 (button height)
	static long taskCompletionTime = 0;
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( WIDTH - 200.0 ),
		30 + 12
	));
	ImGui::Text("CT : %d ms", taskCompletionTime);


	// Style Editor
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( WIDTH - 300.0 ),
		30
	));
	if (ImGui::Button("Style Editor", ImVec2(90, 25))) {
		show_app_style_editor = !show_app_style_editor;
	}
	if (show_app_style_editor)
	{
		ImGui::Begin("Style Editor", &show_app_style_editor);
		ImGui::ShowStyleEditor();
		ImGui::End();
	}


	// Settings
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( WIDTH - 400.0 ),
		30
	));
	if (ImGui::Button("Settings", ImVec2(90, 25))) {
		show_app_settings = true;
	}
	if (show_app_settings)
	{
		ImGui::SetNextWindowPos({ 0, 0 });
		ImGui::SetNextWindowSize({ WIDTH, HEIGHT }); // 1280 x 720
		ImGui::Begin(
			"Lance Settings",
			&show_app_settings,
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse
		);

		ImGui::SetCursorPos(ImVec2(xPos, 35));
		ImGui::PushID("aura_breathing");
		ImGui::Checkbox("Aura Breathing", &aura_breathing);
		ImGui::PopID();

		ImGui::SetCursorPos(ImVec2(xPos, 35 * 2));
		ImGui::PushID("progressbar_anim");
		ImGui::Checkbox("Enable Progress Bar Animation", &progressbar_anim);
		ImGui::PopID();

		ImGui::SetCursorPos(ImVec2(xPos, 35 * 3));
		ImGui::PushID("show_path_for_chaptername_previews");
		ImGui::Checkbox("Show Paths for File Name Previews", &show_path_for_chaptername_previews);
		ImGui::PopID();

		ImGui::End();
	}


	// Theme
	const char* themeType[] = { "No Theme", "Drakula", "Violet Candy" };
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( WIDTH - 600.0 ),
		30
	));
	ImGui::PushItemWidth(190);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 5));
	if (ImGui::Combo("##themeCombo", &theme, themeType, IM_ARRAYSIZE(themeType))) {
		lance_ini::saveTheme(theme);
	}

	ImGui::PopStyleVar();


	// File Type Combo
	static int currentFilePickType = 0;																		// Current File Pick Type
	const char* filePickTypes[] = { "Select Folder", "Select Files" };										// File Pick Types
	ImGui::SetCursorPos(ImVec2(
		xPos, yPos
	));
	ImGui::PushItemWidth(350);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 5));
	ImGui::Combo("##filePickCombo", &currentFilePickType, filePickTypes, IM_ARRAYSIZE(filePickTypes));
	ImGui::PopStyleVar();


	// File Picker
	/*static char inputLocation[50000] = "Select Input File/Folder";*/
	static char inputLocation[50000] = "D:\\Z";
	static char inputFiles[5000000] = "";
	if (currentFilePickType == 0) {
		ImGui::SetCursorPos(ImVec2(
			lance::toShint( xPos ),
			lance::toShint( yPos + (lGap * 1.0) )
		));
		ImGui::PushItemWidth(315);
		ImGui::InputTextWithHint("##inputLocationLabel1", "Select Input Directory", inputLocation, IM_ARRAYSIZE(inputLocation), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		ImGui::SetCursorPos(ImVec2(
			lance::toShint( 335.0 ),
			lance::toShint( yPos + (lGap * 1.0) )
		));
		if (ImGui::Button("...##inputLocationButton1", ImVec2(25, 25))) {
			try {
				char* res = tinyfd_selectFolderDialog("Pick Input Folder", NULL);
				if (res == NULL) {
					throw "Invalid Path";
				}
				strcpy_s(inputLocation, res);
				// Adding names to old files list
				/*vector<string> fileNames = lance::getFileNames(inputLocation + std::string(""));
				static int index = 0;
				for (auto& chapterPath : fileNames) {
					oldFileNames[index] = chapterPath.c_str();
					index++;
				}*/
			}
			catch (...) {
				strcpy_s(msgLabel, "Error : Invalid Input Path");
			}
		}
	}
	else
	{
		ImGui::SetCursorPos(ImVec2(
			lance::toShint( xPos ),
			lance::toShint( yPos + (lGap * 1.0) )
		));
		ImGui::PushItemWidth(315);
		ImGui::InputTextWithHint("##inputLocationLabel2", "Select Input Files", inputFiles, IM_ARRAYSIZE(inputFiles), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		ImGui::SetCursorPos(ImVec2(
			lance::toShint( 335.0 ),
			lance::toShint( yPos + (lGap * 1.0) )
		));
		if (ImGui::Button("...##inputLocationButton2", ImVec2(25, 25))) {
			try {
				auto res = tinyfd_openFileDialog("Pick Input Files", NULL, 0, NULL, NULL, 1);
				if (res == NULL) {
					throw "Invalid Selections";
				}
				strcpy_s(inputFiles, res);
			}
			catch (...) {
				strcpy_s(msgLabel, "Error : Invalid Input Selections");
			}
		}
	}


	// Output Picker
	/*static char outputLocation[50000] = "Select Output Folder";*/
	static char outputLocation[50000] = "D:\\Z";
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 2.0)));
	ImGui::PushItemWidth(315);
	ImGui::InputTextWithHint("##outputLocationLabel", "Select Output Directory", outputLocation, IM_ARRAYSIZE(outputLocation), ImGuiInputTextFlags_ReadOnly);
	ImGui::SameLine();
	ImGui::SetCursorPos(ImVec2(335, yPos + (lGap * 2.0)));
	if (ImGui::Button("...##outputLocationButton", ImVec2(25, 25))) {
		try {
			char* res = tinyfd_selectFolderDialog("Pick Output Folder", "");
			if (res == NULL) {
				throw "Invalid Path";
			}
			strcpy_s(outputLocation, res);
		}
		catch (...) {
			strcpy_s(msgLabel, "Error : Invalid Output Path");
		}
	}


	// Remove Text
	static bool removeCheck = false;
	static char removeText[1000] = "";
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 3.0)));
	ImGui::Checkbox("Remove", &removeCheck);
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 4.0)));
	ImGui::PushItemWidth(leftLayoutWidth);
	if (!removeCheck) {
		ImGui::BeginDisabled();
	}
	ImGui::InputTextWithHint("##removeInput", "Enter text to remove", removeText, IM_ARRAYSIZE(removeText));
	if (!removeCheck) {
		ImGui::EndDisabled();
	}


	// Replace Text
	static bool replaceCheck = false;
	static char replaceText1[1000] = "";
	static char replaceText2[1000] = "";
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 5.0)));
	ImGui::Checkbox("Replace", &replaceCheck);
	if (!replaceCheck) {
		ImGui::BeginDisabled();
	}
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 6.0)));
	ImGui::PushItemWidth(leftLayoutWidth);
	ImGui::InputTextWithHint("##replaceInput1", "Text to be replaced", replaceText1, IM_ARRAYSIZE(replaceText1));
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 7.0)));
	ImGui::PushItemWidth(leftLayoutWidth);
	ImGui::InputTextWithHint("##replaceInput2", "Replacement", replaceText2, IM_ARRAYSIZE(replaceText2));
	if (!replaceCheck) {
		ImGui::EndDisabled();
	}


	// Renamer
	static short int rnPosX = 15;
	static short int rnWidth = 340;

	static char renamerText1[1000] = "";
	static char renamerText2[1000] = "";
	static char renamerText3[1000] = "";
	static char renamerText4[1000] = "";
	static char renamerText5[1000] = "";

	static bool renameCheck = false;
	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 8.0) + 6));
	ImGui::Checkbox("Renamer", &renameCheck);
	if (!renameCheck) {
		ImGui::BeginDisabled();
	}
	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 9.0) + 6));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput1", "Prefix", renamerText1, IM_ARRAYSIZE(renamerText1));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 10.0) + 7));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput2", "Suffix", renamerText2, IM_ARRAYSIZE(renamerText2));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 11.0) + 8));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput3", "Regex", renamerText3, IM_ARRAYSIZE(renamerText3));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 12.0) + 9));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput4", "Numbering - index:start:increment:seperator", renamerText4, IM_ARRAYSIZE(renamerText4));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 13.0) + 10));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput5", "Title - title:mode[prefix,fixed,suffix]", renamerText5, IM_ARRAYSIZE(renamerText5));
	if (!renameCheck) {
		ImGui::EndDisabled();
	}


	// Renamer BG
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 8.0)));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(255, 255, 255, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(255, 255, 255, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(255, 255, 255, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0);
	ImGui::Button("##renamerBG", ImVec2(leftLayoutWidth, yPos + (lGap * 4.5)));
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();


	// Blank Line Options
	const char* blankLineOptions[] = {
		"Remove Blank Lines - None",
		"Remove Blank Lines - Extra",
		"Remove Blank Lines - All"
	};
	static int currentBlankLine = 0;
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 14.5)));
	ImGui::PushItemWidth(leftLayoutWidth);
	ImGui::Combo("##blankLineCombo", &currentBlankLine, blankLineOptions, IM_ARRAYSIZE(blankLineOptions));


	// File Output Type Options
	const char* fileOutputTypeOptions[] = {
		"File Output Type - Single",
		"File Output Type - Multiple",
	};
	static int currentFileOutput = 0;
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 15.5)));
	ImGui::PushItemWidth(leftLayoutWidth);
	ImGui::Combo("##fileOutputTypeCombo", &currentFileOutput, fileOutputTypeOptions, IM_ARRAYSIZE(fileOutputTypeOptions));


	// Preview Button
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 16.5) + 2));
	if (ImGui::Button("Preview", ImVec2((short int)((leftLayoutWidth - 10) / 2), 25 * 2))) {
		strcpy_s(msgLabel, "");
		try {
			std::string outputPreview = "";
			std::string seperator = "\n\n\n\n\n";

			vector<string> filePath;
			if (currentFilePickType == 0) {
				filePath = lance::getFileNames(inputLocation);
			}
			else {
				filePath = lance::getFileNames(inputFiles + std::string(""));
			}

			realFileNames.clear();
			oldFileNames.clear();
			newFileNames.clear();
			stat_cumulative_file_size = 0;
			for (auto& chapterPath : filePath) {
				realFileNames.push_back(chapterPath);
				oldFileNames.push_back(lance::extractOldName(chapterPath, show_path_for_chaptername_previews));
				newFileNames.push_back(lance::fRenameFile(
					renameCheck,
					chapterPath,
					show_path_for_chaptername_previews,
					renamerText1,
					renamerText2,
					renamerText3,
					renamerText4,
					renamerText5
				));

				std::string chapter = lance::getFileContents(chapterPath);
				long long chapter_size = lance::getFileSize(chapterPath);
				stat_cumulative_file_size += chapter_size;
				if (chapter_size + outputPreview.length() < 9000000) {
					outputPreview += chapter + seperator;
				}
			}

			strcpy_s(outputPreviewText, outputPreview.c_str());
		}
		catch (...) {
			strcpy_s(msgLabel, "Error : Invalid Inputs");
		}
	}


	// Output Button
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( xPos + (leftLayoutWidth + 10.0) / 2.0 ),
		lance::toShint( yPos + (lGap * 16.5) + 2 )
	));
	if(ImGui::Button("Generate Output", ImVec2((leftLayoutWidth - 10) / 2, 25 * 2)))
	{
		strcpy_s(msgLabel, "");
		string time = "";
		long ms1 = lance::getCurrentTime('m');

		try {
			vector<string> filePath;
			if (currentFilePickType == 0) {
				filePath = lance::getFileNames(inputLocation);
			}
			else {
				filePath = lance::getFileNames(inputFiles + std::string(""));
			}

			if (strlen(outputLocation) == 0) {
				throw "Error : Invalid Output Path";
			}

			std::string customFilePath = outputLocation + std::string("\\") + outputFileName;
			if (strlen(outputFileName) == 0) {
				std::filesystem::path c1(filePath.front());
				std::filesystem::path c2(filePath.back());
				customFilePath = customFilePath + c1.stem().string() + " - " + c2.stem().string();
			}

			if (enable_markdown_format == true) {
				customFilePath += outputFileExtensions[1];
			}
			else {
				customFilePath += outputFileExtensions[0];
			}

			std::ofstream outputFile(customFilePath, std::ios::app);

			realFileNames.clear();
			oldFileNames.clear();
			newFileNames.clear();

			for (auto& chapterPath : filePath) {
				realFileNames.push_back(chapterPath);
				oldFileNames.push_back(lance::extractOldName(chapterPath, show_path_for_chaptername_previews));
				newFileNames.push_back(lance::fRenameFile(
					renameCheck,
					chapterPath,
					show_path_for_chaptername_previews,
					renamerText1,
					renamerText2,
					renamerText3,
					renamerText4,
					renamerText5
				));

				if (!outputFile.fail())
				{
					std::string chapter = lance::getFileContents(chapterPath);
					chapter = lance::formatChapter(
						chapter,
						removeCheck,
						removeText,
						replaceCheck,
						replaceText1,
						replaceText2,
						currentBlankLine
					);

					if (enable_markdown_format == true) {
						chapter = "# " + chapter;
					}

					outputFile << chapter;
					outputFile << seperator;
				}
			}

			outputFile.close();
		}
		catch (...) {
			strcpy_s(msgLabel, "Error : Invalid Inputs");
		}

		long ms2 = lance::getCurrentTime('m');
		taskCompletionTime = ms2 - ms1;
	}


	// Tabs
	static short int menuPosX = 370;
	static short int menuPosY = 95;
	static short int menuGap = 5; // Margin in X
	static short int tabGap = 55; // Margin in Y

	ImGui::SetCursorPos(ImVec2(menuPosX, yPos));
	if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
	{
		// Configuration Tab
		if (ImGui::BeginTabItem("Configuration")) {

			ImGui::SetCursorPos(ImVec2(menuPosX, menuPosY));
			ImGui::PushItemWidth(905);
			ImGui::InputTextWithHint("##outputFileName", "Enter Custom Filename", outputFileName, IM_ARRAYSIZE(outputFileName));

			ImGui::SetCursorPos(ImVec2(menuPosX, menuPosY + 35));
			ImGui::PushID("enable_markdown_format");
			ImGui::Checkbox("Output in Markdown Format instead of Text Format", &enable_markdown_format);
			ImGui::PopID();

			ImGui::SetCursorPos(ImVec2(menuPosX, menuPosY + 35*2));
			ImGui::InputTextMultiline("##chapterseperator", seperator, IM_ARRAYSIZE(seperator), ImVec2(905, 60), ImGuiInputTextFlags_AllowTabInput);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Enter Custom Chapter Seperator");


			// custom regex, file tools(hasing, encoding conversation, url entity decode)
			// text tools

			ImGui::EndTabItem();
		}

		// Preview Tab
		if (ImGui::BeginTabItem("Preview"))
		{
			ImGui::SetCursorPosX(menuPosX);
			ImGui::InputTextMultiline("##source", outputPreviewText, IM_ARRAYSIZE(outputPreviewText), ImVec2(910, 600), ImGuiInputTextFlags_ReadOnly);
			ImGui::EndTabItem();
		}

		// Chapter Tab
		if (ImGui::BeginTabItem("Chapters"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
			static short int elemHeight = 574;
			static short int chapterBoxWidth = 200;
			static short int chapterAreaWidth = menuPosX + chapterBoxWidth + menuGap;


			// Listbox Labels
			static char chNumLabel[20] = "Chapters";
			ImGui::SetCursorPosX(menuPosX);
			ImGui::PushItemWidth(chapterBoxWidth);
			ImGui::InputText("##chpaterNumberLabel", chNumLabel, IM_ARRAYSIZE(chNumLabel), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			static char chPreLabel[20] = "Chapter Preview";
			ImGui::SetCursorPosX(chapterAreaWidth);
			ImGui::PushItemWidth(WIDTH - chapterAreaWidth);
			ImGui::InputText("##chpaterPreviewLabel", chPreLabel, IM_ARRAYSIZE(chPreLabel), ImGuiInputTextFlags_ReadOnly);


			// Listbox and Textarea
			static int oldChapterName_Index = 0;						// Currently Selected Chapter
			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + tabGap));
			if (ImGui::BeginListBox("##ChapterNumberListBox", ImVec2(chapterBoxWidth, elemHeight)))
			{
				int n = 0;
				for (string item : realFileNames) {
					const bool is_selected = (oldChapterName_Index == n);
					if (ImGui::Selectable(realFileNames[n].c_str(), is_selected)) {
						oldChapterName_Index = n;
						stat_selected_chapter_index = n;
						if (realFileNames.size() > 0) {
							string selectedIndexPath = realFileNames.at(oldChapterName_Index);
							stat_selected_chapter_name = lance::extractOldName(selectedIndexPath, false);
							long long selected_chapter_size = lance::getFileSize(selectedIndexPath);
							stat_selected_chapter_size = selected_chapter_size;
							std::ifstream ch_first_line(selectedIndexPath);
							std::getline(ch_first_line, stat_selected_chapter_firstline);
							if (selected_chapter_size < 490000) {
								string contents = lance::getFileContents(selectedIndexPath);
								strcpy_s(chapterText, contents.c_str());
							}
							else {
								string chapterMessage = "\"" + selectedIndexPath + "\" is too long to show";
								strcpy_s(chapterText, chapterMessage.c_str());
							}
						}
					}

					if (is_selected) {
						ImGui::SetItemDefaultFocus();
						if (realFileNames.size() > 0) {
							string selectedIndexPath = realFileNames.at(oldChapterName_Index);
							stat_selected_chapter_name = lance::extractOldName(selectedIndexPath, false);
							long long selected_chapter_size = lance::getFileSize(selectedIndexPath);
							stat_selected_chapter_size = selected_chapter_size;
							std::ifstream ch_first_line(selectedIndexPath);
							std::getline(ch_first_line, stat_selected_chapter_firstline);
							if (selected_chapter_size < 490000) {
								string contents = lance::getFileContents(selectedIndexPath);
								strcpy_s(chapterText, contents.c_str());
							}
							else {
								string chapterMessage = "\"" + selectedIndexPath + "\" is too long to show";
								strcpy_s(chapterText, chapterMessage.c_str());
							}
						}
					}
						
					n++;
				}
				ImGui::EndListBox();
			}

			ImGui::SetCursorPos(ImVec2(chapterAreaWidth, yPos + tabGap));
			static ImGuiInputTextFlags flags = ImGuiInputTextFlags_ReadOnly;
			ImGui::InputTextMultiline("##source", chapterText, IM_ARRAYSIZE(chapterText), ImVec2((short int)(WIDTH - chapterAreaWidth), elemHeight), flags);

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}

		// Chapter Info Tab
		if (ImGui::BeginTabItem("Chapter Info"))
		{
			ImGui::SetCursorPosX(menuPosX);
			ImGui::Text("Input");
			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 45));
			if (currentFilePickType == 0) {
				ImGui::InputTextMultiline("##stat_input", inputLocation, IM_ARRAYSIZE(inputLocation), ImVec2(905, 25), ImGuiInputTextFlags_ReadOnly);
			}
			else {
				ImGui::InputTextMultiline("##stat_input", inputFiles, IM_ARRAYSIZE(inputFiles), ImVec2(905, 25), ImGuiInputTextFlags_ReadOnly);
			}
			
			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30));
			ImGui::Text("Output Directory");
			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 45));
			ImGui::InputTextMultiline("##stat_output", outputLocation, IM_ARRAYSIZE(outputLocation), ImVec2(905, 25), ImGuiInputTextFlags_ReadOnly);

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 3));
			ImGui::Text("Total File Count : %d", realFileNames.size());

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 4));
			ImGui::Text("Cumulative File Size : %ld bytes", stat_cumulative_file_size);

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 5));
			ImGui::Text("Currently Selected Chapter Index : %d", stat_selected_chapter_index);

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 6));
			ImGui::Text("Currently Selected Chapter Name : %s", stat_selected_chapter_name.c_str());

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 7));
			ImGui::Text("Currently Selected Chapter First Line : %s", stat_selected_chapter_firstline.c_str());

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 8));
			ImGui::Text("Currently Selected Chapter Size : %ld bytes", stat_selected_chapter_size);

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 9));
			ImGui::Text("Currently Selected Chapter Word Count         : %ld", stat_selected_chapter_word_count);
			ImGui::SetCursorPos(ImVec2(menuPosX + 265, yPos + 45 + 30 * 9));
			if (ImGui::Button("Count##word_count", ImVec2(50, 25))) {
				if(realFileNames.size() > 0) stat_selected_chapter_word_count = lance::getWordCount(realFileNames[stat_selected_chapter_index]);
			}

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + 50 + 30 * 10));
			ImGui::Text("Currently Selected Chapter Line Count         : %ld", stat_selected_chapter_line_count);
			ImGui::SetCursorPos(ImVec2(menuPosX + 265, yPos + 45 + 30 * 10));
			if (ImGui::Button("Count##line_count", ImVec2(50, 25))) {
				if(realFileNames.size() > 0) stat_selected_chapter_line_count = lance::getLineCount(realFileNames[stat_selected_chapter_index]);
			}

			ImGui::EndTabItem();
		}

		// Filename Preview Tab
		if (ImGui::BeginTabItem("Filename Preview"))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
			// 2*elemWidth + elemGap = 910 ( 1280 - 370 )
			static short int elemWidth = 458;
			static short int elemHeight = 574;

			// Listbox Labels
			static char oldFileLabel[20] = "Old File Name";
			ImGui::SetCursorPosX(menuPosX);
			ImGui::PushItemWidth(elemWidth);
			ImGui::InputText("##oldFileLabel", oldFileLabel, IM_ARRAYSIZE(oldFileLabel), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			static char newFileLabel[20] = "New File Name";
			ImGui::SetCursorPosX(menuPosX + elemWidth + menuGap);
			ImGui::PushItemWidth(elemWidth);
			ImGui::InputText("##newFileLabel", newFileLabel, IM_ARRAYSIZE(newFileLabel), ImGuiInputTextFlags_ReadOnly);

			// Filename Listboxes
			static int current_filename_idx = 0;														// Currently Selected Filename
			ImGui::SetCursorPos(ImVec2(menuPosX, (short int)(yPos + tabGap)));
			if (ImGui::BeginListBox("##OldFileListBox", ImVec2(elemWidth, elemHeight)))
			{
				int n = 0;
				for (string item : oldFileNames) {
					const bool is_selected = (current_filename_idx == n);
					if (ImGui::Selectable(oldFileNames[n].c_str(), is_selected))
						current_filename_idx = n;

					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}

					n++;
				}
				ImGui::EndListBox();
			}
			ImGui::SetCursorPos(ImVec2((short int)(menuPosX + elemWidth + menuGap), (short int)(yPos + tabGap)));
			if (ImGui::BeginListBox("##NewFileListBox", ImVec2(elemWidth, elemHeight)))
			{
				int n = 0;
				for (string item : newFileNames) {
					const bool is_selected = (current_filename_idx == n);
					if (ImGui::Selectable(newFileNames[n].c_str(), is_selected))
						current_filename_idx = n;

					if (is_selected) {
						ImGui::SetItemDefaultFocus();
					}

					n++;
				}
				ImGui::EndListBox();
			}

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}

		// Logs Tab
		if (ImGui::BeginTabItem("Logs")) {
			lance::ShowExampleAppConsole(&show_app_console);
			ImGui::EndTabItem();
		}

		// Help Tab
		if (ImGui::BeginTabItem("Help")) {
			char help[5000] = 
				"SECTION 1 : Lance\n"
				"ImLance or simply Lance is a bulk text files editor. It is made to handle and merge seperate novel chapters. \n"
				"Merging, Text Removal, Text Replacement are its main features.\n"
				"\n"
				"SECTION 2 : Why I made Lance ?\n"
				"I read a lot of webnovels and lightnovels. Each chapter within these novels is saved as a text file.\n"
				"Now I faced two major problems.\n"
				"Some novels can have as many as 4000 chapters, which means 4000 individual text files. \n"
				"Manually Opening and Closing each chapter is already very cumbersome, not to mention poor Portability and Management Issues.\n"
				"The second problem was that there were 3 blank spaces between each line, which reduced readability.\n"
				"So I made lance to fix similar issues.\n"
				"\n"
				"SECTION 3 : What Lance can do ?\n"
				"Lance can remove or replace specified words from all input text files.\n"
				"It can rename all input text files based on custom parameters.\n"
				"It can remove extra blank spaces from all input text files.\n"
				"It can merge all input text files into one file.\n"
				"It can output files in Markdown format for easier readability and chapter indexing.\n"
				"It can indent, trim, comment, convert case, remove duplicate lines, sort lines, reverse line order and much more !\n"
				"\n"
				"SECTION 4 : Lance Basics\n"
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				""
				"";

			ImGui::SetCursorPosX(menuPosX);
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(255, 255, 255, 0));
			ImGui::InputTextMultiline("##helpTab", help, IM_ARRAYSIZE(help), ImVec2(910, 600), ImGuiInputTextFlags_ReadOnly);
			ImGui::PopStyleColor();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}


	// Progress Bar
	static float progress = 0.0f, progress_dir = 1.0f;
	if (progressbar_anim)
	{
		progress += progress_dir * 0.4f * ImGui::GetIO().DeltaTime;
		if (progress >= +1.1f) { progress = +1.1f; progress_dir *= -1.0f; }
		if (progress <= -0.1f) { progress = -0.1f; progress_dir *= -1.0f; }
	}
	ImGui::SetCursorPos(ImVec2(0, HEIGHT - 25));
	ImGui::PushItemWidth(1280);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::ProgressBar(progress, ImVec2(0.0f, 0.0f), "");
	ImGui::PopStyleVar();


	// Message Label ( On Progress Bar Invisible )
	ImGui::SetCursorPos(ImVec2(0, HEIGHT - 25));
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(255, 255, 255, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(255, 255, 255, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(255, 255, 255, 0));
	ImGui::Button(msgLabel, ImVec2(WIDTH, 25));
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();


	// Aura Breathing
	if(aura_breathing == true)
	{
		static float frameColor = 0.0f, p_dir = 0.5f;
		if (true)
		{
			if (frameColor > 255) {
				frameColor = 0;
			}
			else {
				frameColor += p_dir;
			}
		}

		static HsvColor hsv;
		hsv.h = (int)frameColor;
		hsv.s = 255;
		hsv.v = 255;

		RgbColor rgb;
		unsigned char region, remainder, p, q, t;

		if (hsv.s == 0)
		{
			rgb.r = hsv.v;
			rgb.g = hsv.v;
			rgb.b = hsv.v;
		}

		region = hsv.h / 43;
		remainder = (hsv.h - (region * 43)) * 6;

		p = (hsv.v * (255 - hsv.s)) >> 8;
		q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
		t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

		switch (region)
		{
		case 0:
			rgb.r = hsv.v; rgb.g = t; rgb.b = p;
			break;
		case 1:
			rgb.r = q; rgb.g = hsv.v; rgb.b = p;
			break;
		case 2:
			rgb.r = p; rgb.g = hsv.v; rgb.b = t;
			break;
		case 3:
			rgb.r = p; rgb.g = q; rgb.b = hsv.v;
			break;
		case 4:
			rgb.r = t; rgb.g = p; rgb.b = hsv.v;
			break;
		default:
			rgb.r = hsv.v; rgb.g = p; rgb.b = q;
			break;
		}
		style.Colors[ImGuiCol_Border] = ImColor(rgb.r, rgb.g, rgb.b);
	}
	else
	{
		style.Colors[ImGuiCol_Border] = ImColor(110, 110, 128, 128);
	}

	firstTimeRun = false;

	// Code Ends-----------------------------------------------------------------------------------------------------------------------|
	ImGui::End();
}


// Lance Functions
vector<string> lance::getFileNames(char path[]) {
	auto dirIter = directory_iterator(path);
	vector<string> files;

	for (const auto& file : dirIter) {
		string tmpFileName = file.path().string();
		size_t dotIndex = tmpFileName.find_last_of(".");
		if (dotIndex != string::npos && !file.is_directory() && file.is_regular_file()) {
			string ext = tmpFileName.substr(dotIndex, tmpFileName.length() - dotIndex);
			int validTxtFile = ext.compare(".txt");
			if (validTxtFile == 0) {
				files.push_back(tmpFileName);
			}
		}
	}

	return files;
}
vector<string> lance::getFileNames(string inputFiles) {
	vector<string> files;
	string del = "|";

	int start, end = -1 * del.size();
	do {
		start = end + del.size();
		end = inputFiles.find(del, start);
		files.push_back(inputFiles.substr(start, end - start));
	} while (end != -1);

	return files;
}
long long lance::getFileSize(string path) {
	return std::filesystem::file_size(path);
};
string lance::getFileContents(string path) {
	ifstream fs(path);
	fs.seekg(0, std::ios::end);
	auto size = fs.tellg();
	string buffer((unsigned long)size, ' ');
	fs.seekg(0);
	fs.read(&buffer[0], size);
	fs.close();
	return buffer.c_str();
}

long lance::getCurrentTime(char type) {
	milliseconds time = duration_cast<milliseconds>(
		system_clock::now().time_since_epoch()
	);

	if (type == 's') {
		return (long)(time.count() / 1000);
	}

	return (long)(time.count());
}

long lance::getWordCount(std::string str) 
{
	long wordCount = 0;
	std::ifstream inputFile(str);

	if (inputFile.is_open()) {
		std::string line;         // Declaring a string variable to store each line of text

		while (std::getline(inputFile, line)) {  // Loop through each line in the file
			std::stringstream ss(line);  // Create a stringstream object with the current line content
			std::string word;  // Declare a string variable to store each word

			while (ss >> word) {  // Extract words from the stringstream
				wordCount++;  // Increment word count for each word extracted
			}
		}

		inputFile.close();  // Closing the file after counting words

		return wordCount;  // Outputting the total word count
	}

	return 0;
};
long lance::getLineCount(std::string str) 
{
	long count = 0;
	std::ifstream inFile(str);
	count = std::count(std::istreambuf_iterator<char>(inFile), std::istreambuf_iterator<char>(), '\n');
	return count;
};

std::string lance::formatChapter(
	std::string str,
	bool removeCheck,
	char removeText[],
	bool replaceCheck,
	char replaceText1[],
	char replaceText2[],
	int blankLineOption
)
{
	str = lance::fRemoveText(str, removeCheck, removeText);
	str = lance::fReplaceText(str, replaceCheck, replaceText1, replaceText2);
	str = lance::fRemoveLines(str, blankLineOption);

	return str;
}


// Lance Formatting Functions
std::string lance::fRemoveText(std::string str, bool option, char remove[]) {
	if (option == true) {
		str = std::regex_replace(str, std::regex(remove), "");
	}
	return str;
}
std::string lance::fReplaceText(std::string  str, bool option, char replaceText1[], char replaceText2[]) {
	if (option == true) {
		str = std::regex_replace(str, std::regex(replaceText1), replaceText2);
	}
	return str;
}
std::string lance::fRenameFile(
	bool renameCheck,
	std::string str,
	bool pathVisible,
	char renamerText1[],
	char renamerText2[],
	char renamerText3[],
	char renamerText4[],
	char renamerText5[]
) 
{
	std::filesystem::path p(str);
	if (renameCheck) 
	{
		string newName = std::string(renamerText1) + p.stem().string() + std::string(renamerText2) + p.extension().string();
		if(!pathVisible) return newName;
		string path = p.parent_path().string() + "\\";
		return path + newName;
	}

	if (!pathVisible) return p.filename().string();

	return str;
};
std::string lance::fRemoveLines(std::string str, int option) {
	if (option == 1) {
		return lance::removeExtraEmptyLines(str);
	}
	if (option == 2) {
		return lance::removeEmptyLines(str);
	}
	return str;
}

std::string lance::extractOldName(std::string str, bool pathVisible)
{
	if (pathVisible) return str;
	std::filesystem::path p(str);
	return p.filename().string();
}
std::string lance::removeExtraEmptyLines(std::string str) {
	str = std::regex_replace(str, std::regex("[\n]+"), "");
	return str;
}
std::string lance::removeEmptyLines(std::string str) {
	str = std::regex_replace(str, std::regex("[\r\n]+"), "\n");
	return str;
}
std::string lance::removeEmptySpaces(std::string str) {
	str = std::regex_replace(str, std::regex("\\s*"), "\n");
	return str;
}

static inline void ltrim(std::string& s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
	}));
}
static inline void rtrim(std::string& s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
		}).base(), s.end()
	);
}
static inline void trim(std::string& s) {
	rtrim(s);
	ltrim(s);
}


// Type Conversions
short int lance::toShint(short int x)
{
	return (short int)(x);
}
short int lance::toShint(double x)
{
	return (short int)(x);
}
float lance::toFloat(double x)
{
	return (float)(x);
}


// Lance Ini Functions
void lance_ini::saveTheme(int theme) {
	json j =
	{
		{"theme", theme},
	};

	CHAR my_documents[MAX_PATH];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents);
	string folderPath = my_documents + std::string("\\Lance");
	struct stat sb;

	if (stat(folderPath.c_str(), &sb) != 0) {
		std::filesystem::create_directory(folderPath);
	}

	if (result == S_OK) {
		string path = folderPath + std::string("\\Lance.json");
		std::ofstream file(path);
		file << j;
	}
	else {
		string path = "./Lance.json";
		std::ofstream file(path);
		file << j;
	}
}

int lance_ini::initializeSettings() {
	CHAR my_documents[MAX_PATH];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, my_documents);
	string folderPath = my_documents + std::string("\\Lance");
	struct stat sb;

	if (stat(folderPath.c_str(), &sb) != 0) {
		std::filesystem::create_directory(folderPath);
	}

	if (result == S_OK) {
		string path = my_documents + std::string("\\Lance\\Lance.json");
		std::ifstream file(path);
		if (file.good()) {
			json data = json::parse(file);
			return data["theme"];
		}
		return 0;
	}
	else {
		std::ifstream file("Lance.json");
		if (file.good()) {
			json data = json::parse(file);
			return data["theme"];
		}
		return 0;
	}
}



// Console
enum ImGuiLanceFlags_
{
	ImGuiLanceFlags_None = 0,
	ImGuiLanceFlags_EscapeClearsAll = 1 << 20,
};
struct ExampleAppConsole
{
	char                  InputBuf[256];
	ImVector<char*>       Items;
	ImVector<const char*> Commands;
	ImVector<char*>       History;
	int                   HistoryPos;    // -1: new line, 0..History.Size-1 browsing history.
	ImGuiTextFilter       Filter;
	bool                  AutoScroll;
	bool                  ScrollToBottom;

	ExampleAppConsole()
	{
		ClearLog();
		memset(InputBuf, 0, sizeof(InputBuf));
		HistoryPos = -1;

		// "CLASSIFY" is here to provide the test case where "C"+[tab] completes to "CL" and display multiple matches.
		Commands.push_back("HELP");
		Commands.push_back("HISTORY");
		Commands.push_back("CLEAR");
		Commands.push_back("CLASSIFY");
		AutoScroll = true;
		ScrollToBottom = false;
		AddLog("Welcome to Dear ImGui!");
	}
	~ExampleAppConsole()
	{
		ClearLog();
		for (int i = 0; i < History.Size; i++)
			ImGui::MemFree(History[i]);
	}

	// Portable helpers
	static int   Stricmp(const char* s1, const char* s2) { int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d; }
	static int   Strnicmp(const char* s1, const char* s2, int n) { int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d; }
	static char* Strdup(const char* s) { IM_ASSERT(s); size_t len = strlen(s) + 1; void* buf = ImGui::MemAlloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
	static void  Strtrim(char* s) { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

	void    ClearLog()
	{
		for (int i = 0; i < Items.Size; i++)
			ImGui::MemFree(Items[i]);
		Items.clear();
	}

	void    AddLog(const char* fmt, ...) IM_FMTARGS(2)
	{
		// FIXME-OPT
		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
		buf[IM_ARRAYSIZE(buf) - 1] = 0;
		va_end(args);
		Items.push_back(Strdup(buf));
	}

	void    Draw(const char* title, bool* p_open)
	{
		//ImGui::SetCursorPosX(370);
		/*ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);*/
		ImGui::SetNextWindowPos({ 370, 90 });
		ImGui::SetNextWindowSize({ 909, 600 }); // 1280 x 720
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(255, 255, 255, 0));
		if (!ImGui::Begin(
			title, 
			p_open, 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoResize
		))
		{
			ImGui::End();
			return;
		}

		// As a specific feature guaranteed by the library, after calling Begin() the last Item represent the title bar.
		// So e.g. IsItemHovered() will return true when hovering the title bar.
		// Here we create a context menu only available from the title bar.
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem("Close Console"))
				*p_open = false;
			ImGui::EndPopup();
		}

		// TODO: display items starting from the bottom

		/*if (ImGui::SmallButton("Add Debug Text")) { AddLog("%d some text", Items.Size); AddLog("some more text"); AddLog("display very important message here!"); }
		ImGui::SameLine();
		if (ImGui::SmallButton("Add Debug Error")) { AddLog("[error] something went wrong"); }
		ImGui::SameLine();*/
		if (ImGui::Button("Clear", ImVec2(110, 30))) { ClearLog(); }
		ImGui::SameLine();
		bool copy_to_clipboard = ImGui::Button("Copy", ImVec2(110, 30));
		//static float t = 0.0f; if (ImGui::GetTime() - t > 0.02f) { t = ImGui::GetTime(); AddLog("Spam %f", t); }



		// Options, Filter
		Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 670);
		ImGui::Separator();

		// Reserve enough left-over height for 1 separator + 1 input text
		ImGui::SetCursorPosX(4);
		const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiLanceFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
		{
			if (ImGui::BeginPopupContextWindow())
			{
				if (ImGui::Selectable("Clear")) ClearLog();
				ImGui::EndPopup();
			}

			// Display every line as a separate entry so we can change their color or add custom widgets.
			// If you only want raw text you can use ImGui::TextUnformatted(log.begin(), log.end());
			// NB- if you have thousands of entries this approach may be too inefficient and may require user-side clipping
			// to only process visible items. The clipper will automatically measure the height of your first item and then
			// "seek" to display only items in the visible area.
			// To use the clipper we can replace your standard loop:
			//      for (int i = 0; i < Items.Size; i++)
			//   With:
			//      ImGuiListClipper clipper;
			//      clipper.Begin(Items.Size);
			//      while (clipper.Step())
			//         for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
			// - That your items are evenly spaced (same height)
			// - That you have cheap random access to your elements (you can access them given their index,
			//   without processing all the ones before)
			// You cannot this code as-is if a filter is active because it breaks the 'cheap random-access' property.
			// We would need random-access on the post-filtered list.
			// A typical application wanting coarse clipping and filtering may want to pre-compute an array of indices
			// or offsets of items that passed the filtering test, recomputing this array when user changes the filter,
			// and appending newly elements as they are inserted. This is left as a task to the user until we can manage
			// to improve this example code!
			// If your items are of variable height:
			// - Split them into same height items would be simpler and facilitate random-seeking into your list.
			// - Consider using manual call to IsRectVisible() and skipping extraneous decoration from your items.
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
			if (copy_to_clipboard)
				ImGui::LogToClipboard();
			for (const char* item : Items)
			{
				if (!Filter.PassFilter(item))
					continue;

				// Normally you would store more information in your item than just a string.
				// (e.g. make Items[] an array of structure, store color/type etc.)
				ImVec4 color;
				bool has_color = false;
				if (strstr(item, "[error]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
				else if (strncmp(item, "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
				if (has_color)
					ImGui::PushStyleColor(ImGuiCol_Text, color);
				ImGui::TextUnformatted(item);
				if (has_color)
					ImGui::PopStyleColor();
			}
			if (copy_to_clipboard)
				ImGui::LogFinish();

			// Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
			// Using a scrollbar or mouse-wheel will take away from the bottom edge.
			if (ScrollToBottom || (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
				ImGui::SetScrollHereY(1.0f);
			ScrollToBottom = false;

			ImGui::PopStyleVar();
		}
		ImGui::EndChild();

		ImGui::PopStyleColor();
		ImGui::End();
	}

	void    ExecCommand(const char* command_line)
	{
		AddLog("# %s\n", command_line);

		// Insert into history. First find match and delete it so it can be pushed to the back.
		// This isn't trying to be smart or optimal.
		HistoryPos = -1;
		for (int i = History.Size - 1; i >= 0; i--)
			if (Stricmp(History[i], command_line) == 0)
			{
				ImGui::MemFree(History[i]);
				History.erase(History.begin() + i);
				break;
			}
		History.push_back(Strdup(command_line));

		// Process command
		if (Stricmp(command_line, "CLEAR") == 0)
		{
			ClearLog();
		}
		else if (Stricmp(command_line, "HELP") == 0)
		{
			AddLog("Commands:");
			for (int i = 0; i < Commands.Size; i++)
				AddLog("- %s", Commands[i]);
		}
		else if (Stricmp(command_line, "HISTORY") == 0)
		{
			int first = History.Size - 10;
			for (int i = first > 0 ? first : 0; i < History.Size; i++)
				AddLog("%3d: %s\n", i, History[i]);
		}
		else
		{
			AddLog("Unknown command: '%s'\n", command_line);
		}

		// On command input, we scroll to bottom even if AutoScroll==false
		ScrollToBottom = true;
	}

	// In C++11 you'd be better off using lambdas for this sort of forwarding callbacks
	static int TextEditCallbackStub(ImGuiInputTextCallbackData* data)
	{
		ExampleAppConsole* console = (ExampleAppConsole*)data->UserData;
		return console->TextEditCallback(data);
	}

	int     TextEditCallback(ImGuiInputTextCallbackData* data)
	{
		//AddLog("cursor: %d, selection: %d-%d", data->CursorPos, data->SelectionStart, data->SelectionEnd);
		switch (data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
		{
			// Example of TEXT COMPLETION

			// Locate beginning of current word
			const char* word_end = data->Buf + data->CursorPos;
			const char* word_start = word_end;
			while (word_start > data->Buf)
			{
				const char c = word_start[-1];
				if (c == ' ' || c == '\t' || c == ',' || c == ';')
					break;
				word_start--;
			}

			// Build a list of candidates
			ImVector<const char*> candidates;
			for (int i = 0; i < Commands.Size; i++)
				if (Strnicmp(Commands[i], word_start, (int)(word_end - word_start)) == 0)
					candidates.push_back(Commands[i]);

			if (candidates.Size == 0)
			{
				// No match
				AddLog("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
			}
			else if (candidates.Size == 1)
			{
				// Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
				data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
				data->InsertChars(data->CursorPos, candidates[0]);
				data->InsertChars(data->CursorPos, " ");
			}
			else
			{
				// Multiple matches. Complete as much as we can..
				// So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
				int match_len = (int)(word_end - word_start);
				for (;;)
				{
					int c = 0;
					bool all_candidates_matches = true;
					for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
						if (i == 0)
							c = toupper(candidates[i][match_len]);
						else if (c == 0 || c != toupper(candidates[i][match_len]))
							all_candidates_matches = false;
					if (!all_candidates_matches)
						break;
					match_len++;
				}

				if (match_len > 0)
				{
					data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
					data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
				}

				// List matches
				AddLog("Possible matches:\n");
				for (int i = 0; i < candidates.Size; i++)
					AddLog("- %s\n", candidates[i]);
			}

			break;
		}
		case ImGuiInputTextFlags_CallbackHistory:
		{
			// Example of HISTORY
			const int prev_history_pos = HistoryPos;
			if (data->EventKey == ImGuiKey_UpArrow)
			{
				if (HistoryPos == -1)
					HistoryPos = History.Size - 1;
				else if (HistoryPos > 0)
					HistoryPos--;
			}
			else if (data->EventKey == ImGuiKey_DownArrow)
			{
				if (HistoryPos != -1)
					if (++HistoryPos >= History.Size)
						HistoryPos = -1;
			}

			// A better implementation would preserve the data on the current input line along with cursor position.
			if (prev_history_pos != HistoryPos)
			{
				const char* history_str = (HistoryPos >= 0) ? History[HistoryPos] : "";
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, history_str);
			}
		}
		}
		return 0;
	}
};
static void lance::ShowExampleAppConsole(bool* p_open)
{
	static ExampleAppConsole console;
	console.Draw("Example: Console", p_open);
}
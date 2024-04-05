#include "gui.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include "../imgui/imgui_stdlib.h"
#include "../lib/tinyfiledialogs.h"
#include <string>
#include <regex>
#include <filesystem>
#include <fstream>
#include <chrono>
#include "lance_utils.h"

using std::filesystem::directory_iterator;
using namespace std::chrono;
using std::string;
using std::to_string;
using std::vector;
using std::ifstream;
using std::ofstream;
using lance::getFileNames;
using lance::getFileContents;
using lance::getCurrentTime;
using lance::formatChapter;

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
void gui::Render() noexcept
{
	// Static 
	static bool show_app_style_editor = false;
	static bool show_app_settings = false;
	static bool show_stat = false;
	static bool aura_breathing = false;

	static short int xPos = 10;
	static short int yPos = 60;
	static short int lGap = 35;
	static short int leftLayoutWidth = 350;

	const static char* oldFileNames[] = { "AAAA1", "BBBB2", "CCCC3", "DDDD4" };							// Old File Names
	const static char* newFileNames[] = { "n1", "n2", "n3", "n4" };										// New File Names
	static char outputPreviewText[9000000] = "Demo Output\n";										// Main Output Text
	static char chapterText[50000] = "Demo Chapter\n";												// Split Output Text by Chapters
	static char msgLabel[500] = "Error";


	// Global Style Vars
	static int theme = 1;
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
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( WIDTH - 200.0 ),
		30 + 12
	));
	ImGui::Text("CT : %d ms", 0);


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
	ImGui::Combo("##themeCombo", &theme, themeType, IM_ARRAYSIZE(themeType));
	ImGui::PopStyleVar();


	// File Type Combo
	static int currentFilePickType = 0;																			// Current File Pick Type
	const char* filePickTypes[] = { "Select Folder", "Select Files" };										// File Pick Types
	ImGui::SetCursorPos(ImVec2(
		xPos, yPos
	));
	ImGui::PushItemWidth(350);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 5));
	ImGui::Combo("##filePickCombo", &currentFilePickType, filePickTypes, IM_ARRAYSIZE(filePickTypes));
	ImGui::PopStyleVar();


	// File Picker
	/*static char inputLocation[2000] = "Select Input File/Folder";*/
	static char inputLocation[50000] = "D:\\Z";
	static char inputFiles[5000000] = "";
	if (currentFilePickType == 0) {
		ImGui::SetCursorPos(ImVec2(
			lance::toShint( xPos ),
			lance::toShint( yPos + (lGap * 1.0) )
		));
		ImGui::PushItemWidth(315);
		ImGui::InputText("##inputLocationLabel", inputLocation, IM_ARRAYSIZE(inputLocation), ImGuiInputTextFlags_ReadOnly);
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
		ImGui::InputText("##inputLocationLabel", inputLocation, IM_ARRAYSIZE(inputLocation), ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		ImGui::SetCursorPos(ImVec2(
			lance::toShint( 335.0 ),
			lance::toShint( yPos + (lGap * 1.0) )
		));
		if (ImGui::Button("...##inputLocationButton2", ImVec2(25, 25))) {
			try {
				auto res = tinyfd_openFileDialog("Pick Input Files", NULL, 0, NULL, NULL, 1);
				if (res == NULL) {
					throw "Invalid Path";
				}
				strcpy_s(inputFiles, res);
			}
			catch (...) {
				strcpy_s(msgLabel, "Error : Invalid Input Selections");
			}
		}
	}


	// Output Picker
	/*static char outputLocation[2000] = "Select Output Folder";*/
	static char outputLocation[2000] = "D:\\Z";
	ImGui::SetCursorPos(ImVec2(xPos, yPos + (lGap * 2.0)));
	ImGui::PushItemWidth(315);
	ImGui::InputText("##outputLocationLabel", outputLocation, IM_ARRAYSIZE(outputLocation), ImGuiInputTextFlags_ReadOnly);
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

	static bool renameCheck = false;
	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 8.0) + 6));
	ImGui::Checkbox("Renamer", &renameCheck);
	if (!renameCheck) {
		ImGui::BeginDisabled();
	}
	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 9.0) + 6));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput1", "Prefix", replaceText2, IM_ARRAYSIZE(replaceText2));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 10.0) + 7));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput2", "Suffix", replaceText2, IM_ARRAYSIZE(replaceText2));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 11.0) + 8));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput3", "Regex", replaceText2, IM_ARRAYSIZE(replaceText2));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 12.0) + 9));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput4", "Numbering", replaceText2, IM_ARRAYSIZE(replaceText2));

	ImGui::SetCursorPos(ImVec2(rnPosX, yPos + (lGap * 13.0) + 10));
	ImGui::PushItemWidth(rnWidth);
	ImGui::InputTextWithHint("##renameInput5", "Title", replaceText2, IM_ARRAYSIZE(replaceText2));
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
		std::string outputPreview = "";
		std::string seperator = "\n\n\n\n\n";

		vector<string> filePath;
		if (currentFilePickType == 0) {
			filePath = getFileNames(inputLocation);
		}
		else {
			filePath = getFileNames(inputFiles + std::string(""));
		}

		for (auto& chapterPath : filePath) {
			std::string chapter = lance::getFileContents(chapterPath);
			if (lance::getFileSize(chapterPath) + outputPreview.length() < 9000000) {
				outputPreview += chapter + seperator;
			}
		}
		
		strcpy_s(outputPreviewText, outputPreview.c_str());
	}
	


	// Output Button
	ImGui::SetCursorPos(ImVec2(
		lance::toShint( xPos + (leftLayoutWidth + 10.0) / 2.0 ),
		lance::toShint( yPos + (lGap * 16.5) + 2 )
	));
	if(ImGui::Button("Generate Output", ImVec2((leftLayoutWidth - 10) / 2, 25 * 2)))
	{
		static string time = "";
		//long ms1 = lance::getCurrentTime('m');

		vector<string> filePath;
		if (currentFilePickType == 0) {
			filePath = getFileNames(inputLocation);
		}
		else {
			filePath = getFileNames(inputFiles + std::string(""));
		}

		std::string outputFileName = "\\_filename.txt";
		std::ofstream outputFile(outputLocation + outputFileName, std::ios::app);

		std::string seperator = "\n\n\n\n\n";

		for (auto& chapterPath : filePath) {
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
				
				outputFile << chapter << seperator;
			}
		}

		outputFile.close();


		//long ms2 = lance::getCurrentTime('m');
		//time = time + to_string(ms2 - ms1) + "\n";
		//strcpy_s(outputPreviewText, time.c_str());
	}


	// Tabs
	static short int menuPosX = 370;
	static short int menuGap = 5; // Margin in X
	static short int tabGap = 55; // Margin in Y

	ImGui::SetCursorPos(ImVec2(menuPosX, yPos));
	if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
	{
		// Output Tab
		if (ImGui::BeginTabItem("Output"))
		{
			ImGui::SetCursorPosX(menuPosX);
			static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
			ImGui::InputTextMultiline("##source", outputPreviewText, IM_ARRAYSIZE(outputPreviewText), ImVec2(910, 600), flags);
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
			static char chNumLabel[20] = "Chapter Number";
			ImGui::SetCursorPosX(menuPosX);
			ImGui::PushItemWidth(chapterBoxWidth);
			ImGui::InputText("##chpaterNumberLabel", chNumLabel, IM_ARRAYSIZE(chNumLabel), ImGuiInputTextFlags_ReadOnly);
			ImGui::SameLine();
			static char chPreLabel[20] = "Chapter Preview";
			ImGui::SetCursorPosX(chapterAreaWidth);
			ImGui::PushItemWidth(WIDTH - chapterAreaWidth);
			ImGui::InputText("##chpaterPreviewLabel", chPreLabel, IM_ARRAYSIZE(chPreLabel), ImGuiInputTextFlags_ReadOnly);


			// Listbox and Textarea
			static short int current_chapter_idx = 0;							// Currently Selected Chapter

			ImGui::SetCursorPos(ImVec2(menuPosX, yPos + tabGap));
			if (ImGui::BeginListBox("##ChapterNumberListBox", ImVec2(chapterBoxWidth, elemHeight)))
			{
				for (int n = 0; n < IM_ARRAYSIZE(oldFileNames); n++)
				{
					const bool is_selected = (current_chapter_idx == n);
					if (ImGui::Selectable(oldFileNames[n], is_selected))
						current_chapter_idx = n;

					// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndListBox();
			}

			ImGui::SetCursorPos(ImVec2(chapterAreaWidth, yPos + tabGap));

			static ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;
			ImGui::InputTextMultiline("##source", chapterText, IM_ARRAYSIZE(chapterText), ImVec2((short int)(WIDTH - chapterAreaWidth), elemHeight), flags);

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}

		// Chapter Info
		if (ImGui::BeginTabItem("Chapter Info")) 
		{
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
				for (int n = 0; n < IM_ARRAYSIZE(oldFileNames); n++)
				{
					const bool is_selected = (current_filename_idx == n);
					if (ImGui::Selectable(oldFileNames[n], is_selected))
						current_filename_idx = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndListBox();
			}
			ImGui::SetCursorPos(ImVec2((short int)(menuPosX + elemWidth + menuGap), (short int)(yPos + tabGap)));
			if (ImGui::BeginListBox("##NewFileListBox", ImVec2(elemWidth, elemHeight)))
			{
				for (int n = 0; n < IM_ARRAYSIZE(newFileNames); n++)
				{
					const bool is_selected = (current_filename_idx == n);
					if (ImGui::Selectable(newFileNames[n], is_selected))
						current_filename_idx = n;

					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndListBox();
			}

			ImGui::PopStyleVar();
			ImGui::EndTabItem();
		}

		// Stat Tab
		if (ImGui::BeginTabItem("Stats")) {
			ImGui::EndTabItem();
		}

		// Logs Tab
		if (ImGui::BeginTabItem("Logs")) {
			ImGui::EndTabItem();
		}

		// Help Tab
		if (ImGui::BeginTabItem("Help")) {
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}


	// Progress Bar
	static float progress = 0.0f, progress_dir = 1.0f;
	if (false)
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
		return (long)(time.count()/1000);
	}

	return (long)(time.count());
}

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

std::string lance::fRemoveLines(std::string str, int option) {
	if (option == 1) {
		return lance::removeExtraEmptyLines(str);
	}
	if (option == 2) {
		return lance::removeEmptyLines(str);
	}
	return str;
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
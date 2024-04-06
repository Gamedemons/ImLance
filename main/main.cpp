#include "gui.h"
#include "lance_ini.h"

#include <thread>

int __stdcall wWinMain(
	HINSTANCE instance,
	HINSTANCE previousInstance,
	PWSTR arguments,
	int commandShow)
{
	// create gui
	gui::CreateHWindow("Lance");
	gui::CreateDevice();
	gui::CreateImGui();

	static int iniTheme = -1;
	iniTheme = lance_ini::initializeSettings();
	
	while (gui::isRunning)
	{
		gui::BeginRender();
		gui::Render(iniTheme);
		gui::EndRender();

		std::this_thread::sleep_for(std::chrono::milliseconds(4));
	}

	// destroy gui
	gui::DestroyImGui();
	gui::DestroyDevice();
	gui::DestroyHWindow();

	return EXIT_SUCCESS;
}

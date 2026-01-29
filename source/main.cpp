#include "TemplateScreen.h"
#include "core/Engine.h"
#include "core/globals.h"
#include <iostream>

int main(int, char**)
{
	age::Engine engine;
	// 700 tick rate/instrutions per second
	if (engine.Init(SCREEN_WIDTH, SCREEN_HEIGHT, 1.0 / 1000.0))
	{
		auto start_screen = TemplateScreen();

		engine.AddScreen("start_screen", &start_screen);
		engine.SetActiveScreen("start_screen");
		engine.Run();
	}
	else
	{
		std::cout << "Failed to initialize engine. Aborting..." << std::endl;
	}
}

#include "GameboyScreen.h"
#include "GameboyEngine.h"
#include "core/Engine.h"
#include "core/globals.h"
#include <iostream>

int main(int, char**)
{
	GameboyEngine engine;

	const int gameboy_width = 160;
	const int gameboy_height = 144;
	int scale = 4;
	
	if (engine.Init(gameboy_width * scale,	// window width
					gameboy_height * scale, // window height
					gameboy_width,			// game width units
					gameboy_height,			// game height units
					1.0 / 4194304.0))		// engine tick rate
					//1.0 / 1000000.0))		// engine tick rate
	{
		auto start_screen = GameboyScreen();

		engine.AddScreen("start_screen", &start_screen);
		engine.SetActiveScreen("start_screen");
		engine.Run();
	}
	else
	{
		std::cout << "Failed to initialize engine. Aborting..." << std::endl;
	}
}

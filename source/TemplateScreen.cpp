#include "TemplateScreen.h"
#include "SDL/SDLInput.h"
#include "core/Engine.h"
#include "core/Renderer.h"
#include "core/ResourceManager.h"
#include "core/Utils.h"

#include <iostream>

#define DEBUG 1
constexpr int display_width = 64;
constexpr int display_height = 32;
constexpr int display_size = display_width * display_height;
constexpr int memory_size = 65535;
constexpr int program_address = 0x0100;

static int display[display_height][display_width];
static char memory[memory_size];

static unsigned int PC = program_address;
static unsigned int PEND = 0xFFFF;

void TemplateScreen::Init()
{
	m_input = std::make_unique<age::SDLInput>();
	m_input->SetQuitCallback([this]() { m_engine->Quit(); });

	memset(display, 0, sizeof(int) * display_size);
	memset(memory, 0, memory_size);

	std::string path = age::getResourcesPath() + "/Roms/01-special.gb";

	std::ifstream input(path, std::ios::binary);

	if (input)
	{
		input.seekg(0, input.end);
		auto size = input.tellg();
		input.seekg(0, input.beg);
		input.read(&memory[program_address], size);
		input.close();

		PEND = program_address + size;
		m_initialized = true;
		std::cout << "Success" << std::endl;
	}
	else
	{
		std::cout << "No ROM found - " << path << std::endl;
	}
}

void TemplateScreen::Update(const double dt)
{
	unsigned char b1 = memory[PC++];
	unsigned char b2 = memory[PC++];

	unsigned char n1 = (b1 >> 4) & 0x0F;
	unsigned char n2 = b1 & 0x0F;
	unsigned char n3 = (b2 >> 4) & 0x0F;
	unsigned char n4 = b2 & 0x0F;

	std::cout << b1 << b2 << std::endl;
}

void TemplateScreen::Draw()
{
	age::TextParams params;
	params.text = "Angry Goose Engine - gba";
	params.pos = {6, 6};
	params.height = 1.0f;
	params.color = age::Color::White();
	m_renderer->DrawText(params);
}

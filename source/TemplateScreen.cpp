#include "TemplateScreen.h"
#include "SDL/SDLInput.h"
#include "core/Engine.h"
#include "core/Renderer.h"
#include "core/ResourceManager.h"
#include "core/Utils.h"

#include <iostream>
#include <sstream>

#define DEBUG 1
constexpr int display_width = 160;
constexpr int display_height = 144;
constexpr int display_size = display_width * display_height;
constexpr int memory_size = 65535;
constexpr int program_address = 0x0100;

static int display[display_height][display_width];
static char memory[memory_size];

static unsigned int PC = program_address;
static unsigned int PEND = 0xFFFF;
static unsigned int SP;

static unsigned char A;
static unsigned char F;
static unsigned char B;
static unsigned char C;
static unsigned char D;
static unsigned char E;
static unsigned char H;
static unsigned char L;

void TemplateScreen::Init()
{
	m_input = std::make_unique<age::SDLInput>();
	m_input->SetQuitCallback([this]() { m_engine->Quit(); });
	m_input->m_keyupmap.insert({'c', [&] { m_continue = true; }});

	memset(display, 0, sizeof(int) * display_size);
	memset(memory, 0, memory_size);

	// std::string path = age::getResourcesPath().string() + "/Roms/01-special.gb";
	std::string path = "/Users/owenz0r/Downloads/01-special.gb";

	std::ifstream input(path, std::ios::binary);

	if (input)
	{
		input.seekg(0, input.end);
		auto size = input.tellg();
		input.seekg(0, input.beg);
		input.read(&memory[0], size);
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
	if (m_continue)
	{
		std::cout << "PC 0x" << std::hex << PC << " - ";
		unsigned char b1 = memory[PC++];

		// unsigned char n1 = (b1 >> 4) & 0x0F;
		// unsigned char n2 = b1 & 0x0F;

		std::cout << std::hex << int(b1) << std::endl;

		switch (b1)
		{
			case 0x00:
				{
					std::cout << "NOP" << std::endl;
					break;
				}
			case 0xC3:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "JMP a16 - " << "0x" << std::hex << (int)b3 << (int)b2 << std::endl;

					PC = b3 << 8 | b2;
					break;
				}
			case 0x21:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "LOAD HL d16 - " << "0x" << std::hex << (int)b3 << (int)b2 << std::endl;
					H = b3;
					L = b2;
				}
		}
		m_continue = false;
	}
}

static std::string charToHex(unsigned char c)
{
	std::stringstream ss;
	ss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
	return ss.str();
}

static std::string intToHex(int i)
{
	std::stringstream ss;
	ss << std::hex << std::setw(4) << std::setfill('0') << i;
	return ss.str();
}
void TemplateScreen::Draw()
{
	age::TextParams params;
	params.text = "Registers";
	params.pos = {6, 6};
	params.height = 1.0f;
	params.color = age::Color::White();
	m_renderer->DrawText(params);

	params.text = "AF";
	params.pos.y++;
	m_renderer->DrawText(params);

	params.text = charToHex(A) + " " + charToHex(F);
	params.pos.x = 10;
	m_renderer->DrawText(params);

	params.text = "BC";
	params.pos.x = 6;
	params.pos.y++;
	m_renderer->DrawText(params);

	params.text = charToHex(B) + " " + charToHex(C);
	params.pos.x = 10;
	m_renderer->DrawText(params);

	params.text = "DE";
	params.pos.x = 6;
	params.pos.y++;
	m_renderer->DrawText(params);

	params.text = charToHex(D) + " " + charToHex(E);
	params.pos.x = 10;
	m_renderer->DrawText(params);

	params.text = "HL";
	params.pos.x = 6;
	params.pos.y++;
	m_renderer->DrawText(params);

	params.text = charToHex(H) + " " + charToHex(L);
	params.pos.x = 6;
	params.pos.x = 10;
	m_renderer->DrawText(params);

	params.text = "SP";
	params.pos.x = 6;
	params.pos.y += 2;
	m_renderer->DrawText(params);

	params.text = intToHex(SP);
	params.pos.x = 10;
	m_renderer->DrawText(params);

	params.text = "PC";
	params.pos.x = 6;
	params.pos.y++;
	m_renderer->DrawText(params);

	params.text = intToHex(PC);
	params.pos.x = 10;
	m_renderer->DrawText(params);
}

#include "TemplateScreen.h"
#include "SDL/SDLInput.h"
#include "core/Engine.h"
#include "core/Renderer.h"
#include "core/ResourceManager.h"
#include "core/Utils.h"

#include <_abort.h>
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

static bool IME = true;

static unsigned char A;
static unsigned char F;
static unsigned char B;
static unsigned char C;
static unsigned char D;
static unsigned char E;
static unsigned char H;
static unsigned char L;

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

static bool ZFlag()
{
	return F & 0x01;
}

static void setZ(bool state)
{
	state == true ? F = F | 0x01 : F = F & 0xFE;
}

static bool NFlag()
{
	return F & 0x02;
}

static bool setN(bool state)
{
	state == true ? F = F | 0x02 : F = F & 0xFD;
}

static bool HFlag()
{
	return F & 0x04;
}

static bool setH(bool state)
{
	state == true ? F = F | 0x04 : F = F & 0xFB;
}

static bool CFlag()
{
	return F & 0x08;
}

static void setC(bool state)
{
	state == true ? F = F | 0x08 : F = F & 0xF7;
}

static void push16(const unsigned int value)
{
	SP--;
	unsigned char byte = value >> 8;
	memory[SP--] = byte;
	byte = value & 0xFF;
	memory[SP] = byte;
}

static unsigned int pop16()
{
	unsigned char b2 = memory[SP++];
	unsigned char b3 = memory[SP++];
	
	return b3 << 8 | b2;
}

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
		static int count = 0;
		std::cout << std::dec << count++ << " PC 0x" << std::hex << PC << " - ";
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
			case 0x02:
				{
					std::cout << "LD (BC), A" << std::endl;
					unsigned int address = B << 8 | C;
					memory[address] = A;
					PC++;
					
					break;
				}
			case 0x0d:
				{
					std::cout << "DEC C" << std::endl;
					
					C--;
					
					setZ(C == 0);
					setN(true);
					break;
				}
			case 0x0e:
				{
					unsigned char b2 = memory[PC++];
					std::cout << "LOAD C, d8 - " << charToHex(b2) << std::endl;

					C = b2;

					//setZ(C == 0);
					break;
				}
			case 0x11:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "LOAD DE d16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;

					D = b3;
					E = b2;

					//setZ(D == 0 && E == 0);
					break;
				}
			case 0x12:
				{
					std::cout << "LOAD (DE), A" << std::endl;

					unsigned int address = D << 8 | E;
					memory[address] = A;

					//setZ(A == 0);
					break;
				}
			case 0x14:
				{
					std::cout << "INC D" << std::endl;
					
					D++;
					
					setZ(D == 0);
					setN(false);
					break;
				}
			case 0x18:
				{
					std::cout << "JR s8" << std::endl;
					char b2 = memory[PC++];
					
					PC += b2;
					std::cout << "Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")" << std::endl;
					
					break;
				}
			case 0x1c:
				{
					std::cout << "INC E" << std::endl;

					E++;

					setZ(E == 0);
					setN(false);
					break;
				}
			case 0x20:
				{
					std::cout << "JR NZ, s8" << std::endl;
					char b2 = memory[PC++];
					
					if (!ZFlag())
					{
						
						PC += b2;
						std::cout << "Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")" << std::endl;
					}
					break;
				}
			case 0x21:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "LOAD HL d16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;

					H = b3;
					L = b2;

					//setZ(H == 0 && L == 0);
					break;
				}
			case 0x2a:
				{
					std::cout << "LOAD A, (HL+)" << std::endl;

					unsigned int address = H << 8 | L;
					A = memory[address++];
					L = address & 0xFF;
					H = address >> 8;

					//setZ(A == 0);
					break;
				}
			case 0x31:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					
					std::cout << "LOAD SP, d16" << std::endl;
					
					SP = b3 << 8 | b2;
					
					//setZ(A == 0);
					break;
				}
			case 0x3E:
				{
					unsigned char b2 = memory[PC++];
					
					std::cout << "LOAD A, d8" << std::endl;
					
					A = b2;
					
					//setZ(A == 0);
					break;
				}
			case 0x47:
				{
					B = A;
					std::cout << "LOAD B, A" << std::endl;

					//setZ(B == 0);
					break;
				}
			case 0x4D:
			{
				C = L;
				std::cout << "LOAD C, L" << std::endl;
				
				//setZ(B == 0);
				break;
			}
			case 0x6B:
				{
					L = E;
					std::cout << "LOAD L, E" << std::endl;
					
					break;
				}
			case 0x78:
				{
					A = B;
					std::cout << "LOAD A, B" << std::endl;
					
					//setZ(A == 0);
					break;
				}
			case 0x7C:
				{
					A = H;
					std::cout << "LOAD A, H" << std::endl;
					
					//setZ(A == 0);
					break;
				}
			case 0x7D:
				{
					A = L;
					std::cout << "LOAD A, L" << std::endl;
					
					//setZ(A == 0);
					break;
				}
			case 0xC1:
				{
					unsigned int value = pop16();
					std::cout << "POP BC - " << "0x" << intToHex(value) << std::endl;
					B = value >> 8;
					C = value & 0xFF;
					PC++;
					
					//setZ(PC == 0);
					break;
				}
			case 0xC3:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "JMP a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
					PC = b3 << 8 | b2;

					//setZ(PC == 0);
					break;
				}
			case 0xCD:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "CALL a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
					
//					SP--;
//					unsigned char byte = PC >> 8;
//					memory[SP--] = byte;
//					//SP--;
//					byte = PC & 0xFF;
//					memory[SP] = byte;
					
					push16(PC);
					
					PC = b3 << 8 | b2;
					
					//setZ(PC == 0);
					break;
				}
			case 0xC9:
			{
				//unsigned char b2 = memory[SP++];
				//unsigned char b3 = memory[SP++];
				
				auto address = pop16();
				
				std::cout << "RET - " << "0x" << intToHex(address) << std::endl;
				
				//unsigned int address = b3 << 8 | b2;
				PC = address;
				
				//setZ(PC == 0);
				break;
			}
			case 0xE0:
				{
					unsigned char b2 = memory[PC++];
					std::cout << "LD (a8), A - " << "0xFF" << charToHex(b2) << std::endl;
					unsigned int address = 0xFF << 8 | b2;
					memory[address] = A;
					
					//setZ(PC == 0);
					break;
				}
			case 0xE1:
				{
					unsigned int value = pop16();
					std::cout << "POP HL - " << "0x" << intToHex(value) << std::endl;
					H = value >> 8;
					L = value & 0xFF;
					PC++;
					
					//setZ(PC == 0);
					break;
				}
			case 0xEA:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "LD (a16), A - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
					unsigned int address = b3 << 8 | b2;
					memory[address] = A;
					
					//setZ(PC == 0);
					break;
				}
			case 0xE5:
				{
					unsigned int HL = H << 8 | L;
					std::cout << "PUSH HL - " << "0x" << intToHex(HL) << std::endl;
					push16(HL);
					PC++;

					break;
				}
			case 0xF3:
				{
					IME = false;
					std::cout << "DI - IME DISABLED" << std::endl;
					
					//setZ(A == 0);
					break;
				}
			default:
				abort();
		}
		//m_continue = false;
	}
}

void TemplateScreen::Draw()
{
	age::TextParams params;
	params.text = "Registers";
	params.pos = {6, 6};
	params.height = 1.0f;
	params.color = age::Color::White();
	params.temp = false;
	m_renderer->DrawText(params);

	params.text = "AF";
	params.pos.y++;
	m_renderer->DrawText(params);

	params.text = charToHex(A) + " " + charToHex(F);
	params.pos.x = 10;
	params.temp = true;
	m_renderer->DrawText(params);

	params.text = "BC";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	m_renderer->DrawText(params);

	params.text = charToHex(B) + " " + charToHex(C);
	params.pos.x = 10;
	params.temp = true;
	m_renderer->DrawText(params);

	params.text = "DE";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	m_renderer->DrawText(params);

	params.text = charToHex(D) + " " + charToHex(E);
	params.pos.x = 10;
	params.temp = true;
	m_renderer->DrawText(params);

	params.text = "HL";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	m_renderer->DrawText(params);

	params.text = charToHex(H) + " " + charToHex(L);
	params.pos.x = 6;
	params.pos.x = 10;
	params.temp = true;
	m_renderer->DrawText(params);

	params.text = "SP";
	params.pos.x = 6;
	params.pos.y += 2;
	params.temp = false;
	m_renderer->DrawText(params);

	params.text = intToHex(SP);
	params.pos.x = 10;
	params.temp = true;
	m_renderer->DrawText(params);

	params.text = "PC";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	m_renderer->DrawText(params);

	params.text = intToHex(PC);
	params.pos.x = 10;
	params.temp = true;
	m_renderer->DrawText(params);
}

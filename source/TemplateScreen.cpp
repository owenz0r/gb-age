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
			case 0x01:
			{
				unsigned char b2 = memory[PC++];
				unsigned char b3 = memory[PC++];
				std::cout << "LOAD BC d16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
				
				B = b3;
				C = b2;
				
				break;
			}
			case 0x02:
				{
					std::cout << "LD (BC), A" << std::endl;
					unsigned int address = B << 8 | C;
					memory[address] = A;
					
					break;
				}
			case 0x03:
			{
				std::cout << "INC BC" << std::endl;
				unsigned int value = B << 8 | C;
				value++;
				C = value & 0xFF;
				B = value >> 8;
				
				break;
			}
			case 0x04:
			{
				std::cout << "INC B" << std::endl;
				
				B++;
				
				setZ(B == 0);
				setN(false);
				break;
			}
			case 0x05:
			{
				std::cout << "DEC B" << std::endl;
				
				B--;
				
				setZ(B == 0);
				setN(false);
				break;
			}
			case 0x06:
			{
				std::cout << "LD B, d8" << std::endl;
				B = memory[PC++];
				
				break;
			}
			case 0x0D:
				{
					std::cout << "DEC C" << std::endl;
					
					C--;
					
					setZ(C == 0);
					setN(true);
					break;
				}
			case 0x0A:
			{
				unsigned int address = B << 8 | C;
				A = memory[address];
				std::cout << "LOAD A, (BC) - " << charToHex(address) << std::endl;
				
				break;
			}
			case 0x0E:
				{
					unsigned char b2 = memory[PC++];
					std::cout << "LOAD C, d8 - " << charToHex(b2) << std::endl;
					C = b2;

					break;
				}
			case 0x11:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "LOAD DE d16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;

					D = b3;
					E = b2;

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
			case 0x13:
			{
				std::cout << "INC DE" << std::endl;
				unsigned int value = D << 8 | E;
				value++;
				D = value & 0xFF;
				E = value >> 8;
				
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
			case 0x15:
			{
				std::cout << "DEC D" << std::endl;
				
				D--;
				
				setZ(D == 0);
				setN(false);
				break;
			}
			case 0x16:
			{
				std::cout << "LD D, d8" << std::endl;
				D = memory[PC++];
				
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
			case 0x1A:
			{
				unsigned int address = D << 8 | E;
				A = memory[address];
				std::cout << "LOAD A, (DE) - " << charToHex(address) << std::endl;
				
				break;
			}
			case 0x1C:
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

					break;
				}
			case 0x22:
				{
					std::cout << "LD (HL+), A" << std::endl;
					unsigned int address = H << 8 | L;
					memory[address++] = A;
					
					L = address & 0xFF;
					H = address >> 8;
					
					break;
				}
			case 0x23:
			{
				std::cout << "INC HL" << std::endl;
				unsigned int value = H << 8 | L;
				value++;
				H = value & 0xFF;
				L = value >> 8;
				
				break;
			}
			case 0x24:
			{
				std::cout << "INC H" << std::endl;
				
				H++;
				
				setZ(H == 0);
				setN(false);
				break;
			}
			case 0x25:
			{
				std::cout << "DEC H" << std::endl;
				
				H--;
				
				setZ(H == 0);
				setN(false);
				break;
			}
			case 0x26:
			{
				std::cout << "LD H, d8" << std::endl;
				H = memory[PC++];
				
				break;
			}
			case 0x2A:
				{
					std::cout << "LOAD A, (HL+)" << std::endl;

					unsigned int address = H << 8 | L;
					A = memory[address++];
					L = address & 0xFF;
					H = address >> 8;

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
			case 0x32:
			{
				std::cout << "LD (HL-), A" << std::endl;
				unsigned int address = H << 8 | L;
				memory[address--] = A;
				
				L = address & 0xFF;
				H = address >> 8;
				
				break;
			}
			case 0x33:
			{
				std::cout << "INC SP" << std::endl;
				SP++;
				
				break;
			}
			case 0x34:
			{
				std::cout << "INC (HL)" << std::endl;
				
				unsigned int address = H << 8 | L;
				memory[address]++;
				
				setZ(memory[address] == 0);
				setN(false);
				break;
			}
			case 0x35:
			{
				std::cout << "DEC (HL)" << std::endl;
				
				unsigned int address = H << 8 | L;
				memory[address]--;
				
				setZ(memory[address] == 0);
				setN(false);
				break;
			}
			case 0x36:
			{
				std::cout << "LD (HL), d8" << std::endl;
				
				unsigned int address = H << 8 | L;
				memory[address] = memory[PC++];
				
				break;
			}
			case 0x3A:
			{
				std::cout << "LOAD A, (HL+)" << std::endl;
				
				unsigned int address = H << 8 | L;
				A = memory[address--];
				L = address & 0xFF;
				H = address >> 8;
				
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
			case 0x40:
				{
					B = B;
					std::cout << "LOAD B, B" << std::endl;
					
					break;
				}
			case 0x41:
				{
					B = C;
					std::cout << "LOAD B, C" << std::endl;
					
					break;
				}
			case 0x42:
				{
					B = D;
					std::cout << "LOAD B, D" << std::endl;
					
					break;
				}
			case 0x43:
				{
					B = E;
					std::cout << "LOAD B, E" << std::endl;
					
					break;
				}
			case 0x44:
				{
					B = H;
					std::cout << "LOAD B, H" << std::endl;
					
					break;
				}
			case 0x45:
				{
					B = L;
					std::cout << "LOAD B, L" << std::endl;
					
					break;
				}
			case 0x46:
				{
					unsigned int address = H << 8 | L;
					B = memory[address];
					std::cout << "LOAD B, (HL)" << std::endl;

					break;
				}
			case 0x47:
				{
					B = A;
					std::cout << "LOAD B, A" << std::endl;
					
					break;
				}
			case 0x48:
				{
					C = B;
					std::cout << "LOAD C, B" << std::endl;
					
					break;
				}
			case 0x49:
				{
					C = C;
					std::cout << "LOAD C, C" << std::endl;
					
					break;
				}
			case 0x4A:
				{
					C = D;
					std::cout << "LOAD C, D" << std::endl;
					
					break;
				}
			case 0x4B:
				{
					C = E;
					std::cout << "LOAD C, E" << std::endl;
					
					break;
				}
			case 0x4C:
				{
					C = H;
					std::cout << "LOAD C, H" << std::endl;
					
					break;
				}
			case 0x4D:
				{
					C = L;
					std::cout << "LOAD C, L" << std::endl;
					
					break;
				}
			case 0x4E:
				{
					unsigned int address = H << 8 | L;
					C = memory[address];
					std::cout << "LOAD C, (HL)" << std::endl;
					
					break;
				}
			case 0x4F:
				{
					C = A;
					std::cout << "LOAD C, A" << std::endl;
					
					break;
				}
			case 0x50:
			{
				D = B;
				std::cout << "LOAD D, B" << std::endl;
				
				break;
			}
			case 0x51:
			{
				D = C;
				std::cout << "LOAD D, C" << std::endl;
				
				break;
			}
			case 0x52:
			{
				D = D;
				std::cout << "LOAD D, D" << std::endl;
				
				break;
			}
			case 0x53:
			{
				D = E;
				std::cout << "LOAD D, E" << std::endl;
				
				break;
			}
			case 0x54:
			{
				D = H;
				std::cout << "LOAD D, H" << std::endl;
				
				break;
			}
			case 0x55:
			{
				D = L;
				std::cout << "LOAD D, L" << std::endl;
				
				break;
			}
			case 0x56:
			{
				unsigned int address = H << 8 | L;
				D = memory[address];
				std::cout << "LOAD D, (HL)" << std::endl;
				
				break;
			}
			case 0x57:
			{
				D = A;
				std::cout << "LOAD D, A" << std::endl;
				
				break;
			}
			case 0x58:
			{
				E = B;
				std::cout << "LOAD E, B" << std::endl;
				
				break;
			}
			case 0x59:
			{
				E = C;
				std::cout << "LOAD E, C" << std::endl;
				
				break;
			}
			case 0x5A:
			{
				E = D;
				std::cout << "LOAD E, D" << std::endl;
				
				break;
			}
			case 0x5B:
			{
				E = E;
				std::cout << "LOAD E, E" << std::endl;
				
				break;
			}
			case 0x5C:
			{
				E = H;
				std::cout << "LOAD E, H" << std::endl;
				
				break;
			}
			case 0x5D:
			{
				E = L;
				std::cout << "LOAD E, L" << std::endl;
				
				break;
			}
			case 0x5E:
			{
				unsigned int address = H << 8 | L;
				E = memory[address];
				std::cout << "LOAD E, (HL)" << std::endl;
				
				break;
			}
			case 0x5F:
			{
				E = A;
				std::cout << "LOAD E, A" << std::endl;
				
				break;
			}
			case 0x60:
			{
				H = B;
				std::cout << "LOAD H, B" << std::endl;
				
				break;
			}
			case 0x61:
			{
				H = C;
				std::cout << "LOAD H, C" << std::endl;
				
				break;
			}
			case 0x62:
			{
				H = D;
				std::cout << "LOAD H, D" << std::endl;
				
				break;
			}
			case 0x63:
			{
				H = E;
				std::cout << "LOAD H, E" << std::endl;
				
				break;
			}
			case 0x64:
			{
				H = H;
				std::cout << "LOAD H, H" << std::endl;
				
				break;
			}
			case 0x65:
			{
				H = L;
				std::cout << "LOAD H, L" << std::endl;
				
				break;
			}
			case 0x66:
			{
				unsigned int address = H << 8 | L;
				H = memory[address];
				std::cout << "LOAD H, (HL)" << std::endl;
				
				break;
			}
			case 0x67:
			{
				H = A;
				std::cout << "LOAD H, A" << std::endl;
				
				break;
			}
			case 0x68:
			{
				L = B;
				std::cout << "LOAD L, B" << std::endl;
				
				break;
			}
			case 0x69:
			{
				L = C;
				std::cout << "LOAD L, C" << std::endl;
				
				break;
			}
			case 0x6A:
			{
				L = D;
				std::cout << "LOAD L, D" << std::endl;
				
				break;
			}
			case 0x6B:
			{
				L = E;
				std::cout << "LOAD L, E" << std::endl;
				
				break;
			}
			case 0x6C:
			{
				L = H;
				std::cout << "LOAD L, H" << std::endl;
				
				break;
			}
			case 0x6D:
			{
				L = L;
				std::cout << "LOAD L, L" << std::endl;
				
				break;
			}
			case 0x6E:
			{
				unsigned int address = H << 8 | L;
				L = memory[address];
				std::cout << "LOAD L, (HL)" << std::endl;
				
				break;
			}
			case 0x6F:
			{
				L = A;
				std::cout << "LOAD L, A" << std::endl;
				
				break;
			}
			case 0x70:
			{
				unsigned int address = H << 8 | L;
				memory[address] = B;
				std::cout << "LOAD (HL), B" << std::endl;
				
				break;
			}
			case 0x71:
			{
				unsigned int address = H << 8 | L;
				memory[address] = C;
				std::cout << "LOAD (HL), C" << std::endl;
				
				break;
			}
			case 0x72:
			{
				unsigned int address = H << 8 | L;
				memory[address] = D;
				std::cout << "LOAD (HL), D" << std::endl;
				
				break;
			}
			case 0x73:
			{
				unsigned int address = H << 8 | L;
				memory[address] = E;
				std::cout << "LOAD (HL), E" << std::endl;
				
				break;
			}
			case 0x74:
			{
				unsigned int address = H << 8 | L;
				memory[address] = H;
				std::cout << "LOAD (HL), H" << std::endl;
				
				break;
			}
			case 0x75:
			{
				unsigned int address = H << 8 | L;
				memory[address] = L;
				std::cout << "LOAD (HL), L" << std::endl;
				
				break;
			}
			case 0x77:
			{
				unsigned int address = H << 8 | L;
				memory[address] = A;
				std::cout << "LOAD (HL), A" << std::endl;
				
				break;
			}
			case 0x78:
			{
				A = B;
				std::cout << "LOAD A, B" << std::endl;
				
				break;
			}
			case 0x79:
			{
				A = C;
				std::cout << "LOAD A, C" << std::endl;
				
				break;
			}
			case 0x7A:
			{
				A = D;
				std::cout << "LOAD A, D" << std::endl;
				
				break;
			}
			case 0x7B:
			{
				A = E;
				std::cout << "LOAD A, E" << std::endl;
				
				break;
			}
			case 0x7C:
			{
				A = H;
				std::cout << "LOAD A, H" << std::endl;
				
				break;
			}
			case 0x7D:
			{
				A = L;
				std::cout << "LOAD A, L" << std::endl;
				
				break;
			}
			case 0x7E:
			{
				unsigned int address = H << 8 | L;
				A = memory[address];
				std::cout << "LOAD A, (HL)" << std::endl;
				
				break;
			}
			case 0x7F:
			{
				A = A;
				std::cout << "LOAD A, A" << std::endl;
				
				break;
			}
			case 0xB0:
			{
				std::cout << "OR A, B" << std::endl;
				A = A | B;
				
				setZ(A == 0);
				break;
			}
			case 0xB1:
			{
				std::cout << "OR A, C" << std::endl;
				A = A | C;
				
				setZ(A == 0);
				break;
			}
			case 0xB2:
			{
				std::cout << "OR A, D" << std::endl;
				A = A | D;
				
				setZ(A == 0);
				break;
			}
			case 0xB3:
			{
				std::cout << "OR A, E" << std::endl;
				A = A | E;
				
				setZ(A == 0);
				break;
			}
			case 0xB4:
			{
				std::cout << "OR A, H" << std::endl;
				A = A | H;
				
				setZ(A == 0);
				break;
			}
			case 0xB5:
			{
				std::cout << "OR A, L" << std::endl;
				A = A | L;
				
				setZ(A == 0);
				break;
			}
			case 0xB6:
			{
				std::cout << "OR A, (HL)" << std::endl;
				unsigned int address = H << 8 | L;
				A = A | memory[address];
				
				setZ(A == 0);
				break;
			}
			case 0xB7:
			{
				std::cout << "OR A, A" << std::endl;
				A = A | A;
				
				setZ(A == 0);
				break;
			}
			case 0xC0:
			{
				if (ZFlag())
				{
					//PC++;
					std::cout << "RET NZ - No jump" << std::endl;
				}
				else
				{
					PC = pop16();
					std::cout << "RET NZ - " << "0x" << intToHex(PC) << std::endl;
				}
				break;
			}
			case 0xC1:
				{
					unsigned int value = pop16();
					std::cout << "POP BC - " << "0x" << intToHex(value) << std::endl;
					B = value >> 8;
					C = value & 0xFF;

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
			case 0xC5:
			{
				unsigned int value = B << 8 | C;
				push16(value);
				std::cout << "PUSH BC" << std::endl;
				
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
			case 0xD1:
			{
				unsigned int value = pop16();
				std::cout << "POP DE - " << "0x" << intToHex(value) << std::endl;
				D = value >> 8;
				E = value & 0xFF;
				
				break;
			}
			case 0xD5:
			{
				unsigned int value = D << 8 | E;
				push16(value);
				std::cout << "PUSH DE" << std::endl;
				
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
					//PC++;
					
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

					break;
				}
			case 0xF1:
			{
				unsigned int value = pop16();
				std::cout << "POP AF - " << "0x" << intToHex(value) << std::endl;
				A = value >> 8;
				F = value & 0xFF;

				break;
			}
			case 0xF3:
				{
					IME = false;
					std::cout << "DI - IME DISABLED" << std::endl;
					
					break;
				}
			case 0xF5:
			{
				unsigned int value = A << 8 | F;
				push16(value);
				std::cout << "PUSH AF" << std::endl;
				
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

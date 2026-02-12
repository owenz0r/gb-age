#include "TemplateScreen.h"
#include "SDL/SDLInput.h"
#include "core/Engine.h"
#include "core/Renderer.h"
#include "core/ResourceManager.h"
#include "core/Utils.h"

#include <iostream>
#include <sstream>
#include <assert.h>

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

static void setN(bool state)
{
	state == true ? F = F | 0x02 : F = F & 0xFD;
}

static bool HFlag()
{
	return F & 0x04;
}

static void setH(bool state)
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

static void inc8(unsigned char& reg)
{
	reg++;

	setZ(reg == 0);
	setN(false);
	setH(reg == 0x10);
}

static void inc16(unsigned char& high, unsigned char& low)
{
	unsigned int value = high << 8 | low;
	value++;
	low = value & 0xFF;
	high = value >> 8;
}

static void dec8(unsigned char& reg)
{
	reg--;

	setZ(reg == 0);
	setN(true);
	setH((reg & 0x0F) == 0x0F);
}

static void add16(unsigned char& h1,
				  unsigned char& l1,
				  unsigned int& v2)
{
	unsigned int v1 = h1 << 8 | l1;
	unsigned int result = v1 + v2;
	
	setN(false);
	setC(result > 0xFFFF);
	signed char half = (0xF & (signed char)h1) + ((0xF00 & v2) >> 8);
	setH(half > 0xF);
	
	h1 = (result & 0xFF00) >> 8;
	l1 = result & 0xFF;
}

static void add16(unsigned char& h1,
				  unsigned char& l1,
				  unsigned char& h2,
				  unsigned char& l2)
{
	unsigned int v1 = h1 << 8 | l1;
	unsigned int v2 = h2 << 8 | l2;
	unsigned int result = v1 + v2;
	
	setN(false);
	setC(result > 0xFFFF);
	signed char half = (0xF & (signed char)h1) + (0xF & h2);
	setH(half > 0xF);
	
	h1 = (result & 0xFF00) >> 8;
	l1 = result & 0xFF;
}

static void xorA(unsigned char other)
{
	A = A ^ other;
	
	setZ(A == 0);
	setN(false);
	setH(false);
	setC(false);
}

static void bitmanip(unsigned char& reg, int bit, int value)
{
	assert(bit < 8);
	assert(value < 2);

	unsigned char one = 0x01;
	for (int i = 0; i < bit; ++i)
		one = one << 1;

	if (value == 1)
	{
		reg = reg | one;
	}
	else
	{
		one = ~one;
		reg = reg & one;
	}
}

static void SLA(unsigned char& reg)
{
	unsigned int value = reg;
	value = value << 1;
	reg = value & 0xFF;
	if ((value & 0x0100) == 0x0100)
	{
		setC(true);
	}
	else
	{
		setC(false);
	}
	
	setZ(reg == 0);
	setH(false);
	setN(false);
}

static void SRA(unsigned char& reg)
{
	unsigned int value = reg;
	value = value >> 1;
	
	if (reg & 0x01)
	{
		setC(true);
	}
	else
	{
		setC(false);
	}
	
	if (reg & 0x80)
	{
		value = value | 0x80;
	}
	
	reg = value & 0xFF;
	
	setZ(reg == 0);
	setH(false);
	setN(false);
}

static void RLC(unsigned char& reg)
{
	unsigned int value = reg;
	value = value << 1;
	reg = value & 0xFF;
	if ((value & 0x0100) == 0x0100)
	{
		reg = reg | 0x01;
		setC(true);
	}
	else
	{
		setC(false);
	}

	setZ(reg == 0);
	setH(false);
	setN(false);
}

static void RRC(unsigned char& reg)
{
	unsigned int value = reg;
	value = value >> 1;

	if (reg & 0x01)
	{
		value = value | 0x80;
		setC(true);
	}
	else
	{
		setC(false);
	}

	reg = value & 0xFF;

	setZ(reg == 0);
	setH(false);
	setN(false);
}

static void RL(unsigned char& reg)
{
	unsigned int value = reg;
	value = value << 1;
	
	if (CFlag())
		value = value | 0x01;
	
	reg = value & 0xFF;

	if ((value & 0x0100) == 0x0100)
	{
		setC(true);
	}
	else
	{
		setC(false);
	}

	setZ(reg == 0);
	setH(false);
	setN(false);
}

static void RR(unsigned char& reg)
{
	unsigned int value = reg;
	value = value >> 1;
	
	if (CFlag())
		value = value | 0x80;
	
	if (reg & 0x01)
	{
		setC(true);
	}
	else
	{
		setC(false);
	}
	
	reg = value & 0xFF;
	
	setZ(reg == 0);
	setH(false);
	setN(false);
}

void TemplateScreen::Init()
{
	m_input = std::make_unique<age::SDLInput>();
	m_input->SetQuitCallback([this]() { m_engine->Quit(); });
	m_input->m_keyupmap.insert({'c', [&] { m_continue = true; }});

	memset(display, 0, sizeof(int) * display_size);
	memset(memory, 0, memory_size);

	// std::string path = age::getResourcesPath().string() + "/Roms/01-special.gb";
#ifdef WIN32
	std::string path = "Z:/downloads/01-special.gb";
#else
	std::string path = "/Users/owenz0r/Downloads/01-special.gb";
#endif

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
				inc16(B, C);
				
				break;
			}
			case 0x04:
			{
				std::cout << "INC B" << std::endl;
				inc8(B);

				break;
			}
			case 0x05:
			{
				std::cout << "DEC B" << std::endl;
				dec8(B);

				break;
			}
			case 0x06:
			{
				std::cout << "LD B, d8" << std::endl;
				B = memory[PC++];
				
				break;
			}
			case 0x09:
			{
				std::cout << "ADD HL, BC" << std::endl;
				add16(H, L, B, C);
				
				break;
			}
			case 0x0D:
				{
					std::cout << "DEC C" << std::endl;
					dec8(C);

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
				inc16(D, E);
				
				break;
			}
			case 0x14:
				{
					std::cout << "INC D" << std::endl;
					inc8(D);

					break;
				}
			case 0x15:
			{
				std::cout << "DEC D" << std::endl;
				dec8(D);

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
			case 0x19:
			{
				std::cout << "ADD HL, DE" << std::endl;
				add16(H, L, D, E);
				
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
					inc8(E);
					break;
				}
			case 0x1D:
			{
				std::cout << "DEC E" << std::endl;
				dec8(E);
				
				break;
			}
			case 0x1F:
				{
					std::cout << "RRA" << std::endl;
					
					unsigned int value = A;
					value = value >> 1;

					if (CFlag())
						value = value | 0x80;

					if (A & 0x01)
					{
						setC(true);
					}
					else
					{
						setC(false);
					}

					A = value & 0xFF;

					setZ(false);
					setH(false);
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
					else
					{
						std::cout << "Jumping not taken" << std::endl;
					}
					break;
				}
			case 0x21:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "LOAD HL d16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;

					if (PC == 0xc246)
					{
						int call_init_print = 1;
					}

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
				inc16(H, L);
				
				break;
			}
			case 0x24:
			{
				std::cout << "INC H" << std::endl;
				inc8(H);

				break;
			}
			case 0x25:
			{
				std::cout << "DEC H" << std::endl;
				dec8(H);

				break;
			}
			case 0x26:
			{
				std::cout << "LD H, d8" << std::endl;
				H = memory[PC++];
				
				break;
			}
			case 0x28:
			{
				std::cout << "JR Z, s8" << std::endl;
				char b2 = memory[PC++];
				
				if (ZFlag())
				{
					
					PC += b2;
					std::cout << "Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")" << std::endl;
				}
				break;
			}
			case 0x29:
			{
				std::cout << "ADD HL, HL" << std::endl;
				add16(H, L, H, L);
				
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
			case 0x2C:
				{
					std::cout << "INC L" << std::endl;
					inc8(L);

					break;
				}
			case 0x2D:
			{
				std::cout << "DEC L" << std::endl;
				dec8(L);
				
				break;
			}
			case 0x30:
				{
					std::cout << "JR NC, s8" << std::endl;
					char b2 = memory[PC++];

					if (!CFlag())
					{

						PC += b2;
						std::cout << "Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")" << std::endl;
					}
					else
					{
						std::cout << "Jumping not taken" << std::endl;
					}
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
			case 0x38:
			{
				std::cout << "JR C, s8" << std::endl;
				char b2 = memory[PC++];
				
				if (CFlag())
				{
					
					PC += b2;
					std::cout << "Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")" << std::endl;
				}
				break;
			}
			case 0x39:
			{
				std::cout << "ADD HL, SP" << std::endl;
				add16(H, L, SP);
				
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
			case 0x3C:
				{
					std::cout << "INC A" << std::endl;
					inc8(A);

					break;
				}
			case 0x3D:
			{
				std::cout << "DEC A" << std::endl;
				dec8(A);
				
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
			case 0xA8:
			{
				std::cout << "XOR B" << std::endl;
				xorA(B);
				
				break;
			}
			case 0xA9:
			{
				std::cout << "XOR C" << std::endl;
				xorA(C);
				
				break;
			}
			case 0xAA:
			{
				std::cout << "XOR D" << std::endl;
				xorA(D);
				
				break;
			}
			case 0xAB:
			{
				std::cout << "XOR E" << std::endl;
				xorA(E);
				
				break;
			}
			case 0xAC:
			{
				std::cout << "XOR H" << std::endl;
				xorA(H);
				
				break;
			}
			case 0xAD:
			{
				std::cout << "XOR L" << std::endl;
				xorA(L);
				
				break;
			}
			case 0xAE:
			{
				std::cout << "XOR (HL)" << std::endl;
				unsigned int address = H << 8 | L;
				xorA(memory[address]);
				
				break;
			}
			case 0xAF:
			{
				std::cout << "XOR A" << std::endl;
				xorA(A);
				
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
			case 0xC2:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "JP NZ, a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
					if (!ZFlag())
						PC = b3 << 8 | b2;

					break;
				}
			case 0xC3:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "JMP a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
					PC = b3 << 8 | b2;

					break;
				}
			case 0xC4:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "CALL NZ, a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;

					if (!ZFlag())
					{
						push16(PC);
						PC = b3 << 8 | b2;
					}

					break;
				}
			case 0xC5:
			{
				unsigned int value = B << 8 | C;
				push16(value);
				std::cout << "PUSH BC" << std::endl;
				
				break;
			}
			case 0xC6:
			{
				unsigned char b2 = memory[PC++];
				std::cout << "ADD d8 - " << "0x" << charToHex(b2) << std::endl;
				
				bool carry = ((signed int)A + (signed int)b2) > 0xFF;
				signed char half = (0xF & (signed char)A) + (0xF & b2);
				
				A = A + b2;
				
				setZ(A == 0);
				setN(false);
				
				setH(half > 0xF);
				setC(carry);
				
				break;
			}
			case 0xCB:
			{
					unsigned char b2 = memory[PC++];
					switch (b2)
					{
						case 0x00:
						{
							std::cout << "RLC B" << std::endl;
							RLC(B);

							break;
						}
						case 0x01:
							{
								std::cout << "RLC C" << std::endl;
								RLC(C);

								break;
							}
						case 0x02:
							{
								std::cout << "RLC D" << std::endl;
								RLC(D);

								break;
							}
						case 0x03:
							{
								std::cout << "RLC E" << std::endl;
								RLC(E);

								break;
							}
						case 0x04:
							{
								std::cout << "RLC H" << std::endl;
								RLC(H);

								break;
							}
						case 0x05:
							{
								std::cout << "RLC L" << std::endl;
								RLC(L);

								break;
							}
						case 0x06:
							{
								std::cout << "RLC (HL)" << std::endl;
								
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								RLC(value);
								memory[address] = value;

								break;
							}
						case 0x07:
							{
								std::cout << "RLC A" << std::endl;
								RLC(A);

								break;
							}

						case 0x08:
							{
								std::cout << "RRC B" << std::endl;
								RRC(B);

								break;
							}
						case 0x09:
							{
								std::cout << "RRC C" << std::endl;
								RRC(C);

								break;
							}
						case 0x0A:
							{
								std::cout << "RRC D" << std::endl;
								RRC(D);

								break;
							}
						case 0x0B:
							{
								std::cout << "RRC E" << std::endl;
								RRC(E);

								break;
							}
						case 0x0C:
							{
								std::cout << "RRC H" << std::endl;
								RRC(H);

								break;
							}
						case 0x0D:
							{
								std::cout << "RRC L" << std::endl;
								RRC(L);

								break;
							}
						case 0x0E:
							{
								std::cout << "RRC (HL)" << std::endl;

								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								RRC(value);
								memory[address] = value;

								break;
							}
						case 0x0F:
							{
								std::cout << "RRC A" << std::endl;
								RRC(A);

								break;
							}

						////////////////////////////////////////////

						case 0x10:
						{
							std::cout << "RL B" << std::endl;
							RL(B);
							
							break;
						}
						case 0x11:
						{
							std::cout << "RL C" << std::endl;
							RL(C);
							
							break;
						}
						case 0x12:
						{
							std::cout << "RL D" << std::endl;
							RL(D);
							
							break;
						}
						case 0x13:
						{
							std::cout << "RL E" << std::endl;
							RL(E);
							
							break;
						}
						case 0x14:
						{
							std::cout << "RL H" << std::endl;
							RL(H);
							
							break;
						}
						case 0x15:
						{
							std::cout << "RL L" << std::endl;
							RL(L);
							
							break;
						}
						case 0x16:
						{
							std::cout << "RL (HL)" << std::endl;
							
							unsigned int address = H << 8 | L;
							unsigned char value = memory[address];
							RL(value);
							memory[address] = value;
							
							break;
						}
						case 0x17:
						{
							std::cout << "RL A" << std::endl;
							RL(A);
							
							break;
						}
							
						case 0x18:
						{
							std::cout << "RR B" << std::endl;
							RR(B);
							
							break;
						}
						case 0x19:
						{
							std::cout << "RR C" << std::endl;
							RR(C);
							
							break;
						}
						case 0x1A:
						{
							std::cout << "RR D" << std::endl;
							RR(D);
							
							break;
						}
						case 0x1B:
						{
							std::cout << "RR E" << std::endl;
							RR(E);
							
							break;
						}
						case 0x1C:
						{
							std::cout << "RR H" << std::endl;
							RR(H);
							
							break;
						}
						case 0x1D:
						{
							std::cout << "RR L" << std::endl;
							RR(L);
							
							break;
						}
						case 0x1E:
						{
							std::cout << "RR (HL)" << std::endl;
							
							unsigned int address = H << 8 | L;
							unsigned char value = memory[address];
							RR(value);
							memory[address] = value;
							
							break;
						}
						case 0x1F:
						{
							std::cout << "RR A" << std::endl;
							RR(A);
							
							break;
						}
							
							////////////////////////////////////////////
							
						case 0x20:
						{
							std::cout << "SLA B" << std::endl;
							SLA(B);
							
							break;
						}
						case 0x21:
						{
							std::cout << "SLA C" << std::endl;
							SLA(C);
							
							break;
						}
						case 0x22:
						{
							std::cout << "SLA D" << std::endl;
							SLA(D);
							
							break;
						}
						case 0x23:
						{
							std::cout << "SLA E" << std::endl;
							SLA(E);
							
							break;
						}
						case 0x24:
						{
							std::cout << "SLA H" << std::endl;
							SLA(H);
							
							break;
						}
						case 0x25:
						{
							std::cout << "SLA L" << std::endl;
							SLA(L);
							
							break;
						}
						case 0x26:
						{
							std::cout << "SLA (HL)" << std::endl;
							
							unsigned int address = H << 8 | L;
							unsigned char value = memory[address];
							SLA(value);
							memory[address] = value;
							
							break;
						}
						case 0x27:
						{
							std::cout << "SLA A" << std::endl;
							SLA(A);
							
							break;
						}
							
						case 0x28:
						{
							std::cout << "SRA B" << std::endl;
							SRA(B);
							
							break;
						}
						case 0x29:
						{
							std::cout << "SRA C" << std::endl;
							SRA(C);
							
							break;
						}
						case 0x2A:
						{
							std::cout << "SRA D" << std::endl;
							SRA(D);
							
							break;
						}
						case 0x2B:
						{
							std::cout << "SRA E" << std::endl;
							SRA(E);
							
							break;
						}
						case 0x2C:
						{
							std::cout << "SRA H" << std::endl;
							SRA(H);
							
							break;
						}
						case 0x2D:
						{
							std::cout << "SRA L" << std::endl;
							SRA(L);
							
							break;
						}
						case 0x2E:
						{
							std::cout << "SRA (HL)" << std::endl;
							
							unsigned int address = H << 8 | L;
							unsigned char value = memory[address];
							SRA(value);
							memory[address] = value;
							
							break;
						}
						case 0x2F:
						{
							std::cout << "SRA A" << std::endl;
							SRA(A);
							
							break;
						}
							
							////////////////////////////////////////////

						////////////////////////////////////////////

						case 0x80:
						{
							std::cout << "RES 0, B" << std::endl;
							bitmanip(B, 0, 0);
							break;
						}
						case 0x81:
							{
								std::cout << "RES 0, C" << std::endl;
								bitmanip(C, 0, 0);
								break;
							}
						case 0x82:
							{
								std::cout << "RES 0, D" << std::endl;
								bitmanip(D, 0, 0);
								break;
							}
						case 0x83:
							{
								std::cout << "RES 0, E" << std::endl;
								bitmanip(E, 0, 0);
								break;
							}
						case 0x84:
							{
								std::cout << "RES 0, H" << std::endl;
								bitmanip(H, 0, 0);
								break;
							}
						case 0x85:
							{
								std::cout << "RES 0, L" << std::endl;
								bitmanip(L, 0, 0);
								break;
							}
						case 0x86:
							{
								std::cout << "RES 0, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 0, 0);
								memory[address] = value;
								break;
							}
						case 0x87:
							{
								std::cout << "RES 0, A" << std::endl;
								bitmanip(A, 0, 0);
								break;
							}
						case 0x88:
							{
								std::cout << "RES 1, B" << std::endl;
								bitmanip(B, 1, 0);
								break;
							}
						case 0x89:
							{
								std::cout << "RES 1, C" << std::endl;
								bitmanip(C, 1, 0);
								break;
							}
						case 0x8A:
							{
								std::cout << "RES 1, D" << std::endl;
								bitmanip(D, 1, 0);
								break;
							}
						case 0x8B:
							{
								std::cout << "RES 1, E" << std::endl;
								bitmanip(E, 1, 0);
								break;
							}
						case 0x8C:
							{
								std::cout << "RES 1, H" << std::endl;
								bitmanip(H, 1, 0);
								break;
							}
						case 0x8D:
							{
								std::cout << "RES 1, L" << std::endl;
								bitmanip(L, 1, 0);
								break;
							}
						case 0x8E:
							{
								std::cout << "RES 1, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 1, 0);
								memory[address] = value;
								break;
							}
						case 0x8F:
							{
								std::cout << "RES 1, A" << std::endl;
								bitmanip(A, 1, 0);
								break;
							}

						///////////////////////

						case 0x90:
							{
								std::cout << "RES 2, B" << std::endl;
								bitmanip(B, 2, 0);
								break;
							}
						case 0x91:
							{
								std::cout << "RES 2, C" << std::endl;
								bitmanip(C, 2, 0);
								break;
							}
						case 0x92:
							{
								std::cout << "RES 2, D" << std::endl;
								bitmanip(D, 2, 0);
								break;
							}
						case 0x93:
							{
								std::cout << "RES 2, E" << std::endl;
								bitmanip(E, 2, 0);
								break;
							}
						case 0x94:
							{
								std::cout << "RES 2, H" << std::endl;
								bitmanip(H, 2, 0);
								break;
							}
						case 0x95:
							{
								std::cout << "RES 2, L" << std::endl;
								bitmanip(L, 2, 0);
								break;
							}
						case 0x96:
							{
								std::cout << "RES 2, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 2, 0);
								memory[address] = value;
								break;
							}
						case 0x97:
							{
								std::cout << "RES 2, A" << std::endl;
								bitmanip(A, 2, 0);
								break;
							}
						case 0x98:
							{
								std::cout << "RES 3, B" << std::endl;
								bitmanip(B, 3, 0);
								break;
							}
						case 0x99:
							{
								std::cout << "RES 3, C" << std::endl;
								bitmanip(C, 3, 0);
								break;
							}
						case 0x9A:
							{
								std::cout << "RES 3, D" << std::endl;
								bitmanip(D, 3, 0);
								break;
							}
						case 0x9B:
							{
								std::cout << "RES 3, E" << std::endl;
								bitmanip(E, 3, 0);
								break;
							}
						case 0x9C:
							{
								std::cout << "RES 3, H" << std::endl;
								bitmanip(H, 3, 0);
								break;
							}
						case 0x9D:
							{
								std::cout << "RES 3, L" << std::endl;
								bitmanip(L, 3, 0);
								break;
							}
						case 0x9E:
							{
								std::cout << "RES 3, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 3, 0);
								memory[address] = value;
								break;
							}
						case 0x9F:
							{
								std::cout << "RES 3, A" << std::endl;
								bitmanip(A, 3, 0);
								break;
							}

						///////////////////////

						case 0xA0:
							{
								std::cout << "RES 4, B" << std::endl;
								bitmanip(B, 4, 0);
								break;
							}
						case 0xA1:
							{
								std::cout << "RES 4, C" << std::endl;
								bitmanip(C, 4, 0);
								break;
							}
						case 0xA2:
							{
								std::cout << "RES 4, D" << std::endl;
								bitmanip(D, 4, 0);
								break;
							}
						case 0xA3:
							{
								std::cout << "RES 4, E" << std::endl;
								bitmanip(E, 4, 0);
								break;
							}
						case 0xA4:
							{
								std::cout << "RES 4, H" << std::endl;
								bitmanip(H, 4, 0);
								break;
							}
						case 0xA5:
							{
								std::cout << "RES 4, L" << std::endl;
								bitmanip(L, 4, 0);
								break;
							}
						case 0xA6:
							{
								std::cout << "RES 4, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 4, 0);
								memory[address] = value;
								break;
							}
						case 0xA7:
							{
								std::cout << "RES 4, A" << std::endl;
								bitmanip(A, 4, 0);
								break;
							}
						case 0xA8:
							{
								std::cout << "RES 5, B" << std::endl;
								bitmanip(B, 5, 0);
								break;
							}
						case 0xA9:
							{
								std::cout << "RES 5, C" << std::endl;
								bitmanip(C, 5, 0);
								break;
							}
						case 0xAA:
							{
								std::cout << "RES 5, D" << std::endl;
								bitmanip(D, 5, 0);
								break;
							}
						case 0xAB:
							{
								std::cout << "RES 5, E" << std::endl;
								bitmanip(E, 5, 0);
								break;
							}
						case 0xAC:
							{
								std::cout << "RES 5, H" << std::endl;
								bitmanip(H, 5, 0);
								break;
							}
						case 0xAD:
							{
								std::cout << "RES 5, L" << std::endl;
								bitmanip(L, 5, 0);
								break;
							}
						case 0xAE:
							{
								std::cout << "RES 5, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 5, 0);
								memory[address] = value;
								break;
							}
						case 0xAF:
							{
								std::cout << "RES 5, A" << std::endl;
								bitmanip(A, 5, 0);
								break;
							}

							///////////////////////

						case 0xB0:
							{
								std::cout << "RES 6, B" << std::endl;
								bitmanip(B, 6, 0);
								break;
							}
						case 0xB1:
							{
								std::cout << "RES 6, C" << std::endl;
								bitmanip(C, 6, 0);
								break;
							}
						case 0xB2:
							{
								std::cout << "RES 6, D" << std::endl;
								bitmanip(D, 6, 0);
								break;
							}
						case 0xB3:
							{
								std::cout << "RES 6, E" << std::endl;
								bitmanip(E, 6, 0);
								break;
							}
						case 0xB4:
							{
								std::cout << "RES 6, H" << std::endl;
								bitmanip(H, 6, 0);
								break;
							}
						case 0xB5:
							{
								std::cout << "RES 6, L" << std::endl;
								bitmanip(L, 6, 0);
								break;
							}
						case 0xB6:
							{
								std::cout << "RES 6, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 6, 0);
								memory[address] = value;
								break;
							}
						case 0xB7:
							{
								std::cout << "RES 6, A" << std::endl;
								bitmanip(A, 6, 0);
								break;
							}
						case 0xB8:
							{
								std::cout << "RES 7, B" << std::endl;
								bitmanip(B, 7, 0);
								break;
							}
						case 0xB9:
							{
								std::cout << "RES 7, C" << std::endl;
								bitmanip(C, 7, 0);
								break;
							}
						case 0xBA:
							{
								std::cout << "RES 7, D" << std::endl;
								bitmanip(D, 7, 0);
								break;
							}
						case 0xBB:
							{
								std::cout << "RES 7, E" << std::endl;
								bitmanip(E, 7, 0);
								break;
							}
						case 0xBC:
							{
								std::cout << "RES 7, H" << std::endl;
								bitmanip(H, 7, 0);
								break;
							}
						case 0xBD:
							{
								std::cout << "RES 7, L" << std::endl;
								bitmanip(L, 7, 0);
								break;
							}
						case 0xBE:
							{
								std::cout << "RES 7, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 7, 0);
								memory[address] = value;
								break;
							}
						case 0xBF:
							{
								std::cout << "RES 7, A" << std::endl;
								bitmanip(A, 7, 0);
								break;
							}

						////////////////////////////////

						////////////////////////////////

						case 0xC0:
							{
								std::cout << "SET 0, B" << std::endl;
								bitmanip(B, 0, 1);
								break;
							}
						case 0xC1:
							{
								std::cout << "SET 0, C" << std::endl;
								bitmanip(C, 0, 1);
								break;
							}
						case 0xC2:
							{
								std::cout << "SET 0, D" << std::endl;
								bitmanip(D, 0, 1);
								break;
							}
						case 0xC3:
							{
								std::cout << "SET 0, E" << std::endl;
								bitmanip(E, 0, 1);
								break;
							}
						case 0xC4:
							{
								std::cout << "SET 0, H" << std::endl;
								bitmanip(H, 0, 1);
								break;
							}
						case 0xC5:
							{
								std::cout << "SET 0, L" << std::endl;
								bitmanip(L, 0, 1);
								break;
							}
						case 0xC6:
							{
								std::cout << "SET 0, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 0, 1);
								memory[address] = value;
								break;
							}
						case 0xC7:
							{
								std::cout << "SET 0, A" << std::endl;
								bitmanip(A, 0, 1);
								break;
							}
						case 0xC8:
							{
								std::cout << "SET 1, B" << std::endl;
								bitmanip(B, 1, 1);
								break;
							}
						case 0xC9:
							{
								std::cout << "SET 1, C" << std::endl;
								bitmanip(C, 1, 1);
								break;
							}
						case 0xCA:
							{
								std::cout << "SET 1, D" << std::endl;
								bitmanip(D, 1, 1);
								break;
							}
						case 0xCB:
							{
								std::cout << "SET 1, E" << std::endl;
								bitmanip(E, 1, 1);
								break;
							}
						case 0xCC:
							{
								std::cout << "SET 1, H" << std::endl;
								bitmanip(H, 1, 1);
								break;
							}
						case 0xCD:
							{
								std::cout << "SET 1, L" << std::endl;
								bitmanip(L, 1, 1);
								break;
							}
						case 0xCE:
							{
								std::cout << "SET 1, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 1, 1);
								memory[address] = value;
								break;
							}
						case 0xCF:
							{
								std::cout << "SET 1, A" << std::endl;
								bitmanip(A, 1, 1);
								break;
							}

							///////////////////////

						case 0xD0:
							{
								std::cout << "SET 2, B" << std::endl;
								bitmanip(B, 2, 1);
								break;
							}
						case 0xD1:
							{
								std::cout << "SET 2, C" << std::endl;
								bitmanip(C, 2, 1);
								break;
							}
						case 0xD2:
							{
								std::cout << "SET 2, D" << std::endl;
								bitmanip(D, 2, 1);
								break;
							}
						case 0xD3:
							{
								std::cout << "SET 2, E" << std::endl;
								bitmanip(E, 2, 1);
								break;
							}
						case 0xD4:
							{
								std::cout << "SET 2, H" << std::endl;
								bitmanip(H, 2, 1);
								break;
							}
						case 0xD5:
							{
								std::cout << "SET 2, L" << std::endl;
								bitmanip(L, 2, 1);
								break;
							}
						case 0xD6:
							{
								std::cout << "SET 2, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 2, 1);
								memory[address] = value;
								break;
							}
						case 0xD7:
							{
								std::cout << "SET 2, A" << std::endl;
								bitmanip(A, 2, 1);
								break;
							}
						case 0xD8:
							{
								std::cout << "SET 3, B" << std::endl;
								bitmanip(B, 3, 1);
								break;
							}
						case 0xD9:
							{
								std::cout << "SET 3, C" << std::endl;
								bitmanip(C, 3, 1);
								break;
							}
						case 0xDA:
							{
								std::cout << "SET 3, D" << std::endl;
								bitmanip(D, 3, 1);
								break;
							}
						case 0xDB:
							{
								std::cout << "SET 3, E" << std::endl;
								bitmanip(E, 3, 1);
								break;
							}
						case 0xDC:
							{
								std::cout << "SET 3, H" << std::endl;
								bitmanip(H, 3, 1);
								break;
							}
						case 0xDD:
							{
								std::cout << "SET 3, L" << std::endl;
								bitmanip(L, 3, 1);
								break;
							}
						case 0xDE:
							{
								std::cout << "SET 3, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 3, 1);
								memory[address] = value;
								break;
							}
						case 0xDF:
							{
								std::cout << "SET 3, A" << std::endl;
								bitmanip(A, 3, 1);
								break;
							}

							///////////////////////

						case 0xE0:
							{
								std::cout << "SET 4, B" << std::endl;
								bitmanip(B, 4, 1);
								break;
							}
						case 0xE1:
							{
								std::cout << "SET 4, C" << std::endl;
								bitmanip(C, 4, 1);
								break;
							}
						case 0xE2:
							{
								std::cout << "SET 4, D" << std::endl;
								bitmanip(D, 4, 1);
								break;
							}
						case 0xE3:
							{
								std::cout << "SET 4, E" << std::endl;
								bitmanip(E, 4, 1);
								break;
							}
						case 0xE4:
							{
								std::cout << "SET 4, H" << std::endl;
								bitmanip(H, 4, 1);
								break;
							}
						case 0xE5:
							{
								std::cout << "SET 4, L" << std::endl;
								bitmanip(L, 4, 1);
								break;
							}
						case 0xE6:
							{
								std::cout << "SET 4, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 4, 1);
								memory[address] = value;
								break;
							}
						case 0xE7:
							{
								std::cout << "SET 4, A" << std::endl;
								bitmanip(A, 4, 1);
								break;
							}
						case 0xE8:
							{
								std::cout << "SET 5, B" << std::endl;
								bitmanip(B, 5, 1);
								break;
							}
						case 0xE9:
							{
								std::cout << "SET 5, C" << std::endl;
								bitmanip(C, 5, 1);
								break;
							}
						case 0xEA:
							{
								std::cout << "SET 5, D" << std::endl;
								bitmanip(D, 5, 1);
								break;
							}
						case 0xEB:
							{
								std::cout << "SET 5, E" << std::endl;
								bitmanip(E, 5, 1);
								break;
							}
						case 0xEC:
							{
								std::cout << "SET 5, H" << std::endl;
								bitmanip(H, 5, 1);
								break;
							}
						case 0xED:
							{
								std::cout << "SET 5, L" << std::endl;
								bitmanip(L, 5, 1);
								break;
							}
						case 0xEE:
							{
								std::cout << "SET 5, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 5, 1);
								memory[address] = value;
								break;
							}
						case 0xEF:
							{
								std::cout << "SET 5, A" << std::endl;
								bitmanip(A, 5, 1);
								break;
							}

							///////////////////////

						case 0xF0:
							{
								std::cout << "SET 6, B" << std::endl;
								bitmanip(B, 6, 1);
								break;
							}
						case 0xF1:
							{
								std::cout << "SET 6, C" << std::endl;
								bitmanip(C, 6, 1);
								break;
							}
						case 0xF2:
							{
								std::cout << "SET 6, D" << std::endl;
								bitmanip(D, 6, 1);
								break;
							}
						case 0xF3:
							{
								std::cout << "SET 6, E" << std::endl;
								bitmanip(E, 6, 1);
								break;
							}
						case 0xF4:
							{
								std::cout << "SET 6, H" << std::endl;
								bitmanip(H, 6, 1);
								break;
							}
						case 0xF5:
							{
								std::cout << "SET 6, L" << std::endl;
								bitmanip(L, 6, 1);
								break;
							}
						case 0xF6:
							{
								std::cout << "SET 6, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 6, 1);
								memory[address] = value;
								break;
							}
						case 0xF7:
							{
								std::cout << "SET 6, A" << std::endl;
								bitmanip(A, 6, 1);
								break;
							}
						case 0xF8:
							{
								std::cout << "SET 7, B" << std::endl;
								bitmanip(B, 7, 1);
								break;
							}
						case 0xF9:
							{
								std::cout << "SET 7, C" << std::endl;
								bitmanip(C, 7, 1);
								break;
							}
						case 0xFA:
							{
								std::cout << "SET 7, D" << std::endl;
								bitmanip(D, 7, 1);
								break;
							}
						case 0xFB:
							{
								std::cout << "SET 7, E" << std::endl;
								bitmanip(E, 7, 1);
								break;
							}
						case 0xFC:
							{
								std::cout << "SET 7, H" << std::endl;
								bitmanip(H, 7, 1);
								break;
							}
						case 0xFD:
							{
								std::cout << "SET 7, L" << std::endl;
								bitmanip(L, 7, 1);
								break;
							}
						case 0xFE:
							{
								std::cout << "SET 7, (HL)" << std::endl;
								unsigned int address = H << 8 | L;
								unsigned char value = memory[address];
								bitmanip(value, 7, 1);
								memory[address] = value;
								break;
							}
						case 0xFF:
							{
								std::cout << "SET 7, A" << std::endl;
								bitmanip(A, 7, 1);
								break;
							}
					}
					break;
			}
			case 0xCD:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "CALL a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
					
					if ((b3 << 8 | b2) == 0xc79b) // init runtime
					{
						int wait = 1;
					}
					
					if (PC == 0xc0cd) // print_char
					{
						int wait = 1;
					}
					
					push16(PC);
					
					PC = b3 << 8 | b2;

					if (PC == 0xc17e)
					{
						int call_init_testing = 1;
					}

					if (PC == 0xc79b)
					{
						int wait = 1;
						std::cout << "***** CALL init_runtime" << std::endl;
					}
					
					if (PC == 0xc36d)
					{
						int wait = 1;
						std::cout << "***** CALL console_init" << std::endl;
					}
					
					if (PC == 0xc410)
					{
						int wait = 1;
						std::cout << "***** CALL console_hide" << std::endl;
					}
					
					if (PC == 0xc35c)
					{
						int wait = 1;
						std::cout << "***** CALL conosle_wait_vbl" << std::endl;
					}
					
					if (PC == 0xc456)
					{
						int wait = 1;
						std::cout << "***** CALL conosle_scroll_up" << std::endl;
					}

					//0xc17e - call_init_testing
					//0xc04d - init_testing_init_crc
					//0xc79b - init_runtime
					//0xc36d - console_init
					//0xc410 - console_hide
					//0xc35c - conosle_wait_vbl

					break;
				}
			case 0xCE:
				{
					unsigned char b2 = memory[PC++];
					std::cout << "ADC A, d8" << "0x" << charToHex(b2) << std::endl;

					signed char half = (0xF & (signed char)A) + (0xF & b2);
					if (CFlag())
						half++;
					setH(half > 0xF);
					unsigned int result = (A + b2 + (CFlag() ? 1 : 0));
					setC(result > 0xFF);
					A = result;

					setZ(A == 0);
					setN(false);


					break;
				}
			case 0xC9:
			{	
				auto address = pop16();
				std::cout << "RET - " << "0x" << intToHex(address) << std::endl;
				PC = address;
				
				if (PC == 0xc0cd)
				{
					int yurt = 1;
				}
				
				if (PC == 0xc414) // console_wait_vbl
				{
					int yurt = 1;
				}
				
				if (PC == 0xc370) // console_hide
				{
					int yurt = 1;
				}
				
				if (PC == 0xc39f) // console_scroll_up
				{
					int yurt = 1;
				}
				
				if (PC == 0xc401) // console_waitvbl
				{
					int yurt = 1;
				}
				
				break;
			}
			case 0xD0:
			{
				if (CFlag())
				{
					//PC++;
					std::cout << "RET NC - No jump" << std::endl;
				}
				else
				{
					PC = pop16();
					std::cout << "RET NC - " << "0x" << intToHex(PC) << std::endl;
				}
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
			case 0xD2:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "JP NC, a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
					if (!CFlag())
						PC = b3 << 8 | b2;

					break;
				}
			case 0xD4:
				{
					unsigned char b2 = memory[PC++];
					unsigned char b3 = memory[PC++];
					std::cout << "CALL NC, a16 - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;

					if (!CFlag())
					{
						push16(PC);
						PC = b3 << 8 | b2;
					}

					break;
				}
			case 0xD5:
			{
				unsigned int value = D << 8 | E;
				push16(value);
				std::cout << "PUSH DE" << std::endl;
				
				break;
			}
			case 0xD6:
			{
				unsigned char b2 = memory[PC++];
				std::cout << "SUB d8 - " << "0x" << charToHex(b2) << std::endl;
				
				bool carry = ((signed char)A - (signed char)b2) > 0;
				signed char half = (0xF & (signed char)A) - (0xF & b2);
				
				A = A - b2;
				
				setZ(A == 0);
				setN(true);
				
				setH(half < 0);
				setC(carry);
				
				break;
			}
			case 0xE0:
				{
					unsigned char b2 = memory[PC++];
					std::cout << "LD (a8), A - " << "0xFF" << charToHex(b2) << std::endl;
					unsigned int address = 0xFF << 8 | b2;
					memory[address] = A;
					
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
			case 0xE9:
				{
					std::cout << "JP HL" << std::endl;
					unsigned int address = H << 8 | L;
					PC = address;

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
			case 0xE6:
			{
				unsigned char b2 = memory[PC++];
				std::cout << "AND d8 - " << "0x" << charToHex(b2) << std::endl;
				
				A = A & b2;
				
				setZ(A == 0);
				setN(false);
				setH(true);
				setC(false);
				
				break;
			}
			case 0xEE:
			{
				std::cout << "XOR d8" << std::endl;
				xorA(memory[PC++]);
				
				break;
			}
			case 0xF0:
			{
				unsigned char b2 = memory[PC++];
				std::cout << "LD A, (a8) - " << "0xFF" << charToHex(b2) << std::endl;
				A = memory[0xFF00 + b2];
				
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
			case 0xF6:
			{
				unsigned char b2 = memory[PC++];
				std::cout << "OR d8 - " << "0x" << charToHex(b2) << std::endl;
				
				A = A | b2;
				
				setZ(A == 0);
				setN(false);
				setH(false);
				setC(false);
				
				break;
			}
			case 0xFA:
			{
				unsigned char b2 = memory[PC++];
				unsigned char b3 = memory[PC++];
				std::cout << "LOAD A (a16) - " << "0x" << charToHex(b3) << charToHex(b2) << std::endl;
				
				unsigned int address = b3 << 8 | b2;
				A = memory[address];
				
				break;
			}
			case 0xFE:
			{
				signed char b2 = memory[PC++];
				signed char result = (signed char)A - b2;
				std::cout << "CP d8 - " << charToHex(result) << std::endl;
				
				setZ(result == 0);
				setN(true);
				setC(result < 0);
				
				signed char half = (0xF & (signed char)A) - (0xF & b2);
				setH(half < 0);
				
				break;
			}
			default:
				abort();
		}
		//m_continue = false;
	}
	
	static unsigned char FB = memory[0xFF01];
	
	if (FB != memory[0xFF01])
	{
		int yurt = 1;
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

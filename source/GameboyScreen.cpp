#include "GameboyScreen.h"
#include "SDL/SDLInput.h"
#include "core/Engine.h"
#include "core/Renderer.h"
#include "core/ResourceManager.h"
#include "core/Utils.h"

#include <assert.h>
#include <fstream>
#include <iostream>
#include <sstream>

// #define DEBUG_PRINT

#ifdef DEBUG_PRINT
#define DEBUG_LOG(x) std::cout << x << '\n'
#else
#define DEBUG_LOG(x)
#endif

constexpr int display_width = 160;
constexpr int display_height = 144;
constexpr int display_size = display_width * display_height;
constexpr int memory_size = 65536;
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

constexpr unsigned int SB = 0xFF01;
constexpr unsigned int SC = 0xFF02;
constexpr unsigned int IE = 0xFFFF;
constexpr unsigned int IF = 0xFF0F;

constexpr unsigned int TIMA = 0xFF05;
constexpr unsigned int TMA = 0xFF06;
constexpr unsigned int TAC = 0xFF07;

static std::string console = "";
static std::ofstream logfile;

static unsigned char read_memory(const unsigned int address)
{
	if (address == 0xFF44)
		return 0x90;
	if (address == 0xFF4D)
		return 0xFF;
	return memory[address];
}

static void write_memory(const unsigned int address, unsigned char value)
{
	// if (address == TMA || address == TAC || address == IF || address == IE)
	if (address == IE)
	{
		int yurt = 1;
	}
	memory[address] = value;
}

struct OpcodeTimingData
{
	int min = 4;
	int max = 4;
	void set(int _max, int _min)
	{
		min = _min;
		max = _max;
	}
};

OpcodeTimingData opTimeData[0xFF];

static void initOpcodeTimingData()
{
	// x0
	opTimeData[0x00].set(4, 4);
	opTimeData[0x10].set(4, 4);
	opTimeData[0x20].set(12, 8);
	opTimeData[0x30].set(12, 8);

	opTimeData[0x40].set(4, 4);
	opTimeData[0x50].set(4, 4);
	opTimeData[0x60].set(4, 4);
	opTimeData[0x70].set(8, 8);

	opTimeData[0x80].set(4, 4);
	opTimeData[0x90].set(4, 4);
	opTimeData[0xA0].set(4, 4);
	opTimeData[0xB0].set(4, 4);

	opTimeData[0xC0].set(20, 8);
	opTimeData[0xD0].set(20, 8);
	opTimeData[0xE0].set(12, 12);
	opTimeData[0xF0].set(12, 12);

	// x1
	opTimeData[0x01].set(12, 12);
	opTimeData[0x11].set(12, 12);
	opTimeData[0x21].set(12, 12);
	opTimeData[0x31].set(12, 12);

	opTimeData[0x41].set(4, 4);
	opTimeData[0x51].set(4, 4);
	opTimeData[0x61].set(4, 4);
	opTimeData[0x71].set(8, 8);

	opTimeData[0x81].set(4, 4);
	opTimeData[0x91].set(4, 4);
	opTimeData[0xA1].set(4, 4);
	opTimeData[0xB1].set(4, 4);

	opTimeData[0xC1].set(12, 12);
	opTimeData[0xD1].set(12, 12);
	opTimeData[0xE1].set(12, 12);
	opTimeData[0xF1].set(12, 12);

	// x2
	opTimeData[0x02].set(8, 8);
	opTimeData[0x12].set(8, 8);
	opTimeData[0x22].set(8, 8);
	opTimeData[0x32].set(8, 8);

	opTimeData[0x42].set(4, 4);
	opTimeData[0x52].set(4, 4);
	opTimeData[0x62].set(4, 4);
	opTimeData[0x72].set(8, 8);

	opTimeData[0x82].set(4, 4);
	opTimeData[0x92].set(4, 4);
	opTimeData[0xA2].set(4, 4);
	opTimeData[0xB2].set(4, 4);

	opTimeData[0xC2].set(16, 12);
	opTimeData[0xD2].set(16, 12);
	opTimeData[0xE2].set(8, 8);
	opTimeData[0xF2].set(8, 8);

	// x3
	opTimeData[0x03].set(8, 8);
	opTimeData[0x13].set(8, 8);
	opTimeData[0x23].set(8, 8);
	opTimeData[0x33].set(8, 8);

	opTimeData[0x43].set(4, 4);
	opTimeData[0x53].set(4, 4);
	opTimeData[0x63].set(4, 4);
	opTimeData[0x73].set(8, 8);

	opTimeData[0x83].set(4, 4);
	opTimeData[0x93].set(4, 4);
	opTimeData[0xA3].set(4, 4);
	opTimeData[0xB3].set(4, 4);

	opTimeData[0xC3].set(16, 16);
	opTimeData[0xD3].set(4, 4);
	opTimeData[0xE3].set(4, 4);
	opTimeData[0xF3].set(4, 4);

	// x4
	opTimeData[0x04].set(4, 4);
	opTimeData[0x14].set(4, 4);
	opTimeData[0x24].set(4, 4);
	opTimeData[0x34].set(12, 12);

	opTimeData[0x44].set(4, 4);
	opTimeData[0x54].set(4, 4);
	opTimeData[0x64].set(4, 4);
	opTimeData[0x74].set(8, 8);

	opTimeData[0x84].set(4, 4);
	opTimeData[0x94].set(4, 4);
	opTimeData[0xA4].set(4, 4);
	opTimeData[0xB4].set(4, 4);

	opTimeData[0xC4].set(24, 12);
	opTimeData[0xD4].set(24, 12);
	opTimeData[0xE4].set(4, 4);
	opTimeData[0xF4].set(4, 4);

	// x5
	opTimeData[0x05].set(4, 4);
	opTimeData[0x15].set(4, 4);
	opTimeData[0x25].set(4, 4);
	opTimeData[0x35].set(12, 12);

	opTimeData[0x45].set(4, 4);
	opTimeData[0x55].set(4, 4);
	opTimeData[0x65].set(4, 4);
	opTimeData[0x75].set(8, 8);

	opTimeData[0x85].set(4, 4);
	opTimeData[0x95].set(4, 4);
	opTimeData[0xA5].set(4, 4);
	opTimeData[0xB5].set(4, 4);

	opTimeData[0xC5].set(16, 16);
	opTimeData[0xD5].set(16, 16);
	opTimeData[0xE5].set(16, 16);
	opTimeData[0xF5].set(16, 16);

	// x6
	opTimeData[0x06].set(8, 8);
	opTimeData[0x16].set(8, 8);
	opTimeData[0x26].set(8, 8);
	opTimeData[0x36].set(12, 12);

	opTimeData[0x46].set(8, 8);
	opTimeData[0x56].set(8, 8);
	opTimeData[0x66].set(8, 8);
	opTimeData[0x76].set(4, 4);

	opTimeData[0x86].set(8, 8);
	opTimeData[0x96].set(8, 8);
	opTimeData[0xA6].set(8, 8);
	opTimeData[0xB6].set(8, 8);

	opTimeData[0xC6].set(8, 8);
	opTimeData[0xD6].set(8, 8);
	opTimeData[0xE6].set(8, 8);
	opTimeData[0xF6].set(8, 8);

	// x7
	opTimeData[0x07].set(4, 4);
	opTimeData[0x17].set(4, 4);
	opTimeData[0x27].set(4, 4);
	opTimeData[0x37].set(4, 4);

	opTimeData[0x47].set(4, 4);
	opTimeData[0x57].set(4, 4);
	opTimeData[0x67].set(4, 4);
	opTimeData[0x77].set(8, 8);

	opTimeData[0x87].set(4, 4);
	opTimeData[0x97].set(4, 4);
	opTimeData[0xA7].set(4, 4);
	opTimeData[0xB7].set(4, 4);

	opTimeData[0xC7].set(16, 16);
	opTimeData[0xD7].set(16, 16);
	opTimeData[0xE7].set(16, 16);
	opTimeData[0xF7].set(16, 16);

	// x8
	opTimeData[0x08].set(20, 20);
	opTimeData[0x18].set(12, 12);
	opTimeData[0x28].set(12, 8);
	opTimeData[0x38].set(12, 8);

	opTimeData[0x48].set(4, 4);
	opTimeData[0x58].set(4, 4);
	opTimeData[0x68].set(4, 4);
	opTimeData[0x78].set(4, 4);

	opTimeData[0x88].set(4, 4);
	opTimeData[0x98].set(4, 4);
	opTimeData[0xA8].set(4, 4);
	opTimeData[0xB8].set(4, 4);

	opTimeData[0xC8].set(20, 8);
	opTimeData[0xD8].set(20, 8);
	opTimeData[0xE8].set(16, 16);
	opTimeData[0xF8].set(12, 12);

	// x9
	opTimeData[0x09].set(8, 8);
	opTimeData[0x19].set(8, 8);
	opTimeData[0x29].set(8, 8);
	opTimeData[0x39].set(8, 8);

	opTimeData[0x49].set(4, 4);
	opTimeData[0x59].set(4, 4);
	opTimeData[0x69].set(4, 4);
	opTimeData[0x79].set(4, 4);

	opTimeData[0x89].set(4, 4);
	opTimeData[0x99].set(4, 4);
	opTimeData[0xA9].set(4, 4);
	opTimeData[0xB9].set(4, 4);

	opTimeData[0xC9].set(16, 16);
	opTimeData[0xD9].set(16, 16);
	opTimeData[0xE9].set(4, 4);
	opTimeData[0xF9].set(8, 8);

	// xA
	opTimeData[0x0A].set(8, 8);
	opTimeData[0x1A].set(8, 8);
	opTimeData[0x2A].set(8, 8);
	opTimeData[0x3A].set(8, 8);

	opTimeData[0x4A].set(4, 4);
	opTimeData[0x5A].set(4, 4);
	opTimeData[0x6A].set(4, 4);
	opTimeData[0x7A].set(4, 4);

	opTimeData[0x8A].set(4, 4);
	opTimeData[0x9A].set(4, 4);
	opTimeData[0xAA].set(4, 4);
	opTimeData[0xBA].set(4, 4);

	opTimeData[0xCA].set(16, 12);
	opTimeData[0xDA].set(16, 12);
	opTimeData[0xEA].set(16, 16);
	opTimeData[0xFA].set(16, 16);

	// xB
	opTimeData[0x0B].set(8, 8);
	opTimeData[0x1B].set(8, 8);
	opTimeData[0x2B].set(8, 8);
	opTimeData[0x3B].set(8, 8);

	opTimeData[0x4B].set(4, 4);
	opTimeData[0x5B].set(4, 4);
	opTimeData[0x6B].set(4, 4);
	opTimeData[0x7B].set(4, 4);

	opTimeData[0x8B].set(4, 4);
	opTimeData[0x9B].set(4, 4);
	opTimeData[0xAB].set(4, 4);
	opTimeData[0xBB].set(4, 4);

	opTimeData[0xCB].set(4, 4);
	opTimeData[0xDB].set(4, 4);
	opTimeData[0xEB].set(4, 4);
	opTimeData[0xFB].set(4, 4);

	// xC
	opTimeData[0x0C].set(4, 4);
	opTimeData[0x1C].set(4, 4);
	opTimeData[0x2C].set(4, 4);
	opTimeData[0x3C].set(4, 4);

	opTimeData[0x4C].set(4, 4);
	opTimeData[0x5C].set(4, 4);
	opTimeData[0x6C].set(4, 4);
	opTimeData[0x7C].set(4, 4);

	opTimeData[0x8C].set(4, 4);
	opTimeData[0x9C].set(4, 4);
	opTimeData[0xAC].set(4, 4);
	opTimeData[0xBC].set(4, 4);

	opTimeData[0xCC].set(24, 12);
	opTimeData[0xDC].set(24, 12);
	opTimeData[0xEC].set(4, 4);
	opTimeData[0xFC].set(4, 4);

	// xD
	opTimeData[0x0D].set(4, 4);
	opTimeData[0x1D].set(4, 4);
	opTimeData[0x2D].set(4, 4);
	opTimeData[0x3D].set(4, 4);

	opTimeData[0x4D].set(4, 4);
	opTimeData[0x5D].set(4, 4);
	opTimeData[0x6D].set(4, 4);
	opTimeData[0x7D].set(4, 4);

	opTimeData[0x8D].set(4, 4);
	opTimeData[0x9D].set(4, 4);
	opTimeData[0xAD].set(4, 4);
	opTimeData[0xBD].set(4, 4);

	opTimeData[0xCD].set(24, 24);
	opTimeData[0xDD].set(4, 4);
	opTimeData[0xED].set(4, 4);
	opTimeData[0xFD].set(4, 4);

	// xE
	opTimeData[0x0E].set(8, 8);
	opTimeData[0x1E].set(8, 8);
	opTimeData[0x2E].set(8, 8);
	opTimeData[0x3E].set(8, 8);

	opTimeData[0x4E].set(8, 8);
	opTimeData[0x5E].set(8, 8);
	opTimeData[0x6E].set(8, 8);
	opTimeData[0x7E].set(8, 8);

	opTimeData[0x8E].set(8, 8);
	opTimeData[0x9E].set(8, 8);
	opTimeData[0xAE].set(8, 8);
	opTimeData[0xBE].set(8, 8);

	opTimeData[0xCE].set(8, 8);
	opTimeData[0xDE].set(8, 8);
	opTimeData[0xEE].set(8, 8);
	opTimeData[0xFE].set(8, 8);

	// xF
	opTimeData[0x0F].set(4, 4);
	opTimeData[0x1F].set(4, 4);
	opTimeData[0x2F].set(4, 4);
	opTimeData[0x3F].set(4, 4);

	opTimeData[0x4F].set(4, 4);
	opTimeData[0x5F].set(4, 4);
	opTimeData[0x6F].set(4, 4);
	opTimeData[0x7F].set(4, 4);

	opTimeData[0x8F].set(4, 4);
	opTimeData[0x9F].set(4, 4);
	opTimeData[0xAF].set(4, 4);
	opTimeData[0xBF].set(4, 4);

	opTimeData[0xCF].set(16, 16);
	opTimeData[0xDF].set(16, 16);
	opTimeData[0xEF].set(16, 16);
	opTimeData[0xFF].set(16, 16);
}

static bool conditionHit = false;
static void executeOpcode(unsigned char opcode);
static void handleInterrupts();
static void print_status();

struct CPUData
{
	CPUData()
	{
		initOpcodeTimingData();
	}

	enum class Param
	{
		OPCODE,
		FIRST,
		SECOND
	};
	Param m_param = Param::OPCODE;

	enum class Mode
	{
		FETCH,
		WAIT,
		EXECUTE
	};
	Mode m_state = Mode::FETCH;
	int m_waitTicks = 0;
	bool hasExecuted = false;
	bool halt = false;

	unsigned char m_opcode = 0x00;

	void tick()
	{
		switch (m_state)
		{
			case Mode::FETCH:
				fetch();
				break;
			case Mode::WAIT:
				wait();
				break;
			case Mode::EXECUTE:
				execute();
				break;
		}
	}

	void fetch()
	{
		//print_status();

		handleInterrupts();

		if (!halt)
		{
			hasExecuted = false;
			m_opcode = read_memory(PC++);
			m_waitTicks = opTimeData[m_opcode].min - 2; // read + wait + execute = 4
			m_state = Mode::WAIT;
		}
	}

	void wait()
	{
		m_waitTicks--;
		if (m_waitTicks == 0)
		{
			if (hasExecuted)
				m_state = Mode::FETCH;
			else
				m_state = Mode::EXECUTE;
		}
	}

	void execute()
	{
		conditionHit = false;
		executeOpcode(m_opcode);
		hasExecuted = true;

		if (conditionHit)
		{
			m_waitTicks = opTimeData[m_opcode].max - opTimeData[m_opcode].min; // read + wait + execute = 4
			m_state = Mode::WAIT;
		}
		else
		{
			m_state = Mode::FETCH;
		}
	}
};
CPUData CPU;

struct GPUData
{
	enum class Mode
	{
		OAM_SEARCH,
		PIXEL_TRANSFER,
		HBLANK,
		VBLANK
	};
	Mode m_state = Mode::OAM_SEARCH;
	
	int ticks = 0;
	
	void tick()
	{
		switch (m_state)
		{
			case Mode::OAM_SEARCH:
				oam_search();
				break;
			case Mode::PIXEL_TRANSFER:
				pixel_transfer();
				break;
			case Mode::HBLANK:
				hblank();
				break;
			case Mode::VBLANK:
				vblank();
				break;
			default:
				abort();
		}
	}
	
	void oam_search()
	{
		ticks++;
		if (ticks == 80)
		{
			ticks = 0;
			m_state = Mode::PIXEL_TRANSFER;
		}
	}
	
	void pixel_transfer()
	{
		m_state = Mode::HBLANK;
	}
	
	void hblank()
	{
		m_state = Mode::VBLANK;
	}
	void vblank()
	{
		m_state = Mode::OAM_SEARCH;
	}
	
};
GPUData GPU;

struct TimerData
{
	int ticks = 0;
	void tick()
	{
		unsigned char tac = read_memory(TAC);
		unsigned char enable = tac & 0x04;
		if (enable)
		{
			int mcount = 0;
			unsigned char select = tac & 0x03;
			switch (select)
			{
				case 0x00:
					{
						mcount = 256;
						break;
					}
				case 0x01:
					{
						mcount = 4;
						break;
					}
				case 0x02:
					{
						mcount = 16;
						break;
					}
				case 0x03:
					{
						mcount = 64;
						break;
					}
				default:
					{
						abort();
					}
			}

			if (ticks / 4 == mcount)
			{
				if ((unsigned char)read_memory(TIMA) == 0xFF)
				{
					write_memory(IF, read_memory(IF) | 0x04);
					write_memory(TIMA, read_memory(TMA));
					ticks = 0;
				}
				else
				{
					unsigned char tma = read_memory(TMA);
					unsigned char value = read_memory(TIMA);
					value++;
					write_memory(TIMA, value);
				}
				ticks = 1;
			}
			else
			{
				ticks++;
			}
		}
	}
};
TimerData Timer;

static void print_status()
{
	logfile << std::hex << std::setfill('0') << std::uppercase;
	logfile << "A:" << std::setw(2) << (int)A << " ";
	logfile << "F:" << std::setw(2) << (int)F << " ";
	logfile << "B:" << std::setw(2) << (int)B << " ";
	logfile << "C:" << std::setw(2) << (int)C << " ";
	logfile << "D:" << std::setw(2) << (int)D << " ";
	logfile << "E:" << std::setw(2) << (int)E << " ";
	logfile << "H:" << std::setw(2) << (int)H << " ";
	logfile << "L:" << std::setw(2) << (int)L << " ";

	logfile << "SP:" << std::setw(4) << (int)SP << " ";
	logfile << "PC:" << std::setw(4) << (int)PC << " ";

	logfile << "PCMEM:" << std::setw(2) << (int)read_memory(PC) << "," << std::setw(2) << (int)read_memory(PC + 1)
			<< "," << std::setw(2) << (int)read_memory(PC + 2) << "," << std::setw(2) << (int)read_memory(PC + 3)
			<< std::endl;
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

static bool CFlag()
{
	return F & 0x10;
}

static void setC(bool state)
{
	state == true ? F = F | 0x10 : F = F & 0xE0;
}

static bool HFlag()
{
	return F & 0x20;
}

static void setH(bool state)
{
	state == true ? F = F | 0x20 : F = F & 0xD0;
}

static bool NFlag()
{
	return F & 0x40;
}

static void setN(bool state)
{
	state == true ? F = F | 0x40 : F = F & 0xB0;
}

static bool ZFlag()
{
	return F & 0x80;
}

static void setZ(bool state)
{
	state == true ? F = F | 0x80 : F = F & 0x70;
}

static void push16(const unsigned int value)
{
	SP--;
	unsigned char byte = value >> 8;
	write_memory(SP--, byte);
	byte = value & 0xFF;
	write_memory(SP, byte);
}

static unsigned int pop16()
{
	unsigned char b2 = read_memory(SP++);
	unsigned char b3 = read_memory(SP++);

	return b3 << 8 | b2;
}

static void inc8(unsigned char& reg)
{
	setH((reg & 0x0F) == 0x0F);
	reg++;

	setZ(reg == 0);
	setN(false);
}

static void inc16(unsigned char& high, unsigned char& low)
{
	unsigned int value = high << 8 | low;
	value++;
	low = value & 0xFF;
	high = value >> 8;
}

static void dec16(unsigned char& high, unsigned char& low)
{
	unsigned int value = high << 8 | low;
	value--;
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

static void sub8(unsigned char& r1, unsigned char& r2, int carry = 0)
{
	setC((r2 + carry) > r1);
	signed char half = (0xF & (signed char)r1) - (0xF & r2) - carry;
	setH(half < 0);

	r1 = r1 - r2 - carry;

	setZ(r1 == 0);
	setN(true);
}

static void add8(unsigned char& r1, unsigned char& r2, int carry = 0)
{
	setC(r1 + r2 + carry > 0xFF);
	setH((r1 & 0x0F) + (r2 & 0x0F) + carry > 0x0F);

	r1 = r1 + r2 + carry;

	setZ(r1 == 0);
	setN(false);
}

static void add16(unsigned char& h1, unsigned char& l1, unsigned int& v2)
{
	unsigned int v1 = h1 << 8 | l1;
	unsigned int result = v1 + v2;

	setN(false);
	setC(result > 0xFFFF);
	// signed char half = (0xF & (signed char)h1) + ((0xF00 & v2) >> 8);
	// setH(half > 0xF);
	setH((v1 & 0xFFF) + (v2 & 0xFFF) > 0xFFF);

	if (result > 0xFFFF)
		result -= 0x10000;

	h1 = (result & 0xFF00) >> 8;
	l1 = result & 0xFF;
}

static void add16(unsigned char& h1, unsigned char& l1, unsigned char& h2, unsigned char& l2)
{
	unsigned int v1 = h1 << 8 | l1;
	unsigned int v2 = h2 << 8 | l2;
	unsigned int result = v1 + v2;

	setN(false);
	setC(result > 0xFFFF);
	unsigned int half = (0xFFF & v1) + (0xFFF & v2);
	setH(half > 0xFFF);

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

static void orA(unsigned char other)
{
	A = A | other;

	setZ(A == 0);
	setN(false);
	setH(false);
	setC(false);
}

static void andA(unsigned char other)
{
	A = A & other;

	setZ(A == 0);
	setN(false);
	setH(true);
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

static void SRL(unsigned char& reg)
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

static void compareWithA(unsigned char& value)
{
	unsigned char result = A - value;
	setZ(result == 0);
	setN(true);

	signed char half = (0xF & (signed char)A) - (0xF & value);
	setH(half < 0);
	setC(value > A);
}

static void swap(unsigned char& value)
{
	unsigned char tmp = value;
	value = (value >> 4) & 0x0F;
	tmp = (tmp << 4) & 0xF0;
	value = value | tmp;

	setZ(value == 0);
	setN(false);
	setC(false);
	setH(false);
}

static void bitCompToZ(unsigned char& value, int bit)
{
	unsigned char tmp = value;
	tmp = (tmp >> bit) & 0x01;
	setZ(tmp == 0x00); // compliment

	setN(false);
	setH(true);
}

void GameboyScreen::Init()
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
	// std::string path = "/Users/owenz0r/Downloads/01-special.gb"; // - passed
	std::string path = "/Users/owenz0r/Downloads/02-interrupts.gb"; // - passed
	// std::string path = "/Users/owenz0r/Downloads/03-op sp,hl.gb"; // - passed
	// std::string path = "/Users/owenz0r/Downloads/04-op r,imm.gb"; - passed
	// std::string path = "/Users/owenz0r/Downloads/05-op rp.gb"; - passed
	// std::string path = "/Users/owenz0r/Downloads/06-ld r,r.gb"; - passed
	// std::string path = "/Users/owenz0r/Downloads/07-jr,jp,call,ret,rst.gb"; - passed
	// std::string path = "/Users/owenz0r/Downloads/08-misc instrs.gb"; -- passed
	// std::string path = "/Users/owenz0r/Downloads/09-op r,r.gb"; - passed
	// std::string path = "/Users/owenz0r/Downloads/10-bit ops.gb"; -- passed
	// std::string path = "/Users/owenz0r/Downloads/11-op a,(hl).gb"; -- passed
	//std::string path = "/Users/owenz0r/Downloads/cpu_instrs.gb";
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
		DEBUG_LOG("Success");
	}
	else
	{
		std::cout << "No ROM found - " << path << std::endl;
	}

	// for gameboy doctor
	// this should be the initial state of the registers after boot rom

	A = 0x01;
	F = 0xB0;
	B = 0x00;
	C = 0x13;
	D = 0x00;
	E = 0xD8;
	H = 0x01;
	L = 0x4D;
	SP = 0xFFFE;
	PC = 0x0100;

	logfile.open("log.txt");
	if (!logfile.is_open())
	{
		std::cout << "Failed to open log file" << std::endl;
		return false;
	}
}

void GameboyScreen::Update(const double dt)
{
	if (m_continue)
	{
		CPU.tick();
		GPU.tick();
		Timer.tick();
	}
}

static void handleInterrupts()
{
	auto flag = read_memory(IF);
	auto enable = read_memory(IE);

	if (flag & 0x01 && enable & 0x01) // VBlank
	{
		CPU.halt = false;
		if (IME)
		{
			IME = false;
			write_memory(IF, read_memory(IF) & 0xFE);
			push16(PC);
			PC = 0x0040;
		}
	}
	else if (flag & 0x02 & enable & 0x02) // STAT
	{
		CPU.halt = false;
		if (IME)
		{
			IME = false;
			write_memory(IF, read_memory(IF) & 0xFD);
			push16(PC);
			PC = 0x0048;
		}
	}
	else if (flag & 0x04 & enable & 0x04) // Timer
	{
		CPU.halt = false;
		if (IME)
		{
			IME = false;
			write_memory(IF, read_memory(IF) & 0xFB);
			push16(PC);
			PC = 0x0050;
		}
	}
	else if (flag & 0x08 & enable & 0x08) // Serial
	{
		CPU.halt = false;
		if (IME)
		{
			IME = false;
			write_memory(IF, read_memory(IF) & 0xF7);
			push16(PC);
			PC = 0x0058;
		}
	}
	else if (flag & 0x10 & enable & 0x10) // Joypad
	{
		CPU.halt = false;
		if (IME)
		{
			IME = false;
			write_memory(IF, read_memory(IF) & 0xEF);
			push16(PC);
			PC = 0x0060;
		}
	}
}

static void executeOpcode(unsigned char opcode)
{
	int waits = 0;

	//	if (IME && read_memory(IF) > 0)
	//	{
	//		auto flag = read_memory(IF);
	//		auto enable = read_memory(IE);
	//
	//		if (flag & 0x01 && enable & 0x01) // VBlank
	//		{
	//			IME = false;
	//			write_memory(IF, read_memory(IF) & 0xFE);
	//			push16(PC);
	//			PC = 0x0040;
	//		}
	//		else if (flag & 0x02 & enable & 0x02) // STAT
	//		{
	//			IME = false;
	//			write_memory(IF, read_memory(IF) & 0xFD);
	//			push16(PC);
	//			PC = 0x0048;
	//		}
	//		else if (flag & 0x04 & enable & 0x04) // Timer
	//		{
	//			IME = false;
	//			write_memory(IF, read_memory(IF) & 0xFB);
	//			push16(PC);
	//			PC = 0x0050;
	//		}
	//		else if (flag & 0x08 & enable & 0x08) // Serial
	//		{
	//			IME = false;
	//			write_memory(IF, read_memory(IF) & 0xF7);
	//			push16(PC);
	//			PC = 0x0058;
	//		}
	//		else if (flag & 0x10 & enable & 0x10) // Joypad
	//		{
	//			IME = false;
	//			write_memory(IF, read_memory(IF) & 0xEF);
	//			push16(PC);
	//			PC = 0x0060;
	//		}
	//	}

	static int count = 0;
	DEBUG_LOG(std::dec << count++ << " PC 0x" << std::hex << PC << " - ");
	// unsigned char b1 = read_memory(PC++);

	// unsigned char n1 = (b1 >> 4) & 0x0F;
	// unsigned char n2 = b1 & 0x0F;

	// std::cout << charToHex(b1) << std::endl;

	DEBUG_LOG(std::hex << int(opcode));

	static int icount = 0;
	DEBUG_LOG("instruction - " << std::dec << icount++);

	switch (opcode)
	{
		case 0x00:
			{
				DEBUG_LOG("NOP");
				break;
			}
		case 0x01:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("LOAD BC d16 - " << "0x" << charToHex(b3) << charToHex(b2));

				if (b3 == 0x12)
				{
					int yurt = 1;
				}

				B = b3;
				C = b2;

				break;
			}
		case 0x02:
			{
				DEBUG_LOG("LD (BC), A");
				unsigned int address = B << 8 | C;
				write_memory(address, A);

				break;
			}
		case 0x03:
			{
				DEBUG_LOG("INC BC");
				inc16(B, C);

				break;
			}
		case 0x04:
			{
				DEBUG_LOG("INC B");
				inc8(B);

				break;
			}
		case 0x05:
			{
				DEBUG_LOG("DEC B");
				dec8(B);

				break;
			}
		case 0x06:
			{
				DEBUG_LOG("LD B, d8");
				B = read_memory(PC++);

				break;
			}
		case 0x07:
			{
				DEBUG_LOG("RLCA");
				RLC(A);
				setZ(false);

				break;
			}
		case 0x08:
			{
				DEBUG_LOG("LD (a16), SP");
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				unsigned int address = b3 << 8 | b2;

				write_memory(address, SP & 0x00FF);
				write_memory(address + 1, SP >> 8);

				break;
			}
		case 0x09:
			{
				DEBUG_LOG("ADD HL, BC");
				add16(H, L, B, C);

				break;
			}
		case 0x0B:
			{
				DEBUG_LOG("DEC BC");
				dec16(B, C);

				break;
			}
		case 0x0C:
			{
				DEBUG_LOG("INC C");
				inc8(C);

				break;
			}
		case 0x0D:
			{
				DEBUG_LOG("DEC C");
				dec8(C);

				break;
			}
		case 0x0A:
			{
				unsigned int address = B << 8 | C;
				A = read_memory(address);
				DEBUG_LOG("LOAD A, (BC) - " << charToHex(address));

				break;
			}
		case 0x0E:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("LOAD C, d8 - " << charToHex(b2));
				C = b2;

				break;
			}
		case 0x0F:
			{
				DEBUG_LOG("RRCA");
				RRC(A);
				setZ(false);
				setN(false);
				setH(false);

				break;
			}
		case 0x11:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("LOAD DE d16 - " << "0x" << charToHex(b3) << charToHex(b2));

				D = b3;
				E = b2;

				break;
			}
		case 0x12:
			{
				DEBUG_LOG("LOAD (DE), A");

				unsigned int address = D << 8 | E;
				write_memory(address, A);

				// setZ(A == 0);
				break;
			}
		case 0x13:
			{
				DEBUG_LOG("INC DE");
				inc16(D, E);

				break;
			}
		case 0x14:
			{
				DEBUG_LOG("INC D");
				inc8(D);

				break;
			}
		case 0x15:
			{
				DEBUG_LOG("DEC D");
				dec8(D);

				break;
			}
		case 0x16:
			{
				DEBUG_LOG("LD D, d8");
				D = read_memory(PC++);

				break;
			}
		case 0x17:
			{
				DEBUG_LOG("RL A");
				RL(A);
				setZ(false);

				break;
			}
		case 0x18:
			{
				DEBUG_LOG("JR s8");
				char b2 = read_memory(PC++);

				PC += b2;
				DEBUG_LOG("Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")");

				break;
			}
		case 0x19:
			{
				DEBUG_LOG("ADD HL, DE");
				add16(H, L, D, E);

				break;
			}
		case 0x1A:
			{
				unsigned int address = D << 8 | E;
				A = read_memory(address);
				DEBUG_LOG("LOAD A, (DE) - " << charToHex(address));

				break;
			}
		case 0x1B:
			{
				DEBUG_LOG("DEC DE");
				dec16(D, E);

				break;
			}
		case 0x1C:
			{
				DEBUG_LOG("INC E");
				inc8(E);
				break;
			}
		case 0x1D:
			{
				DEBUG_LOG("DEC E");
				dec8(E);

				break;
			}
		case 0x1E:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("LOAD E, d8 - " << charToHex(b2));
				E = b2;

				break;
			}
		case 0x1F:
			{
				DEBUG_LOG("RRA");

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
				DEBUG_LOG("JR NZ, s8");
				char b2 = read_memory(PC++);

				if (!ZFlag())
				{
					conditionHit = true;
					PC += b2;
					DEBUG_LOG("Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")");
				}
				else
				{
					DEBUG_LOG("Jumping not taken");
				}
				break;
			}
		case 0x21:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("LOAD HL d16 - " << "0x" << charToHex(b3) << charToHex(b2));

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
				DEBUG_LOG("LD (HL+), A");
				unsigned int address = H << 8 | L;
				write_memory(address++, A);

				L = address & 0xFF;
				H = address >> 8;

				break;
			}
		case 0x23:
			{
				DEBUG_LOG("INC HL");
				inc16(H, L);

				break;
			}
		case 0x24:
			{
				DEBUG_LOG("INC H");
				inc8(H);

				break;
			}
		case 0x25:
			{
				DEBUG_LOG("DEC H");
				dec8(H);

				break;
			}
		case 0x26:
			{
				DEBUG_LOG("LD H, d8");
				H = read_memory(PC++);

				break;
			}
		case 0x27:
			{
				DEBUG_LOG("DAA");
				unsigned char adjust = 0x00;

				if (NFlag())
				{
					if (HFlag())
						adjust += 0x06;
					if (CFlag())
						adjust += 0x60;
					A = A - adjust;
				}
				else
				{
					if (HFlag() || (A & 0x0F) > 0x09)
						adjust += 0x06;
					if (CFlag() || A > 0x99)
					{
						adjust += 0x60;
						setC(true);
					}
					A = A + adjust;
				}

				setZ(A == 0);
				setH(false);

				break;
			}
		case 0x28:
			{
				DEBUG_LOG("JR Z, s8");
				char b2 = read_memory(PC++);

				if (ZFlag())
				{
					conditionHit = true;
					PC += b2;
					DEBUG_LOG("Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")");
				}
				break;
			}
		case 0x29:
			{
				DEBUG_LOG("ADD HL, HL");
				add16(H, L, H, L);

				break;
			}

		case 0x2A:
			{
				DEBUG_LOG("LOAD A, (HL+)");

				unsigned int address = H << 8 | L;
				A = read_memory(address++);
				L = address & 0xFF;
				H = address >> 8;

				break;
			}
		case 0x2B:
			{
				DEBUG_LOG("DEC HL");
				dec16(H, L);

				break;
			}
		case 0x2C:
			{
				DEBUG_LOG("INC L");
				inc8(L);

				break;
			}
		case 0x2D:
			{
				DEBUG_LOG("DEC L");
				dec8(L);

				break;
			}
		case 0x2E:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("LOAD L, d8 - " << charToHex(b2));
				L = b2;

				break;
			}
		case 0x2F:
			{
				DEBUG_LOG("CPL");
				A = ~A;

				setN(true);
				setH(true);

				break;
			}
		case 0x30:
			{
				DEBUG_LOG("JR NC, s8");
				char b2 = read_memory(PC++);

				if (!CFlag())
				{
					conditionHit = true;
					PC += b2;
					DEBUG_LOG("Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")");
				}
				else
				{
					DEBUG_LOG("Jumping not taken");
				}
				break;
			}

		case 0x31:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);

				DEBUG_LOG("LOAD SP, d16");

				SP = b3 << 8 | b2;

				// setZ(A == 0);
				break;
			}
		case 0x32:
			{
				DEBUG_LOG("LD (HL-), A");
				unsigned int address = H << 8 | L;
				write_memory(address--, A);

				L = address & 0xFF;
				H = address >> 8;

				break;
			}
		case 0x33:
			{
				DEBUG_LOG("INC SP");
				SP++;

				break;
			}
		case 0x34:
			{
				DEBUG_LOG("INC (HL)");

				unsigned int address = H << 8 | L;
				unsigned char value = read_memory(address);

				setH((value & 0x0F) + 1 > 0x0F);
				value++;
				write_memory(address, value);

				setZ(read_memory(address) == 0);
				setN(false);
				break;
			}
		case 0x35:
			{
				DEBUG_LOG("DEC (HL)");

				unsigned int address = H << 8 | L;
				write_memory(address, read_memory(address) - 1);

				setZ(read_memory(address) == 0);
				setN(true);
				setH((read_memory(address) & 0x0F) == 0x0F);
				break;
			}
		case 0x36:
			{
				DEBUG_LOG("LD (HL), d8");

				unsigned int address = H << 8 | L;
				write_memory(address, read_memory(PC++));

				break;
			}
		case 0x37:
			{
				DEBUG_LOG("SCF");

				setC(true);
				setN(false);
				setH(false);

				break;
			}
		case 0x38:
			{
				DEBUG_LOG("JR C, s8");
				char b2 = read_memory(PC++);

				if (CFlag())
				{
					conditionHit = true;
					PC += b2;
					DEBUG_LOG("Jumping to - " << intToHex(PC) << " (" << charToHex(b2) << ")");
				}
				break;
			}
		case 0x39:
			{
				DEBUG_LOG("ADD HL, SP");
				add16(H, L, SP);

				break;
			}
		case 0x3A:
			{
				DEBUG_LOG("LOAD A, (HL+)");

				unsigned int address = H << 8 | L;
				A = read_memory(address--);
				L = address & 0xFF;
				H = address >> 8;

				break;
			}
		case 0x3B:
			{
				DEBUG_LOG("DEC SP");
				unsigned char high = SP >> 8;
				unsigned char low = SP & 0xFF;
				dec16(high, low);
				SP = high << 8 | low;

				break;
			}
		case 0x3C:
			{
				DEBUG_LOG("INC A");
				inc8(A);

				break;
			}
		case 0x3D:
			{
				DEBUG_LOG("DEC A");
				dec8(A);

				break;
			}
		case 0x3E:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("LOAD A, d8");
				A = b2;

				break;
			}
		case 0x3F:
			{
				DEBUG_LOG("CCF");
				setC(!CFlag());
				setN(false);
				setH(false);

				break;
			}
		case 0x40:
			{
				B = B;
				DEBUG_LOG("LOAD B, B");

				break;
			}
		case 0x41:
			{
				B = C;
				DEBUG_LOG("LOAD B, C");

				break;
			}
		case 0x42:
			{
				B = D;
				DEBUG_LOG("LOAD B, D");

				break;
			}
		case 0x43:
			{
				B = E;
				DEBUG_LOG("LOAD B, E");

				break;
			}
		case 0x44:
			{
				B = H;
				DEBUG_LOG("LOAD B, H");

				break;
			}
		case 0x45:
			{
				B = L;
				DEBUG_LOG("LOAD B, L");

				break;
			}
		case 0x46:
			{
				unsigned int address = H << 8 | L;
				B = read_memory(address);
				DEBUG_LOG("LOAD B, (HL)");

				break;
			}
		case 0x47:
			{
				B = A;
				DEBUG_LOG("LOAD B, A");

				break;
			}
		case 0x48:
			{
				C = B;
				DEBUG_LOG("LOAD C, B");

				break;
			}
		case 0x49:
			{
				C = C;
				DEBUG_LOG("LOAD C, C");

				break;
			}
		case 0x4A:
			{
				C = D;
				DEBUG_LOG("LOAD C, D");

				break;
			}
		case 0x4B:
			{
				C = E;
				DEBUG_LOG("LOAD C, E");

				break;
			}
		case 0x4C:
			{
				C = H;
				DEBUG_LOG("LOAD C, H");

				break;
			}
		case 0x4D:
			{
				C = L;
				DEBUG_LOG("LOAD C, L");

				break;
			}
		case 0x4E:
			{
				unsigned int address = H << 8 | L;
				C = read_memory(address);
				DEBUG_LOG("LOAD C, (HL)");

				break;
			}
		case 0x4F:
			{
				C = A;
				DEBUG_LOG("LOAD C, A");

				break;
			}
		case 0x50:
			{
				D = B;
				DEBUG_LOG("LOAD D, B");

				break;
			}
		case 0x51:
			{
				D = C;
				DEBUG_LOG("LOAD D, C");

				break;
			}
		case 0x52:
			{
				D = D;
				DEBUG_LOG("LOAD D, D");

				break;
			}
		case 0x53:
			{
				D = E;
				DEBUG_LOG("LOAD D, E");

				break;
			}
		case 0x54:
			{
				D = H;
				DEBUG_LOG("LOAD D, H");

				break;
			}
		case 0x55:
			{
				D = L;
				DEBUG_LOG("LOAD D, L");

				break;
			}
		case 0x56:
			{
				unsigned int address = H << 8 | L;
				D = read_memory(address);
				DEBUG_LOG("LOAD D, (HL)");

				break;
			}
		case 0x57:
			{
				D = A;
				DEBUG_LOG("LOAD D, A");

				break;
			}
		case 0x58:
			{
				E = B;
				DEBUG_LOG("LOAD E, B");

				break;
			}
		case 0x59:
			{
				E = C;
				DEBUG_LOG("LOAD E, C");

				break;
			}
		case 0x5A:
			{
				E = D;
				DEBUG_LOG("LOAD E, D");

				break;
			}
		case 0x5B:
			{
				E = E;
				DEBUG_LOG("LOAD E, E");

				break;
			}
		case 0x5C:
			{
				E = H;
				DEBUG_LOG("LOAD E, H");

				break;
			}
		case 0x5D:
			{
				E = L;
				DEBUG_LOG("LOAD E, L");

				break;
			}
		case 0x5E:
			{
				unsigned int address = H << 8 | L;
				E = read_memory(address);
				DEBUG_LOG("LOAD E, (HL)");

				break;
			}
		case 0x5F:
			{
				E = A;
				DEBUG_LOG("LOAD E, A");

				break;
			}
		case 0x60:
			{
				H = B;
				DEBUG_LOG("LOAD H, B");

				break;
			}
		case 0x61:
			{
				H = C;
				DEBUG_LOG("LOAD H, C");

				break;
			}
		case 0x62:
			{
				H = D;
				DEBUG_LOG("LOAD H, D");

				break;
			}
		case 0x63:
			{
				H = E;
				DEBUG_LOG("LOAD H, E");

				break;
			}
		case 0x64:
			{
				H = H;
				DEBUG_LOG("LOAD H, H");

				break;
			}
		case 0x65:
			{
				H = L;
				DEBUG_LOG("LOAD H, L");

				break;
			}
		case 0x66:
			{
				unsigned int address = H << 8 | L;
				H = read_memory(address);
				DEBUG_LOG("LOAD H, (HL)");

				break;
			}
		case 0x67:
			{
				H = A;
				DEBUG_LOG("LOAD H, A");

				break;
			}
		case 0x68:
			{
				L = B;
				DEBUG_LOG("LOAD L, B");

				break;
			}
		case 0x69:
			{
				L = C;
				DEBUG_LOG("LOAD L, C");

				break;
			}
		case 0x6A:
			{
				L = D;
				DEBUG_LOG("LOAD L, D");

				break;
			}
		case 0x6B:
			{
				L = E;
				DEBUG_LOG("LOAD L, E");

				break;
			}
		case 0x6C:
			{
				L = H;
				DEBUG_LOG("LOAD L, H");

				break;
			}
		case 0x6D:
			{
				L = L;
				DEBUG_LOG("LOAD L, L");

				break;
			}
		case 0x6E:
			{
				unsigned int address = H << 8 | L;
				L = read_memory(address);
				DEBUG_LOG("LOAD L, (HL)");

				break;
			}
		case 0x6F:
			{
				L = A;
				DEBUG_LOG("LOAD L, A");

				break;
			}
		case 0x70:
			{
				unsigned int address = H << 8 | L;
				write_memory(address, B);
				DEBUG_LOG("LOAD (HL), B");

				break;
			}
		case 0x71:
			{
				unsigned int address = H << 8 | L;
				write_memory(address, C);
				DEBUG_LOG("LOAD (HL), C");

				break;
			}
		case 0x72:
			{
				unsigned int address = H << 8 | L;
				write_memory(address, D);
				DEBUG_LOG("LOAD (HL), D");

				break;
			}
		case 0x73:
			{
				unsigned int address = H << 8 | L;
				write_memory(address, E);
				DEBUG_LOG("LOAD (HL), E");

				break;
			}
		case 0x74:
			{
				unsigned int address = H << 8 | L;
				write_memory(address, H);
				DEBUG_LOG("LOAD (HL), H");

				break;
			}
		case 0x75:
			{
				unsigned int address = H << 8 | L;
				write_memory(address, L);
				DEBUG_LOG("LOAD (HL), L");

				break;
			}
		case 0x76:
			{
				CPU.halt = true;
				break;
			}
		case 0x77:
			{
				unsigned int address = H << 8 | L;
				write_memory(address, A);
				DEBUG_LOG("LOAD (HL), A");

				break;
			}
		case 0x78:
			{
				A = B;
				DEBUG_LOG("LOAD A, B");

				break;
			}
		case 0x79:
			{
				A = C;
				DEBUG_LOG("LOAD A, C");

				break;
			}
		case 0x7A:
			{
				A = D;
				DEBUG_LOG("LOAD A, D");

				break;
			}
		case 0x7B:
			{
				A = E;
				DEBUG_LOG("LOAD A, E");

				break;
			}
		case 0x7C:
			{
				A = H;
				DEBUG_LOG("LOAD A, H");

				break;
			}
		case 0x7D:
			{
				A = L;
				DEBUG_LOG("LOAD A, L");

				break;
			}
		case 0x7E:
			{
				unsigned int address = H << 8 | L;
				A = read_memory(address);
				DEBUG_LOG("LOAD A, (HL)");

				break;
			}
		case 0x7F:
			{
				A = A;
				DEBUG_LOG("LOAD A, A");

				break;
			}
		case 0x80:
			{
				DEBUG_LOG("ADD B");
				add8(A, B, 0);

				break;
			}
		case 0x81:
			{
				DEBUG_LOG("ADD C");
				add8(A, C, 0);

				break;
			}
		case 0x82:
			{
				DEBUG_LOG("ADD D");
				add8(A, D, 0);

				break;
			}
		case 0x83:
			{
				DEBUG_LOG("ADD E");
				add8(A, E, 0);

				break;
			}
		case 0x84:
			{
				DEBUG_LOG("ADD H");
				add8(A, H, 0);

				break;
			}
		case 0x85:
			{
				DEBUG_LOG("ADD L");
				add8(A, L, 0);

				break;
			}
		case 0x86:
			{
				DEBUG_LOG("ADD (HL)");
				unsigned char value = read_memory(H << 8 | L);
				add8(A, value, 0);

				break;
			}
		case 0x87:
			{
				DEBUG_LOG("ADD A");
				add8(A, A, 0);

				break;
			}
		case 0x88:
			{
				DEBUG_LOG("ADC A, B");
				add8(A, B, CFlag());

				break;
			}
		case 0x89:
			{
				DEBUG_LOG("ADC A, C");
				add8(A, C, CFlag());

				break;
			}
		case 0x8A:
			{
				DEBUG_LOG("ADC A, D");
				add8(A, D, CFlag());

				break;
			}
		case 0x8B:
			{
				DEBUG_LOG("ADC A, E");
				add8(A, E, CFlag());

				break;
			}
		case 0x8C:
			{
				DEBUG_LOG("ADC A, H");
				add8(A, H, CFlag());

				break;
			}
		case 0x8D:
			{
				DEBUG_LOG("ADC A, L");
				add8(A, L, CFlag());

				break;
			}
		case 0x8E:
			{
				DEBUG_LOG("ADC A, (HL)");
				unsigned char value = read_memory(H << 8 | L);
				add8(A, value, CFlag());

				break;
			}
		case 0x8F:
			{
				DEBUG_LOG("ADC A, A");
				add8(A, A, CFlag());

				break;
			}
		case 0x90:
			{
				DEBUG_LOG("SUB B");
				sub8(A, B, 0);

				break;
			}
		case 0x91:
			{
				DEBUG_LOG("SUB C");
				sub8(A, C, 0);

				break;
			}
		case 0x92:
			{
				DEBUG_LOG("SUB D");
				sub8(A, D, 0);

				break;
			}
		case 0x93:
			{
				DEBUG_LOG("SUB E");
				sub8(A, E, 0);

				break;
			}
		case 0x94:
			{
				DEBUG_LOG("SUB H");
				sub8(A, H, 0);

				break;
			}
		case 0x95:
			{
				DEBUG_LOG("SUB L");
				sub8(A, L, 0);

				break;
			}
		case 0x96:
			{
				DEBUG_LOG("SUB (HL)");
				unsigned char value = read_memory(H << 8 | L);
				sub8(A, value, 0);

				break;
			}
		case 0x97:
			{
				DEBUG_LOG("SUB A");
				sub8(A, A, 0);

				break;
			}
		case 0x98:
			{
				DEBUG_LOG("SBC A, B");
				sub8(A, B, CFlag());

				break;
			}
		case 0x99:
			{
				DEBUG_LOG("SBC A, C");
				sub8(A, C, CFlag());

				break;
			}
		case 0x9A:
			{
				DEBUG_LOG("SBC A, D");
				sub8(A, D, CFlag());

				break;
			}
		case 0x9B:
			{
				DEBUG_LOG("SBC A, E");
				sub8(A, E, CFlag());

				break;
			}
		case 0x9C:
			{
				DEBUG_LOG("SBC A, H");
				sub8(A, H, CFlag());

				break;
			}
		case 0x9D:
			{
				DEBUG_LOG("SBC A, L");
				sub8(A, L, CFlag());

				break;
			}
		case 0x9E:
			{
				DEBUG_LOG("SBC A, (HL)");
				unsigned char value = read_memory(H << 8 | L);
				sub8(A, value, CFlag());

				break;
			}
		case 0x9F:
			{
				DEBUG_LOG("SBC A, A");
				sub8(A, A, CFlag());

				break;
			}
		case 0xA0:
			{
				DEBUG_LOG("AND B");
				andA(B);

				break;
			}
		case 0xA1:
			{
				DEBUG_LOG("AND C");
				andA(C);

				break;
			}
		case 0xA2:
			{
				DEBUG_LOG("AND D");
				andA(D);

				break;
			}
		case 0xA3:
			{
				DEBUG_LOG("AND E");
				andA(E);

				break;
			}
		case 0xA4:
			{
				DEBUG_LOG("AND H");
				andA(H);

				break;
			}
		case 0xA5:
			{
				DEBUG_LOG("AND L");
				andA(L);

				break;
			}
		case 0xA6:
			{
				DEBUG_LOG("AND (HL)");
				unsigned int address = H << 8 | L;
				andA(read_memory(address));

				break;
			}
		case 0xA7:
			{
				DEBUG_LOG("AND A");
				andA(A);

				break;
			}
		case 0xA8:
			{
				DEBUG_LOG("XOR B");
				xorA(B);

				break;
			}
		case 0xA9:
			{
				DEBUG_LOG("XOR C");
				xorA(C);

				break;
			}
		case 0xAA:
			{
				DEBUG_LOG("XOR D");
				xorA(D);

				break;
			}
		case 0xAB:
			{
				DEBUG_LOG("XOR E");
				xorA(E);

				break;
			}
		case 0xAC:
			{
				DEBUG_LOG("XOR H");
				xorA(H);

				break;
			}
		case 0xAD:
			{
				DEBUG_LOG("XOR L");
				xorA(L);

				break;
			}
		case 0xAE:
			{
				DEBUG_LOG("XOR (HL)");
				unsigned int address = H << 8 | L;
				xorA(read_memory(address));

				break;
			}
		case 0xAF:
			{
				DEBUG_LOG("XOR A");
				xorA(A);

				break;
			}
		case 0xB0:
			{
				DEBUG_LOG("OR A, B");
				orA(B);

				break;
			}
		case 0xB1:
			{
				DEBUG_LOG("OR A, C");
				orA(C);

				break;
			}
		case 0xB2:
			{
				DEBUG_LOG("OR A, D");
				orA(D);

				break;
			}
		case 0xB3:
			{
				DEBUG_LOG("OR A, E");
				orA(E);

				break;
			}
		case 0xB4:
			{
				DEBUG_LOG("OR A, H");
				orA(H);

				break;
			}
		case 0xB5:
			{
				DEBUG_LOG("OR A, L");
				orA(L);

				break;
			}
		case 0xB6:
			{
				DEBUG_LOG("OR A, (HL)");
				unsigned int address = H << 8 | L;
				orA(read_memory(address));

				break;
			}
		case 0xB7:
			{
				DEBUG_LOG("OR A, A");
				orA(A);

				break;
			}
		case 0xB8:
			{
				DEBUG_LOG("CP B");
				compareWithA(B);

				break;
			}
		case 0xB9:
			{
				DEBUG_LOG("CP C");
				compareWithA(C);

				break;
			}
		case 0xBA:
			{
				DEBUG_LOG("CP D");
				compareWithA(D);

				break;
			}
		case 0xBB:
			{
				DEBUG_LOG("CP E");
				compareWithA(E);

				break;
			}
		case 0xBC:
			{
				DEBUG_LOG("CP H");
				compareWithA(H);

				break;
			}
		case 0xBD:
			{
				DEBUG_LOG("CP L");
				compareWithA(L);

				break;
			}
		case 0xBE:
			{
				DEBUG_LOG("CP (HL)");
				unsigned int address = H << 8 | L;
				unsigned char value = read_memory(address);
				compareWithA(value);

				break;
			}
		case 0xBF:
			{
				DEBUG_LOG("CP A");
				compareWithA(A);

				break;
			}
		case 0xC0:
			{
				if (ZFlag())
				{
					DEBUG_LOG("RET NZ - No jump");
				}
				else
				{
					conditionHit = true;
					PC = pop16();
					DEBUG_LOG("RET NZ - " << "0x" << intToHex(PC));
				}
				break;
			}
		case 0xC1:
			{
				unsigned int value = pop16();
				DEBUG_LOG("POP BC - " << "0x" << intToHex(value));
				B = value >> 8;
				C = value & 0xFF;

				break;
			}
		case 0xC2:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("JP NZ, a16 - " << "0x" << charToHex(b3) << charToHex(b2));
				if (!ZFlag())
				{
					conditionHit = true;
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xC3:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("JMP a16 - " << "0x" << charToHex(b3) << charToHex(b2));
				PC = b3 << 8 | b2;

				break;
			}
		case 0xC4:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("CALL NZ, a16 - " << "0x" << charToHex(b3) << charToHex(b2));

				if (!ZFlag())
				{
					conditionHit = true;
					push16(PC);
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xC5:
			{
				unsigned int value = B << 8 | C;
				push16(value);
				DEBUG_LOG("PUSH BC");

				break;
			}
		case 0xC6:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("ADD d8 - " << "0x" << charToHex(b2));

				bool carry = ((signed int)A + (signed int)b2) > 0xFF;
				signed char half = (0xF & (signed char)A) + (0xF & b2);

				A = A + b2;

				setZ(A == 0);
				setN(false);

				setH(half > 0xF);
				setC(carry);

				break;
			}
		case 0xC7:
			{
				DEBUG_LOG("RST 0");
				push16(PC);
				PC = 0x0000;

				break;
			}
		case 0xC8:
			{
				if (!ZFlag())
				{
					DEBUG_LOG("RET Z - No jump");
				}
				else
				{
					conditionHit = true;
					PC = pop16();
					DEBUG_LOG("RET Z - " << "0x" << intToHex(PC));
				}
				break;
			}
		case 0xC9:
			{
				auto address = pop16();
				DEBUG_LOG("RET - " << "0x" << intToHex(address));
				PC = address;

				break;
			}
		case 0xCA:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("JP Z, a16 - " << "0x" << charToHex(b3) << charToHex(b2));
				if (ZFlag())
				{
					conditionHit = true;
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xCB:
			{
				unsigned char b2 = read_memory(PC++);
				switch (b2)
				{
					case 0x00:
						{
							DEBUG_LOG("RLC B");
							RLC(B);

							break;
						}
					case 0x01:
						{
							DEBUG_LOG("RLC C");
							RLC(C);

							break;
						}
					case 0x02:
						{
							DEBUG_LOG("RLC D");
							RLC(D);

							break;
						}
					case 0x03:
						{
							DEBUG_LOG("RLC E");
							RLC(E);

							break;
						}
					case 0x04:
						{
							DEBUG_LOG("RLC H");
							RLC(H);

							break;
						}
					case 0x05:
						{
							DEBUG_LOG("RLC L");
							RLC(L);

							break;
						}
					case 0x06:
						{
							DEBUG_LOG("RLC (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							RLC(value);
							write_memory(address, value);

							break;
						}
					case 0x07:
						{
							DEBUG_LOG("RLC A");
							RLC(A);

							break;
						}

					case 0x08:
						{
							DEBUG_LOG("RRC B");
							RRC(B);

							break;
						}
					case 0x09:
						{
							DEBUG_LOG("RRC C");
							RRC(C);

							break;
						}
					case 0x0A:
						{
							DEBUG_LOG("RRC D");
							RRC(D);

							break;
						}
					case 0x0B:
						{
							DEBUG_LOG("RRC E");
							RRC(E);

							break;
						}
					case 0x0C:
						{
							DEBUG_LOG("RRC H");
							RRC(H);

							break;
						}
					case 0x0D:
						{
							DEBUG_LOG("RRC L");
							RRC(L);

							break;
						}
					case 0x0E:
						{
							DEBUG_LOG("RRC (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							RRC(value);
							write_memory(address, value);

							break;
						}
					case 0x0F:
						{
							DEBUG_LOG("RRC A");
							RRC(A);

							break;
						}

						////////////////////////////////////////////

					case 0x10:
						{
							DEBUG_LOG("RL B");
							RL(B);

							break;
						}
					case 0x11:
						{
							DEBUG_LOG("RL C");
							RL(C);

							break;
						}
					case 0x12:
						{
							DEBUG_LOG("RL D");
							RL(D);

							break;
						}
					case 0x13:
						{
							DEBUG_LOG("RL E");
							RL(E);

							break;
						}
					case 0x14:
						{
							DEBUG_LOG("RL H");
							RL(H);

							break;
						}
					case 0x15:
						{
							DEBUG_LOG("RL L");
							RL(L);

							break;
						}
					case 0x16:
						{
							DEBUG_LOG("RL (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							RL(value);
							write_memory(address, value);

							break;
						}
					case 0x17:
						{
							DEBUG_LOG("RL A");
							RL(A);

							break;
						}

					case 0x18:
						{
							DEBUG_LOG("RR B");
							RR(B);

							break;
						}
					case 0x19:
						{
							DEBUG_LOG("RR C");
							RR(C);

							break;
						}
					case 0x1A:
						{
							DEBUG_LOG("RR D");
							RR(D);

							break;
						}
					case 0x1B:
						{
							DEBUG_LOG("RR E");
							RR(E);

							break;
						}
					case 0x1C:
						{
							DEBUG_LOG("RR H");
							RR(H);

							break;
						}
					case 0x1D:
						{
							DEBUG_LOG("RR L");
							RR(L);

							break;
						}
					case 0x1E:
						{
							DEBUG_LOG("RR (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							RR(value);
							write_memory(address, value);

							break;
						}
					case 0x1F:
						{
							DEBUG_LOG("RR A");
							RR(A);

							break;
						}

						////////////////////////////////////////////

					case 0x20:
						{
							DEBUG_LOG("SLA B");
							SLA(B);

							break;
						}
					case 0x21:
						{
							DEBUG_LOG("SLA C");
							SLA(C);

							break;
						}
					case 0x22:
						{
							DEBUG_LOG("SLA D");
							SLA(D);

							break;
						}
					case 0x23:
						{
							DEBUG_LOG("SLA E");
							SLA(E);

							break;
						}
					case 0x24:
						{
							DEBUG_LOG("SLA H");
							SLA(H);

							break;
						}
					case 0x25:
						{
							DEBUG_LOG("SLA L");
							SLA(L);

							break;
						}
					case 0x26:
						{
							DEBUG_LOG("SLA (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							SLA(value);
							write_memory(address, value);

							break;
						}
					case 0x27:
						{
							DEBUG_LOG("SLA A");
							SLA(A);

							break;
						}

					case 0x28:
						{
							DEBUG_LOG("SRA B");
							SRA(B);

							break;
						}
					case 0x29:
						{
							DEBUG_LOG("SRA C");
							SRA(C);

							break;
						}
					case 0x2A:
						{
							DEBUG_LOG("SRA D");
							SRA(D);

							break;
						}
					case 0x2B:
						{
							DEBUG_LOG("SRA E");
							SRA(E);

							break;
						}
					case 0x2C:
						{
							DEBUG_LOG("SRA H");
							SRA(H);

							break;
						}
					case 0x2D:
						{
							DEBUG_LOG("SRA L");
							SRA(L);

							break;
						}
					case 0x2E:
						{
							DEBUG_LOG("SRA (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							SRA(value);
							write_memory(address, value);

							break;
						}
					case 0x2F:
						{
							DEBUG_LOG("SRA A");
							SRA(A);

							break;
						}
					case 0x30:
						{
							DEBUG_LOG("SWAP B");
							swap(B);

							break;
						}
					case 0x31:
						{
							DEBUG_LOG("SWAP C");
							swap(C);

							break;
						}
					case 0x32:
						{
							DEBUG_LOG("SWAP D");
							swap(D);

							break;
						}
					case 0x33:
						{
							DEBUG_LOG("SWAP E");
							swap(E);

							break;
						}
					case 0x34:
						{
							DEBUG_LOG("SWAP H");
							swap(H);

							break;
						}
					case 0x35:
						{
							DEBUG_LOG("SWAP L");
							swap(L);

							break;
						}
					case 0x36:
						{
							DEBUG_LOG("SWAP (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							swap(value);
							write_memory(address, value);

							break;
						}
					case 0x37:
						{
							DEBUG_LOG("SWAP A");
							swap(A);

							break;
						}
					case 0x38:
						{
							DEBUG_LOG("SRL B");
							SRL(B);

							break;
						}
					case 0x39:
						{
							DEBUG_LOG("SRL C");
							SRL(C);

							break;
						}
					case 0x3A:
						{
							DEBUG_LOG("SRL D");
							SRL(D);

							break;
						}
					case 0x3B:
						{
							DEBUG_LOG("SRL E");
							SRL(E);

							break;
						}
					case 0x3C:
						{
							DEBUG_LOG("SRL H");
							SRL(H);

							break;
						}
					case 0x3D:
						{
							DEBUG_LOG("SRL L");
							SRL(L);

							break;
						}
					case 0x3E:
						{
							DEBUG_LOG("SRL (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							SRL(value);
							write_memory(address, value);

							break;
						}
					case 0x3F:
						{
							DEBUG_LOG("SRL A");
							SRL(A);

							break;
						}

						////////////////////////////////////////////

					case 0x40:
						{
							DEBUG_LOG("BIT 0, B");
							bitCompToZ(B, 0);

							break;
						}
					case 0x41:
						{
							DEBUG_LOG("BIT 0, C");
							bitCompToZ(C, 0);

							break;
						}
					case 0x42:
						{
							DEBUG_LOG("BIT 0, D");
							bitCompToZ(D, 0);

							break;
						}
					case 0x43:
						{
							DEBUG_LOG("BIT 0, E");
							bitCompToZ(E, 0);

							break;
						}
					case 0x44:
						{
							DEBUG_LOG("BIT 0, H");
							bitCompToZ(H, 0);

							break;
						}
					case 0x45:
						{
							DEBUG_LOG("BIT 0, L");
							bitCompToZ(L, 0);

							break;
						}
					case 0x46:
						{
							DEBUG_LOG("BIT 0, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 0);

							break;
						}
					case 0x47:
						{
							DEBUG_LOG("BIT 0, A");
							bitCompToZ(A, 0);

							break;
						}
					case 0x48:
						{
							DEBUG_LOG("BIT 1, B");
							bitCompToZ(B, 1);

							break;
						}
					case 0x49:
						{
							DEBUG_LOG("BIT 1, C");
							bitCompToZ(C, 1);

							break;
						}
					case 0x4A:
						{
							DEBUG_LOG("BIT 1, D");
							bitCompToZ(D, 1);

							break;
						}
					case 0x4B:
						{
							DEBUG_LOG("BIT 1, E");
							bitCompToZ(E, 1);

							break;
						}
					case 0x4C:
						{
							DEBUG_LOG("BIT 1, H");
							bitCompToZ(H, 1);

							break;
						}
					case 0x4D:
						{
							DEBUG_LOG("BIT 1, L");
							bitCompToZ(L, 1);

							break;
						}
					case 0x4E:
						{
							DEBUG_LOG("BIT 1, (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 1);

							break;
						}
					case 0x4F:
						{
							DEBUG_LOG("BIT 1, A");
							bitCompToZ(A, 1);

							break;
						}

						////////////////////////////////////////////

					case 0x50:
						{
							DEBUG_LOG("BIT 2, B");
							bitCompToZ(B, 2);

							break;
						}
					case 0x51:
						{
							DEBUG_LOG("BIT 2, C");
							bitCompToZ(C, 2);

							break;
						}
					case 0x52:
						{
							DEBUG_LOG("BIT 2, D");
							bitCompToZ(D, 2);

							break;
						}
					case 0x53:
						{
							DEBUG_LOG("BIT 2, E");
							bitCompToZ(E, 2);

							break;
						}
					case 0x54:
						{
							DEBUG_LOG("BIT 2, H");
							bitCompToZ(H, 2);

							break;
						}
					case 0x55:
						{
							DEBUG_LOG("BIT 2, L");
							bitCompToZ(L, 2);

							break;
						}
					case 0x56:
						{
							DEBUG_LOG("BIT 2, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 2);

							break;
						}
					case 0x57:
						{
							DEBUG_LOG("BIT 2, A");
							bitCompToZ(A, 2);

							break;
						}
					case 0x58:
						{
							DEBUG_LOG("BIT 3, B");
							bitCompToZ(B, 3);

							break;
						}
					case 0x59:
						{
							DEBUG_LOG("BIT 3, C");
							bitCompToZ(C, 3);

							break;
						}
					case 0x5A:
						{
							DEBUG_LOG("BIT 3, D");
							bitCompToZ(D, 3);

							break;
						}
					case 0x5B:
						{
							DEBUG_LOG("BIT 3, E");
							bitCompToZ(E, 3);

							break;
						}
					case 0x5C:
						{
							DEBUG_LOG("BIT 3, H");
							bitCompToZ(H, 3);

							break;
						}
					case 0x5D:
						{
							DEBUG_LOG("BIT 3, L");
							bitCompToZ(L, 3);

							break;
						}
					case 0x5E:
						{
							DEBUG_LOG("BIT 3, (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 3);

							break;
						}
					case 0x5F:
						{
							DEBUG_LOG("BIT 3, A");
							bitCompToZ(A, 3);

							break;
						}

						////////////////////////////////////////////

					case 0x60:
						{
							DEBUG_LOG("BIT 4, B");
							bitCompToZ(B, 4);

							break;
						}
					case 0x61:
						{
							DEBUG_LOG("BIT 4, C");
							bitCompToZ(C, 4);

							break;
						}
					case 0x62:
						{
							DEBUG_LOG("BIT 4, D");
							bitCompToZ(D, 4);

							break;
						}
					case 0x63:
						{
							DEBUG_LOG("BIT 4, E");
							bitCompToZ(E, 4);

							break;
						}
					case 0x64:
						{
							DEBUG_LOG("BIT 4, H");
							bitCompToZ(H, 4);

							break;
						}
					case 0x65:
						{
							DEBUG_LOG("BIT 4, L");
							bitCompToZ(L, 4);

							break;
						}
					case 0x66:
						{
							DEBUG_LOG("BIT 4, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 4);

							break;
						}
					case 0x67:
						{
							DEBUG_LOG("BIT 4, A");
							bitCompToZ(A, 4);

							break;
						}
					case 0x68:
						{
							DEBUG_LOG("BIT 5, B");
							bitCompToZ(B, 5);

							break;
						}
					case 0x69:
						{
							DEBUG_LOG("BIT 5, C");
							bitCompToZ(C, 5);

							break;
						}
					case 0x6A:
						{
							DEBUG_LOG("BIT 5, D");
							bitCompToZ(D, 5);

							break;
						}
					case 0x6B:
						{
							DEBUG_LOG("BIT 5, E");
							bitCompToZ(E, 5);

							break;
						}
					case 0x6C:
						{
							DEBUG_LOG("BIT 5, H");
							bitCompToZ(H, 5);

							break;
						}
					case 0x6D:
						{
							DEBUG_LOG("BIT 5, L");
							bitCompToZ(L, 5);

							break;
						}
					case 0x6E:
						{
							DEBUG_LOG("BIT 5, (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 5);

							break;
						}
					case 0x6F:
						{
							DEBUG_LOG("BIT 5, A");
							bitCompToZ(A, 5);

							break;
						}

						////////////////////////////////////////////

					case 0x70:
						{
							DEBUG_LOG("BIT 6, B");
							bitCompToZ(B, 6);

							break;
						}
					case 0x71:
						{
							DEBUG_LOG("BIT 6, C");
							bitCompToZ(C, 6);

							break;
						}
					case 0x72:
						{
							DEBUG_LOG("BIT 6, D");
							bitCompToZ(D, 6);

							break;
						}
					case 0x73:
						{
							DEBUG_LOG("BIT 6, E");
							bitCompToZ(E, 6);

							break;
						}
					case 0x74:
						{
							DEBUG_LOG("BIT 6, H");
							bitCompToZ(H, 6);

							break;
						}
					case 0x75:
						{
							DEBUG_LOG("BIT 6, L");
							bitCompToZ(L, 6);

							break;
						}
					case 0x76:
						{
							DEBUG_LOG("BIT 6, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 6);

							break;
						}
					case 0x77:
						{
							DEBUG_LOG("BIT 6, A");
							bitCompToZ(A, 6);

							break;
						}
					case 0x78:
						{
							DEBUG_LOG("BIT 7, B");
							bitCompToZ(B, 7);

							break;
						}
					case 0x79:
						{
							DEBUG_LOG("BIT 7, C");
							bitCompToZ(C, 7);

							break;
						}
					case 0x7A:
						{
							DEBUG_LOG("BIT 7, D");
							bitCompToZ(D, 7);

							break;
						}
					case 0x7B:
						{
							DEBUG_LOG("BIT 7, E");
							bitCompToZ(E, 7);

							break;
						}
					case 0x7C:
						{
							DEBUG_LOG("BIT 7, H");
							bitCompToZ(H, 7);

							break;
						}
					case 0x7D:
						{
							DEBUG_LOG("BIT 7, L");
							bitCompToZ(L, 7);

							break;
						}
					case 0x7E:
						{
							DEBUG_LOG("BIT 7, (HL)");

							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitCompToZ(value, 7);

							break;
						}
					case 0x7F:
						{
							DEBUG_LOG("BIT 7, A");
							bitCompToZ(A, 7);

							break;
						}

						////////////////////////////////////////////

						////////////////////////////////////////////

					case 0x80:
						{
							DEBUG_LOG("RES 0, B");
							bitmanip(B, 0, 0);
							break;
						}
					case 0x81:
						{
							DEBUG_LOG("RES 0, C");
							bitmanip(C, 0, 0);
							break;
						}
					case 0x82:
						{
							DEBUG_LOG("RES 0, D");
							bitmanip(D, 0, 0);
							break;
						}
					case 0x83:
						{
							DEBUG_LOG("RES 0, E");
							bitmanip(E, 0, 0);
							break;
						}
					case 0x84:
						{
							DEBUG_LOG("RES 0, H");
							bitmanip(H, 0, 0);
							break;
						}
					case 0x85:
						{
							DEBUG_LOG("RES 0, L");
							bitmanip(L, 0, 0);
							break;
						}
					case 0x86:
						{
							DEBUG_LOG("RES 0, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 0, 0);
							write_memory(address, value);
							break;
						}
					case 0x87:
						{
							DEBUG_LOG("RES 0, A");
							bitmanip(A, 0, 0);
							break;
						}
					case 0x88:
						{
							DEBUG_LOG("RES 1, B");
							bitmanip(B, 1, 0);
							break;
						}
					case 0x89:
						{
							DEBUG_LOG("RES 1, C");
							bitmanip(C, 1, 0);
							break;
						}
					case 0x8A:
						{
							DEBUG_LOG("RES 1, D");
							bitmanip(D, 1, 0);
							break;
						}
					case 0x8B:
						{
							DEBUG_LOG("RES 1, E");
							bitmanip(E, 1, 0);
							break;
						}
					case 0x8C:
						{
							DEBUG_LOG("RES 1, H");
							bitmanip(H, 1, 0);
							break;
						}
					case 0x8D:
						{
							DEBUG_LOG("RES 1, L");
							bitmanip(L, 1, 0);
							break;
						}
					case 0x8E:
						{
							DEBUG_LOG("RES 1, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 1, 0);
							write_memory(address, value);
							break;
						}
					case 0x8F:
						{
							DEBUG_LOG("RES 1, A");
							bitmanip(A, 1, 0);
							break;
						}

						///////////////////////

					case 0x90:
						{
							DEBUG_LOG("RES 2, B");
							bitmanip(B, 2, 0);
							break;
						}
					case 0x91:
						{
							DEBUG_LOG("RES 2, C");
							bitmanip(C, 2, 0);
							break;
						}
					case 0x92:
						{
							DEBUG_LOG("RES 2, D");
							bitmanip(D, 2, 0);
							break;
						}
					case 0x93:
						{
							DEBUG_LOG("RES 2, E");
							bitmanip(E, 2, 0);
							break;
						}
					case 0x94:
						{
							DEBUG_LOG("RES 2, H");
							bitmanip(H, 2, 0);
							break;
						}
					case 0x95:
						{
							DEBUG_LOG("RES 2, L");
							bitmanip(L, 2, 0);
							break;
						}
					case 0x96:
						{
							DEBUG_LOG("RES 2, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 2, 0);
							write_memory(address, value);
							break;
						}
					case 0x97:
						{
							DEBUG_LOG("RES 2, A");
							bitmanip(A, 2, 0);
							break;
						}
					case 0x98:
						{
							DEBUG_LOG("RES 3, B");
							bitmanip(B, 3, 0);
							break;
						}
					case 0x99:
						{
							DEBUG_LOG("RES 3, C");
							bitmanip(C, 3, 0);
							break;
						}
					case 0x9A:
						{
							DEBUG_LOG("RES 3, D");
							bitmanip(D, 3, 0);
							break;
						}
					case 0x9B:
						{
							DEBUG_LOG("RES 3, E");
							bitmanip(E, 3, 0);
							break;
						}
					case 0x9C:
						{
							DEBUG_LOG("RES 3, H");
							bitmanip(H, 3, 0);
							break;
						}
					case 0x9D:
						{
							DEBUG_LOG("RES 3, L");
							bitmanip(L, 3, 0);
							break;
						}
					case 0x9E:
						{
							DEBUG_LOG("RES 3, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 3, 0);
							write_memory(address, value);
							break;
						}
					case 0x9F:
						{
							DEBUG_LOG("RES 3, A");
							bitmanip(A, 3, 0);
							break;
						}

						///////////////////////

					case 0xA0:
						{
							DEBUG_LOG("RES 4, B");
							bitmanip(B, 4, 0);
							break;
						}
					case 0xA1:
						{
							DEBUG_LOG("RES 4, C");
							bitmanip(C, 4, 0);
							break;
						}
					case 0xA2:
						{
							DEBUG_LOG("RES 4, D");
							bitmanip(D, 4, 0);
							break;
						}
					case 0xA3:
						{
							DEBUG_LOG("RES 4, E");
							bitmanip(E, 4, 0);
							break;
						}
					case 0xA4:
						{
							DEBUG_LOG("RES 4, H");
							bitmanip(H, 4, 0);
							break;
						}
					case 0xA5:
						{
							DEBUG_LOG("RES 4, L");
							bitmanip(L, 4, 0);
							break;
						}
					case 0xA6:
						{
							DEBUG_LOG("RES 4, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 4, 0);
							write_memory(address, value);
							break;
						}
					case 0xA7:
						{
							DEBUG_LOG("RES 4, A");
							bitmanip(A, 4, 0);
							break;
						}
					case 0xA8:
						{
							DEBUG_LOG("RES 5, B");
							bitmanip(B, 5, 0);
							break;
						}
					case 0xA9:
						{
							DEBUG_LOG("RES 5, C");
							bitmanip(C, 5, 0);
							break;
						}
					case 0xAA:
						{
							DEBUG_LOG("RES 5, D");
							bitmanip(D, 5, 0);
							break;
						}
					case 0xAB:
						{
							DEBUG_LOG("RES 5, E");
							bitmanip(E, 5, 0);
							break;
						}
					case 0xAC:
						{
							DEBUG_LOG("RES 5, H");
							bitmanip(H, 5, 0);
							break;
						}
					case 0xAD:
						{
							DEBUG_LOG("RES 5, L");
							bitmanip(L, 5, 0);
							break;
						}
					case 0xAE:
						{
							DEBUG_LOG("RES 5, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 5, 0);
							write_memory(address, value);
							break;
						}
					case 0xAF:
						{
							DEBUG_LOG("RES 5, A");
							bitmanip(A, 5, 0);
							break;
						}

						///////////////////////

					case 0xB0:
						{
							DEBUG_LOG("RES 6, B");
							bitmanip(B, 6, 0);
							break;
						}
					case 0xB1:
						{
							DEBUG_LOG("RES 6, C");
							bitmanip(C, 6, 0);
							break;
						}
					case 0xB2:
						{
							DEBUG_LOG("RES 6, D");
							bitmanip(D, 6, 0);
							break;
						}
					case 0xB3:
						{
							DEBUG_LOG("RES 6, E");
							bitmanip(E, 6, 0);
							break;
						}
					case 0xB4:
						{
							DEBUG_LOG("RES 6, H");
							bitmanip(H, 6, 0);
							break;
						}
					case 0xB5:
						{
							DEBUG_LOG("RES 6, L");
							bitmanip(L, 6, 0);
							break;
						}
					case 0xB6:
						{
							DEBUG_LOG("RES 6, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 6, 0);
							write_memory(address, value);
							break;
						}
					case 0xB7:
						{
							DEBUG_LOG("RES 6, A");
							bitmanip(A, 6, 0);
							break;
						}
					case 0xB8:
						{
							DEBUG_LOG("RES 7, B");
							bitmanip(B, 7, 0);
							break;
						}
					case 0xB9:
						{
							DEBUG_LOG("RES 7, C");
							bitmanip(C, 7, 0);
							break;
						}
					case 0xBA:
						{
							DEBUG_LOG("RES 7, D");
							bitmanip(D, 7, 0);
							break;
						}
					case 0xBB:
						{
							DEBUG_LOG("RES 7, E");
							bitmanip(E, 7, 0);
							break;
						}
					case 0xBC:
						{
							DEBUG_LOG("RES 7, H");
							bitmanip(H, 7, 0);
							break;
						}
					case 0xBD:
						{
							DEBUG_LOG("RES 7, L");
							bitmanip(L, 7, 0);
							break;
						}
					case 0xBE:
						{
							DEBUG_LOG("RES 7, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 7, 0);
							write_memory(address, value);
							break;
						}
					case 0xBF:
						{
							DEBUG_LOG("RES 7, A");
							bitmanip(A, 7, 0);
							break;
						}

						////////////////////////////////

						////////////////////////////////

					case 0xC0:
						{
							DEBUG_LOG("SET 0, B");
							bitmanip(B, 0, 1);
							break;
						}
					case 0xC1:
						{
							DEBUG_LOG("SET 0, C");
							bitmanip(C, 0, 1);
							break;
						}
					case 0xC2:
						{
							DEBUG_LOG("SET 0, D");
							bitmanip(D, 0, 1);
							break;
						}
					case 0xC3:
						{
							DEBUG_LOG("SET 0, E");
							bitmanip(E, 0, 1);
							break;
						}
					case 0xC4:
						{
							DEBUG_LOG("SET 0, H");
							bitmanip(H, 0, 1);
							break;
						}
					case 0xC5:
						{
							DEBUG_LOG("SET 0, L");
							bitmanip(L, 0, 1);
							break;
						}
					case 0xC6:
						{
							DEBUG_LOG("SET 0, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 0, 1);
							write_memory(address, value);
							break;
						}
					case 0xC7:
						{
							DEBUG_LOG("SET 0, A");
							bitmanip(A, 0, 1);
							break;
						}
					case 0xC8:
						{
							DEBUG_LOG("SET 1, B");
							bitmanip(B, 1, 1);
							break;
						}
					case 0xC9:
						{
							DEBUG_LOG("SET 1, C");
							bitmanip(C, 1, 1);
							break;
						}
					case 0xCA:
						{
							DEBUG_LOG("SET 1, D");
							bitmanip(D, 1, 1);
							break;
						}
					case 0xCB:
						{
							DEBUG_LOG("SET 1, E");
							bitmanip(E, 1, 1);
							break;
						}
					case 0xCC:
						{
							DEBUG_LOG("SET 1, H");
							bitmanip(H, 1, 1);
							break;
						}
					case 0xCD:
						{
							DEBUG_LOG("SET 1, L");
							bitmanip(L, 1, 1);
							break;
						}
					case 0xCE:
						{
							DEBUG_LOG("SET 1, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 1, 1);
							write_memory(address, value);
							break;
						}
					case 0xCF:
						{
							DEBUG_LOG("SET 1, A");
							bitmanip(A, 1, 1);
							break;
						}

						///////////////////////

					case 0xD0:
						{
							DEBUG_LOG("SET 2, B");
							bitmanip(B, 2, 1);
							break;
						}
					case 0xD1:
						{
							DEBUG_LOG("SET 2, C");
							bitmanip(C, 2, 1);
							break;
						}
					case 0xD2:
						{
							DEBUG_LOG("SET 2, D");
							bitmanip(D, 2, 1);
							break;
						}
					case 0xD3:
						{
							DEBUG_LOG("SET 2, E");
							bitmanip(E, 2, 1);
							break;
						}
					case 0xD4:
						{
							DEBUG_LOG("SET 2, H");
							bitmanip(H, 2, 1);
							break;
						}
					case 0xD5:
						{
							DEBUG_LOG("SET 2, L");
							bitmanip(L, 2, 1);
							break;
						}
					case 0xD6:
						{
							DEBUG_LOG("SET 2, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 2, 1);
							write_memory(address, value);
							break;
						}
					case 0xD7:
						{
							DEBUG_LOG("SET 2, A");
							bitmanip(A, 2, 1);
							break;
						}
					case 0xD8:
						{
							DEBUG_LOG("SET 3, B");
							bitmanip(B, 3, 1);
							break;
						}
					case 0xD9:
						{
							DEBUG_LOG("SET 3, C");
							bitmanip(C, 3, 1);
							break;
						}
					case 0xDA:
						{
							DEBUG_LOG("SET 3, D");
							bitmanip(D, 3, 1);
							break;
						}
					case 0xDB:
						{
							DEBUG_LOG("SET 3, E");
							bitmanip(E, 3, 1);
							break;
						}
					case 0xDC:
						{
							DEBUG_LOG("SET 3, H");
							bitmanip(H, 3, 1);
							break;
						}
					case 0xDD:
						{
							DEBUG_LOG("SET 3, L");
							bitmanip(L, 3, 1);
							break;
						}
					case 0xDE:
						{
							DEBUG_LOG("SET 3, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 3, 1);
							write_memory(address, value);
							break;
						}
					case 0xDF:
						{
							DEBUG_LOG("SET 3, A");
							bitmanip(A, 3, 1);
							break;
						}

						///////////////////////

					case 0xE0:
						{
							DEBUG_LOG("SET 4, B");
							bitmanip(B, 4, 1);
							break;
						}
					case 0xE1:
						{
							DEBUG_LOG("SET 4, C");
							bitmanip(C, 4, 1);
							break;
						}
					case 0xE2:
						{
							DEBUG_LOG("SET 4, D");
							bitmanip(D, 4, 1);
							break;
						}
					case 0xE3:
						{
							DEBUG_LOG("SET 4, E");
							bitmanip(E, 4, 1);
							break;
						}
					case 0xE4:
						{
							DEBUG_LOG("SET 4, H");
							bitmanip(H, 4, 1);
							break;
						}
					case 0xE5:
						{
							DEBUG_LOG("SET 4, L");
							bitmanip(L, 4, 1);
							break;
						}
					case 0xE6:
						{
							DEBUG_LOG("SET 4, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 4, 1);
							write_memory(address, value);
							break;
						}
					case 0xE7:
						{
							DEBUG_LOG("SET 4, A");
							bitmanip(A, 4, 1);
							break;
						}
					case 0xE8:
						{
							DEBUG_LOG("SET 5, B");
							bitmanip(B, 5, 1);
							break;
						}
					case 0xE9:
						{
							DEBUG_LOG("SET 5, C");
							bitmanip(C, 5, 1);
							break;
						}
					case 0xEA:
						{
							DEBUG_LOG("SET 5, D");
							bitmanip(D, 5, 1);
							break;
						}
					case 0xEB:
						{
							DEBUG_LOG("SET 5, E");
							bitmanip(E, 5, 1);
							break;
						}
					case 0xEC:
						{
							DEBUG_LOG("SET 5, H");
							bitmanip(H, 5, 1);
							break;
						}
					case 0xED:
						{
							DEBUG_LOG("SET 5, L");
							bitmanip(L, 5, 1);
							break;
						}
					case 0xEE:
						{
							DEBUG_LOG("SET 5, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 5, 1);
							write_memory(address, value);
							break;
						}
					case 0xEF:
						{
							DEBUG_LOG("SET 5, A");
							bitmanip(A, 5, 1);
							break;
						}

						///////////////////////

					case 0xF0:
						{
							DEBUG_LOG("SET 6, B");
							bitmanip(B, 6, 1);
							break;
						}
					case 0xF1:
						{
							DEBUG_LOG("SET 6, C");
							bitmanip(C, 6, 1);
							break;
						}
					case 0xF2:
						{
							DEBUG_LOG("SET 6, D");
							bitmanip(D, 6, 1);
							break;
						}
					case 0xF3:
						{
							DEBUG_LOG("SET 6, E");
							bitmanip(E, 6, 1);
							break;
						}
					case 0xF4:
						{
							DEBUG_LOG("SET 6, H");
							bitmanip(H, 6, 1);
							break;
						}
					case 0xF5:
						{
							DEBUG_LOG("SET 6, L");
							bitmanip(L, 6, 1);
							break;
						}
					case 0xF6:
						{
							DEBUG_LOG("SET 6, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 6, 1);
							write_memory(address, value);
							break;
						}
					case 0xF7:
						{
							DEBUG_LOG("SET 6, A");
							bitmanip(A, 6, 1);
							break;
						}
					case 0xF8:
						{
							DEBUG_LOG("SET 7, B");
							bitmanip(B, 7, 1);
							break;
						}
					case 0xF9:
						{
							DEBUG_LOG("SET 7, C");
							bitmanip(C, 7, 1);
							break;
						}
					case 0xFA:
						{
							DEBUG_LOG("SET 7, D");
							bitmanip(D, 7, 1);
							break;
						}
					case 0xFB:
						{
							DEBUG_LOG("SET 7, E");
							bitmanip(E, 7, 1);
							break;
						}
					case 0xFC:
						{
							DEBUG_LOG("SET 7, H");
							bitmanip(H, 7, 1);
							break;
						}
					case 0xFD:
						{
							DEBUG_LOG("SET 7, L");
							bitmanip(L, 7, 1);
							break;
						}
					case 0xFE:
						{
							DEBUG_LOG("SET 7, (HL)");
							unsigned int address = H << 8 | L;
							unsigned char value = read_memory(address);
							bitmanip(value, 7, 1);
							write_memory(address, value);
							break;
						}
					case 0xFF:
						{
							DEBUG_LOG("SET 7, A");
							bitmanip(A, 7, 1);
							break;
						}
					default:
						abort();
				}
				break;
			}
		case 0xCC:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("CALL Z, a16 - " << "0x" << charToHex(b3) << charToHex(b2));

				if (ZFlag())
				{
					conditionHit = true;
					push16(PC);
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xCD:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("CALL a16 - " << "0x" << charToHex(b3) << charToHex(b2));

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
					DEBUG_LOG("***** CALL init_runtime");
				}

				if (PC == 0xc36d)
				{
					int wait = 1;
					DEBUG_LOG("***** CALL console_init");
				}

				if (PC == 0xc410)
				{
					int wait = 1;
					DEBUG_LOG("***** CALL console_hide");
				}

				if (PC == 0xc35c)
				{
					int wait = 1;
					DEBUG_LOG("***** CALL conosle_wait_vbl");
				}

				if (PC == 0xc456)
				{
					int wait = 1;
					DEBUG_LOG("***** CALL conosle_scroll_up");
				}

				// 0xc17e - call_init_testing
				// 0xc04d - init_testing_init_crc
				// 0xc79b - init_runtime
				// 0xc36d - console_init
				// 0xc410 - console_hide
				// 0xc35c - conosle_wait_vbl

				break;
			}
		case 0xCE:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("ADC A, d8" << "0x" << charToHex(b2));

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
		case 0xCF:
			{
				DEBUG_LOG("RST 1");
				push16(PC);
				PC = 0x0008;

				break;
			}
		case 0xD0:
			{
				if (CFlag())
				{
					// PC++;
					DEBUG_LOG("RET NC - No jump");
				}
				else
				{
					conditionHit = true;
					PC = pop16();
					DEBUG_LOG("RET NC - " << "0x" << intToHex(PC));
				}
				break;
			}
		case 0xD1:
			{
				unsigned int value = pop16();
				DEBUG_LOG("POP DE - " << "0x" << intToHex(value));
				D = value >> 8;
				E = value & 0xFF;

				break;
			}
		case 0xD2:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("JP NC, a16 - " << "0x" << charToHex(b3) << charToHex(b2));
				if (!CFlag())
				{
					conditionHit = true;
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xD4:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("CALL NC, a16 - " << "0x" << charToHex(b3) << charToHex(b2));

				if (!CFlag())
				{
					conditionHit = true;
					push16(PC);
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xD5:
			{
				unsigned int value = D << 8 | E;
				push16(value);
				DEBUG_LOG("PUSH DE");

				break;
			}
		case 0xD6:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("SUB d8 - " << "0x" << charToHex(b2));

				// bool carry = ((signed char)A - (signed char)b2) > 0;
				setC(b2 > A);
				signed char half = (0xF & (signed char)A) - (0xF & b2);

				A = A - b2;

				setZ(A == 0);
				setN(true);
				setH(half < 0);

				break;
			}
		case 0xD7:
			{
				DEBUG_LOG("RST 2");
				push16(PC);
				PC = 0x0010;

				break;
			}
		case 0xD8:
			{
				if (!CFlag())
				{
					DEBUG_LOG("RET C - No jump");
				}
				else
				{
					conditionHit = true;
					PC = pop16();
					DEBUG_LOG("RET C - " << "0x" << intToHex(PC));
				}
				break;
			}
		case 0xD9:
			{
				auto address = pop16();
				DEBUG_LOG("RETI - " << "0x" << intToHex(address));
				PC = address;
				IME = true;

				break;
			}
		case 0xDA:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("JP C, a16 - " << "0x" << charToHex(b3) << charToHex(b2));
				if (CFlag())
				{
					conditionHit = true;
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xDC:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("CALL C, a16 - " << "0x" << charToHex(b3) << charToHex(b2));

				if (CFlag())
				{
					conditionHit = true;
					push16(PC);
					PC = b3 << 8 | b2;
				}

				break;
			}
		case 0xDE:
			{
				DEBUG_LOG("SBC A, d8");
				unsigned char value = read_memory(PC++);
				sub8(A, value, CFlag());

				break;
			}
		case 0xDF:
			{
				DEBUG_LOG("RST 3");
				push16(PC);
				PC = 0x0018;

				break;
			}
		case 0xE0:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("LD (a8), A - " << "0xFF" << charToHex(b2));
				unsigned int address = 0xFF << 8 | b2;
				write_memory(address, A);

				break;
			}
		case 0xE1:
			{
				unsigned int value = pop16();
				DEBUG_LOG("POP HL - " << "0x" << intToHex(value));
				H = value >> 8;
				L = value & 0xFF;
				// PC++;

				// setZ(PC == 0);
				break;
			}
		case 0xE2:
			{
				DEBUG_LOG("LD (C), A");
				write_memory(0xFF00 + C, A);

				break;
			}
		case 0xE7:
			{
				DEBUG_LOG("RST 4");
				push16(PC);
				PC = 0x0020;

				break;
			}
		case 0xE8:
			{
				DEBUG_LOG("ADD SP, s8");
				signed char b2 = read_memory(PC++);

				int result = SP + b2;
				setC((SP & 0xFF) + (unsigned char)b2 > 0xFF);
				setH((SP & 0xF) + ((unsigned char)b2 & 0xF) > 0xF);

				if (result < 0)
					result = result & 0xFFFF;
				if (result > 0xFFFF)
					result -= 0x10000;

				SP = result;

				setZ(false);
				setN(false);

				break;
			}
		case 0xE9:
			{
				DEBUG_LOG("JP HL");
				unsigned int address = H << 8 | L;
				PC = address;

				break;
			}
		case 0xEA:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("LD (a16), A - " << "0x" << charToHex(b3) << charToHex(b2));
				unsigned int address = b3 << 8 | b2;
				write_memory(address, A);

				// setZ(PC == 0);
				break;
			}
		case 0xE5:
			{
				unsigned int HL = H << 8 | L;
				DEBUG_LOG("PUSH HL - " << "0x" << intToHex(HL));
				push16(HL);

				break;
			}
		case 0xE6:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("AND d8 - " << "0x" << charToHex(b2));

				A = A & b2;

				setZ(A == 0);
				setN(false);
				setH(true);
				setC(false);

				break;
			}
		case 0xEE:
			{
				DEBUG_LOG("XOR d8");
				xorA(read_memory(PC++));

				break;
			}
		case 0xEF:
			{
				DEBUG_LOG("RST 5");
				push16(PC);
				PC = 0x0028;

				break;
			}
		case 0xF0:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("LD A, (a8) - " << "0xFF" << charToHex(b2));
				A = read_memory(0xFF00 + b2);

				break;
			}
		case 0xF1:
			{
				unsigned int value = pop16();
				DEBUG_LOG("POP AF - " << "0x" << intToHex(value));
				A = value >> 8;
				F = value & 0xF0;

				break;
			}
		case 0xF2:
			{
				DEBUG_LOG("LD A, (C)");
				A = read_memory(0xFF00 + C);

				break;
			}
		case 0xF3:
			{
				IME = false;
				DEBUG_LOG("DI - IME DISABLED");

				break;
			}
		case 0xF5:
			{
				unsigned int value = A << 8 | F;
				push16(value);
				DEBUG_LOG("PUSH AF");

				break;
			}
		case 0xF6:
			{
				unsigned char b2 = read_memory(PC++);
				DEBUG_LOG("OR d8 - " << "0x" << charToHex(b2));

				A = A | b2;

				setZ(A == 0);
				setN(false);
				setH(false);
				setC(false);

				break;
			}
		case 0xF7:
			{
				DEBUG_LOG("RST 6");
				push16(PC);
				PC = 0x0030;

				break;
			}
		case 0xF8:
			{
				DEBUG_LOG("LD HL, SP+s8");
				signed char b2 = read_memory(PC++);
				unsigned int result = SP + b2;

				setC((SP & 0xFF) + (unsigned char)b2 > 0xFF);
				setH((SP & 0xF) + ((unsigned char)b2 & 0xF) > 0xF);

				if (result > 0xFFFF)
					result -= 0x10000;

				H = result >> 8 & 0xFF;
				L = result & 0xFF;

				setZ(false);
				setN(false);

				break;
			}
		case 0xF9:
			{
				DEBUG_LOG("LD SP, HL");

				SP = H << 8 | L;

				break;
			}
		case 0xFA:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char b3 = read_memory(PC++);
				DEBUG_LOG("LOAD A (a16) - " << "0x" << charToHex(b3) << charToHex(b2));

				unsigned int address = b3 << 8 | b2;
				A = read_memory(address);

				break;
			}
		case 0xFB:
			{
				DEBUG_LOG("EI - IME ENABLED");
				IME = true;

				break;
			}
		case 0xFE:
			{
				unsigned char b2 = read_memory(PC++);
				unsigned char result = A - b2;
				DEBUG_LOG("CP d8 - " << charToHex(result));

				setZ(result == 0);
				setN(true);
				setC(b2 > A);

				signed char half = (0xF & (signed char)A) - (0xF & b2);
				setH(half < 0);

				break;
			}
		case 0xFF:
			{
				DEBUG_LOG("RST 7");
				push16(PC);
				PC = 0x0038;

				break;
			}
		default:
			abort();
	}
	// m_continue = false;

	if (SP > 0xFFFF)
		SP -= 0x10000;

	if ((unsigned char)read_memory(SC) == 0x81)
	{
		unsigned char value = read_memory(SB);
		write_memory(SC, 0x00);
		std::cout << value;
	}
}

static void DrawRegisters(age::Renderer* renderer)
{
	age::TextParams params;
	params.text = "Registers";
	params.pos = {6, 6};
	params.height = 1.0f;
	params.color = age::Color::White();
	params.temp = false;
	renderer->DrawText(params);

	params.text = "AF";
	params.pos.y++;
	renderer->DrawText(params);

	params.text = charToHex(A) + " " + charToHex(F);
	params.pos.x = 10;
	params.temp = true;
	renderer->DrawText(params);

	params.text = "BC";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	renderer->DrawText(params);

	params.text = charToHex(B) + " " + charToHex(C);
	params.pos.x = 10;
	params.temp = true;
	renderer->DrawText(params);

	params.text = "DE";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	renderer->DrawText(params);

	params.text = charToHex(D) + " " + charToHex(E);
	params.pos.x = 10;
	params.temp = true;
	renderer->DrawText(params);

	params.text = "HL";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	renderer->DrawText(params);

	params.text = charToHex(H) + " " + charToHex(L);
	params.pos.x = 6;
	params.pos.x = 10;
	params.temp = true;
	renderer->DrawText(params);

	params.text = "SP";
	params.pos.x = 6;
	params.pos.y += 2;
	params.temp = false;
	renderer->DrawText(params);

	params.text = intToHex(SP);
	params.pos.x = 10;
	params.temp = true;
	renderer->DrawText(params);

	params.text = "PC";
	params.pos.x = 6;
	params.pos.y++;
	params.temp = false;
	renderer->DrawText(params);

	params.text = intToHex(PC);
	params.pos.x = 10;
	params.temp = true;
	renderer->DrawText(params);
}

static void checkerboard(age::Renderer* renderer)
{
	for (int i=0; i < 144; ++i)
	{
		for (int j=0; j < 160; j++)
		{
			auto color = age::Color::Black();
			if (i % 2 == 0)
			{
				color = j % 2 == 0 ? age::Color::White() : age::Color::Black();
			}
			else
			{
				color = j % 2 == 0 ? age::Color::Black() : age::Color::White();
			}
			renderer->DrawQuad(age::Rect(j,i, 1, 1), color);
		}
	}
}

void GameboyScreen::Draw()
{
	//checkerboard(m_renderer);
}

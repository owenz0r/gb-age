#pragma once

#include "core/Screen.h"

class GameboyScreen : public age::Screen
{
	void Init() override;
	void Update(const double dt) override;
	void Draw() override;

  private:
	bool m_continue = true;
};

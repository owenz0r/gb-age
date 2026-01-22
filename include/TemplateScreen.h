#pragma once

#include "core/Screen.h"

class TemplateScreen : public age::Screen
{
	void Init() override;
	void Update(const double dt) override;
	void Draw() override;
};

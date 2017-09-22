/* FACT - XACT Reimplementation for FNA
 * Copyright 2009-2017 Ethan Lee and the MonoGame Team
 *
 * Released under the Microsoft Public License.
 * See LICENSE for details.
 */

/* This is where the actual tool lives! Try to do your work in here! */

#include <FACT_internal.h> /* DO NOT INCLUDE THIS IN REAL CODE! */

#include "imgui.h"

bool show_test_window = true;

void FACTTool_Update()
{
	ImGui::ShowTestWindow(&show_test_window);
}

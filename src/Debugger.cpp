#include "Debugger.h"

Debugger::Debugger()
{
	m_GLDisplayPtr = nullptr;
	m_NESDevicePtr = nullptr;

	m_CPUMemoryEditor.Cols = 32;
	m_CPUMemoryEditor.ReadFn = [](const ImU8* data, size_t offset) -> ImU8 { return ((NESDevice*)data)->CPUPeek((uint16_t)offset); };
	m_CPUMemoryEditor.WriteFn = [](ImU8* data, size_t offset, ImU8 byte) -> void { ((NESDevice*)data)->CPUWrite((uint16_t)offset, byte); };

	m_PPUMemoryEditor.Cols = 32;
	m_PPUMemoryEditor.ReadFn = [](const ImU8* data, size_t offset) -> ImU8 { return ((NESDevice*)data)->PPUPeek((uint16_t)offset); };
	m_PPUMemoryEditor.WriteFn = [](ImU8* data, size_t offset, ImU8 byte) -> void { ((NESDevice*)data)->PPUWrite((uint16_t)offset, byte); };

	m_OAMMemoryEditor.Cols = 8;
	m_OAMMemoryEditor.OptMidColsCount = 4;
	m_OAMMemoryEditor.OptShowAscii = false;
	m_OAMMemoryEditor.OptShowDataPreview = false;
	m_OAMMemoryEditor.OptShowOptions = false;

	m_EnableAsmListings = true;
	m_EnableRegistersTampering = false;

	m_EnableAutomaticAdvance = false;
	m_MaxInstructionsQueued = 5003;//128;
	m_InstructionsQueued = 0;

	m_SelectedPalette = 0;
	m_Patterntable0UpdateMode = 2;
	m_Patterntable1UpdateMode = 2;
	m_PalettesUpdateMode = 2;
	m_NametableUpdateMode = 2;
}

void Debugger::SetGLDisplay(GLDisplay* glDisplay)
{
	m_GLDisplayPtr = glDisplay;
}

void Debugger::SetNESDevice(NESDevice* nesDevice)
{
	m_NESDevicePtr = nesDevice;
}

void Debugger::Initialize()
{

}

void Debugger::Destroy()
{
}

bool Debugger::ShowCPUMemory()
{
	m_CPUMemoryEditor.DrawWindow("CPU Memory Viewer", m_NESDevicePtr, 0x10000);
	return m_CPUMemoryEditor.Open;
}

bool Debugger::ShowPPUMemory()
{
	m_PPUMemoryEditor.DrawWindow("PPU Memory Viewer", m_NESDevicePtr, 0x4000);
	return m_PPUMemoryEditor.Open;
}

bool Debugger::ShowCPUControls()
{
	bool isOpen = true;
	ImGui::SetNextWindowSize(ImVec2(800,800), ImGuiCond_Once);
	ImGui::SetNextWindowSizeConstraints(ImVec2(400, 400), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::Begin("CPU Controls",&isOpen))
	{
		auto& nesCPU = m_NESDevicePtr->GetCPU();

		const static int one = 1; //just one, dont ask..
		auto colors = ImGui::GetStyle().Colors;

		//----------------------------------------------------------------
		// Line 1 - Control buttons
		//----------------
		if (m_NESDevicePtr->DeviceMode != NESDevice::DeviceMode::Pause || (m_EnableAutomaticAdvance))
		{
			if (ImGui::Button("||"))
			{
				m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::Pause;
				if (m_EnableAutomaticAdvance)
				{
					m_EnableAutomaticAdvance = false;
					m_InstructionsQueued = m_MaxInstructionsQueued;
				}
			}
			if (ImGui::IsItemHovered())	
				ImGui::SetTooltip("Pause execution");
		}
		else
		{
			if (ImGui::Button(" >"))
			{
				m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::Running;
			}
			if (ImGui::IsItemHovered())	
				ImGui::SetTooltip("Resume execution");
		}
		ImGui::SameLine();
		//----------------
		if (ImGui::Button(">|"))
		{
			m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::AdvanceCPUInstruction;
		}
		if (ImGui::IsItemHovered())	
			ImGui::SetTooltip("Step one instruction"); 
		ImGui::SameLine();
		//----------------
		if (ImGui::Button(">>"))
		{
			m_EnableAutomaticAdvance = true;
			m_InstructionsQueued = m_MaxInstructionsQueued;
			m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::AdvanceCPUInstruction;
		}
		if (ImGui::IsItemHovered())	
			ImGui::SetTooltip("Step %d instructions", ((m_InstructionsQueued > 0) ? m_InstructionsQueued : m_MaxInstructionsQueued));
		ImGui::SameLine();
		//----------------
		if (m_EnableAutomaticAdvance) ImGui::BeginDisabled();
		ImGui::DragInt("##Steps",(int*) & ((m_InstructionsQueued > 0) ? m_InstructionsQueued : m_MaxInstructionsQueued), 1.0f, 1, 0xFFFF, "%d Instructions");
		if (m_EnableAutomaticAdvance) ImGui::EndDisabled();
		ImGui::SameLine();
		//----------------
		if (ImGui::Button("Frame"))
		{
			m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::AdvancePPUFrame;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Step one ppu frame");
		ImGui::SameLine();
		//----------------
		if (ImGui::Button("Line"))
		{
			m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::AdvancePPULine;
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("Step one ppu scanline");
		//----------------
		ImGui::Separator();
		//----------------

		//----------------------------------------------------------------
		// Line 2 - ASM Listings / Register states
		//----------------

		const float listingWidth = ImGui::GetWindowWidth() * 0.66f; //Using 66% of window width space
		const float listingHeight = ImGui::GetWindowHeight() * 0.66f; //Using 66% of window width space
		const bool isListingVisible = ImGui::BeginChild("ASM Listing", ImVec2(listingWidth, listingHeight), true, ImGuiWindowFlags_MenuBar);
		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("ASM Listing -");
			if ((!m_EnableAsmListings && ImGui::MenuItem("DISABLED")) ||
				(m_EnableAsmListings && ImGui::MenuItem("ENABLED")))
				m_EnableAsmListings = !m_EnableAsmListings;
			ImGui::EndMenuBar();
		}
		if (isListingVisible)
		{
			if (m_EnableAsmListings)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
				ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_FramePadding, ImVec2(2, 1));

				auto asmListing = nesCPU.Disassemble(nesCPU.Registers.PC, 32, true);

				ImGui::PushStyleColor(ImGuiCol_Text, colors[ImGuiCol_TextDisabled]);
				for (int i = 0; i < asmListing.size(); i++)
				{
					if (i == 8)
					{
						ImGui::PopStyleColor();
						ImGui::Separator();
					}

					ImGui::TextUnformatted(asmListing[i].c_str());

					if (i == 8)
					{
						ImGui::Separator();
					}
				}
				ImGui::PopStyleVar(2);
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
				ImGui::TextUnformatted("* asm listing disabled *");
				ImGui::PopStyleColor();
			}
		}
		ImGui::EndChild();
		ImGui::SameLine();
		//----------------
		ImGui::BeginChild("##SendHelp", ImVec2(0, 0), false);

		int registerTextInputFlags = (m_EnableRegistersTampering) ? 0 : ImGuiInputTextFlags_ReadOnly;
		const float registersHeight = listingHeight * 0.5f;
		const bool isRegistersVisible = ImGui::BeginChild("Registers", ImVec2(0, registersHeight), true, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("Registers -");
			if ( (!m_EnableRegistersTampering && ImGui::MenuItem("LOCKED")) ||
				 (m_EnableRegistersTampering && ImGui::MenuItem("UNLOCKED")))
				m_EnableRegistersTampering = !m_EnableRegistersTampering;

			ImGui::EndMenuBar();
		}

		//Status register
		if (isRegistersVisible)
		{
			ImGui::TextUnformatted("Flags :"); ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::NegativeBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("N");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("NegativeBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::NegativeBit, !nesCPU.getFlag(NESCPU::SRFlag::NegativeBit));
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::OverflowBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("O");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("OverflowBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::OverflowBit, !nesCPU.getFlag(NESCPU::SRFlag::OverflowBit));
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::UnusedBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("-");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("UnusedBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::UnusedBit, !nesCPU.getFlag(NESCPU::SRFlag::UnusedBit));
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::BreakBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("B");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("BreakBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::BreakBit, !nesCPU.getFlag(NESCPU::SRFlag::BreakBit));
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::DecimalBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("D"); ImGui::SameLine();
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("DecimalBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::DecimalBit, !nesCPU.getFlag(NESCPU::SRFlag::DecimalBit));
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::InterruptBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("I");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("InterruptBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::InterruptBit, !nesCPU.getFlag(NESCPU::SRFlag::InterruptBit));
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::ZeroBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("Z");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("ZeroBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::ZeroBit, !nesCPU.getFlag(NESCPU::SRFlag::ZeroBit));
			ImGui::SameLine();

			ImGui::PushStyleColor(ImGuiCol_Text, nesCPU.getFlag(NESCPU::SRFlag::CarryBit) ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1));
			ImGui::TextUnformatted("C");
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("CarryBit");
			if (ImGui::IsItemClicked() && m_EnableRegistersTampering)
				nesCPU.setFlag(NESCPU::SRFlag::CarryBit, !nesCPU.getFlag(NESCPU::SRFlag::CarryBit));


			ImGui::TextUnformatted(" SR : "); ImGui::SameLine(); ImGui::InputScalar("##SR", ImGuiDataType_::ImGuiDataType_U8, &nesCPU.Registers.SR, &one, 0, "%02X", registerTextInputFlags);

			ImGui::Separator();
			//Other registers

			ImGui::TextUnformatted("  A : "); ImGui::SameLine(); ImGui::InputScalar("##A", ImGuiDataType_::ImGuiDataType_U8, &nesCPU.Registers.AC, &one, 0, "%02X", registerTextInputFlags);
			ImGui::TextUnformatted("  X : "); ImGui::SameLine(); ImGui::InputScalar("##X", ImGuiDataType_::ImGuiDataType_U8, &nesCPU.Registers.XR, &one, 0, "%02X", registerTextInputFlags);
			ImGui::TextUnformatted("  Y : "); ImGui::SameLine(); ImGui::InputScalar("##Y", ImGuiDataType_::ImGuiDataType_U8, &nesCPU.Registers.YR, &one, 0, "%02X", registerTextInputFlags);
			ImGui::TextUnformatted(" SP : "); ImGui::SameLine(); ImGui::InputScalar("##SP", ImGuiDataType_::ImGuiDataType_U8, &nesCPU.Registers.SP, &one, 0, "%02X", registerTextInputFlags);
			ImGui::TextUnformatted(" PC : "); ImGui::SameLine(); ImGui::InputScalar("##PC", ImGuiDataType_::ImGuiDataType_U16, &nesCPU.Registers.PC, &one, 0, "%02X", registerTextInputFlags);
		}
		ImGui::EndChild();
		//----------------------------------------------------------------
		// Line 2.1 - Status
		//----------------
		const float statusHeight = (listingHeight * 0.5f) - ImGui::GetStyle().ItemSpacing.y;
		const bool isStatusVisible = ImGui::BeginChild("Status", ImVec2(0, statusHeight), true, ImGuiWindowFlags_MenuBar);
		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("Status");
			ImGui::EndMenuBar();
		}
		if (isStatusVisible)
		{
			ImGui::Text("M.  Cycle # : %d", m_NESDevicePtr->DeviceCycle);
			ImGui::Text("CPU Cycle # : %d", nesCPU.State.CyclesTotal);
			ImGui::Text("CPU C.Queue : %d", nesCPU.State.CycleCounter);
			ImGui::TextUnformatted("CPU State :"); ImGui::SameLine();
			
			if (nesCPU.State.Halted)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0, 0, 1));
				ImGui::TextUnformatted("HALTED");
			}
			else if(nesCPU.State.DMATransfer)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 0, 1));
				ImGui::TextUnformatted("DMA Transfer");
			}
			else
			{ 
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 1, 0, 1));
				ImGui::TextUnformatted("Operational");
			}

			ImGui::PopStyleColor();
			ImGui::Separator();
			if (nesCPU.State.Halted && ImGui::Button("UNHALT!"))
				nesCPU.State.Halted = !nesCPU.State.Halted;

		}
		ImGui::EndChild();
		//----------------------------------------------------------------

		ImGui::EndChild();		
	}
	ImGui::End();
	return isOpen;
}

bool Debugger::ShowPPUData()
{
	bool isOpen = true;
	ImGui::SetNextWindowSize(ImVec2(800, 830), ImGuiCond_Once);
	ImGui::SetNextWindowSizeConstraints(ImVec2(400, 400), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::Begin("PPU Data", &isOpen))
	{
		auto& nesPPU = m_NESDevicePtr->GetPPU();

		const static int zero = 0; //zero is zero or is it?... dont ask...again...

		ImGuiIO& io = ImGui::GetIO();
		ImVec2 cursorPos = ImGui::GetCursorScreenPos();

		ImVec2 styleItemSpacing = ImGui::GetStyle().ItemSpacing;
		ImVec2 mainWorkWindowRegionMax = ImGui::GetWindowContentRegionMax();
		ImVec2 mainWindowWorkRegionMin = ImGui::GetWindowContentRegionMin();

		ImVec2 mainWindowWorkRegionSize = ImVec2(
			(mainWorkWindowRegionMax.x - mainWindowWorkRegionMin.x),
			(mainWorkWindowRegionMax.y - mainWindowWorkRegionMin.y)
		);

		//Helper to detect if smth reuires to be updated
		bool isUpdateMode2Required = false;
		bool isUpdateMode3Required = false;

		if (m_StoredPPUFrameCounter1 != nesPPU.PPUFrameCounter)
		{
			isUpdateMode2Required = true;
			m_StoredPPUFrameCounter1 = nesPPU.PPUFrameCounter;
		}

		if (nesPPU.PPUFrameCounter - m_StoredPPUFrameCounter2 >= 60)
		{
			isUpdateMode3Required = true;
			m_StoredPPUFrameCounter2 = nesPPU.PPUFrameCounter;
		}
		//----------------------------------------------------------------
		// Line 1 - Patterntables and palettes
		//----------------

		//Stuff for tooltip
		ImVec4 tint_col = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);   // No tint
		ImVec4 border_col = ImVec4(1.0f, 1.0f, 1.0f, 0.5f); // 50% opaque white

		//Predict menubar height
		const float menubarHeight = (ImGui::GetStyle().FramePadding.y * 2.0f) + ImGui::GetFontSize();

		const float patterntableWindowWidth  = (mainWindowWorkRegionSize.x * 0.35f) - (styleItemSpacing.x * 0.50f);
		const float patterntableWindowHeight = (mainWindowWorkRegionSize.x * 0.35f) - (styleItemSpacing.x * 0.50f);;// +(styleItemSpacing.y * 0.50f);
		const float patterntableContentSize = patterntableWindowWidth;

		
		//Disable padding for child's
		ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		//Patterntable 0
		const bool isPatterntableZeroVisible = ImGui::BeginChild("Patterntable 0", ImVec2(patterntableWindowWidth, patterntableWindowHeight + menubarHeight), true, ImGuiWindowFlags_MenuBar);
		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("Patterntable 0");
			switch (m_Patterntable0UpdateMode)
			{
			case 0: if (ImGui::MenuItem("[DISABLED]"))	 m_Patterntable0UpdateMode = 1; break;
			case 1: if (ImGui::MenuItem("[ASAP]"))		 m_Patterntable0UpdateMode = 2; break;
			case 2: if (ImGui::MenuItem("[PER FRAME]"))  m_Patterntable0UpdateMode = 3; break;
			case 3: if (ImGui::MenuItem("[PER SECOND]")) m_Patterntable0UpdateMode = 0; break;
			}
			ImGui::EndMenuBar();
		}
		if (isPatterntableZeroVisible)
		{
			ImVec2 windowWorkRegionMax = ImGui::GetWindowContentRegionMax();
			ImVec2 windowWorkRegionMin = ImGui::GetWindowContentRegionMin();
			ImVec2 windowWorkRegionSize = ImVec2((windowWorkRegionMax.x - windowWorkRegionMin.x), (windowWorkRegionMax.y - windowWorkRegionMin.y));

			if ( m_Patterntable0UpdateMode == 1							   ||
				 (m_Patterntable0UpdateMode == 2 && isUpdateMode2Required) ||
				 (m_Patterntable0UpdateMode == 3 && isUpdateMode3Required))
					m_GLDisplayPtr->UpdatePatternTexture(0, m_SelectedPalette);

			ImGui::Image((ImTextureID)m_GLDisplayPtr->getPatternTexture(0), windowWorkRegionSize, ImVec2(0, 0), ImVec2(1, 1));
			if (ImGui::IsItemHovered())
			{
				//TODO : Zoomed view
			}
		}
		ImGui::EndChild();
		ImGui::SameLine();

		//Patterntable 1
		const bool isPatterntableOneVisible = ImGui::BeginChild("Patterntable 1", ImVec2(patterntableWindowWidth, patterntableWindowHeight + menubarHeight), true, ImGuiWindowFlags_MenuBar);
		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("Patterntable 1");
			switch (m_Patterntable1UpdateMode)
			{
			case 0: if (ImGui::MenuItem("[DISABLED]"))	 m_Patterntable1UpdateMode = 1; break;
			case 1: if (ImGui::MenuItem("[ASAP]"))		 m_Patterntable1UpdateMode = 2; break;
			case 2: if (ImGui::MenuItem("[PER FRAME]"))  m_Patterntable1UpdateMode = 3; break;
			case 3: if (ImGui::MenuItem("[NES SECOND]")) m_Patterntable1UpdateMode = 0; break;
			}
			ImGui::EndMenuBar();
		}
		if (isPatterntableOneVisible)
		{
			ImVec2 windowWorkRegionMax = ImGui::GetWindowContentRegionMax();
			ImVec2 windowWorkRegionMin = ImGui::GetWindowContentRegionMin();
			ImVec2 windowWorkRegionSize = ImVec2((windowWorkRegionMax.x - windowWorkRegionMin.x), (windowWorkRegionMax.y - windowWorkRegionMin.y));

			if ( m_Patterntable1UpdateMode == 1                           ||
				 (m_Patterntable1UpdateMode == 2 && isUpdateMode2Required)||
				 (m_Patterntable1UpdateMode == 3 && isUpdateMode3Required))
					m_GLDisplayPtr->UpdatePatternTexture(1, m_SelectedPalette);

			ImGui::Image((ImTextureID)m_GLDisplayPtr->getPatternTexture(1), windowWorkRegionSize, ImVec2(0, 0), ImVec2(1, 1));
			if (ImGui::IsItemHovered())
			{
				//TODO : Zoomed view
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::SameLine();

		//----------------------------------------------------------------
		// Line 2.1 - Palettes
		//----------------
		//ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		const float palettesHeight = ImGui::GetWindowHeight() * 0.50f;
		const bool isPalettesVisible = ImGui::BeginChild("Palettes", ImVec2(0, patterntableWindowHeight + menubarHeight), true, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("Palettes");
			switch (m_PalettesUpdateMode)
			{
			case 0: if (ImGui::MenuItem("[DISABLED]"))	 m_PalettesUpdateMode = 1; break;
			case 1: if (ImGui::MenuItem("[ASAP]"))		 m_PalettesUpdateMode = 2; break;
			case 2: if (ImGui::MenuItem("[PER FRAME]"))  m_PalettesUpdateMode = 3; break;
			case 3: if (ImGui::MenuItem("[NES SECOND]")) m_PalettesUpdateMode = 0; break;
			}
			ImGui::EndMenuBar();
		}
		if (isPalettesVisible)
		{
			if ( m_PalettesUpdateMode == 1							  ||
				 (m_PalettesUpdateMode == 2 && isUpdateMode2Required) ||
				 (m_PalettesUpdateMode == 3 && isUpdateMode3Required))
			{
				for (uint16_t address = 0x00; address < 0x1F; address++)
					m_PaletteCache[address] = nesPPU.Palettes[address];
			}

			ImGui::Separator();
			ImGui::TextUnformatted(" ** Background ** ");
			ImGui::Separator();
			ImGui::TextUnformatted("$3F00"); ImGui::SameLine(); DrawPalette(0x3F00);
			ImGui::Separator();
			ImGui::TextUnformatted("$3F04"); ImGui::SameLine(); DrawPalette(0x3F04); 
			ImGui::Separator();
			ImGui::TextUnformatted("$3F08"); ImGui::SameLine(); DrawPalette(0x3F08); 
			ImGui::Separator();
			ImGui::TextUnformatted("$3F0C"); ImGui::SameLine(); DrawPalette(0x3F0C); 

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextUnformatted(" ** Sprites **");
			ImGui::Separator();
			ImGui::TextUnformatted("$3F10"); ImGui::SameLine(); DrawPalette(0x3F10); 
			ImGui::Separator();
			ImGui::TextUnformatted("$3F14"); ImGui::SameLine(); DrawPalette(0x3F14); 
			ImGui::Separator();
			ImGui::TextUnformatted("$3F18"); ImGui::SameLine(); DrawPalette(0x3F18); 
			ImGui::Separator();
			ImGui::TextUnformatted("$3F1C"); ImGui::SameLine(); DrawPalette(0x3F1C); 
			ImGui::Separator();
			ImGui::Spacing();

			

			ImGui::Text("Im happy text!");

		}
		ImGui::EndChild();
		//ImGui::PopStyleVar();
		//----------------------------------------------------------------
		// Line 2 - Nametables and reserved
		//----------------
		
		//Disable padding for child's
		ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		const float nametablesWidth = ImGui::GetWindowWidth() * 0.64f;
		const bool isNametablesVisible = ImGui::BeginChild("Nametables", ImVec2(nametablesWidth, 0), true, ImGuiWindowFlags_MenuBar);
		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("Nametables");
			switch (m_NametableUpdateMode)
			{
			case 0: if (ImGui::MenuItem("[DISABLED]"))	 m_NametableUpdateMode = 1; break;
			case 1: if (ImGui::MenuItem("[ASAP]"))		 m_NametableUpdateMode = 2; break;
			case 2: if (ImGui::MenuItem("[PER FRAME]"))  m_NametableUpdateMode = 3; break;
			case 3: if (ImGui::MenuItem("[NES SECOND]")) m_NametableUpdateMode = 0; break;
			}
			ImGui::EndMenuBar();
		}
		if (isNametablesVisible)
		{
			ImVec2 screenPos = ImGui::GetCursorScreenPos();

			if ( m_NametableUpdateMode == 1							   || 
				 (m_NametableUpdateMode == 2 && isUpdateMode2Required) ||
				 (m_NametableUpdateMode == 3 && isUpdateMode3Required))
					m_GLDisplayPtr->UpdateNametables();

			//Viewport frame
			int vp_x = nesPPU.DBG_ScrollX;
			int vp_y = nesPPU.DBG_ScrollY;

			ImGui::Image((ImTextureID)m_GLDisplayPtr->getNametablesTexture(), ImVec2(nametablesWidth, nametablesWidth * 0.9375f), ImVec2(0, 0), ImVec2(1, 0.9375f));
			if (ImGui::IsItemHovered())
			{
				//TODO : Zoomed view
			}
			
			if (vp_y >= 240) vp_y -= 16;


			ImGui::GetWindowDrawList()->AddRect(
				ImVec2(screenPos.x + vp_x +   0, screenPos.y + vp_y +   0),
				ImVec2(screenPos.x + vp_x + 256, screenPos.y + vp_y + 240),
				ImColor(0.75f,0.75f,0.75f, 1.0f), 1.f, 0, 2.f);

			ImGui::GetWindowDrawList()->AddRect(
				ImVec2(screenPos.x + vp_x +   0, screenPos.y + vp_y - 480),
				ImVec2(screenPos.x + vp_x + 256, screenPos.y + vp_y - 240),
				ImColor(0.75f, 0.75f, 0.75f, 1.0f), 1.f, 0, 2.f);

			ImGui::GetWindowDrawList()->AddRect(
				ImVec2(screenPos.x + vp_x - 512, screenPos.y + vp_y +   0),
				ImVec2(screenPos.x + vp_x - 256, screenPos.y + vp_y + 240),
				ImColor(0.75f, 0.75f, 0.75f, 1.f), 1.f, 0, 2.f);

			ImGui::GetWindowDrawList()->AddRect(
				ImVec2(screenPos.x + vp_x - 512, screenPos.y + vp_y - 480),
				ImVec2(screenPos.x + vp_x - 256, screenPos.y + vp_y - 240),
				ImColor(0.75f, 0.75f, 0.75f, 1.0f), 1.f, 0, 2.f);

		}
		ImGui::EndChild();

		ImGui::PopStyleVar();

		ImGui::SameLine();

		const bool isReservedVisible = ImGui::BeginChild("OAM", ImVec2(0, 0), true, ImGuiWindowFlags_MenuBar);

		if (ImGui::BeginMenuBar())
		{
			ImGui::TextUnformatted("OAM");
			ImGui::EndMenuBar();
		}
		if (isReservedVisible)
		{
			m_OAMMemoryEditor.DrawContents(nesPPU.OAMData, 256);
			//m_OAMMemoryEditor.DrawContents(nesPPU.SecondOAMData, 32);
		}
		ImGui::EndChild();
	}
	ImGui::End();

	return isOpen;
}

void Debugger::Update()
{
	if (m_NESDevicePtr->GetCPU().State.Halted)
	{
		m_EnableAutomaticAdvance = false;
		m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::Pause;
	}

	if (m_EnableAutomaticAdvance == true && 
		m_NESDevicePtr->DeviceMode == NESDevice::DeviceMode::Pause)
	{
		if (m_InstructionsQueued == 0)
		{
			m_EnableAutomaticAdvance = false;
		}
		else if (!m_NESDevicePtr->GetCPU().State.Halted)
		{
			m_NESDevicePtr->DeviceMode = NESDevice::DeviceMode::AdvanceCPUInstruction;
			m_InstructionsQueued--;

			//----------

			auto lst = m_NESDevicePtr->GetCPU().Disassemble(m_NESDevicePtr->GetCPU().Registers.PC, 1, false);
			printf("%-48s\tA:%.2X X:%.2X Y:%.2X P:%.2X SP:%.2X PPU:%3d,%3d CYC:%d\n",
				lst[0].c_str(),
				m_NESDevicePtr->GetCPU().Registers.AC,
				m_NESDevicePtr->GetCPU().Registers.XR,
				m_NESDevicePtr->GetCPU().Registers.YR,
				m_NESDevicePtr->GetCPU().Registers.SR,
				m_NESDevicePtr->GetCPU().Registers.SP,
				m_NESDevicePtr->GetPPU().PPUScanline,
				m_NESDevicePtr->GetPPU().PPUCycle,
				m_NESDevicePtr->GetCPU().State.CyclesTotal
			);
			
			//----------
		}
	}
	m_InternalClock++;
}

bool Debugger::IsCycleHijackActive()
{
	return m_EnableAutomaticAdvance;
}

void Debugger::DrawPalette(uint16_t address)
{
	ImVec2 screenPos = ImGui::GetCursorScreenPos();
	ImVec2 avail = ImGui::GetContentRegionAvail();
	float componentW = avail.x / 4;
	float frameH = ImGui::GetFrameHeight() - ImGui::GetStyle().FramePadding.y * 2;

	NESPPU& nesPPU = m_NESDevicePtr->GetPPU();

	uint8_t			  raw_colors[4];
	NESPPU::RGBPixel rgb_colors[4];

	//First color is 'always' at 0x3F00
	raw_colors[0] = m_PaletteCache[0];
	rgb_colors[0] = nesPPU.GetRGBColor(raw_colors[0]);

	for (uint16_t i = 1; i < 4; i++)
	{
		raw_colors[i] = m_PaletteCache[(address + i) & 0x1F];
		rgb_colors[i] = nesPPU.GetRGBColor(raw_colors[i]);
		ImGui::GetWindowDrawList()->AddRectFilled(
			ImVec2(screenPos.x + componentW * i, screenPos.y),
			ImVec2(screenPos.x + componentW * (i+1), screenPos.y + frameH),
			ImColor(ImVec4(rgb_colors[i].r / 255.f, rgb_colors[i].g / 255.f, rgb_colors[i].b / 255.f, 1.0f)));
	}

	//Discover palette id from address
	uint8_t paletteId = 0xFF;
	switch (address)
	{
	case 0x3F00: paletteId = 0; break;
	case 0x3F04: paletteId = 1; break;
	case 0x3F08: paletteId = 2; break;
	case 0x3F0C: paletteId = 3; break;
	case 0x3F10: paletteId = 4; break;
	case 0x3F14: paletteId = 5; break;
	case 0x3F18: paletteId = 6; break;
	case 0x3F1C: paletteId = 7; break;
	}

	//If selected - draw outline
	if (m_SelectedPalette == paletteId)
	{
		ImGui::GetWindowDrawList()->AddRect(
			ImVec2(screenPos.x, screenPos.y),
			ImVec2(screenPos.x + avail.x, screenPos.y + frameH),
			ImColor(ImVec4(1.f, 1.f, 0.4f, 1.0f)));
	}

	//Invisible button for click and hover detection
	ImGui::PushID(paletteId);
	if (ImGui::InvisibleButton("", ImVec2(avail.x, frameH)))
	{
		m_SelectedPalette = paletteId;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();
		ImGui::Text("Palette #%d at $%.4X", paletteId, address);
		ImGui::Text("Values : $%.2X $%.2X $%.2X $%.2X",
			raw_colors[0],
			raw_colors[1],
			raw_colors[2],
			raw_colors[3]
		);
		ImGui::TextUnformatted("RGB");
		for (uint8_t c = 0; c < 4; c++)
		{
			ImGui::Text("   %d : % 4d % 4d % 4d ", c, rgb_colors[c].r, rgb_colors[c].g, rgb_colors[c].b);
		}
		ImGui::EndTooltip();
	}
	ImGui::PopID();
}
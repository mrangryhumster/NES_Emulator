#include "Emulator.h"
#include <iostream>



int main(int argc, char** argv)
{

	Emulator Emulator;

	int ret = Emulator.Initialize(argc, argv);
	if (ret != 0) return ret;

	return Emulator.MainLoop();
}

Emulator::Emulator()
{
	m_SDLWindow = nullptr;
	m_SDLGlContext = nullptr;
	m_WindowWidth = 0;
	m_WindowHeight = 0;

	m_EmulatorFrameTimeCacheIndex = 0;
	memset(m_EmulatorFrameTimeCache, 0, sizeof(chrono_time::rep) * m_EmulatorFrameTimeCacheSize);
	m_EmulatorTimeAccumulator = 0;
	m_EmulatorFrameTimeRaw = 0;
	m_EmulatorFrameTimeAverage = 0;

	m_DeviceUpdateTargetTiming = 0;
	m_DeviceUpdateResedueTime = 0;
	m_DeviceFramesAccumulator = 0;
	m_DeviceFramesPerSecond = 0;
	m_DeviceFrameTime = 0;

	m_LastDirectory = ".";
}

Emulator::~Emulator()
{
}

int Emulator::Initialize(int argc, char** argv)
{
	//------------------------------------------------------------------------------------------------------------
	// 	Loading configs 
	//------------------------------------------------------------------------------------------------------------
	this->LoadConfigFile();
	//------------------------------------------------------------------------------------------------------------
	// 	SDL, gl3w and Dear ImGui initialization and basic setup 
	//------------------------------------------------------------------------------------------------------------
	SDL_SetMainReady();
	//------------------------------------------------------
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
	{
		printf("Failed to init SDL\n");
		return 1;
	}
	//------------------------------------------------------
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	//------------------------------------------------------
	m_SDLWindow = SDL_CreateWindow(
		"MainWindow",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		m_WindowWidth, m_WindowHeight,
		(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));

	m_SDLGlContext = SDL_GL_CreateContext(m_SDLWindow);;

	SDL_GL_MakeCurrent(m_SDLWindow, m_SDLGlContext);
	SDL_GL_SetSwapInterval(0); // Enable vsync
	//------------------------------------------------------
	if (gl3wInit())
	{
		printf("Failed to init gl3w\n");
		return 2;
	}
	//------------------------------------------------------
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	//------------------------------------------------------
	ImGui::StyleColorsDark();
	//------------------------------------------------------
	ImGui_ImplSDL2_InitForOpenGL(m_SDLWindow, m_SDLGlContext);
	ImGui_ImplOpenGL3_Init("#version 150");

	m_IsEmulatorOpen = true;

	m_ShowCPUMemoryViewer = false;
	m_ShowPPUMemoryViewer = false;
	m_ShowCPUControls = false;
	m_ShowPPUData = false;
	m_ShowImGuiStyleEditor = false;
	m_ViewportWindowedMode = false;

	m_FileDialog.SetFileStyle(IGFD_FileStyleByExtention, ".nes", ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[iNES]");

	m_GLDisplay.SetNESDevice(&m_NESDevice);
	m_GLDisplay.Initialize();

	m_Debugger.SetNESDevice(&m_NESDevice);
	m_Debugger.SetGLDisplay(&m_GLDisplay);
	m_Debugger.Initialize();

	m_NESDevice.Reset();

	//Set update timing (milliseconds per frame for 60 fps)
	m_DeviceUpdateTargetTiming = (1000000.0 / 60.0);
	//Reset frametime cache
	for (size_t index = 0; index < m_EmulatorFrameTimeCacheSize; index++)
		m_EmulatorFrameTimeCache[index] = 0;

	m_RewindBufferSize += 2;
	m_RewindBuffer = new NESState[m_RewindBufferSize];
	m_RewindBufferBegin = 0;
	m_RewindBufferLength = 0;
	m_RewindBufferEnd = 0;
	m_RewindBufferIndex = 0;
	m_RewindRecording = true;
	m_RewindControlLatch = false;
	m_RewindAdvanceLatch = false;
	return 0;
}

int Emulator::MainLoop()
{
	ImGuiIO& ImGuiIO = ImGui::GetIO();

	//Initialize timestamp
	m_EmulatorFrameStartTimestamp = chrono_clock::now();

	//************************************************************************
	while (m_IsEmulatorOpen)
	{
		//************************************
		this->ProcessInput();
		//************************************
		//Check master cycle activation mode
		m_DeviceUpdateResedueTime += m_EmulatorFrameTimeAverage;
		while (m_DeviceUpdateResedueTime >= m_DeviceUpdateTargetTiming)
		{
			m_NESDevice.Update();
			m_DeviceUpdateResedueTime -= m_DeviceUpdateTargetTiming;
			m_DeviceFramesAccumulator++;

			// ---- Rewind feature ----
			this->ProcessRewindUpdates();
			// ------------------------
		}
		//************************************
		m_Debugger.Update();
		//************************************
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(m_SDLWindow);
		ImGui::NewFrame();
		//************************************
		this->ProcessViewport();
		this->ProcessWindows();
		//************************************
		ImGui::Render();
		//************************************
		glViewport(0, 0, (int)ImGuiIO.DisplaySize.x, (int)ImGuiIO.DisplaySize.y);
		glClearColor(0.16f, 0.16f, 0.33f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(m_SDLWindow);
		//************************************
		this->ProcessFrameTime();
		//************************************
	}
	//************************************************************************

	//Save savestates before exiting
	this->SaveStates();
	//Update config file before exiting
	this->UpdateConfigFile();

	SDL_GL_DeleteContext(m_SDLGlContext);
	SDL_DestroyWindow(m_SDLWindow);
	SDL_Quit();

	return 0;
}

void Emulator::ProcessInput()
{
	//************************************************************************
	//Pool SDL events
	static SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		ImGui_ImplSDL2_ProcessEvent(&event);

		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
		{
			m_IsEmulatorOpen = false;
		}
	}
	//************************************************************************
	const Uint8* keyboard = SDL_GetKeyboardState(nullptr);
	//************************************************************************
	NESController& nesController = m_NESDevice.GetController();
	nesController.ResetButtons(0);
	if (m_IsViewportSelected)
	{
		if (keyboard[SDL_SCANCODE_S])			nesController.PushButton(0, NESController::NESButtons::BTN_A);
		if (keyboard[SDL_SCANCODE_A])			nesController.PushButton(0, NESController::NESButtons::BTN_B);
		if (keyboard[SDL_SCANCODE_Q])			nesController.PushButton(0, NESController::NESButtons::BTN_SELECT);
		if (keyboard[SDL_SCANCODE_W])			nesController.PushButton(0, NESController::NESButtons::BTN_START);
		if (keyboard[SDL_SCANCODE_UP])			nesController.PushButton(0, NESController::NESButtons::BTN_UP);
		if (keyboard[SDL_SCANCODE_DOWN])		nesController.PushButton(0, NESController::NESButtons::BTN_DOWN);
		if (keyboard[SDL_SCANCODE_LEFT])		nesController.PushButton(0, NESController::NESButtons::BTN_LEFT);
		if (keyboard[SDL_SCANCODE_RIGHT])		nesController.PushButton(0, NESController::NESButtons::BTN_RIGHT);
	}
	//************************************************************************
	uint32_t pressed_fn_count = 0;
	for (int fn = 0; fn < 8; fn++)
	{
		if (keyboard[SDL_SCANCODE_F1 + fn])
		{
			if (!m_NESSaveStateLatch)
			{
				if (keyboard[SDL_SCANCODE_LSHIFT])
				{
					printf("Saving state in slot #%d\n", fn);
					m_NESDevice.SaveState(m_NESState[fn]);
				}
				else if(m_NESState[fn].IsValid())
				{
					printf("Loading state from slot #%d\n", fn);
					m_NESDevice.LoadState(m_NESState[fn]);
				}
				m_NESSaveStateLatch = true;
			}
			pressed_fn_count++;
		}
	}
	if (pressed_fn_count == 0)
	{
		m_NESSaveStateLatch = false;
	}
	//************************************************************************
	if (m_IsViewportSelected && m_RewindRecording)
	{
		if (keyboard[SDL_SCANCODE_R])
		{
			if (!m_RewindControlLatch)
			{
				m_RewindControlLatch = true;
				m_NESDevice.DeviceMode = NESDevice::DeviceMode::Pause;
			}

			if (m_RewindAdvanceLatchCooldown >= m_EmulatorFrameTimeRaw)
			{
				m_RewindAdvanceLatchCooldown -= m_EmulatorFrameTimeRaw;
			}
			else
			{
				m_RewindAdvanceLatchCooldown = (chrono_time::rep)m_DeviceUpdateTargetTiming;
				m_RewindAdvanceLatch = false;
			}

			
			if ((!keyboard[SDL_SCANCODE_LEFT] && !keyboard[SDL_SCANCODE_RIGHT]) && m_RewindAdvanceLatch)
			{
				m_RewindAdvanceLatch = false;
			}
			else if (keyboard[SDL_SCANCODE_LEFT] && !m_RewindAdvanceLatch)
			{
				m_RewindAdvanceLatch = true;

				if (m_RewindBufferIndex != m_RewindBufferBegin)
				{
					if (m_RewindBufferIndex == 0)
						m_RewindBufferIndex = m_RewindBufferSize;
					m_RewindBufferIndex--;

					m_NESDevice.LoadState(m_RewindBuffer[m_RewindBufferIndex]);
					m_NESDevice.DeviceMode = NESDevice::DeviceMode::AdvancePPUFrame;
				}
			}
			else if (keyboard[SDL_SCANCODE_RIGHT] && !m_RewindAdvanceLatch)
			{
				m_RewindAdvanceLatch = true;

				if (m_RewindBufferIndex != (m_RewindBufferEnd-1))
				{
					m_RewindBufferIndex++;
					if (m_RewindBufferIndex == m_RewindBufferSize)
						m_RewindBufferIndex = 0;

					m_NESDevice.LoadState(m_RewindBuffer[m_RewindBufferIndex]);
					m_NESDevice.DeviceMode = NESDevice::DeviceMode::AdvancePPUFrame;
				}
			}
		}
		else if (m_RewindControlLatch)
		{
			m_RewindBufferEnd = m_RewindBufferIndex;

			//Recalculate buffer length
			if (m_RewindBufferEnd == m_RewindBufferBegin)
				m_RewindBufferLength = 0;
			else if (m_RewindBufferEnd > m_RewindBufferBegin)
				m_RewindBufferLength = m_RewindBufferEnd - m_RewindBufferBegin;
			else if (m_RewindBufferEnd < m_RewindBufferBegin)
				m_RewindBufferLength = m_RewindBufferSize - (m_RewindBufferBegin - m_RewindBufferEnd);

			m_NESDevice.DeviceMode = NESDevice::DeviceMode::Running;
			m_RewindControlLatch = false;
		}
	}
	//************************************************************************
}

void Emulator::ProcessViewport()
{
	m_GLDisplay.UpdateDisplayTexture();

	ImVec2 screenPos = ImGui::GetCursorScreenPos();
	ImGuiWindowFlags DisplayFlags;
	if (m_ViewportWindowedMode)
	{
		DisplayFlags =
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoCollapse;
	}
	else
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		DisplayFlags =
			ImGuiWindowFlags_NoDecoration | 
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoSavedSettings;

		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
	}

	ImGui::SetNextWindowSizeConstraints(ImVec2(400, 400), ImVec2(FLT_MAX, FLT_MAX));
	if (ImGui::Begin("Display", nullptr, DisplayFlags))
	{
		ImVec2 windowSize = ImGui::GetWindowSize();
		windowSize.x -= ImGui::GetStyle().WindowPadding.x * 2;
		windowSize.y -= ImGui::GetStyle().WindowPadding.y * 2;

		//Draw vieport image inside child window
		ImGui::BeginChild("##Viewport", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false);
		m_IsViewportSelected = ImGui::IsWindowFocused();
		ImVec2 windowContentRegionMax = ImGui::GetWindowContentRegionMax();
		ImVec2 windowContentRegionMin = ImGui::GetWindowContentRegionMin();
		ImVec2 windowContentRegionSize = ImVec2(
			(windowContentRegionMax.x - windowContentRegionMin.x),
			(windowContentRegionMax.y - windowContentRegionMin.y)
		);

		int scaleFactor = (int)(windowContentRegionSize.x < windowContentRegionSize.y ?
			windowContentRegionSize.x : windowContentRegionSize.y);
		scaleFactor = (int)std::floor(scaleFactor / 256);

		ImVec2 imagePos = ImVec2(
			(windowContentRegionSize.x / 2.f) - (scaleFactor * 128),
			(windowContentRegionSize.y / 2.f) - (scaleFactor * 128)
		);
		ImVec2 imageSize = ImVec2((float)scaleFactor * 256.f, (float)scaleFactor * 256.f);
		
		ImGui::SetCursorPos(imagePos);
		ImGui::Image((ImTextureID)m_GLDisplay.getDisplayTexture(),
			imageSize, ImVec2(0, 0), ImVec2(1, 1));

		ImVec2 imagePosAbs(screenPos.x + imagePos.x, screenPos.y + imagePos.y);

		// ---- is paused ----
		if (m_NESDevice.DeviceMode == NESDevice::DeviceMode::Pause && !m_Debugger.IsCycleHijackActive() && !m_RewindControlLatch)
		{
			ImGuiWindowFlags window_flags = 
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoMove;

			ImGui::SetNextWindowPos(ImVec2(
				imagePos.x + (imageSize.x * 0.5f),
				imagePos.y + (imageSize.y * 0.5f)
			), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
			if (ImGui::Begin("Example: Simple overlay", 0, window_flags))
			{
				ImGui::TextUnformatted("Emulation paused");
			}
			ImGui::End();
		}

		// ---- draw rewind bar ----
		if (m_RewindControlLatch)
		{
			ImGuiWindowFlags window_flags =
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoMove;

			ImGui::SetNextWindowPos(ImVec2(
				imagePos.x + (imageSize.x * 0.5f),
				imagePos.y + (imageSize.y * 0.5f)
			), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background
			if (ImGui::Begin("Example: Simple overlay", 0, window_flags))
			{
				ImGui::TextUnformatted("Rewindind....");
			}
			ImGui::End();

			//Distance from begin to index
			uint32_t distanceToIndex = 0;
			if (m_RewindBufferIndex > m_RewindBufferBegin)		distanceToIndex = m_RewindBufferIndex - m_RewindBufferBegin;
			else if (m_RewindBufferIndex < m_RewindBufferBegin)	distanceToIndex = m_RewindBufferSize - (m_RewindBufferBegin - m_RewindBufferIndex);

			ImGui::SetCursorPos(ImVec2(
				imagePos.x + (imageSize.x * 0.05f),
				imagePos.y + (imageSize.y * 0.95f)
			));
			

			float bufferLenghtFactor = (float)distanceToIndex / (float)m_RewindBufferLength;
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12);
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, 
				ImVec4(
					1.f - (.25f + bufferLenghtFactor * 0.5f),
						  (.25f + bufferLenghtFactor * 0.5f),
						  (.25f + bufferLenghtFactor * 0.5f),
					0.75f
				));

			std::string overlay = "Frames available " + std::to_string(distanceToIndex);
			ImGui::ProgressBar(bufferLenghtFactor, ImVec2(imageSize.x * 0.9f, 0), overlay.c_str());

			ImGui::PopStyleColor();
			ImGui::PopStyleVar();
		}
		// -------------------------

		ImGui::EndChild();
		ImGui::Separator();

		//Display fps (Temporary)
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
		ImGui::Text("[Emulator % 8.2f FPS", (1000000.0 / m_EmulatorFrameTimeAverage)); ImGui::SameLine();
		ImGui::Text("(% 8.2f ms/f)]", (m_EmulatorFrameTimeAverage / 1000.0)); ImGui::SameLine();
		ImGui::Text("[NES % 8.2d FPS", (m_DeviceFramesPerSecond)); ImGui::SameLine();
		ImGui::Text("(% 8.2f ms/f)]", (m_DeviceFrameTime / 1000.0)); ImGui::SameLine();
		ImGui::Text("[Image scale : x%d]", (scaleFactor)); ImGui::SameLine();

		ImGui::PopStyleVar();
	}	ImGui::End();
}

void Emulator::ProcessWindows()
{
	//************************************************************************
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Menu"))
		{
			if (ImGui::MenuItem("Load ROM"))
			{
				m_FileDialog.OpenDialog(
					"FileDialog_SelectROM",
					"Select ROM",
					".nes",
					m_LastDirectory,
					1, nullptr,
					ImGuiFileDialogFlags_Modal);
			}

			if (ImGui::MenuItem("Save states"))
			{
				this->SaveStates();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Control"))
		{
			if (m_NESDevice.DeviceMode == NESDevice::DeviceMode::Running)
			{
				if (ImGui::MenuItem("Pause")) m_NESDevice.DeviceMode = NESDevice::DeviceMode::Pause;
			}
			else if (m_NESDevice.DeviceMode == NESDevice::DeviceMode::Pause)
			{
				if (ImGui::MenuItem("Resume")) m_NESDevice.DeviceMode = NESDevice::DeviceMode::Running;
			}

			if (ImGui::MenuItem("Reset"))
			{
				m_NESDevice.Reset();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Utils"))
		{
			ImGui::MenuItem("Enable Rewind", NULL, &m_RewindRecording);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Debug"))
		{
			ImGui::MenuItem("Display windowed mode", NULL, &m_ViewportWindowedMode);
			ImGui::MenuItem("Show CPU Memory Viewer", NULL, &m_ShowCPUMemoryViewer);
			ImGui::MenuItem("Show PPU Memory Viewer", NULL, &m_ShowPPUMemoryViewer);
			ImGui::MenuItem("Show CPU Controls", NULL, &m_ShowCPUControls);
			ImGui::MenuItem("Show PPU Data", NULL, &m_ShowPPUData);
			ImGui::MenuItem("Show ImGUI Style Editor", NULL, &m_ShowImGuiStyleEditor);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	//************************************************************************
	if (m_ShowCPUMemoryViewer)	m_ShowCPUMemoryViewer = m_Debugger.ShowCPUMemory();
	if (m_ShowPPUMemoryViewer)	m_ShowPPUMemoryViewer = m_Debugger.ShowPPUMemory();
	if (m_ShowCPUControls)		m_ShowCPUControls = m_Debugger.ShowCPUControls();
	if (m_ShowPPUData)			m_ShowPPUData = m_Debugger.ShowPPUData();
	if (m_ShowImGuiStyleEditor) ImGui::ShowStyleEditor();
	//************************************************************************
	ImGui::SetNextWindowPos(ImVec2((float)m_WindowWidth / 4.f, (float)m_WindowWidth / 4.f), ImGuiCond_Appearing);
	if (m_FileDialog.Display("FileDialog_SelectROM", 32, ImVec2(600, 400)))
	{
		if (m_FileDialog.IsOk())
		{
			m_LastFile = m_FileDialog.GetFilePathName();
			m_LastDirectory = m_FileDialog.GetCurrentPath() + "\\";

			this->SaveStates();
			m_NESDevice.GetCartrige().LoadCartrige(m_LastFile.c_str());
			this->LoadStates();

			m_NESDevice.Reset();
			m_NESDevice.DeviceMode = NESDevice::DeviceMode::Running;
		}
		m_FileDialog.Close();
	}
	//************************************************************************
}

void Emulator::ProcessFrameTime()
{
	//Update frame time
	m_EmulatorFrameTimeRaw = std::chrono::duration_cast<chrono_time>(
		chrono_clock::now() - m_EmulatorFrameStartTimestamp).count();
	m_EmulatorFrameTimeCache[m_EmulatorFrameTimeCacheIndex++] = m_EmulatorFrameTimeRaw;
	//Wrap index if size exceed
	if (m_EmulatorFrameTimeCacheIndex >= m_EmulatorFrameTimeCacheSize) m_EmulatorFrameTimeCacheIndex = 0;
	//Recalculate final frame time
	long long finalTimeAccumulator = 0;
	for (size_t index = 0; index < m_EmulatorFrameTimeCacheSize; index++)
		finalTimeAccumulator += m_EmulatorFrameTimeCache[index];
	m_EmulatorFrameTimeAverage = finalTimeAccumulator / (double)m_EmulatorFrameTimeCacheSize;

	m_EmulatorTimeAccumulator += m_EmulatorFrameTimeRaw;
	if (m_EmulatorTimeAccumulator >= 1000000)
	{
		m_DeviceFrameTime = 1000000.0 / (double)m_DeviceFramesAccumulator;
		m_DeviceFramesPerSecond = m_DeviceFramesAccumulator;

		m_EmulatorTimeAccumulator = 0;
		m_DeviceFramesAccumulator = 0;
	}

	//Update timestamp
	m_EmulatorFrameStartTimestamp = chrono_clock::now();
}

void Emulator::ProcessRewindUpdates()
{
	if (m_RewindRecording && !m_RewindControlLatch && m_NESDevice.DeviceMode != NESDevice::DeviceMode::Pause)
	{
		m_NESDevice.SaveState(m_RewindBuffer[m_RewindBufferEnd]);

		m_RewindBufferIndex = m_RewindBufferEnd;
		if (m_RewindBufferLength < (m_RewindBufferSize-1))
		{
			m_RewindBufferLength++;
			m_RewindBufferEnd++;
			//Wraping rw buffer end
			if (m_RewindBufferEnd == m_RewindBufferSize)
				m_RewindBufferEnd = 0;
		}
		else
		{
			m_RewindBufferBegin++;
			m_RewindBufferEnd++;
			//Wraping rw buffer end
			if (m_RewindBufferBegin == m_RewindBufferSize)
				m_RewindBufferBegin = 0;
			//Wraping rw buffer end
			if (m_RewindBufferEnd == m_RewindBufferSize)
				m_RewindBufferEnd = 0;
		}
	}
}

void Emulator::LoadConfigFile()
{
	//Set defaults
	m_WindowWidth = DEFAULT_CFG_WINDOW_WIDTH;
	m_WindowHeight = DEFAULT_CFG_WINDOW_HEIGHT;
	m_RewindBufferSize = DEFAULT_CFG_REWIND_BUFFER;

	//Update configs from file
	std::ifstream config_file("config.cfg", std::ios_base::in);
	if (config_file.is_open())
	{
		std::vector<std::pair<std::string, std::string>> config_tokens;

		std::string line;
		while (std::getline(config_file, line))
		{
			size_t s_pos = line.find_first_of(':');
			if (s_pos != std::string::npos)
			{
				config_tokens.push_back(std::make_pair<std::string, std::string>(line.substr(0, s_pos), line.substr(s_pos + 1)));
			}
		}
		for (auto config : config_tokens)
		{
			if (config.first == "window_width") m_WindowWidth = std::stoi(config.second);
			if (config.first == "window_height") m_WindowHeight = std::stoi(config.second);
			if (config.first == "rewind_buffer") m_RewindBufferSize = std::stoi(config.second);
		}
		config_file.close();

		if (m_RewindBufferSize <= 2) m_RewindBufferSize = 60;
	}
	else
	{
		std::ofstream new_config_file("config.cfg", std::ios_base::out);
		if (new_config_file.is_open())
		{
			new_config_file << "window_width:" << m_WindowWidth << std::endl;
			new_config_file << "window_height:" << m_WindowHeight << std::endl;
			new_config_file << "rewind_buffer:" << m_RewindBufferSize-2 << std::endl;
		}
		new_config_file.close();
	}
	//----
	//Check if saves folder present - if not create one
	if (!std::filesystem::exists("saves\\"))
	{
		std::filesystem::create_directory("saves");
	}
}

void Emulator::UpdateConfigFile()
{
	SDL_GetWindowSize(m_SDLWindow, (int*)&m_WindowWidth, (int*)&m_WindowHeight);
	std::ofstream new_config_file("config.cfg", std::ios_base::out);
	if (new_config_file.is_open())
	{
		std::string temp;

		new_config_file << "window_width:" << m_WindowWidth << std::endl;
		new_config_file << "window_height:" << m_WindowHeight << std::endl;
		new_config_file << "rewind_buffer:" << m_RewindBufferSize-2 << std::endl;
		new_config_file.close();
	}
}

void Emulator::LoadStates()
{

	//----
	//Clear savestates
	for (uint32_t i = 0; i < 8; i++)
		m_NESState[i].Clear();
	//----
	//Load states for current rom
	if (m_NESDevice.GetCartrige().IsCartrigeReady())
	{
		std::string saveStateFile = "saves\\" + m_NESDevice.GetCartrige().GetROMName() + ".sav";
		if (!NESState::LoadFromFile(saveStateFile.c_str(), m_NESState, 8))
			printf("Failed to load states from file \"%s\"", saveStateFile.c_str());
	}
	//----
}

void Emulator::SaveStates()
{
	//----
	//Save states for current rom
	if (m_NESDevice.GetCartrige().IsCartrigeReady())
	{
		std::string saveStateFile = "saves\\" + m_NESDevice.GetCartrige().GetROMName() + ".sav";
		if (!NESState::SaveToFile(saveStateFile.c_str(), m_NESState, 8))
			printf("Failed to save states to file \"%s\"", saveStateFile.c_str());
	}
	//----
}


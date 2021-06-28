#include "SaffronPCH.h"

#include "Saffron/Core/App.h"
#include "Saffron/Core/Global.h"
#include "Saffron/Gui/SplashScreenPane.h"
#include "Saffron/Graphics/Scene.h"


namespace Se
{
auto AppProperties::CreateFullscreen(String name) -> AppProperties
{
	const auto dm = sf::VideoMode::getDesktopMode();
	const uint halfWidth = dm.width / 3, halfHeight = dm.height / 3;
	return {Move(name), 2 * halfWidth, 2 * halfHeight, dm.width / 2 - halfWidth, dm.height / 2 - halfHeight, true};
}

auto AppProperties::CreateMaximized(String name) -> AppProperties
{
	const auto dm = sf::VideoMode::getDesktopMode();
	return {Move(name), dm.width, dm.height, 0, 0, false};
}

auto AppProperties::CreateCentered(String name, uint windowWidth, uint windowHeight) -> AppProperties
{
	const auto dm = sf::VideoMode::getDesktopMode();
	const uint halfWidth = windowWidth / 2, halfHeight = windowHeight / 2;
	return {Move(name), windowWidth, windowHeight, dm.width / 2 - halfWidth, dm.height / 2 - halfHeight, false};
}

App::App(const AppProperties& properties) :
	SingleTon(this),
	_preLoader(CreateShared<BatchLoader>("Preloader")),
	_window(CreateUnique<class Window>(properties.Name, properties.WindowWidth, properties.WindowHeight)),
	_menuBar(CreateUnique<MenuBar>()),
	_filesystem(CreateUnique<Filesystem>(*_window)),
	_gui(CreateUnique<Gui>()),
	_keyboard(CreateUnique<Keyboard>()),
	_mouse(CreateUnique<Mouse>()),
	_renderTargetManager(CreateUnique<RenderTargetManager>()),
	_run(CreateUnique<class Run>()),
	_name(properties.Name),
	_computeShaderStore(CreateUnique<ComputeShaderStore>()),
	_fontStore(CreateUnique<FontStore>()),
	_imageStore(CreateUnique<ImageStore>()),
	_musicStore(CreateUnique<MusicStore>()),
	_shaderStore(CreateUnique<ShaderStore>()),
	_soundBufferStore(CreateUnique<SoundBufferStore>()),
	_textureStore(CreateUnique<TextureStore>()),
	_fadeIn(FadeType::In, sf::seconds(0.5f))
{
	Log::CoreInfo("--- Saffron 2D Framework ---");
	Log::CoreInfo("Creating application {0}", properties.Name);

	_window->Closed += SE_EV_ACTION(App::OnWindowClose);
	_window->SetIcon("Editor/Saffron_windowIcon.png");

	if (properties.Fullscreen)
	{
		_window->SetFullscreen(true);
	}

	_preLoader->Submit([this]
	{
		Gui::SetStyle(GuiStyle::Dark);
		MenuBar::AddMenu(MenuBarMenu("File", SE_EV_ACTION(App::OnRenderMenuBar)), -1000);
	}, "Initializing GUI");

	Global::Clock::Restart();
}

App::~App()
{
	Log::CoreInfo("Shutting down");
}

void App::PushLayer(std::shared_ptr<Layer> layer)
{
	_layerStack.PushLayer(layer, _preLoader);
}

void App::PushOverlay(std::shared_ptr<Layer> overlay)
{
	_layerStack.PushOverlay(overlay, _preLoader);
}

void App::PopLayer(int count)
{
	_layerStack.PopLayer(count);
}

void App::PopOverlay(int count)
{
	_layerStack.PopOverlay(count);
}

void App::EraseLayer(std::shared_ptr<Layer> layer)
{
	_layerStack.EraseLayer(layer);
}

void App::EraseOverlay(std::shared_ptr<Layer> overlay)
{
	_layerStack.EraseOverlay(overlay);
}

auto App::Window() const -> class Window&
{
	return *_window;
}

void App::Run()
{
	OnInit();

	while (_running)
	{
		if (!_preLoader->IsFinished())
		{
			RunSplashScreen();
			_fadeIn.Start();
		}

		Global::Clock::Restart();
		_window->HandleBufferedEvents();
		_window->HandleBufferedMessages();
		_window->Clear();
		_renderTargetManager->ClearAll();
		_gui->Begin();
		
		if (!_minimized)
		{
			for (const auto& layer : _layerStack)
			{
				layer->OnPreFrame();
			}
			for (const auto& layer : _layerStack)
			{
				layer->OnUpdate();
			}
			for (const auto& layer : _layerStack)
			{
				layer->OnGuiRender();
			}
			for (const auto& layer : _layerStack)
			{
				layer->OnPostFrame();
			}
			_fadeIn.OnUpdate();
			_fadeIn.OnGuiRender();
		}
		OnUpdate();
		_run->Execute();
		
		_gui->End();

		_keyboard->OnUpdate();
		_mouse->OnUpdate();

		_renderTargetManager->DisplayAll();
		_window->Display();
	}

	_layerStack.Clear();

	OnShutdown();
}

void App::Exit()
{
	_preLoader->ForceExit();
	_running = false;
}

void App::OnUpdate()
{
	if (Keyboard::IsDown(sf::Keyboard::LAlt) && Keyboard::IsPressed(sf::Keyboard::Enter))
	{
		_window->SetFullscreen(!_window->IsFullscreen());
	}

	static bool open = false;
	if (Keyboard::IsPressed(sf::Keyboard::Escape))
	{
		if(open)
		{
			ImGui::CloseCurrentPopup();
			open = false;
		}
		else
		{
			ImGui::OpenPopup("###Exitmenu");
			open = true;
		}
	}
	
	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));	
	if (ImGui::BeginPopupModal("###Exitmenu", &open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove))
	{
		if (ImGui::Button("Fullscreen", {100, 0}))
		{
			_window->SetFullscreen(!_window->IsFullscreen());
		}
		
		if(ImGui::Button("Exit", {100, 0}))
		{
			Exit();
		}
		
		ImGui::EndPopup();
	}
}

void App::OnGuiRender()
{
	if (ImGui::Begin("Stats"))
	{
		const auto dt = Global::Clock::FrameTime();
		_fpsTimer += dt;
		if (_fpsTimer.asSeconds() < 1.0f)
		{
			_storedFrameCount++;
			_storedFrametime += dt;
		}
		else
		{
			_cachedFps = static_cast<int>(static_cast<float>(_storedFrameCount) / _storedFrametime.asSeconds());
			_cachedSpf = sf::seconds(_storedFrametime.asSeconds() / static_cast<float>(_storedFrameCount));
			_storedFrameCount = 0;
			_storedFrametime = sf::Time::Zero;
			_fpsTimer = sf::Time::Zero;
		}

		Gui::BeginPropertyGrid();

		Gui::Property("Vendor", "SFML v.2.5.2");
		Gui::Property("Frametime", std::to_string(_cachedSpf.asMicroseconds() / 1000.0f) + " ms");
		Gui::Property("FPS", std::to_string(_cachedFps));


		Gui::EndPropertyGrid();

		static Array<float, 90> values = {};
		static int values_offset = 0;
		static constexpr auto cap = 5.0f * 1.0f / 144.0f;

		static float phase = 0.0f;
		values[values_offset] = GenUtils::Map(dt.asSeconds(), 1.0f / 144.0f, cap, -1.0f, 1.0f);
		values_offset = (values_offset + 1) % values.size();
		phase += 0.10f * values_offset;

		auto shouldPop = false;
		if (cap - dt.asSeconds() < 0.001f)
		{
			ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(1.0f, 0.0f, 0.0f, 1.00f));
			shouldPop = true;
		}
		const auto size = ImVec2(ImGui::GetContentRegionAvailWidth(), 80);
		ImGui::PlotLines("", values.data(), values.size(), values_offset, nullptr, -1.0f, 1.0f, size);
		if (shouldPop)
		{
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();

	_menuBar->Begin();
	_menuBar->End();
}

auto App::OnWindowClose() -> bool
{
	Exit();
	return true;
}

void App::RunSplashScreen() const
{
	_preLoader->Execute();

	SplashScreenPane splashScreenPane(_preLoader);
	while (!splashScreenPane.Finished())
	{
		_window->Clear();
		_renderTargetManager->ClearAll();
		_gui->Begin();
		splashScreenPane.OnUpdate();
		splashScreenPane.OnGuiRender();
		_window->HandleBufferedEvents();
		_window->HandleBufferedMessages();
		_gui->End();
		_run->Execute();
		_renderTargetManager->DisplayAll();
		_window->Display();
		Global::Clock::Restart();
		const auto step = Global::Clock::FrameTime().asSeconds();
		const auto duration = splashScreenPane.BatchLoader()->IsFinished()
			                      ? 0ll
			                      : std::max(0ll, static_cast<long long>(1000.0 / 60.0 - step));
		std::this_thread::sleep_for(std::chrono::milliseconds(duration));
	}
}

auto App::ConfigurationName() -> String
{
#if defined(SE_DEBUG)
	return "Debug";
#elif defined(SE_RELEASE)
	return "Release";
#elif defined(SE_DIST)
	return "Dist";
#else
#error Undefined configuration?
#endif
}

auto App::PlatformName() -> String
{
#if defined(SE_PLATFORM_WINDOWS)
	return "Windows x64";
#elif defined(SE_PLATFORM_LINUX)
	return "Linux x64";
#endif
}

auto App::Name() -> String
{
	return Instance()._name;
}

void App::OnRenderMenuBar()
{
	if (ImGui::MenuItem("Fullscreen", "Alt+Enter", _window->IsFullscreen()))
	{
		_window->SetFullscreen(!_window->IsFullscreen());
	}
	if (ImGui::MenuItem("Exit", "Alt+F4"))
	{
		Exit();
	}
}
}

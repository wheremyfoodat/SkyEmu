#define ZEP_SINGLE_HEADER_BUILD
#include <memory>

#include "sokol_app.h"
#include "zep.h"
#include "zep/imgui/display_imgui.h"
#include "zep/imgui/editor_imgui.h"
#include "zep/mode_repl.h"
#include "zep/mode_standard.h"
#include "zep/regress.h"
#include "zep/tab_window.h"
#include "zep/window.h"

extern "C" {
#include "lua_manager.h"
#include "text_editor.h"

bool se_key_is_just_pressed(int keycode);
}

using namespace Zep;

static Zep::NVec2f GetPixelScale() { return Zep::NVec2f(1.0f); }

static float dpi_pixel_height_from_point_size(float pointSize, float pixelScaleY) {
	const auto fontDotsPerInch = 72.0f;
	auto inches = pointSize / fontDotsPerInch;
	return inches * (pixelScaleY * 96.0f);
}

// A helper struct to init the editor and handle callbacks
struct ZepContainerImGui : public IZepComponent, public IZepReplProvider {
	ZepContainerImGui(const std::string& startupFilePath, const std::string& configPath)
		: impl(std::make_unique<ZepEditor_ImGui>(configPath, GetPixelScale())) {
		impl->RegisterCallback(this);

		ZepRegressExCommand::Register(*impl);
		ZepReplExCommand::Register(*impl, this);
		ZepReplEvaluateOuterCommand::Register(*impl, this);
		ZepReplEvaluateInnerCommand::Register(*impl, this);
		ZepReplEvaluateCommand::Register(*impl, this);

		impl->InitWithText("script.lua", "print(\"Hello World!\")");
		impl->SetGlobalMode(Zep::ZepMode_Standard::StaticName());
	}

	void SetFont(ImFont* font) {
		auto& io = ImGui::GetIO();
		auto& display = static_cast<ZepDisplay_ImGui&>(impl->GetDisplay());

		if (io.Fonts->Fonts.empty()) {
			return;
		}

		ImFont* pImFont = io.FontDefault ? io.FontDefault : io.Fonts->Fonts[0];
		int fontPixelHeight = (int)dpi_pixel_height_from_point_size(pImFont->FontSize, GetPixelScale().y);

		ImVector<ImWchar> ranges;
		ImFontGlyphRangesBuilder builder;
		builder.AddRanges(io.Fonts->GetGlyphRangesDefault());   // Add one of the default ranges
		builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic());  // Add one of the default ranges
		builder.AddRanges(greek_range);
		builder.BuildRanges(&ranges);  // Build the final result (ordered ranges with all the unique characters submitted)

		display.SetFont(ZepTextType::UI, std::make_shared<ZepFont_ImGui>(display, pImFont, fontPixelHeight));
		display.SetFont(ZepTextType::Text, std::make_shared<ZepFont_ImGui>(display, pImFont, fontPixelHeight));
		display.SetFont(ZepTextType::Heading1, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.75)));
		display.SetFont(ZepTextType::Heading2, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.5)));
		display.SetFont(ZepTextType::Heading3, std::make_shared<ZepFont_ImGui>(display, pImFont, int(fontPixelHeight * 1.25)));
	}

	~ZepContainerImGui() {}

	void Destroy() {
		impl->UnRegisterCallback(this);
		impl.reset();
	}

	virtual ZepEditor& GetEditor() const override { return *impl; }

	bool quit = false;
	bool in_focus = false;
	std::unique_ptr<ZepEditor_ImGui> impl;
};

using ZepContainer = ZepContainerImGui;

static void se_handle_text_editor_keypresses(text_editor_t editor) {
	auto& zep = *(ZepContainer*)editor;
	auto& io = ImGui::GetIO();

	if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0) {
		zep.impl->OnMouseMove(Zep::toNVec2f(io.MousePos));
	}

	if (io.MouseClicked[0]) {
		if (zep.impl->OnMouseDown(Zep::toNVec2f(io.MousePos), Zep::ZepMouseButton::Left)) {
			// Hide the mouse click from imgui if we handled it
			io.MouseClicked[0] = false;
		}
	}

	if (io.MouseClicked[1]) {
		if (zep.impl->OnMouseDown(Zep::toNVec2f(io.MousePos), Zep::ZepMouseButton::Right)) {
			// Hide the mouse click from imgui if we handled it
			io.MouseClicked[0] = false;
		}
	}

	if (io.MouseReleased[0]) {
		if (zep.impl->OnMouseUp(Zep::toNVec2f(io.MousePos), Zep::ZepMouseButton::Left)) {
			// Hide the mouse click from imgui if we handled it
			io.MouseClicked[0] = false;
		}
	}

	if (io.MouseReleased[1]) {
		if (zep.impl->OnMouseUp(Zep::toNVec2f(io.MousePos), Zep::ZepMouseButton::Right)) {
			// Hide the mouse click from imgui if we handled it
			io.MouseClicked[0] = false;
		}
	}

	if (ImGui::IsWindowFocused()) {
		zep.in_focus = true;
		bool handled = false;
		uint32_t mod = 0;

		if (io.KeyCtrl) {
			mod |= Zep::ModifierKey::Ctrl;
		}
		if (io.KeyShift) {
			mod |= Zep::ModifierKey::Shift;
		}

		auto pWindow = zep.impl->GetActiveTabWindow()->GetActiveWindow();
		const auto& buffer = pWindow->GetBuffer();

		if (se_key_is_just_pressed(SAPP_KEYCODE_TAB)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::TAB, mod);
			return;
		}
		if (se_key_is_just_pressed(SAPP_KEYCODE_ESCAPE)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::ESCAPE, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_ENTER)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::RETURN, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_DELETE)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::DEL, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_HOME)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::HOME, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_END)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::END, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_BACKSPACE)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::BACKSPACE, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_RIGHT)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::RIGHT, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_LEFT)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::LEFT, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_UP)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::UP, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_DOWN)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::DOWN, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_PAGE_DOWN)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::PAGEDOWN, mod);
			return;
		} else if (se_key_is_just_pressed(SAPP_KEYCODE_PAGE_UP)) {
			buffer.GetMode()->AddKeyPress(Zep::ExtKeys::PAGEUP, mod);
			return;
		} else if (io.KeyCtrl) {
			for (int ch = SAPP_KEYCODE_A; ch <= SAPP_KEYCODE_Z; ch++) {
				if (se_key_is_just_pressed(ch)) {
					buffer.GetMode()->AddKeyPress(ch - SAPP_KEYCODE_A + 'a', mod);
					handled = true;
				}
			}

			if (se_key_is_just_pressed(SAPP_KEYCODE_SPACE)) {
				buffer.GetMode()->AddKeyPress(' ', mod);
				handled = true;
			}
		}

		if (!handled) {
			for (int n = 0; n < io.InputQueueCharacters.Size && io.InputQueueCharacters[n]; n++) {
				// Ignore '\r' - sometimes ImGui generates it!
				if (io.InputQueueCharacters[n] == '\r') continue;
				buffer.GetMode()->AddKeyPress(io.InputQueueCharacters[n], mod);
			}
		}
	}
}

text_editor_t se_create_text_editor() {
	ZepContainer* provider = new ZepContainer("script.lua", "");
	auto& editor = provider->GetEditor();

	Zep::ZepReplExCommand::Register(editor, provider);
	Zep::ZepReplEvaluateOuterCommand::Register(editor, provider);
	Zep::ZepReplEvaluateInnerCommand::Register(editor, provider);
	Zep::ZepReplEvaluateCommand::Register(editor, provider);

	return (void*)provider;
}

void se_destroy_text_editor(text_editor_t editor) { delete (ZepContainer*)editor; }
bool se_text_editor_focused(text_editor_t editor) { return ((ZepContainer*)editor)->in_focus; }

void se_display_text_editor(text_editor_t editor) {
	auto& zep = *(ZepContainer*)editor;
	zep.in_focus = false;

	ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Zep")) {
		if (ImGui::Button("Load Script")) {
			// Fetch the script in text form and load it
			ZepBuffer* buffer = zep.GetEditor().GetMRUBuffer();
			const std::string text = buffer->GetBufferText(buffer->Begin(), buffer->End());
			se_lua_load_string(text.c_str());
		}

		auto min = ImGui::GetCursorScreenPos();
		auto max = ImGui::GetContentRegionAvail();
		max.x = std::max(1.0f, max.x);
		max.y = std::max(1.0f, max.y);

		// Fill the window
		max.x = min.x + max.x;
		max.y = min.y + max.y;
		zep.impl->SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));

		// Handle user input
		se_handle_text_editor_keypresses(editor);

		// Display the editor inside this window
		zep.impl->Display();
	}

	ImGui::End();
}

void se_set_text_editor_fonts(text_editor_t editor) {
	auto& zep = *(ZepContainer*)editor;
	zep.SetFont(nullptr);
}

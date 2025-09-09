#define ZEP_SINGLE_HEADER_BUILD
#include <memory>

#include "zep.h"
#include "zep/filesystem.h"
#include "zep/imgui/display_imgui.h"
#include "zep/imgui/editor_imgui.h"
#include "zep/mode_repl.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/regress.h"
#include "zep/tab_window.h"
#include "zep/theme.h"
#include "zep/window.h"

extern "C" {
#include "text_editor.h"
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
		: spEditor(std::make_unique<ZepEditor_ImGui>(configPath, GetPixelScale())) {
		spEditor->RegisterCallback(this);

		ZepRegressExCommand::Register(*spEditor);
		ZepReplExCommand::Register(*spEditor, this);
		ZepReplEvaluateOuterCommand::Register(*spEditor, this);
		ZepReplEvaluateInnerCommand::Register(*spEditor, this);
		ZepReplEvaluateCommand::Register(*spEditor, this);

		if (!startupFilePath.empty()) {
			spEditor->InitWithFileOrDir(startupFilePath);
		} else {
			spEditor->InitWithText("script.lua", "print(\"Hello World!\")");
		}
	}

	void SetFont(ImFont* font) {
		auto& io = ImGui::GetIO();
		auto& display = static_cast<ZepDisplay_ImGui&>(spEditor->GetDisplay());

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
		spEditor->UnRegisterCallback(this);
		spEditor.reset();
	}

	virtual ZepEditor& GetEditor() const override { return *spEditor; }

	bool quit = false;
	std::unique_ptr<ZepEditor_ImGui> spEditor;
};

using ZepContainer = ZepContainerImGui;

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

void se_display_text_editor(text_editor_t editor) {
	auto& zep = *(ZepContainer*)editor;

	ImGui::Begin("Zep", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

	auto min = ImGui::GetCursorScreenPos();
	auto max = ImGui::GetContentRegionAvail();
	max.x = std::max(1.0f, max.x);
	max.y = std::max(1.0f, max.y);

	// Fill the window
	max.x = min.x + max.x;
	max.y = min.y + max.y;
	zep.spEditor->SetDisplayRegion(Zep::NVec2f(min.x, min.y), Zep::NVec2f(max.x, max.y));

	// Display the editor inside this window
	zep.spEditor->Display();
	zep.spEditor->HandleInput();
	ImGui::End();
}

void se_set_text_editor_fonts(text_editor_t editor) {
	auto& zep = *(ZepContainer*)editor;
	zep.SetFont(nullptr);
}

#include "MainComponent.h"

class KickLabApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "KRAVATEK KICK LAB"; }
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String& commandLine) override {
        window_ = std::make_unique<Window>();
        const auto arguments = juce::StringArray::fromTokens(commandLine, true);
        if (arguments.contains("--smoke-test")) {
            const auto index = arguments.indexOf("--smoke-test");
            if (index + 1 >= arguments.size()) { setApplicationReturnValue(2); quit(); return; }
            const auto output = juce::File::getCurrentWorkingDirectory().getChildFile(arguments[index + 1].unquoted());
            juce::Timer::callAfterDelay(300, [this, output] {
                try { window_->content().runSmokeTest(output); }
                catch (const std::exception& error) {
                    juce::Logger::writeToLog("GUI smoke failed: " + juce::String(error.what()));
                    setApplicationReturnValue(1);
                }
                quit();
            });
        }
    }
    void shutdown() override { window_.reset(); }
    void systemRequestedQuit() override { quit(); }
private:
    class Workspace final : public juce::Viewport {
    public:
        Workspace() {
            instrument_ = new MainComponent;
            setViewedComponent(instrument_, true); setScrollBarsShown(true, true);
            setSize(1380, 940);
        }
        void resized() override {
            juce::Viewport::resized();
            instrument_->setSize(std::max(1100, getMaximumVisibleWidth()), std::max(860, getMaximumVisibleHeight()));
        }
        MainComponent& instrument() {return *instrument_;}
    private:
        MainComponent* instrument_ = nullptr;
    };
    class Window final : public juce::DocumentWindow {
    public:
        Window() : DocumentWindow(juce::String::fromUTF8("KRAVATEK KICK LAB — INSTRUMENT 002"),
                    krui::blue, DocumentWindow::allButtons) {
            setLookAndFeel(&lookAndFeel_);
            setUsingNativeTitleBar(false);
            setTitleBarHeight(32);
            setContentOwned(new Workspace, true);
            setResizable(true, false);
            setResizeLimits(1100, 740, 1800, 1250);
            if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
                const auto area = display->userBounds.reduced(12).toNearestInt();
                centreWithSize(std::min(getWidth(), area.getWidth()), std::min(getHeight(), area.getHeight()));
            } else centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        ~Window() override { setLookAndFeel(nullptr); }
        MainComponent& content() { return static_cast<Workspace*>(getContentComponent())->instrument(); }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    private:
        krui::LookAndFeel lookAndFeel_;
    };
    std::unique_ptr<Window> window_;
};

START_JUCE_APPLICATION(KickLabApplication)

#include "MainComponent.h"

class KickLabApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "KRAVATEK KICK LAB"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
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
    class Window final : public juce::DocumentWindow {
    public:
        Window() : DocumentWindow(juce::String::fromUTF8("KRAVATEK KICK LAB — PROTOTYPE 001"),
                    juce::Colour(0xff101116), DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent, true);
            setResizable(true, false);
            setResizeLimits(760, 520, 1400, 1000);
            centreWithSize(840, 560);
            setVisible(true);
        }
        MainComponent& content() { return *static_cast<MainComponent*>(getContentComponent()); }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window_;
};

START_JUCE_APPLICATION(KickLabApplication)

#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace krui {
inline const juce::Colour blue {0xff171b1e}, dark {0xff101618}, panel {0xff242b2f};
inline const juce::Colour cyan {0xff5ed6c0}, pink {0xffe99abb}, ink {0xffeeeae0}, muted {0xffa4aeae};
inline const juce::Colour yellow {0xffe2c65a}, line {0xff465052};
inline juce::Font mono(float size, bool bold=false) {
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(),size,
        bold ? juce::Font::bold : juce::Font::plain));
}
inline void grid(juce::Graphics& g, juce::Rectangle<int> bounds,int step=24) {
    g.setColour(cyan.withAlpha(.06f));
    for (int x=bounds.getX();x<bounds.getRight();x+=step) g.drawVerticalLine(x,static_cast<float>(bounds.getY()),static_cast<float>(bounds.getBottom()));
    for (int y=bounds.getY();y<bounds.getBottom();y+=step) g.drawHorizontalLine(y,static_cast<float>(bounds.getX()),static_cast<float>(bounds.getRight()));
}
inline void frame(juce::Graphics& g,juce::Rectangle<int> bounds,const juce::String& title) {
    const auto r=bounds.toFloat().reduced(.5f);
    g.setColour(panel); g.fillRoundedRectangle(r,3);
    g.setColour(line); g.drawRoundedRectangle(r,3,1);
    g.setColour(juce::Colour(0xff30383b)); g.fillRect(r.withHeight(29));
    g.setColour(yellow); g.fillRect(r.getX(),r.getY()+7,3.0f,15.0f);
    g.setColour(ink); g.setFont(mono(12,true));
    g.drawText(title,bounds.reduced(12,0).withHeight(29),juce::Justification::centredLeft);
    g.setColour(line); g.drawHorizontalLine(bounds.getY()+29,static_cast<float>(bounds.getX()),static_cast<float>(bounds.getRight()));
}
class Brand final:public juce::Component {
    void paint(juce::Graphics& g) override {
        auto b=getLocalBounds(); g.setColour(yellow);g.fillRect(b);
        g.setColour(blue);g.setFont(mono(10,true));
        g.drawText("INDEPENDENT SOUND MACHINERY",b.reduced(12).withHeight(14),juce::Justification::centredLeft);
        auto name=b.reduced(10,0).withY(23).withHeight(46);
        auto font=juce::Font(juce::FontOptions(42.0f,juce::Font::bold)); font.setExtraKerningFactor(-.055f);
        g.setFont(font); g.drawFittedText("KRAVATEK",name,juce::Justification::centredLeft,1,.7f);
        g.setFont(mono(13,true));g.drawText("KICK LAB     /     0.2",b.reduced(12).withY(b.getBottom()-29).withHeight(18),juce::Justification::centredLeft);
    }
};
class WindowButton final:public juce::Button {
public:
    explicit WindowButton(int type):Button("Window control"),type_(type) {}
    void paintButton(juce::Graphics& g,bool over,bool down) override {
        auto b=getLocalBounds().toFloat().reduced(6);g.setColour(over||down ? yellow:muted);
        if (type_==juce::DocumentWindow::closeButton) {
            g.drawLine(b.getX(),b.getY(),b.getRight(),b.getBottom(),1.5f);g.drawLine(b.getRight(),b.getY(),b.getX(),b.getBottom(),1.5f);
        } else if(type_==juce::DocumentWindow::minimiseButton) g.drawLine(b.getX(),b.getBottom(),b.getRight(),b.getBottom(),1.5f);
        else g.drawRect(b,1.5f);
    }
private:int type_;
};
class LookAndFeel final:public juce::LookAndFeel_V4 {
public:
    LookAndFeel() {
        setColour(juce::ResizableWindow::backgroundColourId,blue);
        setColour(juce::Slider::textBoxTextColourId,ink);setColour(juce::Slider::textBoxBackgroundColourId,dark);
        setColour(juce::Slider::textBoxOutlineColourId,line);setColour(juce::Slider::thumbColourId,yellow);
        setColour(juce::Slider::trackColourId,cyan);setColour(juce::Slider::backgroundColourId,line);
        setColour(juce::PopupMenu::backgroundColourId,dark);setColour(juce::PopupMenu::textColourId,ink);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,yellow);setColour(juce::PopupMenu::highlightedTextColourId,blue);
        setColour(juce::TooltipWindow::backgroundColourId,ink);setColour(juce::TooltipWindow::textColourId,blue);
        setColour(juce::TooltipWindow::outlineColourId,yellow);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool over,bool down) override {
        const bool action=b.getButtonText()=="GENERATE"||b.getButtonText()=="EVOLVE";
        const bool selected=b.getToggleState();
        auto r=b.getLocalBounds().toFloat().reduced(2);
        auto fill=action ? yellow : selected ? cyan : juce::Colour(0xff353f42);
        if (!b.isEnabled()) fill=panel;
        else if(over||down) fill=fill.brighter(down ? .1f:.18f);
        g.setColour(fill);g.fillRoundedRectangle(r,2);
        g.setColour(b.isEnabled() && selected ? cyan:line);g.drawRoundedRectangle(r,2,1);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override {
        auto r=b.getLocalBounds().toFloat().reduced(7);
        const auto text=b.getButtonText();const bool tall=r.getHeight()>50;
        const bool filled=b.getToggleState()||text=="GENERATE"||text=="EVOLVE";
        g.setColour(!b.isEnabled()?line:filled?blue:ink);
        if(tall) {
            const auto c=juce::Point<float>(r.getCentreX(),r.getY()+20);
            if(text=="PLAY") {juce::Path p;p.addTriangle(c.x-8,c.y-11,c.x-8,c.y+11,c.x+11,c.y);g.fillPath(p);}
            else if(text=="STOP") g.fillRect(c.x-9,c.y-9,18.0f,18.0f);
            else if(text=="GENERATE") {
                g.drawRoundedRectangle(c.x-12,c.y-12,24,24,3,1.5f);
                for(auto p:{juce::Point<float>(-6,-6),juce::Point<float>(0,0),juce::Point<float>(6,6)}) g.fillEllipse(c.x+p.x-2,c.y+p.y-2,4,4);
            } else if(text=="EVOLVE") {
                juce::Path p;for(int i=0;i<30;++i) {const auto x=c.x-14+i;const auto y=c.y+std::sin(i*.33f)*10;
                    if(i==0)p.startNewSubPath(x,y);else p.lineTo(x,y);}
                g.strokePath(p,juce::PathStrokeType(2));
                g.drawLine(c.x+11,c.y-12,c.x+11,c.y+12,1);g.fillEllipse(c.x+8,c.y-7,6,6);
            } else {g.drawRect(c.x-12,c.y-1,24.0f,14.0f,1.5f);g.drawLine(c.x,c.y-14,c.x,c.y+5,2);g.drawLine(c.x-5,c.y-8,c.x,c.y-14,2);g.drawLine(c.x+5,c.y-8,c.x,c.y-14,2);}
        }
        g.setFont(mono(tall?11.5f:12,true));
        g.drawText(text,r.toNearestInt().withTrimmedTop(tall?44:0),juce::Justification::centred);
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float position,float start,float end,juce::Slider&) override {
        const auto diameter=static_cast<float>(std::min(w,h))-8;
        if(diameter<8)return;
        const auto c=juce::Point<float>(x+w*.5f,y+h*.5f);const auto radius=diameter*.5f;
        juce::Path base,value;base.addCentredArc(c.x,c.y,radius,radius,0,start,end,true);
        value.addCentredArc(c.x,c.y,radius,radius,0,start,start+position*(end-start),true);
        g.setColour(line);g.strokePath(base,juce::PathStrokeType(3));g.setColour(cyan);g.strokePath(value,juce::PathStrokeType(3));
        for(int i=0;i<9;++i){const auto a=start+i*(end-start)/8-juce::MathConstants<float>::halfPi;
            g.setColour(muted.withAlpha(.55f));g.drawLine(c.x+std::cos(a)*(radius-7),c.y+std::sin(a)*(radius-7),c.x+std::cos(a)*(radius-10),c.y+std::sin(a)*(radius-10),1);}
        const auto knob=juce::Rectangle<float>(diameter-20,diameter-20).withCentre(c);
        g.setColour(dark);g.fillEllipse(knob);g.setColour(line);g.drawEllipse(knob,1);
        const auto a=start+position*(end-start)-juce::MathConstants<float>::halfPi;
        g.setColour(ink);g.drawLine(c.x,c.y,c.x+std::cos(a)*(radius-13),c.y+std::sin(a)*(radius-13),2);
    }
    void drawDocumentWindowTitleBar(juce::DocumentWindow& window,juce::Graphics& g,int width,int height,int x,int w,const juce::Image*,bool) override {
        g.fillAll(blue);g.setColour(muted);g.setFont(mono(11));g.drawText(window.getName(),x+8,0,w-16,height,juce::Justification::centredLeft);
        g.setColour(line);g.fillRect(0,height-1,width,1);
    }
    juce::Button* createDocumentWindowButton(int type) override{return new WindowButton(type);}
};
}

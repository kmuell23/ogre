/*
 * SdkTrays.cpp
 *
 *  Created on: 01.07.2016
 *      Author: parojtbe
 */
module;

#include <cmath>

module Ogre.Components.Bites;

import :Trays;

import Ogre.Components.Overlay;
import Ogre.Core;

import <algorithm>;
import <iomanip>;
import <map>;
import <memory>;
import <ostream>;
import <utility>;

namespace OgreBites
{

// maximal rate to update widgets internally
static unsigned constexpr FRAME_UPDATE_DELAY = 250; // ms

Widget::Widget()
{
    mTrayLoc = TrayLocation::NONE;
    mElement = nullptr;
    mListener = nullptr;
}

void Widget::cleanup()
{
    if (mElement) nukeOverlayElement(mElement);
    mElement = nullptr;
}

void Widget::nukeOverlayElement(Ogre::OverlayElement *element)
{
    auto* container = dynamic_cast<Ogre::OverlayContainer*>(element);
    if (container)
    {
        std::vector<Ogre::OverlayElement*> toDelete;
        for (const auto& p : container->getChildren())
        {
            toDelete.push_back(p.second);
        }

        for (auto & i : toDelete)
        {
            nukeOverlayElement(i);
        }
    }
    if (element)
    {
        Ogre::OverlayContainer* parent = element->getParent();
        if (parent) parent->removeChild(element->getName());
        Ogre::OverlayManager::getSingleton().destroyOverlayElement(element);
    }
}

auto Widget::isCursorOver(Ogre::OverlayElement *element, const Ogre::Vector2 &cursorPos, Ogre::Real voidBorder) -> bool
{
    Ogre::OverlayManager& om = Ogre::OverlayManager::getSingleton();
    Ogre::Real l = element->_getDerivedLeft() * om.getViewportWidth();
    Ogre::Real t = element->_getDerivedTop() * om.getViewportHeight();
    Ogre::Real r = l + element->getWidth();
    Ogre::Real b = t + element->getHeight();

    return (cursorPos.x >= l + voidBorder && cursorPos.x <= r - voidBorder &&
            cursorPos.y >= t + voidBorder && cursorPos.y <= b - voidBorder);
}

auto Widget::cursorOffset(Ogre::OverlayElement *element, const Ogre::Vector2 &cursorPos) -> Ogre::Vector2
{
    Ogre::OverlayManager& om = Ogre::OverlayManager::getSingleton();
    return {cursorPos.x - (element->_getDerivedLeft() * om.getViewportWidth() + element->getWidth() / 2),
                         cursorPos.y - (element->_getDerivedTop() * om.getViewportHeight() + element->getHeight() / 2)};
}

auto Widget::getCaptionWidth(std::string_view caption, Ogre::TextAreaOverlayElement *area) -> Ogre::Real
{
    Ogre::FontPtr font = area->getFont();
    font->load(); // ensure glyph info is there
    Ogre::Real lineWidth = 0;

    for (char i : caption)
    {
        // be sure to provide a line width in the text area
        if (i == ' ')
        {
            if (area->getSpaceWidth() != 0) lineWidth += area->getSpaceWidth();
            else lineWidth += font->getGlyphInfo(' ').advance * area->getCharHeight();
        }
        else if (i == '\n') break;
        // use glyph information to calculate line width
        else lineWidth += font->getGlyphInfo(i).advance * area->getCharHeight();
    }

    return (unsigned int)lineWidth;
}

void Widget::fitCaptionToArea(std::string_view caption, Ogre::TextAreaOverlayElement *area, Ogre::Real maxWidth)
{
    Ogre::FontPtr f = area->getFont();
    f->load();
    auto s = caption;

    size_t nl = s.find('\n');
    if (nl != Ogre::String::npos) s = s.substr(0, nl);

    Ogre::Real width = 0;

    for (unsigned int i = 0; i < s.length(); i++)
    {
        if (s[i] == ' ' && area->getSpaceWidth() != 0) width += area->getSpaceWidth();
        else width += f->getGlyphInfo(s[i]).advance * area->getCharHeight();
        if (width > maxWidth)
        {
            s = s.substr(0, i);
            break;
        }
    }

    area->setCaption(s);
}

Button::Button(std::string_view name, std::string_view caption, Ogre::Real width)
{
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate("SdkTrays/Button", "BorderPanel", name);
    mBP = (Ogre::BorderPanelOverlayElement*)mElement;
    mTextArea = (Ogre::TextAreaOverlayElement*)mBP->getChild(::std::format("{}/ButtonCaption", mBP->getName()));

    mTextArea->setTop(-(mTextArea->getCharHeight() / 2));

    if (width > 0)
    {
        mElement->setWidth(width);
        mFitToContents = false;
    }
    else mFitToContents = true;

    setCaption(caption);
    mState = ButtonState::UP;
}

void Button::setCaption(std::string_view caption)
{
    mTextArea->setCaption(caption);
    if (mFitToContents) mElement->setWidth(getCaptionWidth(caption, mTextArea) + mElement->getHeight() - 12);
}

void Button::_cursorPressed(const Ogre::Vector2 &cursorPos)
{
    if (isCursorOver(mElement, cursorPos, 4)) setState(ButtonState::DOWN);
}

void Button::_cursorReleased(const Ogre::Vector2 &cursorPos)
{
    if (mState == ButtonState::DOWN)
    {
        setState(ButtonState::OVER);
        if (mListener) mListener->buttonHit(this);
    }
}

void Button::_cursorMoved(const Ogre::Vector2 &cursorPos, float wheelDelta)
{
    if (isCursorOver(mElement, cursorPos, 4))
    {
        if (mState == ButtonState::UP) setState(ButtonState::OVER);
    }
    else
    {
        if (mState != ButtonState::UP) setState(ButtonState::UP);
    }
}

void Button::_focusLost()
{
    setState(ButtonState::UP);   // reset button if cursor was lost
}

void Button::setState(const ButtonState &bs)
{
    if (bs == ButtonState::OVER)
    {
        mBP->setBorderMaterialName("SdkTrays/Button/Over");
        mBP->setMaterialName("SdkTrays/Button/Over");
    }
    else if (bs == ButtonState::UP)
    {
        mBP->setBorderMaterialName("SdkTrays/Button/Up");
        mBP->setMaterialName("SdkTrays/Button/Up");
    }
    else
    {
        mBP->setBorderMaterialName("SdkTrays/Button/Down");
        mBP->setMaterialName("SdkTrays/Button/Down");
    }

    mState = bs;
}

TextBox::TextBox(std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real height)
{
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate("SdkTrays/TextBox", "BorderPanel", name);
    mElement->setWidth(width);
    mElement->setHeight(height);
    auto* container = (Ogre::OverlayContainer*)mElement;
    mTextArea = (Ogre::TextAreaOverlayElement*)container->getChild(::std::format("{}/TextBoxText", getName()));
    mCaptionBar = (Ogre::BorderPanelOverlayElement*)container->getChild(::std::format("{}/TextBoxCaptionBar", getName()));
    mCaptionBar->setWidth(width - 4);
    mCaptionTextArea = (Ogre::TextAreaOverlayElement*)mCaptionBar->getChild(::std::format("{}/TextBoxCaption", mCaptionBar->getName()));
    setCaption(caption);
    mScrollTrack = (Ogre::BorderPanelOverlayElement*)container->getChild(::std::format("{}/TextBoxScrollTrack", getName()));
    mScrollHandle = (Ogre::PanelOverlayElement*)mScrollTrack->getChild(::std::format("{}/TextBoxScrollHandle", mScrollTrack->getName()));
    mScrollHandle->hide();
    mDragging = false;
    mScrollPercentage = 0;
    mStartingLine = 0;
    mPadding = 15;
    mText = "";

    refitContents();
}

void TextBox::setPadding(Ogre::Real padding)
{
    mPadding = padding;
    refitContents();
}

void TextBox::setText(std::string_view text)
{
    mText = text;
    mLines.clear();

    Ogre::FontPtr font = mTextArea->getFont();
    font->load(); // ensure glyph info is there

    Ogre::String current{ text };
    bool firstWord = true;
    unsigned int lastSpace = 0;
    unsigned int lineBegin = 0;
    Ogre::Real lineWidth = 0;
    Ogre::Real rightBoundary = mElement->getWidth() - 2 * mPadding + mScrollTrack->getLeft() + 10;

    for (unsigned int i = 0; i < current.length(); i++)
    {
        if (current[i] == ' ')
        {
            if (mTextArea->getSpaceWidth() != 0) lineWidth += mTextArea->getSpaceWidth();
            else lineWidth += font->getGlyphInfo(' ').advance * mTextArea->getCharHeight();
            firstWord = false;
            lastSpace = i;
        }
        else if (current[i] == '\n')
        {
            firstWord = true;
            lineWidth = 0;
            mLines.push_back(current.substr(lineBegin, i - lineBegin));
            lineBegin = i + 1;
        }
        else
        {
            // use glyph information to calculate line width
            lineWidth += font->getGlyphInfo(current[i]).advance * mTextArea->getCharHeight();
            if (lineWidth > rightBoundary)
            {
                if (firstWord)
                {
                    current.insert(i, "\n");
                    i = i - 1;
                }
                else
                {
                    current[lastSpace] = '\n';
                    i = lastSpace - 1;
                }
            }
        }
    }

    mLines.push_back(current.substr(lineBegin));

    unsigned int maxLines = getHeightInLines();

    if (mLines.size() > maxLines)     // if too much text, filter based on scroll percentage
    {
        mScrollHandle->show();
        filterLines();
    }
    else       // otherwise just show all the text
    {
        mTextArea->setCaption(current);
        mScrollHandle->hide();
        mScrollPercentage = 0;
        mScrollHandle->setTop(0);
    }
}

void TextBox::setTextAlignment(Ogre::TextAreaOverlayElement::Alignment ta)
{
    if (ta == Ogre::TextAreaOverlayElement::Alignment::Left) mTextArea->setHorizontalAlignment(Ogre::GuiHorizontalAlignment::LEFT);
    else if (ta == Ogre::TextAreaOverlayElement::Alignment::Center) mTextArea->setHorizontalAlignment(Ogre::GuiHorizontalAlignment::CENTER);
    else mTextArea->setHorizontalAlignment(Ogre::GuiHorizontalAlignment::RIGHT);
    refitContents();
}

void TextBox::refitContents()
{
    mScrollTrack->setHeight(mElement->getHeight() - mCaptionBar->getHeight() - 20);
    mScrollTrack->setTop(mCaptionBar->getHeight() + 10);

    mTextArea->setTop(mCaptionBar->getHeight() + mPadding - 5);
    if (mTextArea->getHorizontalAlignment() == Ogre::GuiHorizontalAlignment::RIGHT) mTextArea->setLeft(-mPadding + mScrollTrack->getLeft());
    else if (mTextArea->getHorizontalAlignment() == Ogre::GuiHorizontalAlignment::LEFT) mTextArea->setLeft(mPadding);
    else mTextArea->setLeft(mScrollTrack->getLeft() / 2);

    setText(getText());
}

void TextBox::setScrollPercentage(Ogre::Real percentage)
{
    mScrollPercentage = Ogre::Math::Clamp<Ogre::Real>(percentage, 0, 1);
    mScrollHandle->setTop((int)(percentage * (mScrollTrack->getHeight() - mScrollHandle->getHeight())));
    filterLines();
}

void TextBox::_cursorPressed(const Ogre::Vector2 &cursorPos)
{
    if (!mScrollHandle->isVisible()) return;   // don't care about clicks if text not scrollable

    Ogre::Vector2 co = Widget::cursorOffset(mScrollHandle, cursorPos);

    if (co.squaredLength() <= 81)
    {
        mDragging = true;
        mDragOffset = co.y;
    }
    else if (Widget::isCursorOver(mScrollTrack, cursorPos))
    {
        Ogre::Real newTop = mScrollHandle->getTop() + co.y;
        Ogre::Real lowerBoundary = mScrollTrack->getHeight() - mScrollHandle->getHeight();
        mScrollHandle->setTop(Ogre::Math::Clamp<int>((int)newTop, 0, (int)lowerBoundary));

        // update text area contents based on new scroll percentage
        mScrollPercentage = Ogre::Math::Clamp<Ogre::Real>(newTop / lowerBoundary, 0, 1);
        filterLines();
    }
}

void TextBox::_cursorMoved(const Ogre::Vector2 &cursorPos, float wheelDelta)
{
    if (mDragging)
    {
        Ogre::Vector2 co = Widget::cursorOffset(mScrollHandle, cursorPos);
        Ogre::Real newTop = mScrollHandle->getTop() + co.y - mDragOffset;
        Ogre::Real lowerBoundary = mScrollTrack->getHeight() - mScrollHandle->getHeight();
        mScrollHandle->setTop(Ogre::Math::Clamp<int>((int)newTop, 0, (int)lowerBoundary));

        // update text area contents based on new scroll percentage
        mScrollPercentage = Ogre::Math::Clamp<Ogre::Real>(newTop / lowerBoundary, 0, 1);
        filterLines();
    }
}

void TextBox::filterLines()
{
    Ogre::String shown = "";
    unsigned int maxLines = getHeightInLines();
    auto newStart = (unsigned int) (mScrollPercentage * (mLines.size() - maxLines) + 0.5);

    mStartingLine = newStart;

    for (unsigned int i = 0; i < maxLines; i++)
    {
        shown = ::std::format("{}{}\n", std::string_view{shown}, mLines[mStartingLine + i]);
    }

    mTextArea->setCaption(shown);    // show just the filtered lines
}

SelectMenu::SelectMenu(std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real boxWidth, size_t maxItemsShown)
     
{
    mSelectionIndex = -1;
    mFitToContents = false;
    mCursorOver = false;
    mExpanded = false;
    mDragging = false;
    mMaxItemsShown = maxItemsShown;
    mItemsShown = 0;
    mElement = (Ogre::BorderPanelOverlayElement*)Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate
            ("SdkTrays/SelectMenu", "BorderPanel", name);
    mTextArea = (Ogre::TextAreaOverlayElement*)((Ogre::OverlayContainer*)mElement)->getChild(::std::format("{}/MenuCaption", name));
    mSmallBox = (Ogre::BorderPanelOverlayElement*)((Ogre::OverlayContainer*)mElement)->getChild(::std::format("{}/MenuSmallBox", name));
    mSmallBox->setWidth(width - 10);
    mSmallTextArea = (Ogre::TextAreaOverlayElement*)mSmallBox->getChild(::std::format("{}/MenuSmallBox/MenuSmallText", name));
    mElement->setWidth(width);

    if (boxWidth > 0)  // long style
    {
        if (width <= 0) mFitToContents = true;
        mSmallBox->setWidth(boxWidth);
        mSmallBox->setTop(2);
        mSmallBox->setLeft(width - boxWidth - 5);
        mElement->setHeight(mSmallBox->getHeight() + 4);
        mTextArea->setHorizontalAlignment(Ogre::GuiHorizontalAlignment::LEFT);
        mTextArea->setAlignment(Ogre::TextAreaOverlayElement::Alignment::Left);
        mTextArea->setLeft(12);
        mTextArea->setTop(10);
    }

    mExpandedBox = (Ogre::BorderPanelOverlayElement*)((Ogre::OverlayContainer*)mElement)->getChild(::std::format("{}/MenuExpandedBox", name));
    mExpandedBox->setWidth(mSmallBox->getWidth() + 10);
    mExpandedBox->hide();
    mScrollTrack = (Ogre::BorderPanelOverlayElement*)mExpandedBox->getChild(::std::format("{}/MenuScrollTrack", mExpandedBox->getName()));
    mScrollHandle = (Ogre::PanelOverlayElement*)mScrollTrack->getChild(::std::format("{}/MenuScrollHandle", mScrollTrack->getName()));

    setCaption(caption);
}

void SelectMenu::copyItemsFrom(SelectMenu *other){
    const Ogre::StringVector& items = other->getItems();
    for(auto const& item : items){
        this->addItem(item);
    }
}

void SelectMenu::setCaption(std::string_view caption)
{
    mTextArea->setCaption(caption);
    if (mFitToContents)
    {
        mElement->setWidth(getCaptionWidth(caption, mTextArea) + mSmallBox->getWidth() + 23);
        mSmallBox->setLeft(mElement->getWidth() - mSmallBox->getWidth() - 5);
    }
}

void SelectMenu::setItems(const Ogre::StringVector &items)
{
    mItems = items;
    mSelectionIndex = -1;

    for (auto & mItemElement : mItemElements)   // destroy all the item elements
    {
        nukeOverlayElement(mItemElement);
    }
    mItemElements.clear();

    mItemsShown = Ogre::Math::Clamp<size_t>(mMaxItemsShown, 1, mItems.size());

    for (unsigned int i = 0; i < mItemsShown; i++)   // create all the item elements
    {
        Ogre::BorderPanelOverlayElement* e =
                (Ogre::BorderPanelOverlayElement*)Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate
                ("SdkTrays/SelectMenuItem", "BorderPanel",
                 ::std::format("{}/Item{}", mExpandedBox->getName(), i + 1));

        e->setTop(6 + i * (mSmallBox->getHeight() - 8));
        e->setWidth(mExpandedBox->getWidth() - 32);

        mExpandedBox->addChild(e);
        mItemElements.push_back(e);
    }

    if (!items.empty()) selectItem(0, false);
    else mSmallTextArea->setCaption("");
}

void SelectMenu::removeItem(std::string_view item)
{
    for (size_t i=0; i < mItems.size(); i++)
    {
        if (item == mItems[i]) {
            removeItem(static_cast<unsigned int>(i));
            i--; // check again same index
        }
    }
}

void SelectMenu::removeItem(size_t index)
{
    if(index >= mItems.size()){
        Ogre::String desc = ::std::format("Menu \"{}\" contains no item at position {}.", getName(), index );
        OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "SelectMenu::removeItem");
    }
    mItems.erase(mItems.begin() + index);

    if (mItems.size() < mItemsShown)
    {
        mItemsShown = static_cast<unsigned int>(mItems.size());
        nukeOverlayElement(mItemElements.back());
        mItemElements.pop_back();
    }
    if((size_t)mSelectionIndex == index){
        if(index < mItems.size()) {
            // update the text of the menu
            selectItem(index);
        } else if (!mItems.empty()){
            // last item was selected and we removed it
            selectItem(index - 1);
        } else {
            // we had only 1 item, but we removed it, so clear caption
            mSmallTextArea->setCaption("");
        }
    }
}

void SelectMenu::clearItems()
{
    mItems.clear();
    mSelectionIndex = -1;
    mSmallTextArea->setCaption("");
}

void SelectMenu::selectItem(size_t index, bool notifyListener)
{
    if (index >= mItems.size())
    {
        Ogre::String desc = ::std::format("Menu \"{}\" contains no item at position {}.", getName() , index);
        OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "SelectMenu::selectItem");
    }

    mSelectionIndex = (int)index;
    fitCaptionToArea(mItems[index], mSmallTextArea, mSmallBox->getWidth() - mSmallTextArea->getLeft() * 2);

    if (mListener && notifyListener) mListener->itemSelected(this);
}

auto SelectMenu::containsItem(std::string_view item) -> bool
{
    bool res = false;
    for (auto & mItem : mItems)
    {
        if (item == mItem)
        {
            res = true;
            break;
        }
    }

    return res;
}

void SelectMenu::selectItem(std::string_view item, bool notifyListener)
{
    for (unsigned int i = 0; i < mItems.size(); i++)
    {
        if (item == mItems[i])
        {
            selectItem(i, notifyListener);
            return;
        }
    }

    Ogre::String desc = ::std::format(R"(Menu "{}" contains no item "{}".)", getName() , item );
    OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "SelectMenu::selectItem");
}

auto SelectMenu::getSelectedItem() -> Ogre::DisplayString
{
    if (mSelectionIndex == -1)
    {
        Ogre::String desc = ::std::format("Menu \"{}\" has no item selected.", getName());
        OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "SelectMenu::getSelectedItem");
        return "";
    }
    else return mItems[mSelectionIndex];
}

void SelectMenu::_cursorPressed(const Ogre::Vector2 &cursorPos)
{
    Ogre::OverlayManager& om = Ogre::OverlayManager::getSingleton();

    if (mExpanded)
    {
        if (mScrollHandle->isVisible())   // check for scrolling
        {
            Ogre::Vector2 co = Widget::cursorOffset(mScrollHandle, cursorPos);

            if (co.squaredLength() <= 81)
            {
                mDragging = true;
                mDragOffset = co.y;
                return;
            }
            else if (Widget::isCursorOver(mScrollTrack, cursorPos))
            {
                Ogre::Real newTop = mScrollHandle->getTop() + co.y;
                Ogre::Real lowerBoundary = mScrollTrack->getHeight() - mScrollHandle->getHeight();
                mScrollHandle->setTop(Ogre::Math::Clamp<int>((int)newTop, 0, (int)lowerBoundary));

                auto scrollPercentage = Ogre::Math::Clamp<Ogre::Real>(newTop / lowerBoundary, 0, 1);
                setDisplayIndex((unsigned int)(scrollPercentage * (mItems.size() - mItemElements.size()) + 0.5));
                return;
            }
        }

        if (!isCursorOver(mExpandedBox, cursorPos, 3)) retract();
        else
        {
            Ogre::Real l = mItemElements.front()->_getDerivedLeft() * om.getViewportWidth() + 5;
            Ogre::Real t = mItemElements.front()->_getDerivedTop() * om.getViewportHeight() + 5;
            Ogre::Real r = l + mItemElements.back()->getWidth() - 10;
            Ogre::Real b = mItemElements.back()->_getDerivedTop() * om.getViewportHeight() +
                    mItemElements.back()->getHeight() - 5;

            if (cursorPos.x >= l && cursorPos.x <= r && cursorPos.y >= t && cursorPos.y <= b)
            {
                if (mHighlightIndex != mSelectionIndex) selectItem(mHighlightIndex);
                retract();
            }
        }
    }
    else
    {
        if (mItems.size() < 2) return;   // don't waste time showing a menu if there's no choice

        if (isCursorOver(mSmallBox, cursorPos, 4))
        {
            mExpandedBox->show();
            mSmallBox->hide();

            // calculate how much vertical space we need
            Ogre::Real idealHeight = mItemsShown * (mSmallBox->getHeight() - 8) + 20;
            mExpandedBox->setHeight(idealHeight);
            mScrollTrack->setHeight(mExpandedBox->getHeight() - 20);

            mExpandedBox->setLeft(mSmallBox->getLeft() - 4);

            // if the expanded menu goes down off the screen, make it go up instead
            if (mSmallBox->_getDerivedTop() * om.getViewportHeight() + idealHeight > om.getViewportHeight())
            {
                mExpandedBox->setTop(mSmallBox->getTop() + mSmallBox->getHeight() - idealHeight + 3);
                // if we're in thick style, hide the caption because it will interfere with the expanded menu
                if (mTextArea->getHorizontalAlignment() == Ogre::GuiHorizontalAlignment::CENTER) mTextArea->hide();
            }
            else mExpandedBox->setTop(mSmallBox->getTop() + 3);

            mExpanded = true;
            mHighlightIndex = mSelectionIndex;
            setDisplayIndex(mHighlightIndex);

            if (mItemsShown < mItems.size())  // update scrollbar position
            {
                mScrollHandle->show();
                Ogre::Real lowerBoundary = mScrollTrack->getHeight() - mScrollHandle->getHeight();
                mScrollHandle->setTop((int)(mDisplayIndex * lowerBoundary / (mItems.size() - mItemElements.size())));
            }
            else mScrollHandle->hide();
        }
    }
}

void SelectMenu::_cursorMoved(const Ogre::Vector2 &cursorPos, float wheelDelta)
{
    Ogre::OverlayManager& om = Ogre::OverlayManager::getSingleton();

    if (mExpanded)
    {
        if (mDragging)
        {
            Ogre::Vector2 co = Widget::cursorOffset(mScrollHandle, cursorPos);
            Ogre::Real newTop = mScrollHandle->getTop() + co.y - mDragOffset;
            Ogre::Real lowerBoundary = mScrollTrack->getHeight() - mScrollHandle->getHeight();
            mScrollHandle->setTop(Ogre::Math::Clamp<int>((int)newTop, 0, (int)lowerBoundary));

            auto scrollPercentage = Ogre::Math::Clamp<Ogre::Real>(newTop / lowerBoundary, 0, 1);
            int newIndex = (int) (scrollPercentage * (mItems.size() - mItemElements.size()) + 0.5);
            if (newIndex != mDisplayIndex) setDisplayIndex(newIndex);
            return;
        }
        else if(fabsf(wheelDelta) > 0.5f)
        {
            int newIndex = Ogre::Math::Clamp<int>(mDisplayIndex + (wheelDelta > 0 ? -1 : 1), 0, (int)(mItems.size() - mItemElements.size()));
            Ogre::Real lowerBoundary = mScrollTrack->getHeight() - mScrollHandle->getHeight();
            mScrollHandle->setTop((int)(newIndex * lowerBoundary / (mItems.size() - mItemElements.size())));
            setDisplayIndex(newIndex);
        }

        Ogre::Real l = mItemElements.front()->_getDerivedLeft() * om.getViewportWidth() + 5;
        Ogre::Real t = mItemElements.front()->_getDerivedTop() * om.getViewportHeight() + 5;
        Ogre::Real r = l + mItemElements.back()->getWidth() - 10;
        Ogre::Real b = mItemElements.back()->_getDerivedTop() * om.getViewportHeight() +
                mItemElements.back()->getHeight() - 5;

        if (cursorPos.x >= l && cursorPos.x <= r && cursorPos.y >= t && cursorPos.y <= b)
        {
            int newIndex = (int)(mDisplayIndex + (cursorPos.y - t) / (b - t) * mItemElements.size());
            if (mHighlightIndex != newIndex)
            {
                mHighlightIndex = newIndex;
                setDisplayIndex(mDisplayIndex);
            }
        }
    }
    else
    {
        if (isCursorOver(mSmallBox, cursorPos, 4))
        {
            mSmallBox->setMaterialName("SdkTrays/MiniTextBox/Over");
            mSmallBox->setBorderMaterialName("SdkTrays/MiniTextBox/Over");
            mCursorOver = true;
        }
        else
        {
            if (mCursorOver)
            {
                mSmallBox->setMaterialName("SdkTrays/MiniTextBox");
                mSmallBox->setBorderMaterialName("SdkTrays/MiniTextBox");
                mCursorOver = false;
            }
        }
    }
}

void SelectMenu::setDisplayIndex(unsigned int index)
{
    index = std::min<int>(index, (int)(mItems.size() - mItemElements.size()));
    mDisplayIndex = index;

    for (int i = 0; i < (int)mItemElements.size(); i++)
    {
        Ogre::BorderPanelOverlayElement *ie = mItemElements[i];
        Ogre::TextAreaOverlayElement *ta = (Ogre::TextAreaOverlayElement*)ie->getChild(::std::format("{}/MenuItemText", ie->getName()));

        fitCaptionToArea(mItems[mDisplayIndex + i], ta, ie->getWidth() - 2 * ta->getLeft());

        if ((mDisplayIndex + i) == mHighlightIndex)
        {
            ie->setMaterialName("SdkTrays/MiniTextBox/Over");
            ie->setBorderMaterialName("SdkTrays/MiniTextBox/Over");
        }
        else
        {
            ie->setMaterialName("SdkTrays/MiniTextBox");
            ie->setBorderMaterialName("SdkTrays/MiniTextBox");
        }
    }
}

void SelectMenu::retract()
{
    mDragging = false;
    mExpanded = false;
    mExpandedBox->hide();
    mTextArea->show();
    mSmallBox->show();
    mSmallBox->setMaterialName("SdkTrays/MiniTextBox");
    mSmallBox->setBorderMaterialName("SdkTrays/MiniTextBox");
}

Label::Label(std::string_view name, std::string_view caption, Ogre::Real width)
{
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate("SdkTrays/Label", "BorderPanel", name);
    mTextArea = (Ogre::TextAreaOverlayElement*)((Ogre::OverlayContainer*)mElement)->getChild(::std::format("{}/LabelCaption", getName()));

    setCaption(caption);
    if (width <= 0) mFitToTray = true;
    else
    {
        mFitToTray = false;
        mElement->setWidth(width);
    }
}

void Label::_cursorPressed(const Ogre::Vector2 &cursorPos)
{
    if (mListener && isCursorOver(mElement, cursorPos, 3)) mListener->labelHit(this);
}

Separator::Separator(std::string_view name, Ogre::Real width)
{
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate("SdkTrays/Separator", "Panel", name);
    if (width <= 0) mFitToTray = true;
    else
    {
        mFitToTray = false;
        mElement->setWidth(width);
    }
}

Slider::Slider(std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real trackWidth, Ogre::Real valueBoxWidth, Ogre::Real minValue, Ogre::Real maxValue, unsigned int snaps)
     
{
    mDragging = false;
    mFitToContents = false;
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate
            ("SdkTrays/Slider", "BorderPanel", name);
    mElement->setWidth(width);
    auto* c = (Ogre::OverlayContainer*)mElement;
    mTextArea = (Ogre::TextAreaOverlayElement*)c->getChild(::std::format("{}/SliderCaption", getName()));
    Ogre::OverlayContainer* valueBox = (Ogre::OverlayContainer*)c->getChild(::std::format("{}/SliderValueBox", getName()));
    valueBox->setWidth(valueBoxWidth);
    valueBox->setLeft(-(valueBoxWidth + 5));
    mValueTextArea = (Ogre::TextAreaOverlayElement*)valueBox->getChild(::std::format("{}/SliderValueText", valueBox->getName()));
    mTrack = (Ogre::BorderPanelOverlayElement*)c->getChild(::std::format("{}/SliderTrack", getName()));
    mHandle = (Ogre::PanelOverlayElement*)mTrack->getChild(::std::format("{}/SliderHandle", mTrack->getName()));

    if (trackWidth <= 0)  // tall style
    {
        mTrack->setWidth(width - 16);
    }
    else  // long style
    {
        if (width <= 0) mFitToContents = true;
        mElement->setHeight(34);
        mTextArea->setTop(10);
        valueBox->setTop(2);
        mTrack->setTop(-23);
        mTrack->setWidth(trackWidth);
        mTrack->setHorizontalAlignment(Ogre::GuiHorizontalAlignment::RIGHT);
        mTrack->setLeft(-(trackWidth + valueBoxWidth + 5));
    }

    setCaption(caption);
    setRange(minValue, maxValue, snaps, false);
}

void Slider::setRange(Ogre::Real minValue, Ogre::Real maxValue, unsigned int snaps, bool notifyListener)
{
    mMinValue = minValue;
    mMaxValue = maxValue;

    if (snaps <= 1 || mMinValue >= mMaxValue)
    {
        mInterval = 0;
        mHandle->hide();
        mValue = minValue;
        if (snaps == 1) mValueTextArea->setCaption(Ogre::StringConverter::toString(mMinValue));
        else mValueTextArea->setCaption("");
    }
    else
    {
        mHandle->show();
        mInterval = (maxValue - minValue) / (snaps - 1);
        setValue(minValue, notifyListener);
    }
}

void Slider::setValue(Ogre::Real value, bool notifyListener)
{
    if (mInterval == 0) return;

    mValue = Ogre::Math::Clamp<Ogre::Real>(value, mMinValue, mMaxValue);

    setValueCaption(Ogre::StringConverter::toString(mValue));

    if (mListener && notifyListener) mListener->sliderMoved(this);

    if (!mDragging) mHandle->setLeft((int)((mValue - mMinValue) / (mMaxValue - mMinValue) *
                                           (mTrack->getWidth() - mHandle->getWidth())));
}

void Slider::setCaption(std::string_view caption)
{
    mTextArea->setCaption(caption);

    if (mFitToContents) mElement->setWidth(getCaptionWidth(caption, mTextArea) +
                                           mValueTextArea->getParent()->getWidth() + mTrack->getWidth() + 26);
}

void Slider::_cursorPressed(const Ogre::Vector2 &cursorPos)
{
    if (!mHandle->isVisible()) return;

    Ogre::Vector2 co = Widget::cursorOffset(mHandle, cursorPos);

    if (co.squaredLength() <= 81)
    {
        mDragging = true;
        mDragOffset = co.x;
    }
    else if (Widget::isCursorOver(mTrack, cursorPos))
    {
        Ogre::Real newLeft = mHandle->getLeft() + co.x;
        Ogre::Real rightBoundary = mTrack->getWidth() - mHandle->getWidth();

        mHandle->setLeft(Ogre::Math::Clamp<int>((int)newLeft, 0, (int)rightBoundary));
        setValue(getSnappedValue(newLeft / rightBoundary));
    }
}

void Slider::_cursorReleased(const Ogre::Vector2 &cursorPos)
{
    if (mDragging)
    {
        mDragging = false;
        mHandle->setLeft((int)((mValue - mMinValue) / (mMaxValue - mMinValue) *
                               (mTrack->getWidth() - mHandle->getWidth())));
    }
}

void Slider::_cursorMoved(const Ogre::Vector2 &cursorPos, float wheelDelta)
{
    if (mDragging)
    {
        Ogre::Vector2 co = Widget::cursorOffset(mHandle, cursorPos);
        Ogre::Real newLeft = mHandle->getLeft() + co.x - mDragOffset;
        Ogre::Real rightBoundary = mTrack->getWidth() - mHandle->getWidth();

        mHandle->setLeft(Ogre::Math::Clamp<int>((int)newLeft, 0, (int)rightBoundary));
        setValue(getSnappedValue(newLeft / rightBoundary));

        // sync mHandle's mPixelLeft with mLeft
        // if multiple "mouseMoved" happened during one frame, mLeft could be incorrect otherwise
        mHandle->_update();
    }
}

ParamsPanel::ParamsPanel(std::string_view name, Ogre::Real width, unsigned int lines)
{
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate
            ("SdkTrays/ParamsPanel", "BorderPanel", name);
    auto* c = (Ogre::OverlayContainer*)mElement;
    mNamesArea = (Ogre::TextAreaOverlayElement*)c->getChild(::std::format("{}/ParamsPanelNames", getName()));
    mValuesArea = (Ogre::TextAreaOverlayElement*)c->getChild(::std::format("{}/ParamsPanelValues", getName()));

    mElement->setWidth(width);
    mElement->setHeight(mNamesArea->getTop() * 2 + lines * mNamesArea->getCharHeight());
}

void ParamsPanel::setAllParamNames(const Ogre::StringVector &paramNames)
{
    mNames = paramNames;
    mValues.clear();
    mValues.resize(mNames.size(), "");
    mElement->setHeight(mNamesArea->getTop() * 2 + mNames.size() * mNamesArea->getCharHeight());
    updateText();
}

void ParamsPanel::setAllParamValues(const Ogre::StringVector &paramValues)
{
    mValues = paramValues;
    mValues.resize(mNames.size(), "");
    updateText();
}

void ParamsPanel::setParamValue(std::string_view paramName, std::string_view paramValue)
{
    for (unsigned int i = 0; i < mNames.size(); i++)
    {
        if (mNames[i] == paramName)
        {
            mValues[i] = paramValue;
            updateText();
            return;
        }
    }

    Ogre::String desc = ::std::format(R"(ParamsPanel "{}" has no parameter "{}".)", getName(), paramName );
    OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "ParamsPanel::setParamValue");
}

void ParamsPanel::setParamValue(unsigned int index, std::string_view paramValue)
{
    if (index >= mNames.size())
    {
        Ogre::String desc = ::std::format("ParamsPanel \"{}\" has no parameter at position {}.", getName() , index);
        OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "ParamsPanel::setParamValue");
    }

    mValues[index] = paramValue;
    updateText();
}

auto ParamsPanel::getParamValue(std::string_view paramName) -> Ogre::DisplayString
{
    for (unsigned int i = 0; i < mNames.size(); i++)
    {
        if (mNames[i] == paramName) return mValues[i];
    }

    Ogre::String desc = ::std::format(R"(ParamsPanel "{}" has no parameter "{}".)", getName() , paramName);
    OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "ParamsPanel::getParamValue");
    return "";
}

auto ParamsPanel::getParamValue(unsigned int index) -> Ogre::DisplayString
{
    if (index >= mNames.size())
    {
        Ogre::String desc = ::std::format("ParamsPanel \"{}\" has no parameter at position {}.", getName(),index);
        OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, desc, "ParamsPanel::getParamValue");
    }

    return mValues[index];
}

void ParamsPanel::updateText()
{
    Ogre::DisplayString namesDS;
    Ogre::DisplayString valuesDS;

    for (unsigned int i = 0; i < mNames.size(); i++)
    {
        namesDS.append(::std::format("{}:\n", mNames[i]));
        valuesDS.append(::std::format("{}\n", mValues[i]));
    }

    mNamesArea->setCaption(namesDS);
    mValuesArea->setCaption(valuesDS);
}

CheckBox::CheckBox(std::string_view name, std::string_view caption, Ogre::Real width)
{
    mCursorOver = false;
    mFitToContents = width <= 0;
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate
            ("SdkTrays/CheckBox", "BorderPanel", name);
    auto* c = (Ogre::OverlayContainer*)mElement;
    mTextArea = (Ogre::TextAreaOverlayElement*)c->getChild(::std::format("{}/CheckBoxCaption", getName()));
    mSquare = (Ogre::BorderPanelOverlayElement*)c->getChild(::std::format("{}/CheckBoxSquare", getName()));
    mX = mSquare->getChild(::std::format("{}/CheckBoxX", mSquare->getName()));
    mX->hide();
    mElement->setWidth(width);

    setCaption(caption);
}

void CheckBox::setCaption(std::string_view caption)
{
    mTextArea->setCaption(caption);
    if (mFitToContents) mElement->setWidth(getCaptionWidth(caption, mTextArea) + mSquare->getWidth() + 23);
}

void CheckBox::setChecked(bool checked, bool notifyListener)
{
    if (checked) mX->show();
    else mX->hide();
    if (mListener && notifyListener) mListener->checkBoxToggled(this);
}

void CheckBox::toggle(bool notifyListener)
{
    setChecked(!isChecked(), notifyListener);
}

void CheckBox::_cursorPressed(const Ogre::Vector2 &cursorPos)
{
    if (mCursorOver && mListener) toggle();
}

void CheckBox::_cursorMoved(const Ogre::Vector2 &cursorPos, float wheelDelta)
{
    if (isCursorOver(mSquare, cursorPos, 5))
    {
        if (!mCursorOver)
        {
            mCursorOver = true;
            mSquare->setMaterialName("SdkTrays/MiniTextBox/Over");
            mSquare->setBorderMaterialName("SdkTrays/MiniTextBox/Over");
        }
    }
    else
    {
        if (mCursorOver)
        {
            mCursorOver = false;
            mSquare->setMaterialName("SdkTrays/MiniTextBox");
            mSquare->setBorderMaterialName("SdkTrays/MiniTextBox");
        }
    }
}

void CheckBox::_focusLost()
{
    mSquare->setMaterialName("SdkTrays/MiniTextBox");
    mSquare->setBorderMaterialName("SdkTrays/MiniTextBox");
    mCursorOver = false;
}

DecorWidget::DecorWidget(std::string_view name, std::string_view templateName)
{
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate(templateName, "", name);
}

ProgressBar::ProgressBar(std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real commentBoxWidth)
     
{
    mElement = Ogre::OverlayManager::getSingleton().createOverlayElementFromTemplate
            ("SdkTrays/ProgressBar", "BorderPanel", name);
    mElement->setWidth(width);
    auto* c = (Ogre::OverlayContainer*)mElement;
    mTextArea = (Ogre::TextAreaOverlayElement*)c->getChild(::std::format("{}/ProgressCaption", getName()));
    Ogre::OverlayContainer* commentBox = (Ogre::OverlayContainer*)c->getChild(::std::format("{}/ProgressCommentBox", getName()));
    commentBox->setWidth(commentBoxWidth);
    commentBox->setLeft(-(commentBoxWidth + 5));
    mCommentTextArea = (Ogre::TextAreaOverlayElement*)commentBox->getChild(::std::format("{}/ProgressCommentText", commentBox->getName()));
    mMeter = c->getChild(::std::format("{}/ProgressMeter", getName()));
    mMeter->setWidth(width - 10);
    mFill = ((Ogre::OverlayContainer*)mMeter)->getChild(::std::format("{}/ProgressFill", mMeter->getName()));
    setCaption(caption);
}

void ProgressBar::setProgress(Ogre::Real progress)
{
    mProgress = Ogre::Math::Clamp<Ogre::Real>(progress, 0, 1);
    mFill->setWidth(std::max<int>((int)mFill->getHeight(), (int)(mProgress * (mMeter->getWidth() - 2 * mFill->getLeft()))));
}

TrayManager::TrayManager(std::string_view name, Ogre::RenderWindow *window, TrayListener *listener) :
    mName(name), mWindow(window), mWidgetDeathRow(), mListener(listener)
{
    mTimer = Ogre::Root::getSingleton().getTimer();
    mLastStatUpdateTime = -FRAME_UPDATE_DELAY; // update immediately on first call

    Ogre::OverlayManager& om = Ogre::OverlayManager::getSingleton();

    Ogre::String nameBase = ::std::format("{}/", mName);
    std::ranges::replace(nameBase, ' ', '_');

    // create overlay layers for everything
    mBackdropLayer = om.create(::std::format("{}BackdropLayer", nameBase));
    mTraysLayer = om.create(::std::format("{}WidgetsLayer", nameBase));
    mPriorityLayer = om.create(::std::format("{}PriorityLayer", nameBase));
    mCursorLayer = om.create(::std::format("{}CursorLayer", nameBase));
    mBackdropLayer->setZOrder(100);
    mTraysLayer->setZOrder(200);
    mPriorityLayer->setZOrder(300);
    mCursorLayer->setZOrder(400);

    // make backdrop and cursor overlay containers
    mCursor = (Ogre::OverlayContainer*)om.createOverlayElementFromTemplate("SdkTrays/Cursor", "Panel", ::std::format("{}Cursor", nameBase));
    mCursorLayer->add2D(mCursor);
    mBackdrop = (Ogre::OverlayContainer*)om.createOverlayElement("Panel", ::std::format("{}Backdrop", nameBase));
    mBackdropLayer->add2D(mBackdrop);
    mDialogShade = (Ogre::OverlayContainer*)om.createOverlayElement("Panel", ::std::format("{}DialogShade", nameBase));
    mDialogShade->setMaterialName("SdkTrays/Shade");
    mDialogShade->hide();
    mPriorityLayer->add2D(mDialogShade);

    Ogre::String trayNames[] =
    { "TopLeft", "Top", "TopRight", "Left", "Center", "Right", "BottomLeft", "Bottom", "BottomRight" };

    for (unsigned int i = 0; i < 9; i++)    // make the real trays
    {
        mTrays[i] = (Ogre::OverlayContainer*)om.createOverlayElementFromTemplate
                ("SdkTrays/Tray", "BorderPanel", ::std::format("{}{}Tray", nameBase , trayNames[i]));
        mTraysLayer->add2D(mTrays[i]);

        mTrayWidgetAlign[i] = Ogre::GuiHorizontalAlignment::CENTER;

        auto const tl = static_cast<TrayLocation>(i);
        // align trays based on location
        if (tl == TrayLocation::TOP || tl == TrayLocation::CENTER || tl == TrayLocation::BOTTOM) mTrays[i]->setHorizontalAlignment(Ogre::GuiHorizontalAlignment::CENTER);
        if (tl == TrayLocation::LEFT || tl == TrayLocation::CENTER || tl == TrayLocation::RIGHT) mTrays[i]->setVerticalAlignment(Ogre::GuiVerticalAlignment::CENTER);
        if (tl == TrayLocation::TOPRIGHT || tl == TrayLocation::RIGHT || tl == TrayLocation::BOTTOMRIGHT) mTrays[i]->setHorizontalAlignment(Ogre::GuiHorizontalAlignment::RIGHT);
        if (tl == TrayLocation::BOTTOMLEFT || tl == TrayLocation::BOTTOM || tl == TrayLocation::BOTTOMRIGHT) mTrays[i]->setVerticalAlignment(Ogre::GuiVerticalAlignment::BOTTOM);
    }

    // create the null tray for free-floating widgets
    mTrays[9] = (Ogre::OverlayContainer*)om.createOverlayElement("Panel", ::std::format("{}NullTray", nameBase));
    mTrayWidgetAlign[9] = Ogre::GuiHorizontalAlignment::LEFT;
    mTraysLayer->add2D(mTrays[9]);
    adjustTrays();

    showTrays();
    showCursor();
}

TrayManager::~TrayManager()
{
    Ogre::OverlayManager& om = Ogre::OverlayManager::getSingleton();

    destroyAllWidgets();

    mWidgetDeathRow.clear();

    om.destroy(mBackdropLayer);
    om.destroy(mTraysLayer);
    om.destroy(mPriorityLayer);
    om.destroy(mCursorLayer);

    closeDialog();

    Widget::nukeOverlayElement(mBackdrop);
    Widget::nukeOverlayElement(mCursor);
    Widget::nukeOverlayElement(mDialogShade);

    for (auto & mTray : mTrays)
    {
        Widget::nukeOverlayElement(mTray);
    }
}

auto TrayManager::screenToScene(Ogre::Camera *cam, const Ogre::Vector2 &pt) -> Ogre::Ray
{
    return cam->getCameraToViewportRay(pt.x, pt.y);
}

auto TrayManager::sceneToScreen(Ogre::Camera *cam, const Ogre::Vector3 &pt) -> Ogre::Vector2
{
    Ogre::Vector3 result = cam->getProjectionMatrix() * cam->getViewMatrix() * pt;
    return {(result.x + 1) / 2, (-result.y + 1) / 2};
}

void TrayManager::showAll()
{
    showBackdrop();
    showTrays();
    showCursor();
}

void TrayManager::hideAll()
{
    hideBackdrop();
    hideTrays();
    hideCursor();
}

void TrayManager::showBackdrop(std::string_view materialName)
{
    if (!materialName.empty()) mBackdrop->setMaterialName(materialName);
    mBackdropLayer->show();
}

void TrayManager::showCursor(std::string_view materialName)
{
    if (!materialName.empty()) getCursorImage()->setMaterialName(materialName);

    if (!mCursorLayer->isVisible())
    {
        mCursorLayer->show();
        refreshCursor();
    }
}

void TrayManager::hideCursor()
{
    mCursorLayer->hide();

    // give widgets a chance to reset in case they're in the middle of something
    for (auto & mWidget : mWidgets)
    {
        for (auto & j : mWidget)
        {
            j->_focusLost();
        }
    }

    setExpandedMenu(nullptr);
}

void TrayManager::refreshCursor()
{
    mCursor->setPosition(mCursorPos.x, mCursorPos.y);
}

void TrayManager::showTrays()
{
    mTraysLayer->show();
    mPriorityLayer->show();
}

void TrayManager::hideTrays()
{
    mTraysLayer->hide();
    mPriorityLayer->hide();

    // give widgets a chance to reset in case they're in the middle of something
    for (auto & mWidget : mWidgets)
    {
        for (auto & j : mWidget)
        {
            j->_focusLost();
        }
    }

    setExpandedMenu(nullptr);
}

void TrayManager::setTrayWidgetAlignment(TrayLocation trayLoc, Ogre::GuiHorizontalAlignment gha)
{
    mTrayWidgetAlign[std::to_underlying(trayLoc)] = gha;

    for (auto & i : mWidgets[std::to_underlying(trayLoc)])
    {
        i->getOverlayElement()->setHorizontalAlignment(gha);
    }
}

void TrayManager::setWidgetPadding(Ogre::Real padding)
{
    mWidgetPadding = std::max<int>((int)padding, 0);
    adjustTrays();
}

void TrayManager::setWidgetSpacing(Ogre::Real spacing)
{
    mWidgetSpacing = std::max<int>((int)spacing, 0);
    adjustTrays();
}

void TrayManager::setTrayPadding(Ogre::Real padding)
{
    mTrayPadding = std::max<int>((int)padding, 0);
    adjustTrays();
}

void TrayManager::adjustTrays()
{
    for (unsigned int i = 0; i < 9; i++)   // resizes and hides trays if necessary
    {
        Ogre::Real trayWidth = 0;
        Ogre::Real trayHeight = mWidgetPadding;
        std::vector<Ogre::OverlayElement*> labelsAndSeps;

        if (mWidgets[i].empty())   // hide tray if empty
        {
            mTrays[i]->hide();
            continue;
        }
        else mTrays[i]->show();

        // arrange widgets and calculate final tray size and position
        for (unsigned int j = 0; j < mWidgets[i].size(); j++)
        {
            Ogre::OverlayElement* e = mWidgets[i][j]->getOverlayElement();

            if (j != 0) trayHeight += mWidgetSpacing;   // don't space first widget

            e->setVerticalAlignment(Ogre::GuiVerticalAlignment::TOP);
            e->setTop(trayHeight);
            using enum Ogre::GuiHorizontalAlignment;
            switch (e->getHorizontalAlignment())
            {
            case LEFT:
                e->setLeft(mWidgetPadding);
                break;
            case RIGHT:
                e->setLeft(-(e->getWidth() + mWidgetPadding));
                break;
            default:
                e->setLeft(-(e->getWidth() / 2));
            }

            // prevents some weird texture filtering problems (just some)
            e->setPosition((int)e->getLeft(), (int)e->getTop());
            e->setDimensions((int)e->getWidth(), (int)e->getHeight());

            trayHeight += e->getHeight();

            auto* l = dynamic_cast<Label*>(mWidgets[i][j]);
            if (l && l->_isFitToTray())
            {
                labelsAndSeps.push_back(e);
                continue;
            }
            auto* s = dynamic_cast<Separator*>(mWidgets[i][j]);
            if (s && s->_isFitToTray())
            {
                labelsAndSeps.push_back(e);
                continue;
            }

            if (e->getWidth() > trayWidth) trayWidth = e->getWidth();
        }

        // add paddings and resize trays
        mTrays[i]->setWidth(trayWidth + 2 * mWidgetPadding);
        mTrays[i]->setHeight(trayHeight + mWidgetPadding);

        for (auto & labelsAndSep : labelsAndSeps)
        {
            labelsAndSep->setWidth((int)trayWidth);
            labelsAndSep->setLeft(-(int)(trayWidth / 2));
        }
    }

    for (unsigned int i = 0; i < 9; i++)    // snap trays to anchors
    {
        auto const tl = static_cast<TrayLocation>(i);
        if (tl == TrayLocation::TOPLEFT || tl == TrayLocation::LEFT || tl == TrayLocation::BOTTOMLEFT)
            mTrays[i]->setLeft(mTrayPadding);
        if (tl == TrayLocation::TOP || tl == TrayLocation::CENTER || tl == TrayLocation::BOTTOM)
            mTrays[i]->setLeft(-mTrays[i]->getWidth() / 2);
        if (tl == TrayLocation::TOPRIGHT || tl == TrayLocation::RIGHT || tl == TrayLocation::BOTTOMRIGHT)
            mTrays[i]->setLeft(-(mTrays[i]->getWidth() + mTrayPadding));

        if (tl == TrayLocation::TOPLEFT || tl == TrayLocation::TOP || tl == TrayLocation::TOPRIGHT)
            mTrays[i]->setTop(mTrayPadding);
        if (tl == TrayLocation::LEFT || tl == TrayLocation::CENTER || tl == TrayLocation::RIGHT)
            mTrays[i]->setTop(-mTrays[i]->getHeight() / 2);
        if (tl == TrayLocation::BOTTOMLEFT || tl == TrayLocation::BOTTOM || tl == TrayLocation::BOTTOMRIGHT)
            mTrays[i]->setTop(-mTrays[i]->getHeight() - mTrayPadding);

        // prevents some weird texture filtering problems (just some)
        mTrays[i]->setPosition((int)mTrays[i]->getLeft(), (int)mTrays[i]->getTop());
        mTrays[i]->setDimensions((int)mTrays[i]->getWidth(), (int)mTrays[i]->getHeight());
    }
}

auto TrayManager::getCursorRay(Ogre::Camera *cam) -> Ogre::Ray
{
    return screenToScene(cam, Ogre::Vector2{mCursor->_getLeft(), mCursor->_getTop()});
}

auto TrayManager::createButton(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width) -> Button *
{
    auto* b = new Button(name, caption, width);
    moveWidgetToTray(b, trayLoc);
    b->_assignListener(mListener);
    return b;
}

auto TrayManager::createTextBox(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real height) -> TextBox *
{
    auto* tb = new TextBox(name, caption, width, height);
    moveWidgetToTray(tb, trayLoc);
    tb->_assignListener(mListener);
    return tb;
}

auto TrayManager::createThickSelectMenu(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width, unsigned int maxItemsShown, const Ogre::StringVector &items) -> SelectMenu *
{
    auto* sm = new SelectMenu(name, caption, width, 0, maxItemsShown);
    moveWidgetToTray(sm, trayLoc);
    sm->_assignListener(mListener);
    if (!items.empty()) sm->setItems(items);
    return sm;
}

auto TrayManager::createLongSelectMenu(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real boxWidth, unsigned int maxItemsShown, const Ogre::StringVector &items) -> SelectMenu *
{
    auto* sm = new SelectMenu(name, caption, width, boxWidth, maxItemsShown);
    moveWidgetToTray(sm, trayLoc);
    sm->_assignListener(mListener);
    if (!items.empty()) sm->setItems(items);
    return sm;
}

auto TrayManager::createLongSelectMenu(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real boxWidth, unsigned int maxItemsShown, const Ogre::StringVector &items) -> SelectMenu *
{
    return createLongSelectMenu(trayLoc, name, caption, 0, boxWidth, maxItemsShown, items);
}

auto TrayManager::createLabel(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width) -> Label *
{
    auto* l = new Label(name, caption, width);
    moveWidgetToTray(l, trayLoc);
    l->_assignListener(mListener);
    return l;
}

auto TrayManager::createSeparator(TrayLocation trayLoc, std::string_view name, Ogre::Real width) -> Separator *
{
    auto* s = new Separator(name, width);
    moveWidgetToTray(s, trayLoc);
    return s;
}

auto TrayManager::createThickSlider(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real valueBoxWidth, Ogre::Real minValue, Ogre::Real maxValue, unsigned int snaps) -> Slider *
{
    auto* s = new Slider(name, caption, width, 0, valueBoxWidth, minValue, maxValue, snaps);
    moveWidgetToTray(s, trayLoc);
    s->_assignListener(mListener);
    return s;
}

auto TrayManager::createLongSlider(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real trackWidth, Ogre::Real valueBoxWidth, Ogre::Real minValue, Ogre::Real maxValue, unsigned int snaps) -> Slider *
{
    if (trackWidth <= 0) trackWidth = 1;
    auto* s = new Slider(name, caption, width, trackWidth, valueBoxWidth, minValue, maxValue, snaps);
    moveWidgetToTray(s, trayLoc);
    s->_assignListener(mListener);
    return s;
}

auto TrayManager::createLongSlider(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real trackWidth, Ogre::Real valueBoxWidth, Ogre::Real minValue, Ogre::Real maxValue, unsigned int snaps) -> Slider *
{
    return createLongSlider(trayLoc, name, caption, 0, trackWidth, valueBoxWidth, minValue, maxValue, snaps);
}

auto TrayManager::createParamsPanel(TrayLocation trayLoc, std::string_view name, Ogre::Real width, unsigned int lines) -> ParamsPanel *
{
    auto* pp = new ParamsPanel(name, width, lines);
    moveWidgetToTray(pp, trayLoc);
    return pp;
}

auto TrayManager::createParamsPanel(TrayLocation trayLoc, std::string_view name, Ogre::Real width, const Ogre::StringVector &paramNames) -> ParamsPanel *
{
    auto* pp = new ParamsPanel(name, width, (Ogre::uint)paramNames.size());
    pp->setAllParamNames(paramNames);
    moveWidgetToTray(pp, trayLoc);
    return pp;
}

auto TrayManager::createCheckBox(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width) -> CheckBox *
{
    auto* cb = new CheckBox(name, caption, width);
    moveWidgetToTray(cb, trayLoc);
    cb->_assignListener(mListener);
    return cb;
}

auto TrayManager::createDecorWidget(TrayLocation trayLoc, std::string_view name, std::string_view templateName) -> DecorWidget *
{
    auto* dw = new DecorWidget(name, templateName);
    moveWidgetToTray(dw, trayLoc);
    return dw;
}

auto TrayManager::createProgressBar(TrayLocation trayLoc, std::string_view name, std::string_view caption, Ogre::Real width, Ogre::Real commentBoxWidth) -> ProgressBar *
{
    auto* pb = new ProgressBar(name, caption, width, commentBoxWidth);
    moveWidgetToTray(pb, trayLoc);
    return pb;
}

void TrayManager::showFrameStats(TrayLocation trayLoc, size_t place)
{
    if (!areFrameStatsVisible())
    {
        Ogre::StringVector stats;
        stats.push_back("Average FPS");
        stats.push_back("Best FPS");
        stats.push_back("Worst FPS");
        stats.push_back("Triangles");
        stats.push_back("Batches");

        mFpsLabel = createLabel(TrayLocation::NONE, ::std::format("{}/FpsLabel", mName), "FPS:", 180);
        mFpsLabel->_assignListener(this);
        mStatsPanel = createParamsPanel(TrayLocation::NONE, ::std::format("{}/StatsPanel", mName), 180, stats);
    }

    moveWidgetToTray(mFpsLabel, trayLoc, place);
    moveWidgetToTray(mStatsPanel, trayLoc, locateWidgetInTray(mFpsLabel) + 1);
}

void TrayManager::hideFrameStats()
{
    if (areFrameStatsVisible())
    {
        destroyWidget(mFpsLabel);
        destroyWidget(mStatsPanel);
        mFpsLabel = nullptr;
        mStatsPanel = nullptr;
    }
}

void TrayManager::showLogo(TrayLocation trayLoc, size_t place)
{
    if (!isLogoVisible()) mLogo = createDecorWidget(TrayLocation::NONE, ::std::format("{}/Logo", mName), "SdkTrays/Logo");
    moveWidgetToTray(mLogo, trayLoc, place);
}

void TrayManager::hideLogo()
{
    if (isLogoVisible())
    {
        destroyWidget(mLogo);
        mLogo = nullptr;
    }
}

void TrayManager::showOkDialog(std::string_view caption, std::string_view message)
{
    Ogre::OverlayElement* e;

    if (mDialog)
    {
        mDialog->setCaption(caption);
        mDialog->setText(message);

        if (mOk) return;
        else
        {
            mYes->cleanup();
            mNo->cleanup();
            delete mYes;
            delete mNo;
            mYes = nullptr;
            mNo = nullptr;
        }
    }
    else
    {
        // give widgets a chance to reset in case they're in the middle of something
        for (auto & mWidget : mWidgets)
        {
            for (auto & j : mWidget)
            {
                j->_focusLost();
            }
        }

        mDialogShade->show();

        mDialog = new TextBox(::std::format("{}/DialogBox", mName), caption, 300, 208);
        mDialog->setText(message);
        e = mDialog->getOverlayElement();
        mDialogShade->addChild(e);
        e->setVerticalAlignment(Ogre::GuiVerticalAlignment::CENTER);
        e->setLeft(-(e->getWidth() / 2));
        e->setTop(-(e->getHeight() / 2));

        mCursorWasVisible = isCursorVisible();
        showCursor();
    }

    mOk = new Button(::std::format("{}/OkButton", mName), "OK", 60);
    mOk->_assignListener(this);
    e = mOk->getOverlayElement();
    mDialogShade->addChild(e);
    e->setVerticalAlignment(Ogre::GuiVerticalAlignment::CENTER);
    e->setLeft(-(e->getWidth() / 2));
    e->setTop(mDialog->getOverlayElement()->getTop() + mDialog->getOverlayElement()->getHeight() + 5);
}

void TrayManager::showYesNoDialog(std::string_view caption, std::string_view question)
{
    Ogre::OverlayElement* e;

    if (mDialog)
    {
        mDialog->setCaption(caption);
        mDialog->setText(question);

        if (mOk)
        {
            mOk->cleanup();
            delete mOk;
            mOk = nullptr;
        }
        else return;
    }
    else
    {
        // give widgets a chance to reset in case they're in the middle of something
        for (auto & mWidget : mWidgets)
        {
            for (auto & j : mWidget)
            {
                j->_focusLost();
            }
        }

        mDialogShade->show();

        mDialog = new TextBox(::std::format("{}/DialogBox", mName), caption, 300, 208);
        mDialog->setText(question);
        e = mDialog->getOverlayElement();
        mDialogShade->addChild(e);
        e->setVerticalAlignment(Ogre::GuiVerticalAlignment::CENTER);
        e->setLeft(-(e->getWidth() / 2));
        e->setTop(-(e->getHeight() / 2));

        mCursorWasVisible = isCursorVisible();
        showCursor();
    }

    mYes = new Button(::std::format("{}/YesButton", mName), "Yes", 58);
    mYes->_assignListener(this);
    e = mYes->getOverlayElement();
    mDialogShade->addChild(e);
    e->setVerticalAlignment(Ogre::GuiVerticalAlignment::CENTER);
    e->setLeft(-(e->getWidth() + 2));
    e->setTop(mDialog->getOverlayElement()->getTop() + mDialog->getOverlayElement()->getHeight() + 5);

    mNo = new Button(::std::format("{}/NoButton", mName), "No", 50);
    mNo->_assignListener(this);
    e = mNo->getOverlayElement();
    mDialogShade->addChild(e);
    e->setVerticalAlignment(Ogre::GuiVerticalAlignment::CENTER);
    e->setLeft(3);
    e->setTop(mDialog->getOverlayElement()->getTop() + mDialog->getOverlayElement()->getHeight() + 5);
}

void TrayManager::closeDialog()
{
    if (mDialog)
    {
        if (mOk)
        {
            mOk->cleanup();
            delete mOk;
            mOk = nullptr;
        }
        else
        {
            mYes->cleanup();
            mNo->cleanup();
            delete mYes;
            delete mNo;
            mYes = nullptr;
            mNo = nullptr;
        }

        mDialogShade->hide();
        mDialog->cleanup();
        delete mDialog;
        mDialog = nullptr;

        if (!mCursorWasVisible) hideCursor();
    }
}

auto TrayManager::isDialogVisible() noexcept -> bool
{
    return mDialog != nullptr;
}

auto TrayManager::getWidget(TrayLocation trayLoc, std::string_view name) -> Widget *
{
    for (auto & i : mWidgets[std::to_underlying(trayLoc)])
    {
        if (i->getName() == name) return i;
    }
    return nullptr;
}

auto TrayManager::getWidget(std::string_view name) -> Widget *
{
    for (auto & mWidget : mWidgets)
    {
        for (auto & j : mWidget)
        {
            if (j->getName() == name) return j;
        }
    }
    return nullptr;
}

auto TrayManager::getNumWidgets() noexcept -> unsigned int
{
    unsigned int total = 0;

    for (auto & mWidget : mWidgets)
    {
        total += mWidget.size();
    }

    return total;
}

auto TrayManager::locateWidgetInTray(Widget *widget) -> int
{
    for (unsigned int i = 0; i < mWidgets[std::to_underlying(widget->getTrayLocation())].size(); i++)
    {
        if (mWidgets[std::to_underlying(widget->getTrayLocation())][i] == widget) return i;
    }
    return -1;
}

void TrayManager::destroyWidget(Widget *widget)
{
    if (!widget) OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, "Widget does not exist.", "TrayManager::destroyWidget");

    // in case special widgets are destroyed manually, set them to 0
    if (widget == mLogo) mLogo = nullptr;
    else if (widget == mStatsPanel) mStatsPanel = nullptr;
    else if (widget == mFpsLabel) mFpsLabel = nullptr;

    mTrays[std::to_underlying(widget->getTrayLocation())]->removeChild(widget->getName());

    WidgetList& wList = mWidgets[std::to_underlying(widget->getTrayLocation())];
    auto it = std::ranges::find(wList, widget);
    if(it != wList.end())
        wList.erase(it);

    if (widget == mExpandedMenu) setExpandedMenu(nullptr);

    widget->cleanup();

    mWidgetDeathRow.emplace_back(widget);

    adjustTrays();
}

void TrayManager::destroyAllWidgetsInTray(TrayLocation trayLoc)
{
    while (!mWidgets[std::to_underlying(trayLoc)].empty()) destroyWidget(mWidgets[std::to_underlying(trayLoc)][0]);
}

void TrayManager::destroyAllWidgets()
{
    for (unsigned int i = 0; i < 10; i++)  // destroy every widget in every tray (including null tray)
    {
        destroyAllWidgetsInTray((TrayLocation)i);
    }
}

void TrayManager::moveWidgetToTray(Widget *widget, TrayLocation trayLoc, size_t place)
{
    if (!widget) OGRE_EXCEPT(Ogre::ExceptionCodes::ITEM_NOT_FOUND, "Widget does not exist.", "TrayManager::moveWidgetToTray");

    // remove widget from old tray
    WidgetList& wList = mWidgets[std::to_underlying(widget->getTrayLocation())];
    auto it = std::ranges::find(wList, widget);
    if (it != wList.end())
    {
        wList.erase(it);
        mTrays[std::to_underlying(widget->getTrayLocation())]->removeChild(widget->getName());
    }

    // insert widget into new tray at given position, or at the end if unspecified or invalid
    if (place > mWidgets[std::to_underlying(trayLoc)].size()) place = mWidgets[std::to_underlying(trayLoc)].size();
    mWidgets[std::to_underlying(trayLoc)].insert(mWidgets[std::to_underlying(trayLoc)].begin() + place, widget);
    mTrays[std::to_underlying(trayLoc)]->addChild(widget->getOverlayElement());

    widget->getOverlayElement()->setHorizontalAlignment(mTrayWidgetAlign[std::to_underlying(trayLoc)]);

    // adjust trays if necessary
    if (widget->getTrayLocation() != TrayLocation::NONE || trayLoc != TrayLocation::NONE) adjustTrays();

    widget->_assignToTray(trayLoc);
}

void TrayManager::clearTray(TrayLocation trayLoc)
{
    if (trayLoc == TrayLocation::NONE) return;      // can't clear the null tray

    while (!mWidgets[std::to_underlying(trayLoc)].empty())   // remove every widget from given tray
    {
        removeWidgetFromTray(mWidgets[std::to_underlying(trayLoc)][0]);
    }
}

void TrayManager::clearAllTrays()
{
    for (unsigned int i = 0; i < 9; i++)
    {
        clearTray((TrayLocation)i);
    }
}

void TrayManager::frameRendered(const Ogre::FrameEvent &evt)
{
    mWidgetDeathRow.clear();


    unsigned long currentTime = mTimer->getMilliseconds();
    if (areFrameStatsVisible() && currentTime - mLastStatUpdateTime >= FRAME_UPDATE_DELAY)
    {
        Ogre::RenderTarget::FrameStats stats = mWindow->getStatistics();

        mLastStatUpdateTime = currentTime;

        Ogre::String s("FPS: ");
        s += Ogre::StringConverter::toString((int)stats.lastFPS);

        mFpsLabel->setCaption(s);

        if (mStatsPanel->getOverlayElement()->isVisible())
        {
            Ogre::StringVector values;
            Ogre::StringStream oss;

            oss.str("");
            oss << std::fixed << std::setprecision(1) << stats.avgFPS;
            Ogre::String str = oss.str();
            values.push_back(str);

            oss.str("");
            oss << std::fixed << std::setprecision(1) << stats.bestFPS;
            str = oss.str();
            values.push_back(str);

            oss.str("");
            oss << std::fixed << std::setprecision(1) << stats.worstFPS;
            str = oss.str();
            values.push_back(str);

            str = Ogre::StringConverter::toString(stats.triangleCount);
            values.push_back(str);

            str = Ogre::StringConverter::toString(stats.batchCount);
            values.push_back(str);

            mStatsPanel->setAllParamValues(values);
        }
    }
}

void TrayManager::windowUpdate()
{
    unsigned long currentTime = mTimer->getMilliseconds();
    if (currentTime - mLastStatUpdateTime >= FRAME_UPDATE_DELAY)
    {
        mLastStatUpdateTime = currentTime;
        mWindow->update();
    }
}

void TrayManager::labelHit(Label *label)
{
    if (mStatsPanel->getOverlayElement()->isVisible())
    {
        mStatsPanel->getOverlayElement()->hide();
        mFpsLabel->getOverlayElement()->setWidth(150);
        removeWidgetFromTray(mStatsPanel);
    }
    else
    {
        mStatsPanel->getOverlayElement()->show();
        mFpsLabel->getOverlayElement()->setWidth(180);
        moveWidgetToTray(mStatsPanel, mFpsLabel->getTrayLocation(), locateWidgetInTray(mFpsLabel) + 1);
    }
}

void TrayManager::buttonHit(Button *button)
{
    if (mListener)
    {
        if (button == mOk) mListener->okDialogClosed(mDialog->getText());
        else mListener->yesNoDialogClosed(mDialog->getText(), button == mYes);
    }
    closeDialog();
}

auto TrayManager::mousePressed(const MouseButtonDownEvent &evt) noexcept -> bool
{
    if (evt.button != ButtonType::LEFT) return false;

    Ogre::Vector2 cursorPos{mCursor->getLeft(), mCursor->getTop()};

    mTrayDrag = false;

    if (mExpandedMenu)   // only check top priority widget until it passes on
    {
        mExpandedMenu->_cursorPressed(cursorPos);
        if (!mExpandedMenu->isExpanded()) setExpandedMenu(nullptr);
        return true;
    }

    if (mDialog)   // only check top priority widget until it passes on
    {
        mDialog->_cursorPressed(cursorPos);
        if (mOk) mOk->_cursorPressed(cursorPos);
        else
        {
            mYes->_cursorPressed(cursorPos);
            mNo->_cursorPressed(cursorPos);
        }
        return true;
    }

    // process widgets in reverse ZOrder
    for (int i = 8; i >= 0; --i)   // check if mouse is over a non-null tray
    {
        if (mTrays[i]->isVisible() && Widget::isCursorOver(mTrays[i], cursorPos, 2))
        {
            mTrayDrag = true;   // initiate a drag that originates in a tray
            break;
        }
    }

    for (int j = (int)mWidgets[9].size() - 1; j >= 0 ; --j)  // check if mouse is over a non-null tray's widgets
    {
        if(j >= (int)mWidgets[9].size()) continue;
        Widget* w = mWidgets[9][j];
        if (w->getOverlayElement()->isVisible() && Widget::isCursorOver(w->getOverlayElement(), cursorPos))
        {
            mTrayDrag = true;   // initiate a drag that originates in a tray
            break;
        }
    }

    if (!mTrayDrag) return false;   // don't process if mouse press is not in tray

    for (int i = 9; i >= 0; --i)
    {
        if (!mTrays[i]->isVisible()) continue;

        for (int j = (int)mWidgets[i].size() - 1; j >= 0; --j)
        {
            if(j >= (int)mWidgets[i].size()) continue;
            Widget* w = mWidgets[i][j];
            if (!w->getOverlayElement()->isVisible()) continue;
            w->_cursorPressed(cursorPos);    // send event to widget

            auto* m = dynamic_cast<SelectMenu*>(w);
            if (m && m->isExpanded())       // a menu has begun a top priority session
            {
                setExpandedMenu(m);
                return true;
            }
        }
    }

    return true;   // a tray click is not to be handled by another party
}

auto TrayManager::mouseReleased(const MouseButtonUpEvent &evt) noexcept -> bool
{
    if (evt.button != ButtonType::LEFT) return false;

    Ogre::Vector2 cursorPos{mCursor->getLeft(), mCursor->getTop()};

    if (mExpandedMenu)   // only check top priority widget until it passes on
    {
        mExpandedMenu->_cursorReleased(cursorPos);
        return true;
    }

    if (mDialog)   // only check top priority widget until it passes on
    {
        mDialog->_cursorReleased(cursorPos);
        if (mOk) mOk->_cursorReleased(cursorPos);
        else
        {
            mYes->_cursorReleased(cursorPos);
            // very important to check if second button still exists, because first button could've closed the popup
            if (mNo) mNo->_cursorReleased(cursorPos);
        }
        return true;
    }

    if (!mTrayDrag) return false;    // this click did not originate in a tray, so don't process

    // process trays and widgets in reverse ZOrder
    for (int i = 9; i >= 0; --i)
    {
        if (!mTrays[i]->isVisible()) continue;

        for (int j = (int)mWidgets[i].size() - 1; j >= 0; --j)
        {
            if(j >= (int)mWidgets[i].size()) continue;
            Widget* w = mWidgets[i][j];
            if (!w->getOverlayElement()->isVisible()) continue;
            w->_cursorReleased(cursorPos);    // send event to widget
        }
    }

    mTrayDrag = false;   // stop this drag
    return true;         // this click did originate in this tray, so don't pass it on
}

auto TrayManager::mouseMoved(const MouseMotionEvent &evt) noexcept -> bool
{
    // thats a separate event. Ignore for now.
    static float constexpr wheelDelta = 0;

    auto vpScale = Ogre::OverlayManager::getSingleton().getPixelRatio();

    // always keep track of the mouse pos for refreshCursor()
    mCursorPos = Ogre::Vector2{static_cast<Ogre::Real>(evt.x), static_cast<Ogre::Real>(evt.y)} / vpScale;
    mCursor->setPosition(mCursorPos.x, mCursorPos.y);

    if (mExpandedMenu)   // only check top priority widget until it passes on
    {
        mExpandedMenu->_cursorMoved(mCursorPos, wheelDelta);
        return true;
    }

    if (mDialog)   // only check top priority widget until it passes on
    {
        mDialog->_cursorMoved(mCursorPos, wheelDelta);
        if(mOk) mOk->_cursorMoved(mCursorPos, wheelDelta);
        else
        {
            mYes->_cursorMoved(mCursorPos, wheelDelta);
            mNo->_cursorMoved(mCursorPos, wheelDelta);
        }
        return true;
    }

    // process trays and widgets in reverse ZOrder
    for (int i = 9; i >= 0; --i)
    {
        if (!mTrays[i]->isVisible()) continue;

        for (int j = (int)mWidgets[i].size() - 1; j >= 0; --j)
        {
            if(j >= (int)mWidgets[i].size()) continue;
            Widget* w = mWidgets[i][j];
            if (!w->getOverlayElement()->isVisible()) continue;
            w->_cursorMoved(mCursorPos, wheelDelta);    // send event to widget
        }
    }

    if (mTrayDrag) return true;  // don't pass this event on if we're in the middle of a drag
    return false;
}

auto TrayManager::mouseWheelRolled(const MouseWheelEvent& evt) noexcept -> bool
{
    if (mExpandedMenu)
    {
        mExpandedMenu->_cursorMoved(mCursorPos, evt.y);
        return true;
    }
    return false;
}

void TrayManager::setExpandedMenu(SelectMenu *m)
{
    if (!mExpandedMenu && m)
    {
        auto* c = (Ogre::OverlayContainer*)m->getOverlayElement();
        Ogre::OverlayContainer* eb = (Ogre::OverlayContainer*)c->getChild(::std::format("{}/MenuExpandedBox", m->getName()));
        eb->_update();
        eb->setPosition
                ((unsigned int)(eb->_getDerivedLeft() * Ogre::OverlayManager::getSingleton().getViewportWidth()),
                 (unsigned int)(eb->_getDerivedTop() * Ogre::OverlayManager::getSingleton().getViewportHeight()));
        c->removeChild(eb->getName());
        mPriorityLayer->add2D(eb);
    }
    else if(mExpandedMenu && !m)
    {
        Ogre::OverlayContainer* eb = mPriorityLayer->getChild(::std::format("{}/MenuExpandedBox", mExpandedMenu->getName()));
        mPriorityLayer->remove2D(eb);
        ((Ogre::OverlayContainer*)mExpandedMenu->getOverlayElement())->addChild(eb);
    }

    mExpandedMenu = m;
}

}

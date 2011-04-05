/*
 *  The ManaPlus Client
 *  Copyright (C) 2011  The ManaPlus Developers
 *
 *  This file is part of The ManaPlus Client.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "gui/setupitem.h"

#include "configuration.h"
#include "main.h"
#include "log.h"

#include "gui/editdialog.h"

#include "gui/widgets/button.h"
#include "gui/widgets/checkbox.h"
#include "gui/widgets/horizontcontainer.h"
#include "gui/widgets/label.h"
#include "gui/widgets/layouthelper.h"
#include "gui/widgets/tabbedarea.h"
#include "gui/widgets/textfield.h"
#include "gui/widgets/vertcontainer.h"

#include "utils/dtor.h"
#include "utils/gettext.h"


SetupItem::SetupItem(std::string text, std::string description,
                     std::string keyName, SetupTabScroll *parent,
                     std::string eventName, bool mainConfig) :
    mText(text),
    mDescription(description),
    mKeyName(keyName),
    mParent(parent),
    mEventName(eventName),
    mMainConfig(mainConfig),
    mUseDefault(false),
    mValue(""),
    mDefault(""),
    mWidget(0)
{
}

SetupItem::SetupItem(std::string text, std::string description,
                     std::string keyName, SetupTabScroll *parent,
                     std::string eventName, std::string def, bool mainConfig) :
    mText(text),
    mDescription(description),
    mKeyName(keyName),
    mParent(parent),
    mEventName(eventName),
    mMainConfig(mainConfig),
    mUseDefault(true),
    mValue(""),
    mDefault(def),
    mWidget(0)
{
}

SetupItem::~SetupItem()
{

}

Configuration *SetupItem::getConfig()
{
    if (mMainConfig)
        return &config;
    else
        return &serverConfig;
}

void SetupItem::load()
{
    Configuration *cfg = getConfig();
    if (mUseDefault)
        mValue = cfg->getValue(mKeyName, mDefault);
    else
        mValue = cfg->getStringValue(mKeyName);
}

void SetupItem::save()
{
    Configuration *cfg = getConfig();
    cfg->setValue(mKeyName, mValue);
}

std::string SetupItem::getActionEventId()
{
    if (!mWidget)
        return "";

    return mWidget->getActionEventId();
}

void SetupItem::action(const gcn::ActionEvent &event)
{
    if (!mWidget)
        return;

    if (event.getId() == mWidget->getActionEventId())
        action();
}

void SetupItem::action()
{
    fromWidget();
}

void SetupItem::apply(std::string eventName _UNUSED_)
{
    save();
}

void SetupItem::cancel(std::string eventName _UNUSED_)
{
    load();
    toWidget();
}

void SetupItem::externalUpdated(std::string eventName _UNUSED_)
{
    load();
    toWidget();
}

SetupItemCheckBox::SetupItemCheckBox(std::string text, std::string description,
                                     std::string keyName,
                                     SetupTabScroll *parent,
                                     std::string eventName, bool mainConfig) :
    SetupItem(text, description, keyName, parent, eventName, mainConfig)
{
    createControls();
}

SetupItemCheckBox::SetupItemCheckBox(std::string text, std::string description,
                                     std::string keyName,
                                     SetupTabScroll *parent,
                                     std::string eventName, std::string def,
                                     bool mainConfig) :
    SetupItem(text, description, keyName, parent, eventName, def, mainConfig)
{
    createControls();
}

SetupItemCheckBox::~SetupItemCheckBox()
{
    mWidget = 0;
}

void SetupItemCheckBox::createControls()
{
    load();
    mCheckBox = new CheckBox(mText, mValue != "0", mParent, mEventName);
    mWidget = mCheckBox;
    mParent->getContainer()->add(mWidget);
    mParent->addControl(this);
    mParent->addActionListener(this);
    mWidget->addActionListener(this);
}

void SetupItemCheckBox::fromWidget()
{
    if (!mCheckBox)
        return;

    if (mCheckBox->isSelected())
        mValue = "1";
    else
        mValue = "0";
}

void SetupItemCheckBox::toWidget()
{
    if (!mCheckBox)
        return;

    mCheckBox->setSelected(mValue != "0");
}


SetupItemTextField::SetupItemTextField(std::string text,
                                       std::string description,
                                       std::string keyName,
                                       SetupTabScroll *parent,
                                       std::string eventName,
                                       bool mainConfig) :
    SetupItem(text, description, keyName, parent, eventName, mainConfig),
    mHorizont(0),
    mLabel(0),
    mTextField(0),
    mButton(0),
    mEditDialog(0)
{
    createControls();
}

SetupItemTextField::SetupItemTextField(std::string text,
                                       std::string description,
                                       std::string keyName,
                                       SetupTabScroll *parent,
                                       std::string eventName, std::string def,
                                       bool mainConfig) :
    SetupItem(text, description, keyName, parent, eventName, def, mainConfig),
    mHorizont(0),
    mLabel(0),
    mTextField(0),
    mButton(0),
    mEditDialog(0)
{
    createControls();
}

SetupItemTextField::~SetupItemTextField()
{
    mHorizont = 0;
    mWidget = 0;
    mTextField = 0;
    mLabel = 0;
    mButton = 0;
}

void SetupItemTextField::createControls()
{
    load();
    mHorizont = new HorizontContainer(32, 2);

    mLabel = new Label(mText);
    mTextField = new TextField(mValue, true, mParent, mEventName);
    mButton = new Button(_("Edit"), mEventName + "_EDIT", mParent);
    mWidget = mTextField;
    mTextField->setWidth(200);
    mHorizont->add(mLabel);
    mHorizont->add(mTextField);
    mHorizont->add(mButton);

    mParent->getContainer()->add(mHorizont, true, 4);
    mParent->addControl(this);
    mParent->addControl(this, mEventName + "_EDIT");
    mParent->addControl(this, mEventName + "_EDIT_OK");
    mParent->addActionListener(this);
    mWidget->addActionListener(this);
    mButton->addActionListener(this);
}

void SetupItemTextField::fromWidget()
{
    if (!mTextField)
        return;

    mValue = mTextField->getText();
}

void SetupItemTextField::toWidget()
{
    if (!mTextField)
        return;

    mTextField->setText(mValue);
}

void SetupItemTextField::action(const gcn::ActionEvent &event)
{
    if (!mTextField)
        return;

    if (event.getId() == mWidget->getActionEventId())
    {
        fromWidget();
    }
    else if (event.getId() == mEventName + "_EDIT")
    {
        mEditDialog =  new EditDialog(mText, mTextField->getText(),
            mEventName + "_EDIT_OK");
        mEditDialog->addActionListener(this);
    }
    else if (event.getId() == mEventName + "_EDIT_OK")
    {
        mTextField->setText(mEditDialog->getMsg());
        mEditDialog = 0;
    }
}

void SetupItemTextField::apply(std::string eventName)
{
    if (eventName != mEventName)
        return;

    fromWidget();
    save();
}

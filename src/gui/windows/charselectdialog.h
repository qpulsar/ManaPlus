/*
 *  The ManaPlus Client
 *  Copyright (C) 2004-2009  The Mana World Development Team
 *  Copyright (C) 2009-2010  The Mana Developers
 *  Copyright (C) 2011-2015  The ManaPlus Developers
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

#ifndef GUI_WINDOWS_CHARSELECTDIALOG_H
#define GUI_WINDOWS_CHARSELECTDIALOG_H

#include "gui/widgets/window.h"

#include "net/charserverhandler.h"

#include "listeners/actionlistener.h"
#include "listeners/keylistener.h"

class Button;
class CharacterDisplay;
class CharacterViewBase;
class LoginData;
class TextDialog;

/**
 * Character selection dialog.
 *
 * \ingroup Interface
 */
class CharSelectDialog final : public Window,
                               public ActionListener,
                               public KeyListener
{
    public:
        friend class CharDeleteConfirm;
        friend class Net::CharServerHandler;

        /**
         * Constructor.
         */
        explicit CharSelectDialog(LoginData *const data);

        A_DELETE_COPY(CharSelectDialog)

        ~CharSelectDialog();

        void action(const ActionEvent &event) override final;

        void keyPressed(KeyEvent &event) override final;

        enum SelectAction
        {
            Focus = 0,
            Choose
        };

        /**
         * Attempt to select the character with the given name. Returns whether
         * a character with the given name was found.
         *
         * \param action determines what to do when a character with the given
         *               name was found (just focus or also try to choose this
         *               character).
         */
        bool selectByName(const std::string &name,
                          const SelectAction action = Focus);

        void askPasswordForDeletion(const int index);

        void close() override final;

        void widgetResized(const Event &event) override final;

        void updateState();

        void postInit() override final;

        void setName(const BeingId id, const std::string &newName);

    private:
        void attemptCharacterDelete(const int index,
                                    const std::string &email);

        void attemptCharacterSelect(const int index);

        void setCharacters(const Net::Characters &characters);

        void setCharacter(Net::Character *const character);

        void use(const int selected);

        void lock();
        void unlock();
        void setLocked(const bool locked);

        LoginData *mLoginData;

        Button *mSwitchLoginButton;
        Button *mChangePasswordButton;
        Button *mUnregisterButton;
        Button *mChangeEmailButton;
        Button *mPlayButton;
        Button *mInfoButton;
        Button *mDeleteButton;
        Button *mRenameButton;
        CharacterViewBase *mCharacterView;

        std::vector<CharacterDisplay*> mCharacterEntries;

        Net::CharServerHandler *mCharServerHandler;
        TextDialog *mDeleteDialog;
        int mDeleteIndex;
        bool mLocked;
        bool mSmallScreen;
};

#endif  // GUI_WINDOWS_CHARSELECTDIALOG_H

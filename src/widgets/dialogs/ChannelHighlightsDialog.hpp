// SPDX-FileCopyrightText: 2026 The Satorino authors
//
// SPDX-License-Identifier: MIT

#pragma once

#include "widgets/BasePopup.hpp"

class QMenu;

namespace chatterino {
class Channel;
class Split;
}  // namespace chatterino

namespace chatterino::satorino {

void addChannelHighlightsAction(QMenu *menu, Split *split);

class ChannelHighlightsDialog : public BasePopup
{
public:
    ChannelHighlightsDialog(const Channel &channel, QWidget *parent);
};

}  // namespace chatterino::satorino

/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2021 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "Screen.h"

class ScreenList : public QList<Screen>
{
  int m_width = 5;

public:
  explicit ScreenList(int width = 5);

  /**
   * @brief addScreenByPriority adds a new screen according to the following
   * priority: 1.left side of the server 2.right side of the server 3.top 4.down
   * 5.top left-hand diagonally
   * 6.top right-hand diagonally
   * 7.bottom right-hand diagonally
   * 8.bottom left-hand diagonally
   * 9.In case all places from the list have already booked, place in any spare
   * place
   * @param newScreen
   */
  void addScreenByPriority(const Screen &newScreen);

  /**
   * @brief addScreenToFirstEmpty adds screen into the first empty place
   * @param newScreen
   */
  void addScreenToFirstEmpty(const Screen &newScreen);

  /**
   * @brief addScreenAwayFromServer adds a new screen into the empty place
   * farthest from the server, so it never shares an edge with the server
   * unless the grid leaves no other choice. Used for headless screens
   * (bridge clients), where an accidental edge-switch would forward all
   * input to a device with no display.
   * @param newScreen
   */
  void addScreenAwayFromServer(const Screen &newScreen);

  /**
   * @brief Returns true if screens are equal
   * @param sc
   */
  bool operator==(const ScreenList &sc) const;
};

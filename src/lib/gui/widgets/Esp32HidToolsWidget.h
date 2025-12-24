/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTextEdit>
#include <cstdint>
#include <vector>

namespace deskflow::gui {

class Esp32HidToolsWidget : public QDialog
{
  Q_OBJECT

public:
  explicit Esp32HidToolsWidget(const QString &devicePath, QWidget *parent = nullptr);
  ~Esp32HidToolsWidget() override = default;

private Q_SLOTS:
  void refreshPorts();
  void onBrowseFactory();
  void onFlashFactory();
  void onDownloadAndFlashFactory();
  void onBrowseUpgrade();
  void onCheckUpgrade();
  void onFlashOnline();
  void onFlashLocal();
  void onCopyInfo();
  void onCopySerialClicked();
  void onActivateClicked();
  void onPortChanged(int index);
  void onTabChanged(int index);
  void onGenerateOrder();
  void onCopyOrderContent();
  void onEmailOrder();

private:
  void refreshDeviceState();
  // Port Selection
  QComboBox *m_portCombo;
  QPushButton *m_refreshPortsBtn;

  // Factory Tab
  QLineEdit *m_factoryPathEdit;
  QPushButton *m_factoryBrowseBtn;
  QPushButton *m_factoryFlashBtn;
  QPushButton *m_downloadFlashBtn;
  QPushButton *m_copyInfoBtn;

  // Upgrade Tab
  // Upgrade Tab
  // Online Section
  QLabel *m_lblCurrentVersion;
  QLabel *m_lblLatestVersion;
  QPushButton *m_checkUpgradeBtn;
  QPushButton *m_flashOnlineBtn;

  // Manual Section
  QLineEdit *m_upgradePathEdit;
  QPushButton *m_upgradeBrowseBtn;
  QPushButton *m_flashLocalBtn;

  // Activation Tab
  QWidget *m_tabActivation;
  QLabel *m_labelActivationState;
  QLabel *m_lineSerial;
  QPushButton *m_btnCopySerial;
  QWidget *m_groupActivationInput; // Container to hide/show
  QLineEdit *m_lineActivationKey;
  QPushButton *m_btnActivate;

  // Order Tab
  QLineEdit *m_orderName;
  QLineEdit *m_orderEmail;
  QRadioButton *m_orderOption1;
  QRadioButton *m_orderOption2;
  QRadioButton *m_orderOption3;
  QRadioButton *m_orderOption4;
  QLineEdit *m_orderDeviceSecret;
  QLabel *m_orderSerialLabel;
  QComboBox *m_orderTotalProfiles;
  QPushButton *m_btnGenerateOrder;
  QPushButton *m_btnCopyOrder;
  QPushButton *m_btnEmailOrder;

  // Common
  QTextEdit *m_logOutput;

  // State
  QString m_devicePath;
  bool m_isTaskRunning = false;

  void setupUI();
  void log(const QString &message);
  void setControlsEnabled(bool enabled);
  template <typename Function> void runBackgroundTask(Function func);
  std::vector<uint8_t> readFile(const QString &path);
  void flashFirmware(const std::vector<uint8_t> &data);
  QString composeOrderContent(QString &outPrefix, int &outOption);
  void reject() override;
};

} // namespace deskflow::gui

/*
 * Deskflow-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkReply>
#include <QPushButton>
#include <QRadioButton>
#include <QTabWidget>
#include <QTextEdit>
#include <cstdint>
#include <vector>

class QNetworkAccessManager;

namespace deskflow::gui {

struct OrderPrice
{
  double price;
  QString desc;
};

struct PriceConfig
{
  double license;
  double profile_4;
  double profile_6;
  double combo_discount;
};

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

  void onCopyOrderContent();
  void onEmailOrder();
  void updatePaymentDetails();
  void updatePaymentReference();
  void onPayNowClicked();

private:
  void refreshDeviceState();
  bool confirmFactoryFlash();
  void showFactoryFlashSuccess();
  void fetchPrices();
  void onPriceResponse(QNetworkReply *reply);

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
  QLabel *m_lblPaymentDetails;
  QLabel *m_lblPaymentOwner;
  QLineEdit *m_paymentRefNo;
  QLineEdit *m_paymentTransId;
  QCheckBox *m_chkManualPayment;
  QPushButton *m_btnPayNow;

  QPushButton *m_btnCopyOrder;
  QPushButton *m_btnEmailOrder;

  // Common
  QTabWidget *m_tabWidget;
  QTextEdit *m_logOutput;

  // State
  QString m_devicePath;
  bool m_isTaskRunning = false;
  PriceConfig m_prices;
  QNetworkAccessManager *m_network;

  void setupUI();
  void log(const QString &message);
  void setControlsEnabled(bool enabled);
  template <typename Function> void runBackgroundTask(Function func);
  std::vector<uint8_t> readFile(const QString &path);
  void flashFirmware(const std::vector<uint8_t> &data);
  OrderPrice calculateOrderPrice(int option, int totalProfiles);
  QString composeOrderContent(QString &outPrefix, int &outOption);
  void reject() override;
  void changeEvent(QEvent *event) override;
  void updateText();
};

} // namespace deskflow::gui

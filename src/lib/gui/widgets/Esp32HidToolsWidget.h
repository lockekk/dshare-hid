/*
 * dshare-hid -- created by locke.huang@gmail.com
 */

#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class QNetworkAccessManager;
enum class FlashResult : int;

namespace deskflow::bridge {
enum class ActivationState : uint8_t;
}

namespace deskflow::gui {

struct OrderPrice
{
  double price;
  QString desc;        // in English: the order email and the PayPal item name are read by the seller
  QString displayDesc; // the same in the user's language, for the Order tab
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
  // What the device reported after a factory flash or an upgrade
  enum class FactoryFlashOutcome
  {
    AppStarted,
    FactoryWithoutApp,
    NotDetected
  };
  struct FactoryFlashCheck
  {
    FactoryFlashOutcome outcome;
    deskflow::bridge::ActivationState activationState;
    QString pairingNotice; // BridgeFirmwareNotice: the flash took the device to 1.2.1; only an upgrade shows it
    QString version;       // the application firmware's version, when it started
  };

  // What the dongle answers before a new dongle setup or an upgrade
  enum class DongleSetup
  {
    NoAnswer,         // no DShare-HID firmware: a new dongle, or the port could not be opened
    App,              // the application firmware runs
    Factory,          // the factory firmware runs, the application firmware is there
    FactoryWithoutApp // the factory firmware runs without it: an earlier update did not finish
  };
  struct DongleCheck
  {
    DongleSetup setup = DongleSetup::NoAnswer;
    bool portOpened = false;
    QString version; // the application firmware's version, e.g. "v1.2.1"; empty in the factory firmware
    QString serial;  // the application firmware's serial number, when it gave one
    bool upgradeConfirmed = false; // VersionNote::ConfirmUpgrade: an upgrade that didn't report back worked
    QString pairingNotice;         // BridgeFirmwareNotice, when the version this check noted makes it due
  };

  // What checkDongle() does with the application firmware's version, for BridgeFirmwareNotice
  enum class VersionNote
  {
    None,
    BeforeUpgrade, // note it: the version an upgrade starts from
    ConfirmUpgrade // note it only to confirm an earlier upgrade that didn't report back in time
  };

  void refreshDeviceState();
  QString selectedPort();
  bool confirmNewDongleSetup();
  DongleCheck checkDongle(const std::string &port, VersionNote note);
  bool dongleNeedsSetup(const std::string &port);
  void checkNewDongleSetup();
  void showDongleSetUp(bool setUp);
  void setUpNewDongle(const std::string &port, const std::vector<uint8_t> &package);
  bool readyForUpgrade(const DongleCheck &check);
  void upgradeDongle(const std::string &port, const std::vector<uint8_t> &package, const DongleCheck &check);
  FactoryFlashCheck waitForAppAfterFlash(const std::string &port);
  void showFactoryFlashResult(const FactoryFlashCheck &check);
  void showUpgradeSuccess(const QString &pairingNotice);
  QString noApplicationFirmwareMessage() const;
  QString flashFailureText(FlashResult result, bool newDongle) const;
  void logFlashFailure(FlashResult result);
  void fetchPrices();
  void onPriceResponse(QNetworkReply *reply);

  // Port Selection
  QComboBox *m_portCombo;
  QPushButton *m_refreshPortsBtn;

  // New Dongle Setup Tab
  QLabel *m_alreadySetUpLbl;
  QWidget *m_newDongleSetupPanel; // the setup itself, hidden while m_alreadySetUpLbl shows
  QLabel *m_newDongleIntroLbl;
  QLineEdit *m_factoryPathEdit;
  QPushButton *m_factoryBrowseBtn;
  QPushButton *m_factoryFlashBtn;
  QPushButton *m_downloadFlashBtn;

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
  QRadioButton *m_orderOption2;
  QRadioButton *m_orderOption4;
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
  QSplitter *m_splitter;
  QProgressBar *m_progressBar;
  QTextEdit *m_logOutput;

  // State
  QString m_devicePath;
  bool m_isTaskRunning = false;
  bool m_onlineFlashAllowed = false;
  int m_progressPercent = -1;   // last percentage sent to the progress bar, by the worker thread
  std::string m_lastFlashError; // the flash tool's last "Error..." line in this task, by the worker thread
  QString m_lastDeviceState;    // the last state line the Activation and Order tabs logged
  PriceConfig m_prices;
  QNetworkAccessManager *m_network;

  int showWideMessageBox(
      QMessageBox::Icon icon, const QString &title, const QString &text,
      QMessageBox::StandardButtons buttons = QMessageBox::Ok
  );

  void log(const QString &message);
  void logDetail(const std::string &message);
  void logDeviceState(const QString &state);
  void setProgress(size_t done, size_t total);
  void setControlsEnabled(bool enabled);
  template <typename Function> void runBackgroundTask(Function func);
  std::vector<uint8_t> readFile(const QString &path);
  std::function<void()> createReplugCallback();
  OrderPrice calculateOrderPrice(int option, int totalProfiles);
  QString composeOrderContent(QString &outPrefix, int &outOption);
  void reject() override;
  void changeEvent(QEvent *event) override;
  void updateText();
};

} // namespace deskflow::gui

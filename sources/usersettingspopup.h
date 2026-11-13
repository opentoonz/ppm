#pragma once

#include <QDialog>

class QLineEdit;
class QComboBox;

class UserSettingsPopup : public QDialog {
  Q_OBJECT

  QLineEdit *m_tvpPath, *m_cspPath, *m_xdtsViewerPath, *m_otPath, *m_otArg,
      *m_userName;
  QComboBox* m_languageCombo;
  void editMappings();
  QString chooseApplication(const QString& title, const QString& currentPath);

public:
  UserSettingsPopup(QWidget* parent   = nullptr,
                    Qt::WindowFlags f = Qt::WindowFlags());
protected slots:
  void onTvpBtnClicked();
  void onTvpPathEdited();
  void onCspBtnClicked();
  void onCspPathEdited();
  void onXdtsBtnClicked();
  void onXdtsPathEdited();
  void onOtBtnClicked();
  void onOtPathEdited();
  void onOtArgEdited();
  void onUserNameEdited();
  void onLanguageActivated();
};
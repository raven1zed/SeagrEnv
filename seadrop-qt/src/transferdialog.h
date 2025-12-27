/**
 * @file transferdialog.h
 * @brief Transfer progress dialog
 */

#ifndef TRANSFERDIALOG_H
#define TRANSFERDIALOG_H

#include "seadropbridge.h"
#include <QDialog>
#include <memory>


QT_BEGIN_NAMESPACE
namespace Ui {
class TransferDialog;
}
QT_END_NAMESPACE

class TransferDialog : public QDialog {
  Q_OBJECT

public:
  explicit TransferDialog(QWidget *parent = nullptr);
  ~TransferDialog();

  void setTransferInfo(const TransferInfo &info);
  void updateProgress(const TransferInfo &info);

signals:
  void cancelRequested(const QString &transferId);

private:
  std::unique_ptr<Ui::TransferDialog> ui;
  QString transferId_;
};

#endif // TRANSFERDIALOG_H

/**
 * @file transferdialog.cpp
 * @brief Transfer dialog implementation
 */

#include "transferdialog.h"
#include "ui_transferdialog.h"

TransferDialog::TransferDialog(QWidget *parent)
    : QDialog(parent), ui(std::make_unique<Ui::TransferDialog>()) {

  ui->setupUi(this);
  setWindowTitle("File Transfer");

  connect(ui->cancelButton, &QPushButton::clicked, this,
          [this]() { emit cancelRequested(transferId_); });
}

TransferDialog::~TransferDialog() = default;

void TransferDialog::setTransferInfo(const TransferInfo &info) {
  transferId_ = info.transferId;
  ui->fileNameLabel->setText(info.fileName);
  ui->progressBar->setValue(info.progressPercent);
  ui->statusLabel->setText(info.isIncoming ? "Receiving..." : "Sending...");

  QString sizeText =
      QString("%1 / %2 bytes").arg(info.transferredBytes).arg(info.totalBytes);
  ui->sizeLabel->setText(sizeText);
}

void TransferDialog::updateProgress(const TransferInfo &info) {
  ui->progressBar->setValue(info.progressPercent);

  QString sizeText =
      QString("%1 / %2 bytes").arg(info.transferredBytes).arg(info.totalBytes);
  ui->sizeLabel->setText(sizeText);

  if (info.progressPercent >= 100) {
    ui->statusLabel->setText("Complete!");
    ui->cancelButton->setText("Close");
  }
}

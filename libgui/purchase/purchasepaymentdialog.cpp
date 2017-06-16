/*
 * purchasepaymentdialog.cpp
 * Copyright 2017 - ~, Apin <apin.klas@gmail.com>
 *
 * This file is part of Turbin.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "purchasepaymentdialog.h"
#include "ui_purchasepaymentdialog.h"
#include "preference.h"
#include "message.h"
#include "global_constant.h"
#include "flashmessagemanager.h"
#include <QMessageBox>

using namespace LibGUI;
using namespace LibG;

PurchasePaymentDialog::PurchasePaymentDialog(LibG::MessageBus *bus, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::PurchasePaymentDialog)
{
    ui->setupUi(this);
    setMessageBus(bus);
    connect(ui->pushSave, SIGNAL(clicked(bool)), SLOT(saveClicked()));
}

PurchasePaymentDialog::~PurchasePaymentDialog()
{
    delete ui;
}

void PurchasePaymentDialog::fill(const QVariantMap &data)
{
    ui->labelSuplier->setText(data["suplier"].toString());
    ui->labelTotal->setText(Preference::toString(data["final"].toDouble()));
    mId = data["id"].toInt();
    QDate date = data["payment_date"].toDate();
    ui->linePayment->setText(data["payment_number"].toString());
    if(date.isValid())
        ui->datePayment->setDate(data["payment_date"].toDate());
    else
        ui->datePayment->setDate(QDate::currentDate());
}

void PurchasePaymentDialog::messageReceived(LibG::Message *msg)
{
    if(msg->isTypeCommand(MSG_TYPE::PURCHASE, MSG_COMMAND::UPDATE)) {
        if(msg->isSuccess()) {
            FlashMessageManager::showMessage(tr("Purchase payment edited successfully"));
            close();
        } else {
            QMessageBox::critical(this, tr("Error"), msg->data("error").toString());
            ui->pushSave->setEnabled(true);
        }
    }
}

void PurchasePaymentDialog::saveClicked()
{
    Message msg(MSG_TYPE::PURCHASE, MSG_COMMAND::UPDATE);
    QVariantMap data;
    data.insert("payment_number", ui->linePayment->text());
    data.insert("payment_date", ui->datePayment->date());
    msg.addData("id", mId);
    msg.addData("data", data);
    sendMessage(&msg);
    ui->pushSave->setEnabled(false);
}
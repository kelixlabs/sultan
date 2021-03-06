/*
 * cashierwidget.cpp
 * Copyright 2017 - ~, Apin <apin.klas@gmail.com>
 *
 * This file is part of Sultan.
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
#include "cashierwidget.h"
#include "ui_cashierwidget.h"
#include "global_constant.h"
#include "message.h"
#include "db_constant.h"
#include "cashiertablemodel.h"
#include "cashieritem.h"
#include "guiutil.h"
#include "keyevent.h"
#include "paycashdialog.h"
#include "preference.h"
#include "global_setting_const.h"
#include "usersession.h"
#include "printer.h"
#include "escp.h"
#include "paymentcashsuccessdialog.h"
#include "searchitemdialog.h"
#include "transactionlistdialog.h"
#include "usersession.h"
#include "dbutil.h"
#include "saveloadslotdialog.h"
#include "cashierhelpdialog.h"
#include "advancepaymentdialog.h"
#include "flashmessagemanager.h"
#include "paycashlessdialog.h"
#include "checkpricedialog.h"
#include "util.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QKeyEvent>
#include <QShortcut>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QStringBuilder>
#include <QDebug>
#include <functional>

using namespace LibG;
using namespace LibGUI;
using namespace LibPrint;

CashierWidget::CashierWidget(LibG::MessageBus *bus, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CashierWidget),
    mModel(new CashierTableModel(bus, this)),
    mPayCashDialog(new PayCashDialog(this)),
    mAdvancePaymentDialog(new AdvancePaymentDialog(bus, this)),
    mPayCashlessDialog(new PayCashlessDialog(bus, this))
{
    ui->setupUi(this);
    setMessageBus(bus);
    ui->verticalLayout->setAlignment(Qt::AlignTop);
    ui->tableView->setModel(mModel);
    ui->tableView->verticalHeader()->hide();
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    ui->tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    GuiUtil::setColumnWidth(ui->tableView, QList<int>() << 50 << 160 << 150 << 75 << 75 << 100 << 90 << 120);
    ui->tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->labelVersion->setText(CONSTANT::ABOUT_APP_NAME.arg(qApp->applicationVersion()));
    auto keyevent = new KeyEvent(ui->tableView);
    keyevent->addConsumeKey(Qt::Key_Return);
    keyevent->addConsumeKey(Qt::Key_Delete);
    ui->tableView->installEventFilter(keyevent);
    connect(ui->lineBarcode, SIGNAL(returnPressed()), SLOT(barcodeEntered()));
    connect(mModel, SIGNAL(totalChanged(double)), SLOT(totalChanged(double)));
    connect(mModel, SIGNAL(selectRow(QModelIndex)), SLOT(selectRow(QModelIndex)));
    connect(keyevent, SIGNAL(keyPressed(QObject*,QKeyEvent*)), SLOT(tableKeyPressed(QObject*,QKeyEvent*)));
    connect(mPayCashDialog, SIGNAL(requestPay(int,double)), SLOT(payRequested(int,double)));
    connect(mAdvancePaymentDialog, SIGNAL(payRequested(int,double)), SLOT(payRequested(int,double)));
    connect(mPayCashlessDialog, SIGNAL(requestPay(int,double)), SLOT(payRequested(int,double)));
    new QShortcut(QKeySequence(Qt::Key_F4), this, SLOT(payCash()));
    new QShortcut(QKeySequence(Qt::Key_F5), this, SLOT(openDrawer()));
    new QShortcut(QKeySequence(Qt::Key_F2), this, SLOT(openSearch()));
    new QShortcut(QKeySequence(Qt::Key_F6), this, SLOT(openPreviousTransaction()));
    new QShortcut(QKeySequence(Qt::Key_PageDown), this, SLOT(updateLastInputed()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_Delete), this, SLOT(newTransaction()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_N), this, SLOT(newTransaction()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_S), this, SLOT(saveCartTriggered()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_O), this, SLOT(loadCartTriggered()));
    new QShortcut(QKeySequence(Qt::Key_F1), this, SLOT(openHelp()));
    new QShortcut(QKeySequence(Qt::Key_F3), this, SLOT(scanCustomer()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::Key_F3), this, SLOT(resetCustomer()));
    new QShortcut(QKeySequence(Qt::Key_F8), this, SLOT(payAdvance()));
    new QShortcut(QKeySequence(Qt::Key_F9), this, SLOT(payCashless()));
    new QShortcut(QKeySequence(Qt::Key_F7), this, SLOT(openCheckPrice()));
    ui->labelTitle->setText(Preference::getString(SETTING::MARKET_NAME, "Sultan Minimarket"));
    ui->labelSubtitle->setText(GuiUtil::toHtml(Preference::getString(SETTING::MARKET_SUBNAME, "Jln. Bantul\nYogyakarta")));
}

CashierWidget::~CashierWidget()
{
    delete ui;
}

void CashierWidget::showEvent(QShowEvent *event)
{
    ui->lineBarcode->setFocus(Qt::TabFocusReason);
    bool isTax = Preference::getBool(SETTING::USE_TAX);
    if(!isTax) ui->widgetTax->hide();
    else ui->widgetTax->show();
    QWidget::showEvent(event);
}

void CashierWidget::messageReceived(LibG::Message *msg)
{
    if(!msg->isSuccess()) {
        QMessageBox::critical(this, tr("Error"), msg->data("error").toString());
        if(msg->isTypeCommand(MSG_TYPE::ITEM, MSG_COMMAND::CASHIER_PRICE))
            ui->lineBarcode->selectAll();
        return;
    }
    if(msg->isTypeCommand(MSG_TYPE::ITEM, MSG_COMMAND::CASHIER_PRICE)) {
        ui->lineBarcode->clear();
        ui->lineBarcode->setEnabled(true);
        const QString &name = msg->data("item").toMap()["name"].toString();
        const QString &barcode = msg->data("item").toMap()["barcode"].toString();
        const QString &unit = msg->data("item").toMap()["unit"].toString();
        ui->labelName->setText(name);
        const QVariantList &list = msg->data("prices").toList();
        double price = list.first().toMap()["price"].toDouble();
        for(int i = 1; i < list.size(); i++) {
            if(list[i].toMap()["count"].toFloat() == 1.0f) {
                price = list[i].toMap()["price"].toDouble();
                break;
            }
        }
        ui->labelPrice->setText(Preference::toString(price));
        mModel->addItem(mCount, name, barcode, unit, list);
    } else if(msg->isTypeCommand(MSG_TYPE::SOLD, MSG_COMMAND::NEW_SOLD)) {
        const QVariantMap &data = msg->data();
        mPayCashDialog->hide();
        mAdvancePaymentDialog->hide();
        mPayCashlessDialog->hide();
        openDrawer();
        printBill(data);
        PaymentCashSuccessDialog dialog(data, this);
        dialog.exec();
        mModel->reset();
        resetCustomer(true);
        if(mSaveSlot >= 0) removeSlot(mSaveSlot);
    } else if(msg->isTypeCommand(MSG_TYPE::CUSTOMER, MSG_COMMAND::QUERY)) {
        const QList<QVariant> &list = msg->data("data").toList();
        if(list.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), tr("Customer not found"));
        } else {
            const QVariantMap &d = list.first().toMap();
            mModel->fillCustomer(d);
            updateCustomerLabel();
        }
    }
}

void CashierWidget::cutPaper()
{
    if(!Preference::getBool(SETTING::PRINTER_CASHIER_AUTOCUT)) return;
    const QString &command = Escp::cutPaperCommand();
    int type = Preference::getInt(SETTING::PRINTER_CASHIER_TYPE);
    Printer::instance()->print(type == PRINT_TYPE::DEVICE ? Preference::getString(SETTING::PRINTER_CASHIER_DEVICE) : Preference::getString(SETTING::PRINTER_CASHIER_NAME),
                               command, type);
}

void CashierWidget::saveToSlot(int slot)
{
    QDir dir = QDir::home();
    dir.mkdir(".sultan");
    dir.cd(".sultan");
    QFile file(dir.absoluteFilePath(QString("trans_%1.trans").arg(slot)));
    if(file.exists()) file.remove();
    if(!file.open(QFile::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Unable to open save file"));
        return;
    }
    QVariantMap obj;
    obj.insert("customer", mModel->getCustomer()->toMap());
    obj.insert("cart", mModel->getCart());
    const QJsonDocument &doc = QJsonDocument::fromVariant(obj);
    file.write(qCompress(doc.toJson(QJsonDocument::Compact)));
    //file.write(doc.toJson());
    file.close();
}

void CashierWidget::loadFromSlot(int slot)
{
    QDir dir = QDir::home();
    dir.mkdir(".sultan");
    dir.cd(".sultan");
    QFile file(dir.absoluteFilePath(QString("trans_%1.trans").arg(slot)));
    if(!file.exists()) {
        QMessageBox::critical(this, tr("Error"), tr("File not exists"));
        return;
    }
    if(!file.open(QFile::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Unable to open file"));
        return;
    }
    QJsonParseError err;
    const QJsonDocument &doc = QJsonDocument::fromJson(qUncompress(file.readAll()), &err);
    if(err.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, tr("Error"), tr("Error parsing json file"));
        return;
    }
    QJsonObject obj = doc.object();
    mModel->loadCart(obj.value("cart").toArray().toVariantList());
    mModel->fillCustomer(obj.value("customer").toObject().toVariantMap());
    updateCustomerLabel();
    ui->tableView->selectRow(mModel->rowCount(QModelIndex()) - 1);
    file.close();
}

void CashierWidget::removeSlot(int slot)
{
    QDir dir(qApp->applicationDirPath());
    QFile file(dir.absoluteFilePath(QString("trans_%1.trans").arg(slot)));
    if(file.exists()) file.remove();
    mSaveSlot = -1;
}

void CashierWidget::updateCustomerLabel()
{
    auto cust = mModel->getCustomer();
    ui->labelCustomerNumber->setText(cust->number.isEmpty() ? tr("None") : cust->number);
    ui->labelCustomerName->setText(cust->name.isEmpty() ? tr("None") : cust->name);
    ui->labelCustomerPoin->setText(QString::number(cust->reward));
}

double CashierWidget::getTax()
{
    return Preference::getBool(SETTING::USE_TAX) ?
                Util::calculateDiscount(Preference::getString(SETTING::TAX_VALUE), mModel->getTotal()) : 0;
}

void CashierWidget::barcodeEntered()
{
    QString barcode = ui->lineBarcode->text();
    if(barcode.isEmpty()) return;
    if(barcode.contains("*")) {
        const QStringList ls = barcode.split("*");
        if(ls.size() > 1) {
            barcode = ls[1];
            mCount = ls[0].toFloat();
        }
    } else {
        mCount = 1.0f;
    }
    if(Preference::getBool(SETTING::CASHIER_NAMEBASED)) {
        SearchItemDialog dialog(mMessageBus, this);
        dialog.setNameField(ui->lineBarcode->text());
        dialog.exec();
        const QString &barcode = dialog.getSelectedBarcode();
        if(barcode.isEmpty()) return;
        LibG::Message msg(MSG_TYPE::ITEM, MSG_COMMAND::CASHIER_PRICE);
        msg.addData("barcode", barcode);
        sendMessage(&msg);
    } else {
        LibG::Message msg(MSG_TYPE::ITEM, MSG_COMMAND::CASHIER_PRICE);
        msg.addData("barcode", barcode);
        sendMessage(&msg);
    }
}

void CashierWidget::totalChanged(double value)
{
    bool isTax = Preference::getBool(SETTING::USE_TAX);
    if(!isTax) {
        ui->labelTotal->setText(Preference::toString(value));
    } else {
        double tax = Util::calculateDiscount(Preference::getString(SETTING::TAX_VALUE), value);
        ui->labelSubTotal->setText(Preference::toString(value));
        ui->labelTax->setText(Preference::toString(tax));
        ui->labelTotal->setText(Preference::toString(value + tax));
    }
}

void CashierWidget::selectRow(const QModelIndex &index)
{
    ui->tableView->selectRow(index.row());
    ui->tableView->scrollTo(index);
}

void CashierWidget::tableKeyPressed(QObject */*sender*/, QKeyEvent *event)
{
    const QModelIndex &index = ui->tableView->currentIndex();
    if(!index.isValid()) return;
    auto item = static_cast<CashierItem*>(index.internalPointer());
    if(event->key() == Qt::Key_Return && !item->isReturn()) {
        bool ok = false;
        double count = QInputDialog::getDouble(this, tr("Edit count"), tr("Input new count"), item->count, 0, 1000000, 1, &ok);
        if(ok)
            mModel->addItem(count - item->count, item->name, item->barcode, item->unit);
    } else if(event->key() == Qt::Key_Delete){
        if(item->isReturn()) {
            mModel->removeReturn(item);
        } else {
            mModel->addItem(-item->count, item->name, item->barcode, item->unit);
        }
    }
}

void CashierWidget::payCash()
{
    if(mModel->isEmpty()) return;
    mPayCashDialog->fill(mModel->getTotal() + getTax());
    mPayCashDialog->show();
}

void CashierWidget::payCashless()
{
    if(mModel->isEmpty()) return;
    mPayCashlessDialog->showDialog(mModel->getTotal() + getTax());
}

void CashierWidget::payAdvance()
{
    if(mModel->isEmpty()) return;
    if(!mModel->getCustomer()->isValid()) {
        FlashMessageManager::showMessage(tr("Advance payment only for valid customer"), FlashMessage::Error);
        return;
    }
    mAdvancePaymentDialog->setup(mModel->getTotal() + getTax(), mModel->getCustomer());
    mAdvancePaymentDialog->show();
}

void CashierWidget::openDrawer()
{
    if(!Preference::getBool(SETTING::PRINTER_CASHIER_KICK)) return;
    const QString &command = Escp::openDrawerCommand();
    int type = Preference::getInt(SETTING::PRINTER_CASHIER_TYPE);
    Printer::instance()->print(type == PRINT_TYPE::DEVICE ? Preference::getString(SETTING::PRINTER_CASHIER_DEVICE) : Preference::getString(SETTING::PRINTER_CASHIER_NAME),
                               command, type);
}

void CashierWidget::updateLastInputed()
{
    const QModelIndex &index = ui->tableView->currentIndex();
    if(!index.isValid()) return;
    auto item = static_cast<CashierItem*>(index.internalPointer());
    if(item->isReturn()) return;
    bool ok = false;
    double count = QInputDialog::getDouble(this, tr("Edit count"), tr("Input new count"), item->count, 0, 1000000, 1, &ok);
    if(ok)
        mModel->addItem(count - item->count, item->name, item->barcode, item->unit);
}

void CashierWidget::payRequested(int type, double value)
{
    QVariantMap data;
    double tax = getTax();
    data.insert("number", Util::genSoldNumber());
    data.insert("cart", mModel->getCart());
    data.insert("user_id", UserSession::id());
    data.insert("machine_id", Preference::getInt(SETTING::MACHINE_ID));
    data.insert("subtotal", mModel->getTotal());
    data.insert("tax", tax);
    if(type == PAYMENT::CASH) {
        data.insert("payment", value);
        data.insert("total", mModel->getTotal() + tax);
        data.insert("bank_id", 0);
    } else {
        data.insert("payment", mModel->getTotal());
        data.insert("additional_charge", value);
        data.insert("total", mModel->getTotal() + value + tax);
        data.insert("card_number", mPayCashlessDialog->getCardNumber());
        data.insert("bank_id", mPayCashlessDialog->getBank());
        data.insert("card_type", mPayCashlessDialog->getCardType());
    }
    data.insert("customer_id", mModel->getCustomer()->id);
    data.insert("payment_type", type);
    data.insert("reward", mModel->getRewardPoin());
    Message msg(MSG_TYPE::SOLD, MSG_COMMAND::NEW_SOLD);
    msg.setData(data);
    sendMessage(&msg);
}

void CashierWidget::printBill(const QVariantMap &data)
{
    int type = Preference::getInt(SETTING::PRINTER_CASHIER_TYPE, -1);
    bool isTax = Preference::getBool(SETTING::USE_TAX);
    if(type < 0) {
        QMessageBox::critical(this, tr("Error"), tr("Please setting printer first"));
        return;
    }
    int paymentType = data["payment_type"].toInt();
    const QString &prName = Preference::getString(SETTING::PRINTER_CASHIER_NAME);
    const QString &prDevice = Preference::getString(SETTING::PRINTER_CASHIER_DEVICE);
    const QString &title = Preference::getString(SETTING::PRINTER_CASHIER_TITLE, "Sultan Minimarket");
    const QString &subtitle = Preference::getString(SETTING::PRINTER_CASHIER_SUBTITLE, "Jogonalan Lor RT 2 Bantul");
    const QString &footer = Preference::getString(SETTING::PRINTER_CASHIER_FOOTER, "Barang dibeli tidak dapat ditukar");
    int cpi10 = Preference::getInt(SETTING::PRINTER_CASHIER_CPI10, 32);
    int cpi12 = Preference::getInt(SETTING::PRINTER_CASHIER_CPI12, 40);

    auto escp = new Escp(Escp::SIMPLE, cpi10, cpi12);
    escp->cpi10()->doubleHeight(true)->centerText(title)->newLine()->doubleHeight(false)->cpi12()->
            centerText(subtitle)->newLine(2);
    escp->leftText(LibDB::DBUtil::sqlDateToDateTime(data["created_at"].toString()).toString("dd-MM-yy hh:mm"))->newLine();
    escp->column(QList<int>{50, 50})->leftText(data["number"].toString())->rightText(UserSession::username());
    escp->newLine()->column(QList<int>())->line(QChar('='));
    const QVariantList &l = data["cart"].toList();
    for(auto v : l) {
        QVariantMap m = v.toMap();
        escp->leftText(m["name"].toString())->newLine();
        QString s = QString("%1 x %2").
                arg(Preference::toString(m["count"].toFloat())).
                 arg(Preference::toString(m["price"].toDouble()));
        if(m["discount"].toDouble() != 0) {
            s = s % " (" % Preference::toString(-m["discount"].toDouble()) % ")";
        }
        escp->column(QList<int>{70, 30})->leftText(s);
        escp->rightText(Preference::toString(m["final"].toDouble()))->column(QList<int>())->newLine();
    }
    escp->line();
    escp->column(QList<int>{50, 50});
    if(isTax) {
        escp->leftText(tr("Sub-total"))->rightText(Preference::toString(data["subtotal"].toDouble()))->newLine()->
                leftText(tr("Tax"))->rightText(Preference::toString(data["tax"].toDouble()))->newLine();
        if(paymentType == PAYMENT::NON_CASH)
            escp->leftText(tr("Card Charge"))->rightText(Preference::toString(data["additional_charge"].toDouble()))->newLine();
        escp->leftText(tr("Total"))->rightText(Preference::toString(data["total"].toDouble()))->newLine();
    } else {
        if(paymentType == PAYMENT::NON_CASH) {
            escp->leftText(tr("Sub-total"))->rightText(Preference::toString(data["subtotal"].toDouble()))->newLine()->
                leftText(tr("Card Charge"))->rightText(Preference::toString(data["additional_charge"].toDouble()))->newLine();
        }
        escp->leftText(tr("Total"))->rightText(Preference::toString(data["total"].toDouble()))->newLine();
    }
    if(paymentType == PAYMENT::CASH) {
        escp->leftText(tr("Payment"))->rightText(Preference::toString(data["payment"].toDouble()))->newLine()->
            leftText(tr("Change"))->rightText(Preference::toString(data["payment"].toDouble() - data["total"].toDouble()))->newLine();
    } else {
        escp->leftText(tr("Card Number"))->rightText(data["card_number"].toString())->newLine();
    }
    escp->column(QList<int>())->doubleHeight(false);
    if(data.contains("customer")) {
        const QVariantMap &cust = data["customer"].toMap();
        escp->line('=')->column(QList<int>{50, 50})->leftText(tr("Cust Number"))->rightText(cust["number"].toString())->newLine();
        escp->leftText(tr("Reward Poin"))->rightText(QString::number(cust["reward"].toInt()))->newLine();
        double credit = cust["credit"].toDouble();
        if(credit > 0) {
            escp->leftText(tr("Credit"))->rightText(Preference::toString(cust["credit"].toDouble()))->newLine();
        }
    }
    escp->column(QList<int>())->doubleHeight(false)->line()->leftText(footer, true)->newLine(Preference::getInt(SETTING::PRINTER_CASHIER_LINEFEED, 3));
    Printer::instance()->print(type == PRINT_TYPE::DEVICE ? prDevice : prName, escp->data(), type);
    delete escp;
    cutPaper();
}

void CashierWidget::openSearch()
{
    SearchItemDialog dialog(mMessageBus, this);
    dialog.exec();
    const QString &barcode = dialog.getSelectedBarcode();
    if(barcode.isEmpty()) return;
    ui->lineBarcode->setText(barcode);
    barcodeEntered();
}

void CashierWidget::openPreviousTransaction()
{
    TransactionListDialog dialog(mMessageBus, this);
    dialog.setPrintFunction(std::bind(&CashierWidget::printBill, this, std::placeholders::_1));
    dialog.exec();
}

void CashierWidget::newTransaction()
{
    if(mModel->isEmpty()) return;
    if(mSaveSlot >= 0) {
        saveToSlot(mSaveSlot);
    } else {
        int res = QMessageBox::question(this, tr("New transaction confirmation"), tr("Are you sure want to ignore this transaction and start new one?"));
        if(res != QMessageBox::Yes) return;
    }
    mModel->reset();
    resetCustomer(true);
    mSaveSlot = -1;
}

void CashierWidget::saveCartTriggered()
{
    if(mModel->isEmpty()) return;
    if(mSaveSlot < 0) {
        SaveLoadSlotDialog dialog(true, this);
        dialog.exec();
        if(dialog.getSelectedSlot() < 0) return;
        saveToSlot(dialog.getSelectedSlot());
        mSaveSlot = dialog.getSelectedSlot();
    }
}

void CashierWidget::loadCartTriggered()
{
    if(!mModel->isEmpty() && mSaveSlot < 0) {
        int ret = QMessageBox::question(this, tr("Confirmation"), tr("Your cart is not empty, do you want to ignore current cart?"));
        if(ret != QMessageBox::Yes) {
            return;
        }
    } else if(!mModel->isEmpty() && mSaveSlot >= 0) {
        saveToSlot(mSaveSlot);
    }
    SaveLoadSlotDialog dialog(false, this);
    dialog.exec();
    if(dialog.getSelectedSlot() < 0) return;
    loadFromSlot(dialog.getSelectedSlot());
    mSaveSlot = dialog.getSelectedSlot();
}

void CashierWidget::openHelp()
{
    CashierHelpDialog dialog(this);
    dialog.exec();
}

void CashierWidget::scanCustomer()
{
    const QString &str = QInputDialog::getText(this, tr("Input Customer"), tr("Scan customer ID"));
    if(!str.isEmpty()) {
        Message msg(MSG_TYPE::CUSTOMER, MSG_COMMAND::QUERY);
        msg.addFilter("number", COMPARE::EQUAL, str);
        sendMessage(&msg);
    }
}

void CashierWidget::resetCustomer(bool dontShowMessage)
{
    if(!dontShowMessage)
        FlashMessageManager::showMessage(tr("Customer reseted"));
    mModel->getCustomer()->reset();
    updateCustomerLabel();
}

void CashierWidget::openCheckPrice()
{
    QString barcode = "";
    const QModelIndex &index = ui->tableView->currentIndex();
    if(index.isValid() && GuiUtil::isWidgetFocused(ui->tableView)) {
        auto item = static_cast<CashierItem*>(index.internalPointer());
        barcode = item->barcode;
    }
    CheckPriceDialog dialog(mMessageBus, barcode, this);
    dialog.exec();
}

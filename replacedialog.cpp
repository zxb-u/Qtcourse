#include "replacedialog.h"
#include "ui_replacedialog.h"
#include <QMessageBox>

ReplaceDialog::ReplaceDialog(QWidget *parent, QPlainTextEdit * textEdit) :
    QDialog(parent),
    ui(new Ui::ReplaceDialog)
{
    ui->setupUi(this);
    pTextEdit = textEdit;

    ui->rbDown->setChecked(true);
}

ReplaceDialog::~ReplaceDialog()
{
    delete ui;
}

void ReplaceDialog::on_btFindNext_clicked()
{
    QString target = ui->searchText->text();
    if (target.isEmpty() || pTextEdit == nullptr) return;

    QString text = pTextEdit->toPlainText();
    QTextCursor c = pTextEdit->textCursor();
    int index = -1;
    Qt::CaseSensitivity cs = ui->cbCaseSensetive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;

    if (ui->rbDown->isChecked()) {
        index = text.indexOf(target, c.position(), cs);
        if (index < 0) {
            if (QMessageBox::question(this, "提示", "已到达文档末尾，是否从开头继续？") == QMessageBox::Yes) {
                c.setPosition(0);
                pTextEdit->setTextCursor(c);
                index = text.indexOf(target, 0, cs);
            }
        }else {
            c.setPosition(index);
            c.setPosition(index + target.length(), QTextCursor::KeepAnchor);
            pTextEdit->setTextCursor(c);
        }
    }
    else if (ui->rbUp->isChecked()) {
        int startPos = qMax(0, c.position() - 1);
        index = text.lastIndexOf(target, startPos, cs);
        if (index < 0) {
            if (QMessageBox::question(this, "提示", "已到达文档开头，是否从末尾继续？") == QMessageBox::Yes) {
                c.setPosition(text.length());
                pTextEdit->setTextCursor(c);
                index = text.lastIndexOf(target, text.length() - 1, cs);
            }
        }else {
            c.setPosition(index + target.length());
            c.setPosition(index, QTextCursor::KeepAnchor);
            pTextEdit->setTextCursor(c);
        }
    }

    if (index < 0) {
        QMessageBox msg(this);
        msg.setWindowTitle("提示");
        msg.setText(QString("找不到") + target);
        msg.setWindowFlag(Qt::Drawer);
        msg.setIcon(QMessageBox::Information);
        msg.setStandardButtons(QMessageBox::Ok);
        msg.exec();
    }
}


void ReplaceDialog::on_btReplace_clicked()
{
    QString target = ui->searchText->text();
    QString to = ui->targetText->text();

    if( (pTextEdit !=nullptr) && (target != "") && (to != "") ){
        QString selText = pTextEdit->textCursor().selectedText();

        if(selText == target){
            pTextEdit->insertPlainText(to);
        }

        on_btFindNext_clicked();
    }
}


void ReplaceDialog::on_btReplaceAll_clicked()
{
    QString target = ui->searchText->text();
    QString to = ui->targetText->text();

    if( (pTextEdit !=nullptr) && (target != "") && (to != "") ){
        QString text = pTextEdit->toPlainText();

        text.replace(target, to, ui->cbCaseSensetive->isChecked() ?
                         Qt::CaseSensitive : Qt::CaseInsensitive);

        pTextEdit->clear();
        pTextEdit->insertPlainText(text);

    }
}


void ReplaceDialog::on_btCancel_clicked()
{
    accept();
}


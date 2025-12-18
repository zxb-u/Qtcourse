#ifndef LOGINVIEW_H
#define LOGINVIEW_H

#include <QWidget>

namespace Ui {
class LoginView;
}

class LoginView : public QWidget
{
    Q_OBJECT

public:
    explicit LoginView(QWidget *parent = nullptr);
    ~LoginView();



signals:
    void loginSuccess();
    void loginFailed();

private slots:
    void on_btSignin_clicked();

private:
    Ui::LoginView *ui;
};

#endif // LOGINVIEW_H

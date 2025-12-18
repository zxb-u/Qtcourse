#ifndef IDATABASE_H
#define IDATABASE_H

#include <QObject>
#include <QtSql>
#include <QSqlDatabase>

class IDatabase : public QObject
{
    Q_OBJECT
public:
    static IDatabase &getInstance()
    {
        static IDatabase instance;
        return instance;
    }

QString userLogin(QString userName, QString password);

private:
    explicit IDatabase(QObject *parent = nullptr);
    IDatabase(IDatabase const &) = delete;
    void operator=(IDatabase const &) = delete;
    void ininDatabase();

    QSqlDatabase database;

signals:

public:
    bool initPatientModel();
    bool searchPatient(QString filter);
    int addNewPatient();
    bool deleteCurrentPatient();
    bool submitPatientEdit();
    void revertPatientEdit();

    QSqlTableModel *patientTabModel; //数据模型
    QItemSelectionModel *thePatientSelection; //选择模型
};

#endif // IDATABASE_H

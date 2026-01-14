#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QLineEdit>
// Qt Charts头文件（兼容所有Qt版本）
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QValueAxis>
#include <QCategoryAxis>
#include <QBarCategoryAxis>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_filterBtn_clicked();
    void on_saveScoreBtn_clicked();
    void on_subjectComboBox_currentTextChanged(const QString &subject);
    void on_calcScoreBtn_clicked();
    void on_scoreInputBtn_clicked();
    void on_showChartBtn_clicked();
    void on_classCourseFilterChanged();
    void on_exportExcelBtn_clicked();  // ！！！添加这一行！！！

private:
    Ui::MainWindow *ui;

    // 数据库相关
    QSqlDatabase db;
    QStandardItemModel *scoreModel = nullptr;
    QSortFilterProxyModel *filterModel = nullptr;
    QList<int> courseIds;
    QList<QString> courseNames;
    QString currentClass = "全部";
    QMap<int, QMap<int, int>> scoreMap;

    // 编辑相关
    QLineEdit *currentEditor = nullptr;
    bool isInputMode = false;

    // 趋势图相关（兼容所有Qt版本）
    QChart *scoreChart = nullptr;
    QChartView *chartView = nullptr;

    // 核心函数
    void initDatabase();
    void loadAllCourses();
    void initFilterModel();
    void loadAllData(QString cls = "全部");
    void updateTotalScore(int row);
    void clearCurrentEditor();
    void setTableReadOnly(bool isReadOnly);
    void createScoreChart();
};

#endif // MAINWINDOW_H

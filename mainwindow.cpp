#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDate>
#include <QIntValidator>
#include <QMessageBox>
#include <QDebug>
#include <QPainter>
#include <QSqlError>
#include <QChart>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QCategoryAxis>
#include <QValueAxis>
#include <QBarCategoryAxis>
#include <QChartView>

// Excel导出需要的头文件
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QFont>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isInputMode(false)
    , currentEditor(nullptr)
{
    ui->setupUi(this);
    this->setWindowTitle("学生成绩管理系统 - 趋势图版");
    this->resize(1400, 800);

    // 初始化趋势图对象
    scoreChart = new QChart();
    chartView = new QChartView(scoreChart);

    // 1. 初始化数据库（仅创建表结构，不插入测试数据）
    initDatabase();
    // 2. 加载课程列表
    loadAllCourses();
    // 3. 初始化下拉框
    ui->classComboBox->clear();
    ui->classComboBox->addItem("全部");
    QSqlQuery q("SELECT DISTINCT class FROM student ORDER BY class");
    while (q.next()) {
        ui->classComboBox->addItem(q.value(0).toString());
    }
    ui->subjectComboBox->clear();
    ui->subjectComboBox->addItems({"数学", "语文", "英语", "总分"});
    ui->chartTypeComboBox->clear();
    ui->chartTypeComboBox->addItems({"按学生", "按班级"});

    // 4. 加载数据+初始化模型（读取数据库现有数据）
    loadAllData("全部");
    initFilterModel();
    setTableReadOnly(true);

    // 绑定信号槽
    connect(ui->subjectComboBox, &QComboBox::currentTextChanged, this, &MainWindow::on_subjectComboBox_currentTextChanged);
    connect(ui->classComboBox, &QComboBox::currentTextChanged, this, &MainWindow::on_classCourseFilterChanged);
    connect(ui->showChartBtn, &QPushButton::clicked, this, &MainWindow::on_showChartBtn_clicked);
    connect(ui->subjectComboBox, &QComboBox::currentTextChanged, this, &MainWindow::on_showChartBtn_clicked);
    connect(ui->classComboBox, &QComboBox::currentTextChanged, this, &MainWindow::on_showChartBtn_clicked);
    connect(ui->chartTypeComboBox, &QComboBox::currentTextChanged, this, &MainWindow::on_showChartBtn_clicked);
    connect(ui->exportExcelBtn, &QPushButton::clicked,this, &MainWindow::on_exportExcelBtn_clicked);

    // 初始化趋势图容器
    if (ui->chartWidget) {
        chartView->setParent(ui->chartWidget);
        chartView->setGeometry(0, 0, ui->chartWidget->width(), ui->chartWidget->height());
        chartView->setRenderHint(QPainter::Antialiasing);
    }

    // 双击编辑逻辑（最终修复版）
    connect(ui->studentTableView, &QTableView::doubleClicked, this, [=](const QModelIndex &proxyIndex) {
        if (!isInputMode) return;
        // 1. 跳过非成绩列（学生ID、姓名、班级、总分）
        int proxyCol = proxyIndex.column();
        if (proxyCol < 3 || proxyCol >= filterModel->columnCount() - 1) return;

        // 2. 转换为源模型索引
        QModelIndex sourceIndex = filterModel->mapToSource(proxyIndex);
        int sourceRow = sourceIndex.row();
        int sourceCol = sourceIndex.column();

        // 3. 获取学生ID和课程ID
        int stuId = scoreModel->item(sourceRow, 0)->text().toInt();
        int courseColIndex = sourceCol - 3;
        if (courseColIndex < 0 || courseColIndex >= courseIds.size()) return;
        int courseId = courseIds[courseColIndex];

        // 4. 创建编辑框
        clearCurrentEditor();
        currentEditor = new QLineEdit(ui->studentTableView);
        currentEditor->setValidator(new QIntValidator(0, 100, currentEditor));
        currentEditor->setText(scoreModel->item(sourceRow, sourceCol)->text());
        currentEditor->selectAll();

        // 5. 编辑完成后同步数据
        connect(currentEditor, &QLineEdit::editingFinished, this, [=]() {
            bool ok;
            int newScore = currentEditor->text().toInt(&ok);
            if (ok && newScore >= 0 && newScore <= 100) {
                // 更新模型显示
                scoreModel->setItem(sourceRow, sourceCol, new QStandardItem(QString::number(newScore)));
                // 强制同步scoreMap
                scoreMap[stuId][courseId] = newScore;
                // 更新总分
                updateTotalScore(sourceRow);
            } else {
                QMessageBox::warning(this, "输入错误", "请输入0-100的有效分数！");
            }
            clearCurrentEditor();
        });

        // 6. 显示编辑框
        ui->studentTableView->setIndexWidget(proxyIndex, currentEditor);
        currentEditor->setFocus();
    });
}

MainWindow::~MainWindow()
{
    clearCurrentEditor();
    delete chartView;
    delete scoreChart;
    delete filterModel;
    delete scoreModel;
    delete ui;
}

// 1. 初始化数据库（仅创建表结构，不插入任何测试数据）
void MainWindow::initDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("D:/zxb13/score_system.db");
    if (!db.open()) {
        QMessageBox::critical(this, "数据库错误", "连接失败：" + db.lastError().text());
        exit(1);
    }
    QSqlQuery q;
    // 仅创建表结构，不检查数据量，不插入测试数据
    q.exec("CREATE TABLE IF NOT EXISTS student (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, class TEXT)");
    q.exec("CREATE TABLE IF NOT EXISTS course (course_id INTEGER PRIMARY KEY AUTOINCREMENT, course_name TEXT)");
    q.exec("CREATE TABLE IF NOT EXISTS score (id INTEGER PRIMARY KEY AUTOINCREMENT, student_id INTEGER, course_id INTEGER, score INTEGER)");
}

// 2. 加载课程列表（读取数据库现有课程）
void MainWindow::loadAllCourses()
{
    courseIds.clear();
    courseNames.clear();
    QSqlQuery q("SELECT course_id, course_name FROM course ORDER BY course_id ASC");
    while (q.next()) {
        courseIds.append(q.value(0).toInt());
        courseNames.append(q.value(1).toString());
        qDebug() << "课程ID：" << q.value(0).toInt() << " 课程名：" << q.value(1).toString();
    }
}

// 3. 初始化筛选模型
void MainWindow::initFilterModel()
{
    if (filterModel) delete filterModel;
    filterModel = new QSortFilterProxyModel(this);
    if (scoreModel) {
        filterModel->setSourceModel(scoreModel);
        filterModel->setFilterKeyColumn(2);
        filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
        ui->studentTableView->setModel(filterModel);
    }
}

// 4. 加载数据（仅读取数据库现有数据，无默认值插入）
void MainWindow::loadAllData(QString cls)
{
    currentClass = cls;
    scoreMap.clear();
    if (scoreModel) delete scoreModel;
    scoreModel = new QStandardItemModel(this);
    QStringList headers = {"学生ID", "姓名", "班级"};
    headers.append(courseNames);
    headers.append("总分");
    scoreModel->setHorizontalHeaderLabels(headers);

    QSqlQuery stuQuery;
    if (cls == "全部") {
        stuQuery.exec("SELECT id, name, class FROM student ORDER BY id");
    } else {
        stuQuery.exec(QString("SELECT id, name, class FROM student WHERE class='%1' ORDER BY id").arg(cls));
    }

    int row = 0;
    while (stuQuery.next()) {
        int stuId = stuQuery.value(0).toInt();
        QStandardItem *idItem = new QStandardItem(stuQuery.value(0).toString());
        idItem->setEditable(false);
        scoreModel->setItem(row, 0, idItem);
        QStandardItem *nameItem = new QStandardItem(stuQuery.value(1).toString());
        nameItem->setEditable(false);
        scoreModel->setItem(row, 1, nameItem);
        QStandardItem *classItem = new QStandardItem(stuQuery.value(2).toString());
        classItem->setEditable(false);
        scoreModel->setItem(row, 2, classItem);

        int totalScore = 0;
        // 遍历课程列表，读取数据库现有成绩
        for (int i = 0; i < courseIds.size(); i++) {
            int courseId = courseIds[i];
            QSqlQuery scoreQ;
            scoreQ.prepare("SELECT score FROM score WHERE student_id = ? AND course_id = ?");
            scoreQ.addBindValue(stuId);
            scoreQ.addBindValue(courseId);
            scoreQ.exec();

            QString scoreStr = "0"; // 数据库无数据时显示0
            if (scoreQ.next()) {
                scoreStr = scoreQ.value(0).toString();
                totalScore += scoreQ.value(0).toInt();
                scoreMap[stuId][courseId] = scoreQ.value(0).toInt();
                qDebug() << "读取成绩：学生ID" << stuId << "课程ID" << courseId << "分数" << scoreStr;
            }
            QStandardItem *scoreItem = new QStandardItem(scoreStr);
            scoreModel->setItem(row, 3+i, scoreItem);
        }
        QStandardItem *totalItem = new QStandardItem(QString::number(totalScore));
        totalItem->setEditable(false);
        scoreModel->setItem(row, 3+courseNames.size(), totalItem);
        row++;
    }
    ui->studentTableView->horizontalHeader()->setStretchLastSection(true);
    ui->studentTableView->setAlternatingRowColors(true);
}

// 5. 更新总分
void MainWindow::updateTotalScore(int row)
{
    if (row < 0 || row >= scoreModel->rowCount()) return;
    int total = 0;
    for (int i = 0; i < courseNames.size(); i++) {
        QStandardItem *item = scoreModel->item(row, 3+i);
        if (item) {
            bool ok;
            int score = item->text().toInt(&ok);
            if (ok) total += score;
        }
    }
    QStandardItem *totalItem = scoreModel->item(row, 3+courseNames.size());
    if (totalItem) {
        totalItem->setText(QString::number(total));
    }
}

// 6. 清理编辑框
void MainWindow::clearCurrentEditor()
{
    if (currentEditor) {
        ui->studentTableView->setIndexWidget(ui->studentTableView->currentIndex(), nullptr);
        currentEditor->deleteLater();
        currentEditor = nullptr;
    }
}

// 7. 设置表格只读
void MainWindow::setTableReadOnly(bool isReadOnly)
{
    if (!scoreModel) return;
    int totalCol = 3 + courseNames.size();
    for (int row = 0; row < scoreModel->rowCount(); row++) {
        for (int col = 3; col < totalCol; col++) {
            QStandardItem *item = scoreModel->item(row, col);
            if (item) item->setEditable(!isReadOnly);
        }
    }
}

// 8. 班级筛选
void MainWindow::on_classCourseFilterChanged()
{
    if (!filterModel) return;
    QString targetClass = ui->classComboBox->currentText();
    filterModel->setFilterFixedString("");
    if (targetClass != "全部") {
        filterModel->setFilterFixedString(targetClass);
    }
}

// ========== 槽函数 ==========
void MainWindow::on_filterBtn_clicked()
{
    clearCurrentEditor();
    if (isInputMode && !scoreMap.isEmpty()) {
        QMessageBox::warning(this, "提示", "筛选会重置数据，建议先保存已修改的成绩！");
    }
    loadAllData(ui->classComboBox->currentText());
    setTableReadOnly(!isInputMode);
    initFilterModel();
    QMessageBox::information(this, "筛选完成", "已显示【" + ui->classComboBox->currentText() + "】的学生数据");
    on_subjectComboBox_currentTextChanged(ui->subjectComboBox->currentText());
}

// 保存成绩（仅更新/插入数据库现有数据，无默认数据）
void MainWindow::on_saveScoreBtn_clicked()
{
    clearCurrentEditor();
    qDebug() << "===== 保存前scoreMap数据 =====";
    for (auto stuIt = scoreMap.begin(); stuIt != scoreMap.end(); ++stuIt) {
        int stuId = stuIt.key();
        qDebug() << "学生ID：" << stuId;
        for (auto courseIt = stuIt.value().begin(); courseIt != stuIt.value().end(); ++courseIt) {
            int courseId = courseIt.key();
            int score = courseIt.value();
            qDebug() << "  课程ID：" << courseId << " 分数：" << score;
        }
    }

    if (scoreMap.isEmpty()) {
        QMessageBox::warning(this, "提示", "暂无需要保存的成绩数据！");
        return;
    }
    if (!db.transaction()) {
        QMessageBox::warning(this, "错误", "开启事务失败：" + db.lastError().text());
        return;
    }
    bool saveSuccess = true;
    for (auto stuIt = scoreMap.begin(); stuIt != scoreMap.end() && saveSuccess; ++stuIt) {
        int stuId = stuIt.key();
        for (auto courseIt = stuIt.value().begin(); courseIt != stuIt.value().end() && saveSuccess; ++courseIt) {
            int courseId = courseIt.key();
            int score = courseIt.value();

            // 先查询是否存在记录
            QSqlQuery checkExist;
            checkExist.prepare("SELECT id FROM score WHERE student_id=? AND course_id=?");
            checkExist.addBindValue(stuId);
            checkExist.addBindValue(courseId);
            if (checkExist.exec() && checkExist.next()) {
                // 存在则更新
                QSqlQuery updateQ;
                updateQ.prepare("UPDATE score SET score=? WHERE id=?");
                updateQ.addBindValue(score);
                updateQ.addBindValue(checkExist.value(0).toInt());
                if (!updateQ.exec()) {
                    saveSuccess = false;
                    qDebug() << "更新失败：" << updateQ.lastError().text();
                }
            } else {
                // 不存在则插入（仅插入修改过的数据）
                QSqlQuery insertQ;
                insertQ.prepare("INSERT INTO score (student_id, course_id, score) VALUES (?, ?, ?)");
                insertQ.addBindValue(stuId);
                insertQ.addBindValue(courseId);
                insertQ.addBindValue(score);
                if (!insertQ.exec()) {
                    saveSuccess = false;
                    qDebug() << "插入失败：" << insertQ.lastError().text();
                }
            }
        }
    }
    if (saveSuccess) {
        db.commit();
        QMessageBox::information(this, "保存成功", "所有成绩已同步到数据库！");
        // 保存后重载数据，确保显示最新值
        loadAllData(currentClass);
        initFilterModel();
        on_subjectComboBox_currentTextChanged(ui->subjectComboBox->currentText());
        // 移除这一行：on_showChartBtn_clicked();
    } else {
        db.rollback();
    }
}
void MainWindow::on_subjectComboBox_currentTextChanged(const QString &subject)
{
    if (!scoreModel) {
        ui->avgScoreLabel->setText("平均分：0");
        return;
    }
    int count = 0;
    double sum = 0.0;
    int totalCol = scoreModel->columnCount() - 1;
    QString targetClass = ui->classComboBox->currentText();
    for (int row = 0; row < scoreModel->rowCount(); row++) {
        QString studentClass = scoreModel->item(row, 2)->text();
        if (targetClass != "全部" && studentClass != targetClass) continue;
        if (subject == "总分") {
            double total = scoreModel->item(row, totalCol)->text().toDouble();
            sum += total;
            count++;
        } else {
            int col = -1;
            for (int i = 0; i < courseNames.size(); i++) {
                if (courseNames[i].trimmed() == subject.trimmed()) {
                    col = 3 + i;
                    break;
                }
            }
            if (col != -1) {
                double score = scoreModel->item(row, col)->text().toDouble();
                sum += score;
                count++;
            }
        }
    }
    if (count > 0) {
        double avg = sum / count;
        ui->avgScoreLabel->setText(QString("平均分：%1").arg(avg, 0, 'f', 1));
    } else {
        ui->avgScoreLabel->setText("平均分：0");
        QMessageBox::warning(this, "提示", "暂无【" + targetClass + "】的" + subject + "成绩数据！");
    }
}

void MainWindow::on_calcScoreBtn_clicked()
{
    on_subjectComboBox_currentTextChanged(ui->subjectComboBox->currentText());
}

void MainWindow::on_scoreInputBtn_clicked()
{
    clearCurrentEditor();
    isInputMode = !isInputMode;
    setTableReadOnly(!isInputMode);
    if (isInputMode) {
        ui->scoreInputBtn->setText("退出录入");
        QMessageBox::information(this, "模式切换", "已进入成绩录入模式，双击成绩列可编辑（0-100分）");
    } else {
        ui->scoreInputBtn->setText("成绩录入");
        QMessageBox::information(this, "模式切换", "已退出录入模式，表格恢复只读");
    }
}

// ========== 趋势图核心功能 ==========
void MainWindow::createScoreChart()
{
    if (!scoreModel || scoreModel->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "暂无成绩数据，无法生成趋势图！");
        return;
    }

    // 清空旧图表
    scoreChart->removeAllSeries();
    QList<QAbstractAxis*> allAxes = scoreChart->axes();
    foreach (QAbstractAxis* axis, allAxes) {
        scoreChart->removeAxis(axis);
    }
    scoreChart->legend()->hide();

    QString subject = ui->subjectComboBox->currentText().trimmed();
    QString chartType = ui->chartTypeComboBox->currentText().trimmed();
    QString targetClass = ui->classComboBox->currentText().trimmed();

    if (chartType == "按学生") {
        // 折线图：按学生显示
        QLineSeries *series = new QLineSeries();
        QStringList studentNames;
        int totalCol = scoreModel->columnCount() - 1;

        int validRow = 0;
        double maxScore = 0.0;
        double minScore = 100.0;

        for (int row = 0; row < scoreModel->rowCount(); row++) {
            QString studentClass = scoreModel->item(row, 2)->text();
            if (targetClass != "全部" && studentClass != targetClass) {
                continue;
            }

            QString name = scoreModel->item(row, 1)->text().trimmed();
            studentNames.append(name);

            double score = 0.0;
            if (subject == "总分") {
                score = scoreModel->item(row, totalCol)->text().toDouble();
            } else {
                int col = -1;
                for (int i = 0; i < courseNames.size(); i++) {
                    if (courseNames[i].trimmed() == subject.trimmed()) {
                        col = 3 + i;
                        break;
                    }
                }
                if (col != -1) {
                    score = scoreModel->item(row, col)->text().toDouble();
                }
            }

            series->append(validRow, score);
            validRow++;

            if (score > maxScore) maxScore = score;
            if (score < minScore) minScore = score;
        }

        if (studentNames.isEmpty()) {
            QMessageBox::warning(this, "提示", "没有找到学生数据！");
            return;
        }

        series->setName(subject + "成绩");
        series->setPen(QPen(QColor(70, 130, 180), 2));
        series->setPointsVisible(true);
        series->setPointLabelsVisible(true);
        series->setPointLabelsFormat("@yPoint");
        series->setPointLabelsFont(QFont("Arial", 9));

        scoreChart->addSeries(series);
        scoreChart->setTitle(QString("%1 - %2成绩趋势图").arg(targetClass).arg(subject));

        // X轴
        QValueAxis *axisX = new QValueAxis();
        axisX->setTitleText("");
        axisX->setRange(0, validRow - 1);
        axisX->setLabelsVisible(false);
        axisX->setGridLineVisible(false);
        axisX->setLineVisible(true);

        scoreChart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);

        // Y轴
        QValueAxis *axisY = new QValueAxis();
        axisY->setTitleText("分数");

        if (subject == "总分") {
            int courseCount = courseNames.size();
            double maxTotal = courseCount * 100.0;
            axisY->setRange(0, maxTotal);
            int tickCount = static_cast<int>(maxTotal / 50.0) + 1;
            if (tickCount < 5) tickCount = 5;
            if (tickCount > 15) tickCount = 15;
            axisY->setTickCount(tickCount);
        } else {
            axisY->setRange(0, 100);
            axisY->setTickCount(11);
        }

        axisY->setLabelFormat("%d");
        axisY->setLabelsFont(QFont("Arial", 10));
        axisY->setGridLineVisible(true);

        scoreChart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);

        scoreChart->legend()->setVisible(true);
        scoreChart->legend()->setAlignment(Qt::AlignBottom);

    } else {
        // 柱状图：按班级显示
        QBarSeries *series = new QBarSeries();
        series->setBarWidth(0.7);
        series->setLabelsVisible(true);
        series->setLabelsFormat("@value分");
        series->setLabelsPosition(QBarSeries::LabelsCenter);

        // 获取所有班级
        QSqlQuery classQuery("SELECT DISTINCT class FROM student ORDER BY class");
        QStringList allClasses;
        QMap<QString, double> classAverages;

        while (classQuery.next()) {
            QString cls = classQuery.value(0).toString().trimmed();
            allClasses.append(cls);
            classAverages[cls] = 0.0;
        }

        if (allClasses.isEmpty()) {
            QMessageBox::warning(this, "提示", "没有找到班级数据！");
            return;
        }

        // 计算每个班级的平均分
        double maxAvg = 0.0;
        foreach (QString cls, allClasses) {
            int count = 0;
            double sum = 0.0;
            int totalCol = scoreModel->columnCount() - 1;

            for (int row = 0; row < scoreModel->rowCount(); row++) {
                if (scoreModel->item(row, 2)->text().trimmed() != cls) continue;

                double score = 0.0;
                if (subject == "总分") {
                    score = scoreModel->item(row, totalCol)->text().toDouble();
                } else {
                    int col = -1;
                    for (int i = 0; i < courseNames.size(); i++) {
                        if (courseNames[i].trimmed() == subject.trimmed()) {
                            col = 3 + i;
                            break;
                        }
                    }
                    if (col != -1) {
                        score = scoreModel->item(row, col)->text().toDouble();
                    }
                }
                sum += score;
                count++;
            }

            double avg = count > 0 ? sum / count : 0.0;
            classAverages[cls] = avg;
            if (avg > maxAvg) maxAvg = avg;
        }

        // 为每个班级创建柱子
        QVector<QColor> colors = {
            QColor(65, 105, 225),
            QColor(220, 20, 60),
            QColor(50, 205, 50),
            QColor(255, 165, 0),
            QColor(138, 43, 226)
        };

        int colorIndex = 0;

        foreach (QString cls, allClasses) {
            double avg = classAverages[cls];
            QBarSet *set = new QBarSet(cls);
            *set << avg;

            set->setColor(colors[colorIndex % colors.size()]);
            set->setBorderColor(Qt::black);
            set->setLabel(QString("%1\n%2分").arg(cls).arg(QString::number(avg, 'f', 1)));

            series->append(set);
            colorIndex++;
        }

        scoreChart->addSeries(series);
        scoreChart->setTitle(QString("%1 - 班级平均成绩对比").arg(subject));

        // X轴
        QBarCategoryAxis *axisX = new QBarCategoryAxis();
        axisX->setTitleText("");
        axisX->setLabelsVisible(false);
        axisX->setGridLineVisible(false);
        axisX->setLineVisible(true);

        axisX->append(allClasses);
        scoreChart->addAxis(axisX, Qt::AlignBottom);
        series->attachAxis(axisX);

        // Y轴
        QValueAxis *axisY = new QValueAxis();
        axisY->setTitleText("平均分");

        if (subject == "总分") {
            int courseCount = courseNames.size();
            double maxPossibleAvg = courseCount * 100.0;
            double suggestedMax = qMax(maxAvg * 1.2, 100.0);
            if (suggestedMax > maxPossibleAvg) {
                suggestedMax = maxPossibleAvg;
            }
            double roundedMax = ceil(suggestedMax / 50.0) * 50.0;
            axisY->setRange(0, roundedMax);
            int tickCount = static_cast<int>(roundedMax / 50.0) + 1;
            axisY->setTickCount(tickCount);
        } else {
            axisY->setRange(0, 100);
            axisY->setTickCount(11);
        }

        axisY->setLabelFormat("%d");
        axisY->setLabelsFont(QFont("Arial", 10));
        axisY->setGridLineVisible(true);

        scoreChart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisY);

        scoreChart->legend()->setVisible(true);
        scoreChart->legend()->setAlignment(Qt::AlignBottom);
        scoreChart->legend()->setFont(QFont("微软雅黑", 9));
    }

    // 设置图表样式
    scoreChart->setBackgroundBrush(QBrush(QColor(240, 240, 240)));
    scoreChart->setAnimationOptions(QChart::SeriesAnimations);

    chartView->setChart(scoreChart);
    chartView->setRenderHint(QPainter::Antialiasing);
    chartView->setRubberBand(QChartView::RectangleRubberBand);
    chartView->setInteractive(true);

    if (ui->chartWidget) {
        chartView->resize(ui->chartWidget->size());
    }
    chartView->show();

    if (ui->chartWidget) {
        ui->chartWidget->update();
    }
}

void MainWindow::on_showChartBtn_clicked()
{
    createScoreChart();
}

// Excel导出按钮点击事件
void MainWindow::on_exportExcelBtn_clicked()
{
    if (!scoreModel || scoreModel->rowCount() == 0) {
        QMessageBox::warning(this, "提示", "没有数据可导出！");
        return;
    }

    QString className = ui->classComboBox->currentText();
    QString subjectName = ui->subjectComboBox->currentText();

    QString defaultName = QString("成绩报表_%1_%2.csv")
                              .arg(className)
                              .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));

    QString fileName = QFileDialog::getSaveFileName(
        this,
        "导出数据",
        QDir::homePath() + "/" + defaultName,
        "CSV文件 (*.csv)"
        );

    if (fileName.isEmpty()) {
        return;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法创建文件！");
        return;
    }

    QTextStream out(&file);

    // 写入表头
    QStringList headers;
    for (int col = 0; col < scoreModel->columnCount(); ++col) {
        QString header = scoreModel->headerData(col, Qt::Horizontal).toString();
        if (header.contains(",")) {
            header = "\"" + header + "\"";
        }
        headers << header;
    }
    out << headers.join(",") << "\n";

    // 写入数据行
    for (int row = 0; row < scoreModel->rowCount(); ++row) {
        QStringList rowData;
        for (int col = 0; col < scoreModel->columnCount(); ++col) {
            QStandardItem *item = scoreModel->item(row, col);
            QString value = item ? item->text() : "";

            if (value.contains(",") || value.contains("\"") || value.contains("\n")) {
                value = "\"" + value.replace("\"", "\"\"") + "\"";
            }

            rowData << value;
        }
        out << rowData.join(",") << "\n";
    }

    file.close();

    QFileInfo fileInfo(fileName);
    QMessageBox::information(this, "导出成功",
                             QString("数据已导出为CSV文件！\n\n文件：%1\n大小：%2 KB\n\n可以用Excel打开查看。")
                                 .arg(fileName)
                                 .arg(fileInfo.size() / 1024));

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "打开文件",
        "是否用Excel打开文件？",
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(fileName));
    }
}

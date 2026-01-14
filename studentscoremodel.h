#ifndef STUDENTSCOREMODEL_H
#define STUDENTSCOREMODEL_H

#include <QAbstractTableModel>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QMap>
#include <QDate>
#include <QColor>
#include <QSqlError>

class StudentScoreModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit StudentScoreModel(QSqlDatabase db, QObject *parent = nullptr);

    // 重写Model核心方法
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // 自定义方法
    void loadData(const QString &courseName);  // 加载学生+课程分数
    bool saveScores();                         // 保存分数到数据库
    int getCourseId(const QString &courseName); // 对外暴露课程ID查询

private:
    QSqlDatabase m_db;                          // 数据库连接
    QString m_selectedCourse;                   // 当前选中课程
    int m_courseId = -1;                        // 当前课程ID
    QList<QMap<QString, QVariant>> m_studentData; // 学生基础数据
    QMap<int, int> m_scoreMap;                  // 编辑后的分数缓存（学生ID->分数）

    // 辅助方法
    int getStudentScore(int stuId, int courseId) const; // 查询学生某课程分数
};

#endif // STUDENTSCOREMODEL_H

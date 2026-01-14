#include "studentscoremodel.h"
#include <QSqlError>
#include <QDebug>

StudentScoreModel::StudentScoreModel(QSqlDatabase db, QObject *parent)
    : QAbstractTableModel(parent), m_db(db)
{
}

// 返回学生行数
int StudentScoreModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_studentData.size();
}

// 返回列数（3列：基础信息；4列：含分数）
int StudentScoreModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return (m_selectedCourse.isEmpty() || m_selectedCourse == "请选择课程") ? 3 : 4;
}

// 读取单元格数据
QVariant StudentScoreModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_studentData.size()) return QVariant();

    const QMap<QString, QVariant> &stu = m_studentData[index.row()];
    int stuId = stu["id"].toInt();

    // 显示/编辑角色
    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case 0: return stu["id"];
        case 1: return stu["name"];
        case 2: return stu["class"];
        case 3: {
            // 优先显示编辑后的分数，无则显示数据库分数
            if (m_scoreMap.contains(stuId)) {
                return QString::number(m_scoreMap[stuId]);
            } else {
                int score = getStudentScore(stuId, m_courseId);
                return score == -1 ? QString("") : QString::number(score);
            }
        }
        default: return QVariant();
        }
    }

    // 分数列高亮样式
    if (role == Qt::BackgroundRole && index.column() == 3) {
        return QColor(245, 250, 255);
    }

    return QVariant();
}

// 写入编辑后的分数
bool StudentScoreModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || index.column() != 3) return false;

    int stuId = m_studentData[index.row()]["id"].toInt();
    QString scoreStr = value.toString().trimmed();

    // 空值清空分数，有效值校验范围
    if (scoreStr.isEmpty()) {
        m_scoreMap.remove(stuId);
    } else {
        bool ok;
        int score = scoreStr.toInt(&ok);
        if (ok && score >= 0 && score <= 100) {
            m_scoreMap[stuId] = score;
        } else {
            return false;
        }
    }

    // 通知视图更新
    emit dataChanged(index, index);
    return true;
}

// 设置单元格权限（分数列可编辑）
Qt::ItemFlags StudentScoreModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::NoItemFlags;
    return (index.column() == 3)
               ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable)
               : (Qt::ItemIsEnabled | Qt::ItemIsSelectable);
}

// 设置表头
QVariant StudentScoreModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return "学生ID";
        case 1: return "姓名";
        case 2: return "班级";
        case 3: return m_selectedCourse + "分数";
        }
    }
    return QVariant();
}

// 加载学生数据和指定课程分数
void StudentScoreModel::loadData(const QString &courseName)
{
    m_selectedCourse = courseName;
    m_courseId = getCourseId(courseName);
    m_scoreMap.clear();

    beginResetModel();
    m_studentData.clear();

    // 查询学生基础数据
    QSqlQuery query(m_db);
    if (query.exec("SELECT id, name, class FROM student ORDER BY id")) {
        while (query.next()) {
            QMap<QString, QVariant> stu;
            stu["id"] = query.value(0).toInt();
            stu["name"] = query.value(1).toString();
            stu["class"] = query.value(2).toString();
            m_studentData.append(stu);
        }
    }
    endResetModel();
}

// 保存分数到数据库（事务保障）
bool StudentScoreModel::saveScores()
{
    if (m_courseId == -1 || m_scoreMap.isEmpty()) return false;

    // 开启事务
    if (!m_db.transaction()) return false;

    QSqlQuery query(m_db);
    bool success = true;

    // 遍历编辑后的分数，新增/更新数据库
    for (auto it = m_scoreMap.begin(); it != m_scoreMap.end(); ++it) {
        int stuId = it.key();
        int score = it.value();

        if (getStudentScore(stuId, m_courseId) != -1) {
            // 更新已有分数
            query.exec(QString("UPDATE score SET score=%1 WHERE student_id=%2 AND course_id=%3")
                           .arg(score).arg(stuId).arg(m_courseId));
        } else {
            // 新增分数
            query.exec(QString("INSERT INTO score (student_id, course_id, score, exam_date) VALUES (%1, %2, %3, '%4')")
                           .arg(stuId).arg(m_courseId).arg(score).arg(QDate::currentDate().toString("yyyy-MM-dd")));
        }

        if (query.lastError().isValid()) {
            success = false;
            break;
        }
    }

    // 提交/回滚事务
    success ? m_db.commit() : m_db.rollback();
    if (success) m_scoreMap.clear();
    return success;
}

// 根据课程名查ID
int StudentScoreModel::getCourseId(const QString &courseName)
{
    if (courseName == "请选择课程" || courseName.isEmpty()) return -1;

    QSqlQuery query(m_db);
    query.exec(QString("SELECT course_id FROM course WHERE course_name='%1'").arg(courseName));
    return query.next() ? query.value(0).toInt() : -1;
}

// 查询学生某课程分数
int StudentScoreModel::getStudentScore(int stuId, int courseId) const
{
    if (courseId == -1) return -1;

    QSqlQuery query(m_db);
    query.exec(QString("SELECT score FROM score WHERE student_id=%1 AND course_id=%2").arg(stuId).arg(courseId));
    return query.next() ? query.value(0).toInt() : -1;
}
